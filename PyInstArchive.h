#ifndef PYINSTARCHIVE_H
#define PYINSTARCHIVE_H

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint> 

// Structure for Table of Contents Entry
struct CTOCEntry {
    uint32_t position;              // Position of the entry
    uint32_t cmprsdDataSize;       // Compressed data size
    uint32_t uncmprsdDataSize;     // Uncompressed data size
    uint8_t cmprsFlag;             // Compression flag
    char typeCmprsData;            // Type of compressed data
    std::string name;              // Name of the entry

    // Constructor
    CTOCEntry(uint32_t pos, uint32_t cmprsdSize, uint32_t uncmprsdSize, uint8_t flag, char type, const std::string& n)
        : position(pos), cmprsdDataSize(cmprsdSize), uncmprsdDataSize(uncmprsdSize), cmprsFlag(flag), typeCmprsData(type), name(n) {}

    // Getters for entry details
    uint32_t getCompressedDataSize() const {
        return cmprsdDataSize; 
    }

    const std::string& getName() const {
        return name; 
    }
};

// Class for handling the PyInstaller Archive
class PyInstArchive {
public:
    // Constructor
    PyInstArchive(const std::string& path);

    // Member functions
    bool open();
    void close();
    bool checkFile();
    bool getCArchiveInfo();
    void parseTOC();
    void viewFiles();

private:
    std::string filePath;          // Path to the archive file
    std::ifstream fPtr;           // File stream for reading the archive
    uint64_t fileSize;            // Size of the file
    uint64_t cookiePos;           // Position of the cookie
    uint64_t overlayPos;          // Position of the overlay
    uint64_t overlaySize;         // Size of the overlay
    uint64_t tableOfContentsPos;  // Position of the TOC
    uint64_t tableOfContentsSize; // Size of the TOC
    uint8_t pyinstVer;            // PyInstaller version
    uint8_t pymaj;                // Python major version
    uint8_t pymin;                // Python minor version
    std::vector<CTOCEntry> tocList; // List of TOC entries
    uint32_t lengthofPackage;     // Length of the package
    uint32_t toc;                 // Table of contents
    uint32_t tocLen;              // Length of the table of contents

    // Constants for PyInstaller cookie sizes
    static const uint8_t PYINST20_COOKIE_SIZE = 24;
    static const uint8_t PYINST21_COOKIE_SIZE = 24 + 64;
    static const std::string MAGIC;
};

#endif // PYINSTARCHIVE_H
