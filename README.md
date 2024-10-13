# PyInstaller Archive Viewer

A C++ tool to inspect and extract contents from PyInstaller archives.

## Features
- Opens and reads PyInstaller archive files.
- Detects PyInstaller version (2.0 or 2.1+).
- Parses and lists files from the archive.

## Requirements
- Windows
- C++17
- CMake

## Usage

### Build
1. Clone the repo:
    ```sh
    git clone https://github.com/your_username/PyInstallerArchiveViewer.git
    cd PyInstallerArchiveViewer
    ```
2. Build:
   Build with Visual studio 
    ```

### Run
    sh
    PyInstallerArchiveViewer.exe path/to/your/archive
    

## Example

```cpp
#include <iostream>
#include "PyInstArchive.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <path/to/pyinstaller_archive>" << std::endl;
        return 1;
    }

    PyInstArchive archive(argv[1]);

    if (!archive.open()) return 1;
    if (!archive.checkFile()) {
        archive.close();
        return 1;
    }
    if (!archive.getCArchiveInfo()) {
        archive.close();
        return 1;
    }

    archive.viewFiles();
    archive.close();

    return 0;
}
