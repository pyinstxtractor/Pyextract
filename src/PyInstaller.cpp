#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <random>
#include <sstream>
#include <filesystem>
#include <queue>
#include <condition_variable>
#include <future>
#include <iomanip>
#include <chrono>
#include <Windows.h>

#include "../include/PyInstArchive.h"
#include "../include/zlib.h"

/**
 * @brief The magic string used to identify PyInstaller archives.
 *
 * This constant represents a specific sequence of bytes (`MEI\014\013\012\013\016`)
 * that is used by PyInstaller to mark the beginning of the archive's metadata.
 * The program uses this to verify if a file is a valid PyInstaller-generated archive.
 */
const std::string PyInstArchive::MAGIC = "MEI\014\013\012\013\016";

PyInstArchive::PyInstArchive(const std::string& path) : filePath(path), fileSize(0), cookiePos(-1) {}

/**
 * @brief Opens the PyInstaller archive file for reading.
 *
 * This method attempts to open the file at the provided `filePath` in binary mode.
 * It also checks if the file is successfully opened and calculates its size.
 *
 * @return true if the file was successfully opened, false otherwise.
 */
bool PyInstArchive::open() {
		fPtr.open(filePath, std::ios::binary);
		if (!fPtr.is_open()) {
				std::cerr << "[!] Error: Could not open " << filePath << std::endl;
				return false;
		}
		fPtr.seekg(0, std::ios::end);
		fileSize = fPtr.tellg();
		fPtr.seekg(0, std::ios::beg);
		return true;
}

/**
 * @brief Closes the file stream if it is open.
 *
 * This method ensures that the file stream associated with the PyInstaller archive
 * is properly closed when it is no longer needed. It prevents resource leaks by
 * releasing the file handle.
 */
void PyInstArchive::close() {
		if (fPtr.is_open()) {
				fPtr.close();
		}
}

/**
 * @brief Retrieves the list of Table of Contents (TOC) entries from the PyInstaller archive.
 *
 * This method returns a constant reference to the vector containing the TOC entries.
 * The TOC entries represent individual files within the PyInstaller archive, including their
 * positions, compressed sizes, uncompressed sizes, compression flags, data types, and names.
 *
 * @return A constant reference to a vector of CTOCEntry objects representing the TOC entries.
 *
 * @note The vector returned by this method is read-only, ensuring the TOC entries cannot be modified
 *       directly through the returned reference. To modify the TOC entries, use appropriate member functions.
 */
const std::vector<CTOCEntry>& PyInstArchive::getTOCList() const {
		return tocList;
}

/**
 * @brief Check if the file is a valid PyInstaller archive and determine its version.
 *
 * This function processes the file specified by filePath, verifies if it's a valid PyInstaller
 * archive by searching for the magic string, and determines the PyInstaller version used to create the archive.
 *
 * @return True if the file is valid and the version is determined, otherwise false.
 */
bool PyInstArchive::checkFile() {
		std::cout << "[+] Processing " << filePath << std::endl;
		const size_t searchChunkSize = 8192;

		if (!isFileValid(searchChunkSize)) {
				return false;
		}

		if (!findCookie(searchChunkSize)) {
				return false;
		}

		determinePyinstallerVersion();
		return true;
}

/**
 * @brief Validate the file size to ensure it's large enough to contain the magic string.
 *
 * This function checks if the file size is smaller than the size of the magic string,
 * which would indicate that the file is too short or truncated to be a valid PyInstaller archive.
 *
 * @param searchChunkSize The size of the chunk to be searched.
 * @return True if the file size is valid, otherwise false.
 */
bool PyInstArchive::isFileValid(size_t searchChunkSize) {
		if (fileSize < MAGIC.size()) {
				std::cerr << "[!] Error: File is too short or truncated" << std::endl;
				return false;
		}
		return true;
}

/**
 * @brief Search for the magic string in the file to find the cookie position.
 *
 * This function reads through the file in chunks, searching for the magic string that indicates
 * the start of the PyInstaller archive's metadata. It sets the cookie position if the magic string is found.
 *
 * @param searchChunkSize The size of the chunk to be searched.
 * @return True if the cookie position is found, otherwise false.
 */
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

/**
 * @brief Determine the version of the PyInstaller used to create the archive.
 *
 * This function reads a specific section of the file and checks for the presence of the word "python".
 * If found, it sets the PyInstaller version to 2.1 or higher; otherwise, it sets the version to 2.0.
 */
void PyInstArchive::determinePyinstallerVersion() {
		fPtr.seekg(cookiePos + PYINST20_COOKIE_SIZE, std::ios::beg);
		std::vector<char> buffer64(64);
		fPtr.read(buffer64.data(), 64);

		if (fPtr.gcount() < 64) {
				throw std::runtime_error("Incomplete version buffer read");
		}

		std::string bufferStr(buffer64.data(), buffer64.size());
		std::string bufferLower = bufferStr;
		std::transform(bufferLower.begin(), bufferLower.end(), bufferLower.begin(), ::tolower);

		if (bufferLower.find("python") != std::string::npos) {
				pyinstVer = 21; // PyInstaller 2.1+
		}
		else {
				pyinstVer = 20; // PyInstaller 2.0
		}
}

/**
 * @brief Swaps the byte order of a 32-bit integer to correct endianness.
 *
 * This function is used to convert multi-byte integers between different
 * byte orders (big-endian to little-endian or vice versa).
 *
 * @param value The 32-bit integer whose bytes need to be swapped.
 * @return The integer with swapped byte order.
 */
uint32_t swapBytes(uint32_t value) {
		return ((value >> 24) & 0x000000FF) |
				((value >> 8) & 0x0000FF00) |
				((value << 8) & 0x00FF0000) |
				((value << 24) & 0xFF000000);
}

/**
 * @brief Retrieves the number of physical CPU cores on the system.
 *
 * This function uses the Windows API to obtain information about the system's logical processors
 * and their relationship to physical CPU cores. It first determines the required buffer size for
 * the processor information, allocates the buffer, and then retrieves the information.
 *
 * The function iterates through the retrieved data to count the number of physical cores and
 * returns this count. If an error occurs at any stage, the function outputs an error message and
 * returns a default value of 1.
 *
 * @return The number of physical CPU cores on the system. If an error occurs, returns 1.
 *
 * @note This function is platform-specific and intended for use on Windows systems.
 * @note The function uses `malloc` for buffer allocation and `free` for deallocation.
 */
size_t getPhysicalCoreCount() {
		DWORD length = 0;
		// Initial call to get buffer size
		GetLogicalProcessorInformation(nullptr, &length);
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
				std::cerr << "[!] Error: Unable to determine buffer size for processor information.\n";
				return 1; // Default to 1 if unable to determine
		}

		// Allocate buffer for processor information
		SYSTEM_LOGICAL_PROCESSOR_INFORMATION* buffer = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION*>(malloc(length));
		if (buffer == nullptr) {
				std::cerr << "[!] Error: Memory allocation failed.\n";
				return 1;
		}

		// Retrieve processor information
		if (!GetLogicalProcessorInformation(buffer, &length)) {
				std::cerr << "[!] Error: Unable to get logical processor information.\n";
				free(buffer);
				return 1;
		}

		DWORD processorCoreCount = 0;
		DWORD count = length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);

		// Count the number of physical cores
		for (DWORD i = 0; i < count; ++i) {
				if (buffer[i].Relationship == RelationProcessorCore) {
						processorCoreCount++;
				}
		}

		free(buffer);
		return static_cast<size_t>(processorCoreCount);
}

/**
 * @brief Get information about the CArchive.
 *
 * This function reads the PyInstaller archive to extract metadata, such as the Python version,
 * table of contents position, and sizes, and overlays size and position.
 *
 * @return True if the information is successfully extracted, otherwise false.
 */
bool PyInstArchive::getCArchiveInfo() {
		try {
				uint32_t lengthofPackage, toc, tocLen, pyver;
				readArchiveData(lengthofPackage, toc, tocLen, pyver);
				calculateOverlayInfo(lengthofPackage, toc, tocLen);

#ifdef _DEBUG
				debugOutput(lengthofPackage);
#endif

				parseTOC();

#ifdef _DEBUG
				debugEntrySizes();
#endif

		}
		catch (...) {
				std::cerr << "[!] Error: The file is not a PyInstaller archive" << std::endl;
				return false;
		}
		return true;
}

/**
 * @brief Read the archive data and extract necessary values.
 *
 * This function reads the PyInstaller archive's cookie section to extract metadata,
 * such as the length of the package, table of contents position and length, and Python version.
 *
 * @param lengthofPackage Reference to store the length of the package.
 * @param toc Reference to store the position of the table of contents.
 * @param tocLen Reference to store the length of the table of contents.
 * @param pyver Reference to store the Python version.
 */
void PyInstArchive::readArchiveData(uint32_t& lengthofPackage, uint32_t& toc, uint32_t& tocLen, uint32_t& pyver) {
		fPtr.seekg(cookiePos, std::ios::beg);
		char buffer[PYINST21_COOKIE_SIZE];
		size_t cookieSize = (pyinstVer == 20) ? PYINST20_COOKIE_SIZE : PYINST21_COOKIE_SIZE;
		fPtr.read(buffer, cookieSize);

		if (fPtr.gcount() < static_cast<std::streamsize>(cookieSize)) {
				throw std::runtime_error("Incomplete cookie read");
		}

		// Verify magic
		if (std::memcmp(buffer, MAGIC.c_str(), MAGIC.size()) != 0) {
				throw std::runtime_error("Invalid PyInstaller archive");
		}

		// Parse common fields (magic, lengthofPackage, toc, tocLen, pyver)
		lengthofPackage = swapBytes(*reinterpret_cast<uint32_t*>(buffer + 8));
		toc = swapBytes(*reinterpret_cast<uint32_t*>(buffer + 12));
		tocLen = swapBytes(*reinterpret_cast<uint32_t*>(buffer + 16));
		pyver = swapBytes(*reinterpret_cast<uint32_t*>(buffer + 20));
}


/**
 * @brief Calculate the overlay size and position, and table of contents position and size.
 *
 * This function calculates the overlay size and position, and the table of contents position and size
 * based on the extracted archive metadata.
 *
 * @param lengthofPackage The length of the package extracted from the archive.
 * @param toc The position of the table of contents extracted from the archive.
 * @param tocLen The length of the table of contents extracted from the archive.
 */
void PyInstArchive::calculateOverlayInfo(uint32_t lengthofPackage, uint32_t toc, uint32_t tocLen) {
		uint64_t tailBytes = fileSize - cookiePos - ((pyinstVer == 20) ? PYINST20_COOKIE_SIZE : PYINST21_COOKIE_SIZE);
		overlaySize = static_cast<uint64_t>(lengthofPackage) + tailBytes;
		overlayPos = fileSize - overlaySize;
		tableOfContentsPos = overlayPos + toc;
		tableOfContentsSize = tocLen;

		uint64_t altTableOfContentsPos = cookiePos + ((pyinstVer == 20) ? PYINST20_COOKIE_SIZE : PYINST21_COOKIE_SIZE) + toc;

		if (tableOfContentsPos >= fileSize || tableOfContentsPos + tableOfContentsSize > fileSize) {
				tableOfContentsPos = altTableOfContentsPos;
				if (tableOfContentsPos >= fileSize || tableOfContentsPos + tableOfContentsSize > fileSize) {
						throw std::runtime_error("Table of Contents position out of bounds");
				}
		}
}

#ifdef _DEBUG
/**
 * @brief Output debug information about the archive.
 *
 * This function outputs debug information about the overlay size and position,
 * and the table of contents position and size.
 *
 * @param lengthofPackage The length of the package extracted from the archive.
 */
void PyInstArchive::debugOutput(uint32_t lengthofPackage) {
		std::cout << "[+] Length of package: " << lengthofPackage << " bytes" << std::endl;
		std::cout << "[DEBUG] overlaySize: " << overlaySize << std::endl;
		std::cout << "[DEBUG] overlayPos: " << overlayPos << std::endl;
		std::cout << "[DEBUG] tableOfContentsPos: " << tableOfContentsPos << std::endl;
		std::cout << "[DEBUG] tableOfContentsSize: " << tableOfContentsSize << std::endl;
}

/**
 * @brief Output debug information about the entry sizes in the CArchive.
 *
 * This function outputs debug information about the entry sizes in the CArchive, including the
 * name and compressed data size of each entry.
 */
void PyInstArchive::debugEntrySizes() {
		std::cout << "[DEBUG] Entry sizes in the CArchive:" << std::endl;
		for (const auto& entry : tocList) {
				std::cout << "[DEBUG] Entry Name: " << entry.getName()
						<< ", Compressed Size: " << entry.getCompressedDataSize() << " bytes"
						<< std::endl;
		}
}
#endif

/**
 * @brief Parse the Table of Contents (TOC) of the CArchive.
 *
 * This function reads the TOC from the PyInstaller archive and extracts
 * the necessary metadata for each entry, storing them in a list.
 */
void PyInstArchive::parseTOC() {
		fPtr.seekg(tableOfContentsPos, std::ios::beg);
		tocList.clear();
		uint32_t parsedLen = 0;

		while (parsedLen < tableOfContentsSize) {
				uint32_t entrySize;
				if (!readEntrySize(entrySize)) {
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
				catch (const std::exception&) {
						parsedLen += entrySize;
						continue;
				}
		}
}

/**
 * @brief Read the size of the next TOC entry.
 *
 * @param entrySize Reference to store the size of the TOC entry.
 * @return True if the entry size was successfully read, otherwise false.
 */
bool PyInstArchive::readEntrySize(uint32_t& entrySize) {
		fPtr.read(reinterpret_cast<char*>(&entrySize), sizeof(entrySize));
		if (fPtr.gcount() < static_cast<std::streamsize>(sizeof(entrySize))) {
				return false;
		}
		entrySize = swapBytes(entrySize);
		if (entrySize < sizeofEntry() || entrySize > tableOfContentsSize) {
				return false;
		}

		return true;
}

/**
 * @brief Read the fields of a TOC entry.
 *
 * @param entryPos Reference to store the entry position.
 * @param cmprsdDataSize Reference to store the compressed data size.
 * @param uncmprsdDataSize Reference to store the uncompressed data size.
 * @param cmprsFlag Reference to store the compression flag.
 * @param typeCmprsData Reference to store the type of compressed data.
 * @param nameBuffer Buffer to store the entry name.
 * @param entrySize The size of the entry.
 */
void PyInstArchive::readEntryFields(uint32_t& entryPos, uint32_t& cmprsdDataSize, uint32_t& uncmprsdDataSize, uint8_t& cmprsFlag, char& typeCmprsData, std::vector<char>& nameBuffer, uint32_t entrySize) {
		uint32_t nameLen = sizeofEntry();
		if (entrySize < nameLen) {
				throw std::runtime_error("Invalid TOC entry size");
		}

		char buffer[14];
		fPtr.read(buffer, sizeof(buffer));
		size_t bytesRead = fPtr.gcount();

		if (bytesRead < sizeof(buffer)) {
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
				throw std::runtime_error("Incomplete TOC entry name read");
		}
}

/**
 * @brief Decode the entry name from the buffer and handle invalid names.
 *
 * @param nameBuffer Buffer containing the entry name.
 * @param parsedLen The current parsed length of the TOC.
 * @return The decoded and normalized entry name.
 */
std::string PyInstArchive::decodeEntryName(std::vector<char>& nameBuffer, uint32_t parsedLen) {
		std::string name(nameBuffer.data(), nameBuffer.size());
		name.erase(std::remove(name.begin(), name.end(), '\0'), name.end());
		bool isValid = !name.empty() && name[0] != '/' && name[0] != '\\' &&
				name.find_first_not_of(" \t\n\r") != std::string::npos;
		for (char c : name) {
				if (c < 32 || c > 126) {
						isValid = false;
						break;
				}
		}
		static uint32_t counter = 0;
		if (!isValid) {
				name = "unnamed_" + std::to_string(parsedLen) + "_" + std::to_string(counter++);
		}

		std::replace(name.begin(), name.end(), '.', '_');

		return name;
}

/**
 * @brief Add a TOC entry to the list.
 *
 * @param entryPos The position of the entry.
 * @param cmprsdDataSize The compressed data size of the entry.
 * @param uncmprsdDataSize The uncompressed data size of the entry.
 * @param cmprsFlag The compression flag of the entry.
 * @param typeCmprsData The type of compressed data.
 * @param name The name of the entry.
 */
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

/**
 * @brief Get the size of the standard TOC entry fields.
 *
 * @return The size of the standard TOC entry fields.
 */
uint32_t PyInstArchive::sizeofEntry() const {
		return sizeof(uint32_t) +
				sizeof(uint32_t) +
				sizeof(uint32_t) +
				sizeof(uint32_t) +
				sizeof(uint8_t) +
				sizeof(char);
}
/**
 * @brief Displays the list of files in the PyInstaller archive.
 *
 * This method iterates over the Table of Contents (TOC) and prints the names
 * and uncompressed sizes of the embedded files.
 */
void PyInstArchive::displayInfo() {
		std::cout << "[+] Archive Info:" << std::endl;
		for (const auto& entry : tocList) {
				try {
						std::string name = entry.getName();
						if (name.empty()) {
								name = "Unnamed_File";
						}
						std::cout << "File: " << name << ", Size: " << entry.getCompressedDataSize() << " bytes" << std::endl;
				}
				catch (const std::exception&) {
						// Silently skip invalid entries
				}
		}
}

/**
 * @brief Measures and outputs the total time taken to decompress and extract files from the PyInstaller archive.
 *
 * This function launches asynchronous tasks for each file in the Table of Contents (TOC) to decompress
 * and extract them to the specified output directory. The tasks are managed using `std::async` and the
 * time taken for the entire extraction process is measured using `std::chrono`.
 * 
 * @param outputDir The directory where the extracted files will be saved.
 */
void PyInstArchive::timeExtractionProcess(const std::string& outputDir) {
		// Determine the number of physical cores to use as threads
		size_t numThreads = getPhysicalCoreCount();

		auto start = std::chrono::high_resolution_clock::now();

		// Call MultiThreadedFileExtract with the required arguments
		MultiThreadedFileExtract(tocList, outputDir, numThreads);

		auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> elapsed = end - start;

		std::cout << "[*] Extraction completed in " << elapsed.count() << " seconds.\n";
}

/**
 * @brief Decompresses and extracts all files from the PyInstaller archive using multithreading.
 *
 * The `MultiThreadedFileExtract` method initializes a thread pool and enqueues tasks to decompress
 * and extract each file specified in the Table of Contents (TOC) entries. It leverages multithreading
 * to improve extraction performance by processing multiple files concurrently.
 *
 * @param tocEntries A vector of TOC entries representing the files to extract from the archive.
 * @param outputDir  The directory where the extracted files will be saved.
 *
 * @note The function creates a thread pool with a number of threads equal to the hardware concurrency.
 *       If the hardware concurrency cannot be determined, it defaults to 4 threads.
 * @note Each TOC entry is processed by a separate task that calls `decompressAndExtractFile`.
 * @note The mutexes `mtx` and `printMtx` are used within the tasks to ensure thread-safe operations.
 * @note The ThreadPool destructor ensures all tasks are completed before the program continues.
 */
void PyInstArchive::MultiThreadedFileExtract(const std::vector<CTOCEntry>& tocEntries, const std::string& outputDir, size_t numThreads) {
		size_t maxCores = getPhysicalCoreCount();  // Function to get number of physical cores

		// Validate user-specified number of threads
		if (numThreads == 0) {
				numThreads = maxCores;
				std::cout << "[*] Using all available physical cores: " << numThreads << "\n";
		}
		else {
				if (numThreads > maxCores) {
						std::cout << "[!] Specified number of cores (" << numThreads << ") exceeds available physical cores (" << maxCores << "). Using maximum available cores.\n";
						numThreads = maxCores;
				}
				else {
						std::cout << "[*] Using user-specified number of cores: " << numThreads << "\n";
				}
		}

		if (numThreads == 0) numThreads = 1;  // Ensure at least one thread

		// Initialize ThreadPool with the specified number of threads
		ThreadPool pool(numThreads);

		// Enqueue tasks
		for (const auto& tocEntry : tocEntries) {
				pool.enqueue([this, &tocEntry, &outputDir] {
						this->decompressAndExtractFile(tocEntry, outputDir, mtx, printMtx);
						});
		}
}

/**
 * @brief Decompresses and extracts a single file from the PyInstaller archive.
 *
 * This method handles the decompression and extraction of a single file specified by the
 * Table of Contents (TOC) entry. It reads the compressed data from the archive file,
 * decompresses it if necessary, and writes the output to the specified directory,
 * preserving the file structure. Thread safety is ensured through mutex locks for file
 * access and console output, allowing concurrent execution in a multithreaded environment.
 *
 * @param tocEntry  The Table of Contents entry representing the file to extract.
 * @param outputDir The directory where the extracted file will be saved.
 * @param mtx       Mutex to synchronize access to the file stream `fPtr` for reading.
 * @param printMtx  Mutex to synchronize console output to prevent message interleaving.
 *
 * @note The function checks if the data is compressed and handles decompression using zlib.
 * @note Any errors during reading, decompression, or writing are logged to the console.
 * @note The function assumes that the output directory exists or can be created.
 * @note This method is designed to be thread-safe and can be called concurrently by multiple threads.
 */
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

/**
 * @brief Parses command-line arguments and initiates the archive processing.
 *
 * This function handles the parsing of command-line arguments to determine the appropriate
 * operation to perform on the PyInstaller archive. It supports specifying the number of cores
 * to use for extraction, the command to execute (either to display information or to extract files),
 * the path to the archive, and the optional output directory.
 *
 * Supported arguments:
 * - `-cores N`: Specifies the number of cores to use for the extraction process. If not provided or set to 0, all available physical cores are used.
 * - `-i`: Command to display information about the archive (filenames, sizes).
 * - `-u`: Command to extract files from the archive.
 * - `<archive_path>`: The path to the PyInstaller archive file.
 * - `[output_dir]`: Optional output directory where the extracted files will be saved. Defaults to "unpacked".
 *
 * Example usage:
 * - `unpack.exe -cores 4 -u archive_file.exe output_dir`
 * - `unpack.exe -i archive_file.exe`
 *
 * @param argc The number of command-line arguments.
 * @param argv The array of command-line arguments.
 */
void parseArgs(int argc, char* argv[]) {
		// Default values
		int numCores = 0; // 0 indicates 'use all available physical cores'
		std::string command;
		std::string archivePath;
		std::string outputDir = "unpacked"; // Default output directory
		int argIndex = 1;

		// Check if there are enough arguments
		if (argc < 3) {
				std::cerr << "[!] Usage: " << argv[0] << " [-cores N] [-i | -u] <archive_path> [output_dir]" << std::endl;
				exit(1);
		}

		// Parse arguments
		while (argIndex < argc) {
				std::string arg = argv[argIndex];

				if (arg == "-cores") {
						// Handle the -cores argument
						argIndex++;
						if (argIndex >= argc) {
								std::cerr << "[!] Error: Expected number after -cores" << std::endl;
								exit(1);
						}
						numCores = atoi(argv[argIndex]);
						if (numCores <= 0) {
								std::cerr << "[!] Invalid number of cores specified. Using all available physical cores." << std::endl;
								numCores = 0;
						}
						argIndex++;
				}
				else if (arg == "-i" || arg == "-u") {
						// Handle the command (-i or -u)
						command = arg;
						argIndex++;
				}
				else if (archivePath.empty()) {
						// First argument that's not an option is the archive path
						archivePath = arg;
						argIndex++;
				}
				else {
						// Optional output directory
						outputDir = arg;
						argIndex++;
				}
		}

		// Validate required arguments
		if (command.empty() || archivePath.empty()) {
				std::cerr << "[!] Usage: " << argv[0] << " [-cores N] [-i | -u] <archive_path> [output_dir]" << std::endl;
				exit(1);
		}

		// Check if the output directory exists, create it if it doesn't
		if (!std::filesystem::exists(outputDir)) {
				std::filesystem::create_directories(outputDir);
		}

		PyInstArchive archive(archivePath);

		if (!archive.open()) {
				std::cerr << "[!] Error: Could not open " << archivePath << std::endl;
				return;
		}

		if (!archive.checkFile()) {
				std::cerr << "[!] Error: Invalid file " << archivePath << std::endl;
				return;
		}

		if (!archive.getCArchiveInfo()) {
				std::cerr << "[!] Error: Could not extract TOC from " << archivePath << std::endl;
				return;
		}

		if (command == "-i") {
				archive.displayInfo();  // Display information about the archive (filenames, sizes)
		}
		else if (command == "-u") {
				archive.parseTOC();  // Parse the Table of Contents before extraction
				archive.MultiThreadedFileExtract(archive.getTOCList(), outputDir, static_cast<size_t>(numCores));
		}
		else {
				std::cerr << "[!] Unknown command: " << command << std::endl;
		}
}

/**
 * @brief The entry point for the application.
 *
 * This function processes the command-line arguments by calling `parseArgs`,
 * which handles the input commands to either display information or extract
 * files from a PyInstaller archive.
 *
 * @param argc The number of command-line arguments.
 * @param argv The array of command-line arguments.
 *
 * @return Returns 0 upon successful execution.
 *
 * @note The `parseArgs` function handles any errors or invalid arguments,
 *       so no error handling is required in this function.
 */
int main(int argc, char* argv[]) {
		parseArgs(argc, argv);
		return 0;
}
