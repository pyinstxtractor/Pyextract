#ifndef PYINSTARCHIVE_H
#define PYINSTARCHIVE_H

#include "qlistwidget.h"
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <mutex>

// Structure for Table of Contents Entry
struct CTOCEntry {
    uint32_t position;              // Position of the entry
    uint32_t cmprsdDataSize;        // Compressed data size
    uint32_t uncmprsdDataSize;      // Uncompressed data size
    uint8_t cmprsFlag;              // Compression flag
    char typeCmprsData;             // Type of compressed data
    std::string name;               // Name of the entry

    // Constructor
    CTOCEntry(uint32_t pos, uint32_t cmprsdSize, uint32_t uncmprsdSize, uint8_t flag, char type, const std::string& n)
        : position(pos), cmprsdDataSize(cmprsdSize), uncmprsdDataSize(uncmprsdSize), cmprsFlag(flag), typeCmprsData(type), name(n) {
    }

    // Getters for entry details
    uint32_t getCompressedDataSize() const {
        return cmprsdDataSize;
    }

    const std::string& getName() const {
        return name;
    }

    bool isCompressed() const {
        return cmprsFlag != 0;
    }
};

// Class for handling the PyInstaller Archive
class PyInstArchive {
public:

    PyInstArchive(const std::string& path);
    void displayInfo(QListWidget* listWidget);
    bool open();

    bool checkFile();
    bool isFileValid(size_t searchChunkSize);
    bool findCookie(size_t searchChunkSize);
    void determinePyinstallerVersion();

    bool getCArchiveInfo();
    void readArchiveData(uint32_t& lengthofPackage, uint32_t& toc, uint32_t& tocLen, uint32_t& pyver);
    void calculateOverlayInfo(uint32_t lengthofPackage, uint32_t toc, uint32_t tocLen);
    void debugOutput(uint32_t lengthofPackage);

    void parseTOC();
    bool readEntrySize(uint32_t& entrySize);
    void readEntryFields(uint32_t& entryPos, uint32_t& cmprsdDataSize, uint32_t& uncmprsdDataSize, uint8_t& cmprsFlag, char& typeCmprsData, std::vector<char>& nameBuffer, uint32_t entrySize);
    std::string decodeEntryName(std::vector<char>& nameBuffer, uint32_t parsedLen);
    void addTOCEntry(uint32_t entryPos, uint32_t cmprsdDataSize, uint32_t uncmprsdDataSize, uint8_t cmprsFlag, char typeCmprsData, std::string name);
    uint32_t sizeofEntry() const;

    void timeExtractionProcess(const std::string& outputDir);
    void MultiThreadedFileExtract(const std::vector<CTOCEntry>& tocEntries, const std::string& outputDir, size_t numThreads);
    void decompressAndExtractFile(const CTOCEntry& tocEntry, const std::string& outputDir, std::mutex& mtx, std::mutex& printMtx);
    const std::vector<CTOCEntry>& getTOCList() const;
private:
    std::mutex mtx;       // Protects file pointer access
    std::mutex printMtx;  // Protects console output

    std::string filePath;             // Path to the archive file
    std::ifstream fPtr;               // File stream for reading the archive
    uint64_t fileSize;                // Size of the file
    uint64_t cookiePos;               // Position of the cookie
    uint64_t overlayPos;              // Position of the overlay
    uint64_t overlaySize;             // Size of the overlay
    uint64_t tableOfContentsPos;      // Position of the TOC
    uint64_t tableOfContentsSize;     // Size of the TOC
    uint8_t pyinstVer;                // PyInstaller version
    uint8_t pymaj;                    // Python major version
    uint8_t pymin;                    // Python minor version
    std::vector<CTOCEntry> tocList;   // List of TOC entries
    uint32_t lengthofPackage;         // Length of the package
    uint32_t toc;                     // Table of contents
    uint32_t tocLen;                  // Length of the table of contents

    // Constants for PyInstaller cookie sizes
    static const uint8_t PYINST20_COOKIE_SIZE = 24;
    static const uint8_t PYINST21_COOKIE_SIZE = 24 + 64;
    static const std::string MAGIC ;
};

#endif // PYINSTARCHIVE_H
