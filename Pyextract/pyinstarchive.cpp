#include <QListWidget>
#include <filesystem>
#include <QDebug>

#ifdef Q_OS_WIN
#include <Qtzlib/zlib.h>
#elif defined(Q_OS_MAC)
#include <zlib.h>
#endif

#include "pyinstarchive.h"

const std::string PyInstArchive::MAGIC = "MEI\014\013\012\013\016";

PyInstArchive::PyInstArchive(const std::string& path) : filePath(path), fileSize(0), cookiePos(-1) {}

const std::vector<CTOCEntry>& PyInstArchive::getTOCList() const {
    return tocList;
}

bool PyInstArchive::checkFile() {
    qDebug() << "[+] Processing" << QString::fromStdString(filePath);
    const size_t searchChunkSize = 8192;

    qDebug() << "[+] Checking file validity...";
    if (!isFileValid(searchChunkSize)) {
        qDebug() << "[-] File invalid!";
        return false;
    }

    qDebug() << "[+] Finding cookie...";
    if (!findCookie(searchChunkSize)) {
        qDebug() << "[-] Cookie not found!";
        return false;
    }

    qDebug() << "[+] Determining PyInstaller version...";
    determinePyinstallerVersion();

    qDebug() << "[+] File check complete!";
    return true;
}

bool PyInstArchive::open() {
    fPtr.open(filePath, std::ios::binary);
    if (!fPtr.is_open()) {
        qDebug() << "[!] Error: Could not open " << filePath;
        return false;
    }
    fPtr.seekg(0, std::ios::end);
    fileSize = fPtr.tellg();
    fPtr.seekg(0, std::ios::beg);
    return true;
}

bool PyInstArchive::isFileValid(size_t searchChunkSize) {
    qDebug() << "[INFO] Checking file validity. File size:" << fileSize << "MAGIC size:" << MAGIC.size();

    if (fileSize < MAGIC.size()) {
        qDebug() << "[!] Error: File is too short or truncated. File size:" << fileSize << "MAGIC size:" << MAGIC.size();
        return false;
    }

    qDebug() << "[INFO] File is valid. File size is sufficient.";
    return true;
}

bool PyInstArchive::findCookie(size_t searchChunkSize) {
    uint64_t endPos = fileSize;
    cookiePos = -1;

    QByteArray buffer(searchChunkSize + MAGIC.size() - 1, '\0');

    while (endPos >= MAGIC.size()) {
        uint64_t startPos = (endPos >= searchChunkSize) ? endPos - searchChunkSize : 0;
        size_t chunkSize = endPos - startPos;

        fPtr.seekg(startPos, std::ios::beg);
        fPtr.read(buffer.data(), chunkSize);

        for (size_t i = chunkSize; i < buffer.size(); ++i) {
            buffer[i] = buffer[i - chunkSize];
        }

        for (size_t i = chunkSize; i-- > 0;) {
            if (std::memcmp(buffer.data() + i, MAGIC.c_str(), MAGIC.size()) == 0) {
                cookiePos = startPos + i;
                return true;
            }
        }

        endPos = startPos + MAGIC.size() - 1;
        if (startPos == 0) {
            break;
        }
    }

    qDebug() << "[!] Error: Missing cookie, unsupported pyinstaller version or not a pyinstaller archive";
    return false;
}

uint32_t swapBytes(uint32_t value) {
    return ((value >> 24) & 0x000000FF) |
           ((value >> 8) & 0x0000FF00) |
           ((value << 8) & 0x00FF0000) |
           ((value << 24) & 0xFF000000);
}

void PyInstArchive::determinePyinstallerVersion() {
    fPtr.seekg(cookiePos + PYINST20_COOKIE_SIZE, std::ios::beg);
    std::vector<char> buffer64(64);
    fPtr.read(buffer64.data(), 64);
    std::string bufferStr(buffer64.data(), buffer64.size());

    if (bufferStr.find("python") != std::string::npos) {
        qDebug() << "[+] Pyinstaller version: 2.1+" ;
        pyinstVer = 21;
    }
    else {
        qDebug() << "[+] Pyinstaller version: 2.0" ;
        pyinstVer = 20;
    }
}

bool PyInstArchive::getCArchiveInfo() {
    try {
        uint32_t lengthofPackage, toc, tocLen, pyver;
        readArchiveData(lengthofPackage, toc, tocLen, pyver);
        calculateOverlayInfo(lengthofPackage, toc, tocLen);
        parseTOC();

    }
    catch (...) {
        qDebug() << "[!] Error: The file is not a PyInstaller archive" ;
        return false;
    }
    return true;
}

void PyInstArchive::readArchiveData(uint32_t& lengthofPackage, uint32_t& toc, uint32_t& tocLen, uint32_t& pyver) {
    fPtr.seekg(cookiePos, std::ios::beg);
    char buffer[PYINST21_COOKIE_SIZE];
    fPtr.read(buffer, (pyinstVer == 20) ? PYINST20_COOKIE_SIZE : PYINST21_COOKIE_SIZE);

    if (pyinstVer == 20 || pyinstVer == 21) {
        lengthofPackage = swapBytes(*reinterpret_cast<uint32_t*>(buffer + 8));
        toc = swapBytes(*reinterpret_cast<uint32_t*>(buffer + 12));
        tocLen = swapBytes(*reinterpret_cast<uint32_t*>(buffer + 16));
        pyver = swapBytes(*reinterpret_cast<uint32_t*>(buffer + 20));
    }
}

void PyInstArchive::calculateOverlayInfo(uint32_t lengthofPackage, uint32_t toc, uint32_t tocLen) {
    uint64_t tailBytes = fileSize - cookiePos - ((pyinstVer == 20) ? PYINST20_COOKIE_SIZE : PYINST21_COOKIE_SIZE);
    overlaySize = static_cast<uint64_t>(lengthofPackage) + tailBytes;
    overlayPos = fileSize - overlaySize;
    tableOfContentsPos = overlayPos + toc;
    tableOfContentsSize = tocLen;
}

bool PyInstArchive::parseTOC() {
    uint32_t parsedLen = 0;
    qDebug() << "[DEBUG] Starting TOC parsing. tableOfContentsSize=" << tableOfContentsSize;

    // Seek to the TOC start
    fPtr.seekg(tableOfContentsPos, std::ios::beg);
    if (fPtr.fail()) {
        qDebug() << "[DEBUG] Failed to seek to TOC start at position=" << tableOfContentsPos;
        return false;
    }
    qDebug() << "[DEBUG] File pointer positioned at TOC start: " << fPtr.tellg();

    while (parsedLen < tableOfContentsSize) {
        qDebug() << "[DEBUG] Parsing TOC entry at parsedLen=" << parsedLen << ", current file position=" << fPtr.tellg();

        uint32_t entrySize;
        if (!readEntrySize(entrySize)) {
            qDebug() << "[DEBUG] Failed to read entrySize. Stopping TOC parsing.";
            break;
        }

        if (entrySize < sizeofEntry() || entrySize > 1024) { // Arbitrary max name length
            qDebug() << "[DEBUG] Invalid entrySize=" << entrySize << ". Skipping entry.";
            parsedLen += sizeof(entrySize); // Move past the invalid entrySize
            fPtr.seekg(parsedLen + tableOfContentsPos, std::ios::beg); // Reposition file pointer
            if (fPtr.fail()) {
                qDebug() << "[DEBUG] Failed to seek after invalid entrySize. Stopping TOC parsing.";
                break;
            }
            continue; // Skip to next entry
        }

        std::vector<char> nameBuffer(entrySize - sizeofEntry());
        uint32_t entryPos, cmprsdDataSize, uncmprsdDataSize;
        uint8_t cmprsFlag;
        char typeCmprsData;

        if (!readEntryFields(entryPos, cmprsdDataSize, uncmprsdDataSize, cmprsFlag, typeCmprsData, nameBuffer, entrySize)) {
            qDebug() << "[DEBUG] Failed to read entry fields. Skipping entry.";
            parsedLen += entrySize;
            fPtr.seekg(parsedLen + tableOfContentsPos, std::ios::beg); // Reposition file pointer
            if (fPtr.fail()) {
                qDebug() << "[DEBUG] Failed to seek after failed entry fields. Stopping TOC parsing.";
                break;
            }
            continue; // Skip to next entry
        }

        // Initialize CTOCEntry with constructor
        CTOCEntry entry(entryPos, cmprsdDataSize, uncmprsdDataSize, cmprsFlag, typeCmprsData, decodeEntryName(nameBuffer, parsedLen));
        tocList.emplace_back(entry);

        parsedLen += entrySize;
        qDebug() << "[DEBUG] Added TOC entry. New parsedLen=" << parsedLen << ", entrySize=" << entrySize;
    }

    qDebug() << "[DEBUG] TOC parsing complete. Found " << tocList.size() << " entries.";
    return !tocList.empty();
}

bool needsByteSwap = true; // Set to false for macOS archives manually or with detection logic

bool PyInstArchive::readEntrySize(uint32_t& entrySize) {
    qDebug() << "[DEBUG] readEntrySize: Current file position=" << fPtr.tellg();

    char buffer[sizeof(entrySize)];
    fPtr.read(buffer, sizeof(entrySize));
    if (fPtr.gcount() < sizeof(entrySize)) {
        qDebug() << "[DEBUG] readEntrySize: Failed to read 4 bytes for entrySize. gcount=" << fPtr.gcount();
        return false;
    }

    // Log raw bytes
    QString hexDump;
    for (size_t i = 0; i < sizeof(entrySize); ++i) {
        hexDump += QString("%1 ").arg(static_cast<unsigned char>(buffer[i]), 2, 16, QChar('0'));
    }
    qDebug() << "[DEBUG] readEntrySize: Raw entrySize bytes=" << hexDump;

    // Copy bytes to entrySize
    memcpy(&entrySize, buffer, sizeof(entrySize));
    entrySize = swapBytes(entrySize);
    qDebug() << "[DEBUG] readEntrySize: Swapped entrySize=" << entrySize;
    return true;
}



bool PyInstArchive::readEntryFields(uint32_t& entryPos, uint32_t& cmprsdDataSize, uint32_t& uncmprsdDataSize, uint8_t& cmprsFlag, char& typeCmprsData, std::vector<char>& nameBuffer, uint32_t entrySize) {
    uint32_t nameLen = sizeofEntry(); // 18 bytes: 4 + 4*3 + 1 + 1
    qDebug() << "[DEBUG] readEntryFields: entrySize=" << entrySize << ", nameLen=" << (entrySize - nameLen);

    fPtr.read(reinterpret_cast<char*>(&entryPos), sizeof(entryPos));
    fPtr.read(reinterpret_cast<char*>(&cmprsdDataSize), sizeof(cmprsdDataSize));
    fPtr.read(reinterpret_cast<char*>(&uncmprsdDataSize), sizeof(uncmprsdDataSize));
    fPtr.read(reinterpret_cast<char*>(&cmprsFlag), sizeof(cmprsFlag));
    fPtr.read(reinterpret_cast<char*>(&typeCmprsData), sizeof(typeCmprsData));

    if (fPtr.gcount() < (sizeof(entryPos) + sizeof(cmprsdDataSize) + sizeof(uncmprsdDataSize) + sizeof(cmprsFlag) + sizeof(typeCmprsData))) {
        qDebug() << "[DEBUG] readEntryFields: Failed to read fixed fields. gcount=" << fPtr.gcount();
        return false;
    }

    entryPos = swapBytes(entryPos);
    cmprsdDataSize = swapBytes(cmprsdDataSize);
    uncmprsdDataSize = swapBytes(uncmprsdDataSize);

    fPtr.read(nameBuffer.data(), entrySize - nameLen);
    if (fPtr.gcount() < static_cast<std::streamsize>(entrySize - nameLen)) {
        qDebug() << "[DEBUG] readEntryFields: Failed to read nameBuffer. Expected=" << (entrySize - nameLen) << ", gcount=" << fPtr.gcount();
        return false;
    }

    QString hexDump;
    for (char c : nameBuffer) {
        hexDump += QString("%1 ").arg(static_cast<unsigned char>(c), 2, 16, QChar('0'));
    }
    qDebug() << "[DEBUG] readEntryFields: nameBuffer hex=" << hexDump;
    return true;
}




std::string PyInstArchive::decodeEntryName(std::vector<char>& nameBuffer, uint32_t parsedLen) {
    size_t nullPos = 0;
    for (size_t i = 0; i < nameBuffer.size(); ++i) {
        if (nameBuffer[i] == '\0') {
            nullPos = i;
            break;
        }
    }
    if (nullPos == 0) nullPos = nameBuffer.size();

    QString hexDump;
    for (size_t i = 0; i < nullPos; ++i) {
        hexDump += QString("%1 ").arg(static_cast<unsigned char>(nameBuffer[i]), 2, 16, QChar('0'));
    }
    qDebug() << "[DEBUG] Raw nameBuffer (hex, up to null):" << hexDump;

    QString name = QString::fromUtf8(nameBuffer.data(), nullPos);
    if (name.contains(QChar(0xFFFD))) {
        qDebug() << "[DEBUG] Invalid UTF-8, falling back to Latin-1 (Windows-1252 approximation)";
        name = QString::fromLatin1(nameBuffer.data(), nullPos);
    } else {
        qDebug() << "[DEBUG] UTF-8 decoding successful";
    }

    name = name.trimmed().replace(QChar(0), "");

    bool isValid = true;
    if (name.isEmpty() || name.startsWith('/')) {
        isValid = false;
    } else {
        for (QChar c : name) {
            if (c.unicode() < 32 || c == QChar(':') || c == QChar('\\') || c == QChar('*') ||
                c == QChar('?') || c == QChar('"') || c == QChar('<') || c == QChar('>') || c == QChar('|')) {
                isValid = false;
                break;
            }
        }
    }

    if (!isValid) {
        qDebug() << "[DEBUG] Using fallback name for invalid or non-printable name:" << name;
        name = QString("unnamed_%1").arg(parsedLen);
    }

    qDebug() << "[DEBUG] Decoded name:" << name;
    return name.toStdString();
}

void PyInstArchive::addTOCEntry(uint32_t entryPos, uint32_t cmprsdDataSize, uint32_t uncmprsdDataSize, uint8_t cmprsFlag, char typeCmprsData, const std::string& name) {
    tocList.emplace_back(
        overlayPos + entryPos,
        cmprsdDataSize,
        uncmprsdDataSize,
        cmprsFlag,
        typeCmprsData,
        name
        );
}

uint32_t PyInstArchive::sizeofEntry() const {
    return sizeof(uint32_t) + sizeof(uint32_t) * 3 + sizeof(uint8_t) + sizeof(char);
}

void PyInstArchive::decompressAndExtractFile(const CTOCEntry& tocEntry, const std::string& outputDir, std::mutex& mtx, std::mutex& printMtx) {
    std::vector<char> compressedData;

    {
        std::lock_guard<std::mutex> lock(mtx);
        fPtr.seekg(tocEntry.position, std::ios::beg);
        compressedData.resize(tocEntry.getCompressedDataSize());
        fPtr.read(compressedData.data(), tocEntry.getCompressedDataSize());
    }

    std::vector<char> decompressedData;
    if (tocEntry.isCompressed()) {
        decompressedData.resize(tocEntry.uncmprsdDataSize);

        z_stream strm = {};
        strm.avail_in = static_cast<uInt>(tocEntry.getCompressedDataSize());
        strm.next_in = reinterpret_cast<Bytef*>(compressedData.data());
        strm.avail_out = static_cast<uInt>(tocEntry.uncmprsdDataSize);
        strm.next_out = reinterpret_cast<Bytef*>(decompressedData.data());

        if (inflateInit(&strm) != Z_OK) {
            std::lock_guard<std::mutex> lock(printMtx);
            qDebug() << "[!] Error: Could not initialize zlib for decompression\n";
            return;
        }

        int result = inflate(&strm, Z_FINISH);
        inflateEnd(&strm);

        if (result != Z_STREAM_END) {
            std::lock_guard<std::mutex> lock(printMtx);
            qDebug() << "[!] Error: Decompression failed for " << tocEntry.getName() << "\n";
            return;
        }
    }
    else {
        decompressedData = std::move(compressedData);
    }

    // Extract File
    std::filesystem::path outputFilePath = std::filesystem::path(outputDir) / tocEntry.getName();
    std::filesystem::create_directories(outputFilePath.parent_path());

    {
        std::ofstream outFile(outputFilePath, std::ios::binary);
        if (!outFile.is_open()) {
            std::lock_guard<std::mutex> lock(printMtx);
            qDebug() << "[!] Error: Could not open output file " << QString::fromStdString(outputFilePath.string());
            return;
        }
        outFile.write(decompressedData.data(), decompressedData.size());
    }

    {
        std::lock_guard<std::mutex> lock(printMtx);
        qDebug() << "[+] Extracted: " << tocEntry.getName() << " (" << decompressedData.size() << " bytes)\n";
    }
}

void PyInstArchive::displayInfo(QListWidget* listWidget) {
    if (!listWidget) return;
    listWidget->clear();

    for (const auto& entry : tocList) {
        QString itemText = QString::fromStdString(entry.getName());
        listWidget->addItem(itemText);
    }
}

void PyInstArchive::timeExtractionProcess(const std::string& outputDir) {

    auto start = std::chrono::high_resolution_clock::now();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    qDebug() << "[*] Extraction completed in" << elapsed.count() << "seconds.";
}
