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

void PyInstArchive::parseTOC() {
    fPtr.seekg(tableOfContentsPos, std::ios::beg);
    tocList.clear();
    uint32_t parsedLen = 0;

    while (parsedLen < tableOfContentsSize) {
        uint32_t entrySize;
        if (!readEntrySize(entrySize)) break;

        std::vector<char> nameBuffer(entrySize - sizeofEntry());
        uint32_t entryPos, cmprsdDataSize, uncmprsdDataSize;
        uint8_t cmprsFlag;
        char typeCmprsData;

        readEntryFields(entryPos, cmprsdDataSize, uncmprsdDataSize, cmprsFlag, typeCmprsData, nameBuffer, entrySize);

        std::string name = decodeEntryName(nameBuffer, parsedLen);
        addTOCEntry(entryPos, cmprsdDataSize, uncmprsdDataSize, cmprsFlag, typeCmprsData, name);

        parsedLen += entrySize;
    }

    qDebug() << "[+] Found " << tocList.size() << " files in CArchive" ;
}

bool PyInstArchive::readEntrySize(uint32_t& entrySize) {
    fPtr.read(reinterpret_cast<char*>(&entrySize), sizeof(entrySize));
    if (fPtr.gcount() < sizeof(entrySize)) return false;

    entrySize = swapBytes(entrySize);
    return true;
}

void PyInstArchive::readEntryFields(uint32_t& entryPos, uint32_t& cmprsdDataSize, uint32_t& uncmprsdDataSize, uint8_t& cmprsFlag, char& typeCmprsData, std::vector<char>& nameBuffer, uint32_t entrySize) {
    uint32_t nameLen = sizeofEntry();
    fPtr.read(reinterpret_cast<char*>(&entryPos), sizeof(entryPos));
    fPtr.read(reinterpret_cast<char*>(&cmprsdDataSize), sizeof(cmprsdDataSize));
    fPtr.read(reinterpret_cast<char*>(&uncmprsdDataSize), sizeof(uncmprsdDataSize));
    fPtr.read(reinterpret_cast<char*>(&cmprsFlag), sizeof(cmprsFlag));
    fPtr.read(reinterpret_cast<char*>(&typeCmprsData), sizeof(typeCmprsData));

    entryPos = swapBytes(entryPos);
    cmprsdDataSize = swapBytes(cmprsdDataSize);
    uncmprsdDataSize = swapBytes(uncmprsdDataSize);

    fPtr.read(nameBuffer.data(), entrySize - nameLen);
}

std::string PyInstArchive::decodeEntryName(std::vector<char>& nameBuffer, uint32_t parsedLen) {
    std::string name(nameBuffer.data(), nameBuffer.size());
    name.erase(std::remove(name.begin(), name.end(), '\0'), name.end());

    if (name.empty() || name[0] == '/') {
        name = "unnamed_" + std::to_string(parsedLen);
    }

    return name;
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
