#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include "PyInstArchive.h"
#include <winsock2.h>
#include <random>
#include <sstream>
#include <filesystem>
#include "zlib.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "oleaut32.lib")

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
 * @brief Checks if the file is a valid PyInstaller archive.
 *
 * This method searches for the magic string (a unique identifier) in the PyInstaller archive
 * and determines the version of PyInstaller used. If the magic string is found, it sets the
 * cookie position and identifies the PyInstaller version.
 *
 * @return true if the file is a valid PyInstaller archive, false otherwise.
 */
bool PyInstArchive::checkFile() {
    std::cout << "[+] Processing " << filePath << std::endl;
    const size_t searchChunkSize = 8192;
    uint64_t endPos = fileSize;
    cookiePos = -1;

    if (endPos < MAGIC.size()) {
        std::cerr << "[!] Error: File is too short or truncated" << std::endl;
        return false;
    }

    while (true) {
        uint64_t startPos = endPos >= searchChunkSize ? endPos - searchChunkSize : 0;
        size_t chunkSize = endPos - startPos;
        if (chunkSize < MAGIC.size()) {
            break;
        }
        fPtr.seekg(startPos, std::ios::beg);
        std::vector<char> data(chunkSize);
        fPtr.read(data.data(), chunkSize);

        auto offs = std::string(data.data(), chunkSize).rfind(MAGIC);
        if (offs != std::string::npos) {
            cookiePos = startPos + offs;
            break;
        }
        endPos = startPos + MAGIC.size() - 1;
        if (startPos == 0) {
            break;
        }
    }

    if (cookiePos == -1) {
        std::cerr << "[!] Error: Missing cookie, unsupported pyinstaller version or not a pyinstaller archive" << std::endl;
        return false;
    }

    fPtr.seekg(cookiePos + PYINST20_COOKIE_SIZE, std::ios::beg);
    std::vector<char> buffer(64);
    fPtr.read(buffer.data(), 64);
    if (std::string(buffer.data(), 64).find("python") != std::string::npos) {
        std::cout << "[+] Pyinstaller version: 2.1+" << std::endl;
        pyinstVer = 21;
    }
    else {
        pyinstVer = 20;
        std::cout << "[+] Pyinstaller version: 2.0" << std::endl;
    }

    return true;
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
 * @brief Extracts and parses CArchive information from the PyInstaller file.
 *
 * This function reads the package length, table of contents (TOC), and Python version
 * from the PyInstaller archive. It adjusts byte order for multi-byte values based on
 * the endianness and calculates offsets for further extraction.
 *
 * @return true if the archive information was successfully parsed, false if an error occurred.
 */
bool PyInstArchive::getCArchiveInfo() {
    try {
        uint32_t lengthofPackage, toc, tocLen, pyver;

        if (pyinstVer == 20) {
            fPtr.seekg(cookiePos, std::ios::beg);
            char buffer[PYINST20_COOKIE_SIZE];
            fPtr.read(buffer, PYINST20_COOKIE_SIZE);
            std::memcpy(&lengthofPackage, buffer + 8, 4);
            std::memcpy(&toc, buffer + 12, 4);
            std::memcpy(&tocLen, buffer + 16, 4);
            std::memcpy(&pyver, buffer + 20, 4);
        }
        else if (pyinstVer == 21) {
            fPtr.seekg(cookiePos, std::ios::beg);
            char buffer[PYINST21_COOKIE_SIZE];
            fPtr.read(buffer, PYINST21_COOKIE_SIZE);
            std::memcpy(&lengthofPackage, buffer + 8, 4);
            std::memcpy(&toc, buffer + 12, 4);
            std::memcpy(&tocLen, buffer + 16, 4);
            std::memcpy(&pyver, buffer + 20, 4);
        }

        // Convert values to host byte order (correcting endianness)
        lengthofPackage = swapBytes(lengthofPackage);
        toc = swapBytes(toc);
        tocLen = swapBytes(tocLen);
        pyver = swapBytes(pyver);

        if (pyver >= 100) {
            pymaj = pyver / 100;
            pymin = pyver % 100;
        }
        else {
            pymaj = pyver / 10;
            pymin = pyver % 10;
        }

        std::cout << "[+] Python version: " << static_cast<int>(pymaj) << "." << static_cast<int>(pymin) << std::endl;

        uint64_t tailBytes = fileSize - cookiePos - (pyinstVer == 20 ? PYINST20_COOKIE_SIZE : PYINST21_COOKIE_SIZE);
        overlaySize = static_cast<uint64_t>(lengthofPackage) + tailBytes;
        overlayPos = fileSize - overlaySize;
        tableOfContentsPos = overlayPos + toc;
        tableOfContentsSize = tocLen;

        std::cout << "[+] Length of package: " << lengthofPackage << " bytes" << std::endl;
        std::cout << "[DEBUG] overlaySize: " << overlaySize << std::endl;
        std::cout << "[DEBUG] overlayPos: " << overlayPos << std::endl;
        std::cout << "[DEBUG] tableOfContentsPos: " << tableOfContentsPos << std::endl;
        std::cout << "[DEBUG] tableOfContentsSize: " << tableOfContentsSize << std::endl;

        parseTOC();

        std::cout << "[INFO] Entry sizes in the CArchive:" << std::endl;
        for (const auto& entry : tocList) {
            std::cout << "[INFO] Entry Name: " << entry.getName()
                << ", Compressed Size: " << entry.getCompressedDataSize() << " bytes"
                << std::endl;
        }
    }
    catch (...) {
        std::cerr << "[!] Error: The file is not a PyInstaller archive" << std::endl;
        return false;
    }
    return true;
}

/**
 * @brief Parses the Table of Contents (TOC) from the PyInstaller archive.
 *
 * This function reads the TOC from the archive, which contains information about the
 * embedded files, such as their size, position in the archive, compression status, and type.
 * Each entry is stored in a list for further processing.
 */
void PyInstArchive::parseTOC() {
   
    // Set the file pointer to the position of the Table of Contents
    fPtr.seekg(tableOfContentsPos, std::ios::beg);

    tocList.clear();  // Clear any existing TOC entries
    uint32_t parsedLen = 0;  // Initialize parsed length

    // Continue parsing until the total size of the TOC is reached
    while (parsedLen < tableOfContentsSize) {
        uint32_t entrySize;
        fPtr.read(reinterpret_cast<char*>(&entrySize), sizeof(entrySize));  // Read the entry size
        entrySize = swapBytes(entrySize);  // Convert entry size to host byte order

        // Debugging output for entry size
        std::cout << "[DEBUG] Entry Size: " << entrySize << ", Parsed Length: " << parsedLen << std::endl;

        // Calculate the length of the name and allocate buffer
        uint32_t nameLen = sizeof(uint32_t) + sizeof(uint32_t) * 3 + sizeof(uint8_t) + sizeof(char);
        std::vector<char> nameBuffer(entrySize - nameLen);  // Create buffer for the name

        // Variables to hold entry information
        uint32_t entryPos, cmprsdDataSize, uncmprsdDataSize;
        uint8_t cmprsFlag;
        char typeCmprsData;

        // Read the other fields from the file
        fPtr.read(reinterpret_cast<char*>(&entryPos), sizeof(entryPos));
        fPtr.read(reinterpret_cast<char*>(&cmprsdDataSize), sizeof(cmprsdDataSize));
        fPtr.read(reinterpret_cast<char*>(&uncmprsdDataSize), sizeof(uncmprsdDataSize));
        fPtr.read(reinterpret_cast<char*>(&cmprsFlag), sizeof(cmprsFlag));
        fPtr.read(reinterpret_cast<char*>(&typeCmprsData), sizeof(typeCmprsData));
        fPtr.read(nameBuffer.data(), entrySize - nameLen);

        // Debugging output for each field read
        std::cout << "[DEBUG] Entry Position: " << swapBytes(entryPos) << std::endl;
        std::cout << "[DEBUG] Compressed Data Size: " << swapBytes(cmprsdDataSize) << std::endl;
        std::cout << "[DEBUG] Uncompressed Data Size: " << swapBytes(uncmprsdDataSize) << std::endl;
        std::cout << "[DEBUG] Compression Flag: " << static_cast<int>(cmprsFlag) << std::endl;
        std::cout << "[DEBUG] Type of Compressed Data: " << typeCmprsData << std::endl;

        // Decode the name from the buffer and remove null characters
        std::string name(nameBuffer.data(), nameBuffer.size());
        name.erase(std::remove(name.begin(), name.end(), '\0'), name.end());

        // Debugging output for the name
        std::cout << "[DEBUG] Name: '" << name << "'" << std::endl;

        // Handle invalid names and normalize
        if (name.empty() || name[0] == '/') {
            name = "unnamed_" + std::to_string(parsedLen);
            std::cout << "[DEBUG] Normalized Name: '" << name << "'" << std::endl;  // Debugging normalized name
        }

        // Add the entry to the TOC list
        tocList.emplace_back(
            overlayPos + swapBytes(entryPos),
            swapBytes(cmprsdDataSize),
            swapBytes(uncmprsdDataSize),
            cmprsFlag,
            typeCmprsData,
            name
        );

        // Update the parsed length by the size of the current entry
        parsedLen += entrySize;
    }

    // Output the total number of entries found in the TOC
    std::cout << "[+] Found " << tocList.size() << " files in CArchive" << std::endl;

}

/**
 * @brief Displays the list of files in the PyInstaller archive.
 *
 * This method iterates over the Table of Contents (TOC) and prints the names
 * and uncompressed sizes of the embedded files.
 */
void PyInstArchive::displayInfo() {
    std::cout << "[+] Archive Info:" << std::endl;
    // Print out relevant information about the PyInstaller archive
    // For example: number of files, archive version, etc.
    for (const auto& entry : tocList) {
        std::cout << "File: " << entry.getName() << ", Size: " << entry.getCompressedDataSize() << " bytes" << std::endl;
    }
}

/**
 * @brief Extracts files from a PyInstaller archive to a specified directory.
 *
 * This method iterates through the Table of Contents (TOC) of the archive and extracts each file.
 * For compressed entries, it decompresses the data using zlib and writes it to the output directory.
 * Uncompressed data is written as-is.
 *
 * @param outputDir The directory where extracted files will be saved.
 *
 * @note The output directory and its subdirectories will be created if they do not exist.
 * @note Errors are logged if any file extraction or decompression fails.
 */
void PyInstArchive::extractFiles(const std::string& outputDir) {
    for (const auto& tocEntry : tocList) {
        // Move the file pointer to the position of the compressed data
        fPtr.seekg(tocEntry.position, std::ios::beg);

        // Read the compressed data into a vector
        std::vector<char> compressedData(tocEntry.getCompressedDataSize());
        fPtr.read(compressedData.data(), tocEntry.getCompressedDataSize());

        std::vector<char> decompressedData;

        if (tocEntry.isCompressed()) {
            // If the entry is compressed, decompress the data using zlib
            decompressedData.resize(tocEntry.uncmprsdDataSize);

            z_stream strm = {};
            strm.avail_in = tocEntry.getCompressedDataSize();
            strm.next_in = reinterpret_cast<Bytef*>(compressedData.data());
            strm.avail_out = tocEntry.uncmprsdDataSize;
            strm.next_out = reinterpret_cast<Bytef*>(decompressedData.data());

            if (inflateInit(&strm) != Z_OK) {
                std::cerr << "[!] Error: Could not initialize zlib for decompression" << std::endl;
                continue;
            }

            int result = inflate(&strm, Z_FINISH);
            inflateEnd(&strm);

            if (result != Z_STREAM_END) {
                std::cerr << "[!] Error: Decompression failed for " << tocEntry.getName() << std::endl;
                continue;
            }
        }
        else {
            // If the data is not compressed, just use the original data
            decompressedData = compressedData;
        }

        // Construct the full output file path
        std::filesystem::path outputFilePath = std::filesystem::path(outputDir) / tocEntry.getName();

        // Ensure the directory exists
        std::filesystem::create_directories(outputFilePath.parent_path());

        // Write the decompressed data to the output file
        std::ofstream outFile(outputFilePath, std::ios::binary);
        if (!outFile.is_open()) {
            std::cerr << "[!] Error: Could not open output file " << outputFilePath << std::endl;
            continue;
        }

        outFile.write(decompressedData.data(), decompressedData.size());
        outFile.close();

        std::cout << "[+] Extracted: " << tocEntry.getName() << " (" << decompressedData.size() << " bytes)" << std::endl;
    }
}

/**
 * @brief Decompresses data using zlib.
 *
 * Decompresses `compressedData` into `decompressedData` using zlib.
 * Ensure `decompressedData` has enough space for the decompressed output.
 *
 * @param compressedData Input vector of compressed data.
 * @param decompressedData Output vector for decompressed data.
 *
 * @note Prints an error message if decompression fails.
 */
void PyInstArchive::decompressData(const std::vector<char>& compressedData, std::vector<char>& decompressedData) {
    uLongf decompressedSize = decompressedData.size();
    int result = uncompress(reinterpret_cast<Bytef*>(decompressedData.data()), &decompressedSize,
        reinterpret_cast<const Bytef*>(compressedData.data()), compressedData.size());

    if (result != Z_OK) {
        std::cerr << "[!] Error: Decompression failed" << std::endl;
        // Optionally, you could also throw an exception or handle the error more specifically
    }
}

/**
 * @brief Parses command-line arguments for interacting with a PyInstaller archive.
 *
 * This method processes the command-line arguments, checks if the required parameters
 * are provided, and then opens the specified PyInstaller archive. It can either display
 * information about the archive or extract its files to the specified output directory.
 *
 * @param argc The number of command-line arguments.
 * @param argv The array of command-line arguments.
 *
 * @note The command must be either "-i" to display archive information or "-u" to extract files.
 *       The archive path is required, and an optional output directory can be specified.
 * @note If the output directory does not exist, it will be created automatically.
 * @note Errors are logged if any arguments are invalid or if the archive cannot be processed.
 */
void parseArgs(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "[!] Usage: " << argv[0] << " [-i | -u] <archive_path> [output_dir]" << std::endl;
        exit(1);
    }

    std::string command = argv[1];    // Command (-i or -u)
    std::string archivePath = argv[2]; // Archive file path
    std::string outputDir = (argc > 3) ? argv[3] : "unpacked"; // Output directory (default to "output")

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
        archive.extractFiles(outputDir);  // Extract files to the specified directory
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
