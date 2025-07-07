#include <QListWidget>
#include <filesystem>
#include <QDebug>
#include <iostream>
#include <QUuid>

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
		std::vector<char> buffer(searchChunkSize + MAGIC.size() - 1);

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

		std::cerr << "[!] Error: Missing cookie, unsupported pyinstaller version or not a pyinstaller archive" << std::endl;
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
		if (fPtr.gcount() < 64) {
				qDebug() << "[!] Error: Failed to read version detection buffer, got" << fPtr.gcount() << "bytes";
				throw std::runtime_error("Incomplete version buffer read");
		}

		std::string bufferStr(buffer64.data(), buffer64.size());
		// Convert buffer to lowercase for case-insensitive comparison
		std::string bufferLower = bufferStr;
		std::transform(bufferLower.begin(), bufferLower.end(), bufferLower.begin(), ::tolower);

		// Log the buffer for debugging
		QByteArray byteArray(buffer64.data(), buffer64.size());
		qDebug() << "[DEBUG] Version detection buffer (hex):" << byteArray.toHex();
		qDebug() << "[DEBUG] Version detection buffer (string):" << QString::fromStdString(bufferStr);

		if (bufferLower.find("python") != std::string::npos) {
				qDebug() << "[+] Pyinstaller version: 2.1+";
				pyinstVer = 21;
		}
		else {
				qDebug() << "[+] Pyinstaller version: 2.0";
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
				qDebug() << "[!] Error: The file is not a PyInstaller archive";
				return false;
		}
		return true;
}

void PyInstArchive::readArchiveData(uint32_t& lengthofPackage, uint32_t& toc, uint32_t& tocLen, uint32_t& pyver) {
		fPtr.seekg(cookiePos, std::ios::beg);
		char buffer[PYINST21_COOKIE_SIZE];
		size_t cookieSize = (pyinstVer == 20) ? PYINST20_COOKIE_SIZE : PYINST21_COOKIE_SIZE;
		fPtr.read(buffer, cookieSize);

		if (fPtr.gcount() < static_cast<std::streamsize>(cookieSize)) {
				qDebug() << "[!] Error: Failed to read cookie, expected" << cookieSize << "bytes, got" << fPtr.gcount();
				throw std::runtime_error("Incomplete cookie read");
		}

		// Verify magic
		if (std::memcmp(buffer, MAGIC.c_str(), MAGIC.size()) != 0) {
				qDebug() << "[!] Error: Invalid magic in cookie";
				throw std::runtime_error("Invalid PyInstaller archive");
		}

		// Parse common fields (magic, lengthofPackage, toc, tocLen, pyver)
		lengthofPackage = swapBytes(*reinterpret_cast<uint32_t*>(buffer + 8));
		toc = swapBytes(*reinterpret_cast<uint32_t*>(buffer + 12));
		tocLen = swapBytes(*reinterpret_cast<uint32_t*>(buffer + 16));
		pyver = swapBytes(*reinterpret_cast<uint32_t*>(buffer + 20));

		// Log raw cookie for debugging
		QByteArray byteArray(buffer, cookieSize);
		qDebug() << "[DEBUG] Raw cookie (hex):" << byteArray.toHex();

		// For newer PyInstaller versions, read additional fields if present
		if (pyinstVer == 21 && cookieSize > 84) {
				qDebug() << "[DEBUG] Detected potential newer PyInstaller version, ignoring extra cookie bytes";
		}

		qDebug() << "[DEBUG] Cookie data: lengthofPackage=" << lengthofPackage
				<< ", toc=" << toc << ", tocLen=" << tocLen << ", pyver=" << pyver;
}

void PyInstArchive::calculateOverlayInfo(uint32_t lengthofPackage, uint32_t toc, uint32_t tocLen) {
		uint64_t tailBytes = fileSize - cookiePos - ((pyinstVer == 20) ? PYINST20_COOKIE_SIZE : PYINST21_COOKIE_SIZE);
		overlaySize = static_cast<uint64_t>(lengthofPackage) + tailBytes;
		overlayPos = fileSize - overlaySize;
		tableOfContentsPos = overlayPos + toc;
		tableOfContentsSize = tocLen;

		uint64_t altTableOfContentsPos = cookiePos + ((pyinstVer == 20) ? PYINST20_COOKIE_SIZE : PYINST21_COOKIE_SIZE) + toc;
		qDebug() << "[DEBUG] Standard tableOfContentsPos=" << tableOfContentsPos
				<< ", Alternative tableOfContentsPos=" << altTableOfContentsPos;

		if (tableOfContentsPos >= fileSize || tableOfContentsPos + tableOfContentsSize > fileSize) {
				qDebug() << "[!] Error: Invalid tableOfContentsPos=" << tableOfContentsPos
						<< ", fileSize=" << fileSize << ", tocLen=" << tableOfContentsSize;
				qDebug() << "[DEBUG] Trying alternative tableOfContentsPos=" << altTableOfContentsPos;
				tableOfContentsPos = altTableOfContentsPos;
				if (tableOfContentsPos >= fileSize || tableOfContentsPos + tableOfContentsSize > fileSize) {
						throw std::runtime_error("Table of Contents position out of bounds");
				}
		}

		qDebug() << "[DEBUG] Overlay: size=" << overlaySize << ", pos=" << overlayPos;
		qDebug() << "[DEBUG] TOC: pos=" << tableOfContentsPos << ", size=" << tableOfContentsSize;
}

void PyInstArchive::parseTOC() {
		fPtr.seekg(tableOfContentsPos, std::ios::beg);
		tocList.clear();
		uint32_t parsedLen = 0;

		qDebug() << "[DEBUG] Parsing TOC at position" << tableOfContentsPos << ", size" << tableOfContentsSize;

		while (parsedLen < tableOfContentsSize) {
				uint32_t entrySize;
				if (!readEntrySize(entrySize)) {
						qDebug() << "[!] Warning: Failed to read TOC entry size at parsedLen=" << parsedLen;
						break;
				}

				try {
						std::vector<char> nameBuffer(entrySize - sizeofEntry());
						uint32_t entryPos, cmprsdDataSize, uncmprsdDataSize;
						uint8_t cmprsFlag;
						char typeCmprsData;

						readEntryFields(entryPos, cmprsdDataSize, uncmprsdDataSize, cmprsFlag, typeCmprsData, nameBuffer, entrySize);

						std::string name = decodeEntryName(nameBuffer, parsedLen);
						addTOCEntry(entryPos, cmprsdDataSize, uncmprsdDataSize, cmprsFlag, typeCmprsData, name);

						parsedLen += entrySize;
				}
				catch (const std::exception& e) {
						qDebug() << "[!] Warning: Failed to parse TOC entry at parsedLen=" << parsedLen << ":" << e.what();
						parsedLen += entrySize; 
						continue;
				}
		}

		if (parsedLen != tableOfContentsSize) {
				qDebug() << "[!] Warning: Parsed" << parsedLen << "bytes, expected" << tableOfContentsSize;
		}

		qDebug() << "[+] Found " << tocList.size() << " files in CArchive";
}

bool PyInstArchive::readEntrySize(uint32_t& entrySize) {
		fPtr.read(reinterpret_cast<char*>(&entrySize), sizeof(entrySize));
		if (fPtr.gcount() < static_cast<std::streamsize>(sizeof(entrySize))) {
				qDebug() << "[!] Error: Failed to read TOC entry size, expected" << sizeof(entrySize)
						<< "bytes, got" << fPtr.gcount();
				return false;
		}

		entrySize = swapBytes(entrySize);
		// Validate entrySize
		if (entrySize < sizeofEntry() || entrySize > tableOfContentsSize) {
				qDebug() << "[!] Error: Invalid TOC entry size=" << entrySize
						<< ", expected at least" << sizeofEntry() << ", max" << tableOfContentsSize;
				return false;
		}

		qDebug() << "[DEBUG] TOC entrySize=" << entrySize;
		return true;
}

void PyInstArchive::readEntryFields(uint32_t& entryPos, uint32_t& cmprsdDataSize, uint32_t& uncmprsdDataSize,
		uint8_t& cmprsFlag, char& typeCmprsData, std::vector<char>& nameBuffer,
		uint32_t entrySize) {
		// Log current file position
		std::streampos currentPos = fPtr.tellg();
		qDebug() << "[DEBUG] Reading TOC entry at file position=" << currentPos;

		uint32_t nameLen = sizeofEntry();
		if (entrySize < nameLen) {
				qDebug() << "[!] Error: TOC entry size too small:" << entrySize << ", expected at least" << nameLen;
				throw std::runtime_error("Invalid TOC entry size");
		}

		char buffer[14]; 
		fPtr.read(buffer, sizeof(buffer));
		size_t bytesRead = fPtr.gcount();

		if (bytesRead < sizeof(buffer)) {
				qDebug() << "[!] Error: Failed to read TOC entry fields, expected" << sizeof(buffer)
						<< "bytes, got" << bytesRead << "at position" << currentPos;
				throw std::runtime_error("Incomplete TOC entry read");
		}

		entryPos = swapBytes(*reinterpret_cast<uint32_t*>(buffer));
		cmprsdDataSize = swapBytes(*reinterpret_cast<uint32_t*>(buffer + 4));
		uncmprsdDataSize = swapBytes(*reinterpret_cast<uint32_t*>(buffer + 8));
		cmprsFlag = *reinterpret_cast<uint8_t*>(buffer + 12);
		typeCmprsData = *reinterpret_cast<char*>(buffer + 13);

		size_t nameBufferSize = entrySize - nameLen;
		nameBuffer.resize(nameBufferSize);
		fPtr.read(nameBuffer.data(), nameBufferSize);

		if (fPtr.gcount() < static_cast<std::streamsize>(nameBufferSize)) {
				qDebug() << "[!] Error: Failed to read TOC entry name, expected" << nameBufferSize
						<< "bytes, got" << fPtr.gcount() << "at position" << fPtr.tellg();
				throw std::runtime_error("Incomplete TOC entry name read");
		}

		QByteArray byteArray(nameBuffer.data(), nameBufferSize);
		qDebug() << "[DEBUG] TOC entry: entryPos=" << entryPos
				<< ", cmprsdDataSize=" << cmprsdDataSize
				<< ", uncmprsdDataSize=" << uncmprsdDataSize
				<< ", cmprsFlag=" << cmprsFlag
				<< ", typeCmprsData=" << typeCmprsData
				<< ", nameBuffer (hex)=" << byteArray.toHex();
}


std::string PyInstArchive::decodeEntryName(std::vector<char>& nameBuffer, uint32_t parsedLen) {
		// Try to interpret the buffer as a UTF-8 string
		QString qName = QString::fromUtf8(nameBuffer.data(), nameBuffer.size());
		std::string name = qName.toStdString();

		name.erase(std::remove(name.begin(), name.end(), '\0'), name.end());

		// Check if the name is valid
		bool isValid = !name.empty() && name[0] != '/' && name[0] != '\\' &&
				name.find_first_not_of(" \t\n\r") != std::string::npos;

		// Additional check for non-printable or invalid characters
		for (char c : name) {
				if (c < 32 || c > 126) {
						isValid = false;
						break;
				}
		}

		if (!isValid) {
				name = "unnamed_" + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString() + "_" + std::to_string(parsedLen);
				qDebug() << "[!] Warning: Invalid or unreadable file name, using fallback:" << QString::fromStdString(name);
		}

		std::replace(name.begin(), name.end(), '..', '__');

		return name;
}

void PyInstArchive::addTOCEntry(uint32_t entryPos, uint32_t cmprsdDataSize, uint32_t uncmprsdDataSize, uint8_t cmprsFlag, char typeCmprsData, std::string name) {
		if ((typeCmprsData == 's' || typeCmprsData == 'm') && name.find('.') == std::string::npos) {
				name += ".pyc";
		}
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
		return sizeof(uint32_t) +
				sizeof(uint32_t) +
				sizeof(uint32_t) +
				sizeof(uint32_t) +
				sizeof(uint8_t) +
				sizeof(char);
}

void PyInstArchive::decompressAndExtractFile(const CTOCEntry& tocEntry, const std::string& outputDir, std::mutex& mtx, std::mutex& printMtx) {
		std::vector<char> compressedData;

		// Read Compressed Data with File Lock
		{
				std::lock_guard<std::mutex> lock(mtx);
				fPtr.seekg(tocEntry.position, std::ios::beg);
				compressedData.resize(tocEntry.getCompressedDataSize());
				fPtr.read(compressedData.data(), tocEntry.getCompressedDataSize());
		}

		// Decompress Data
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
						std::cerr << "[!] Error: Could not initialize zlib for decompression\n";
						return;
				}

				int result = inflate(&strm, Z_FINISH);
				inflateEnd(&strm);

				if (result != Z_STREAM_END) {
						std::lock_guard<std::mutex> lock(printMtx);
						std::cerr << "[!] Error: Decompression failed for " << tocEntry.getName() << "\n";
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
						std::cerr << "[!] Error: Could not open output file " << outputFilePath << "\n";
						return;
				}
				outFile.write(decompressedData.data(), decompressedData.size());
		}

		// Log Extraction Success
		{
				std::lock_guard<std::mutex> lock(printMtx);
				std::cout << "[+] Extracted: " << tocEntry.getName() << " (" << decompressedData.size() << " bytes)\n";
		}
}

void PyInstArchive::displayInfo(QListWidget* listWidget) {
		if (!listWidget) {
				qDebug() << "[!] Error: QListWidget is null";
				return;
		}

		// Set a fallback font to avoid font-related crashes
		QFont fallbackFont("Arial", 10);
		listWidget->setFont(fallbackFont);

		listWidget->clear();
		for (const auto& entry : tocList) {
				try {
						QString itemText = QString::fromStdString(entry.getName());
						if (itemText.isEmpty()) {
								itemText = "Unnamed_File";
								qDebug() << "[!] Warning: Empty TOC entry name, using fallback name";
						}
						qDebug() << "[+] Adding TOC Entry:" << itemText;
						listWidget->addItem(itemText);
				}
				catch (const std::exception& e) {
						qDebug() << "[!] Error adding item to QListWidget:" << e.what();
				}
		}
}

void PyInstArchive::timeExtractionProcess(const std::string& outputDir) {

		auto start = std::chrono::high_resolution_clock::now();

		auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> elapsed = end - start;

		qDebug() << "[*] Extraction completed in" << elapsed.count() << "seconds.";
}
