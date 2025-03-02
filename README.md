# PyInstaller Archive Viewer

A C++ tool to inspect and extract contents from PyInstaller archives.

## Features
- Opens and reads PyInstaller archive files.
- Detects PyInstaller version (2.0 or 2.1+).
- Parses and lists files from the archive.
- Unpack files.

## Requirements
- Windows
- C++17
- Visual Studio

## Usage

### Build
1. Clone the repo:
    ```sh
    git clone https://github.com/pyinstxtractor/Pyextract.git
    cd Pyextract
    ```
2. Open the solution in Visual Studio:
    - Open Visual Studio
    - Open the `PyInstaller-C++.vcxproj` project file
    - Build the project

### Command-line arguments
* `-i` (Info): Display information about the archive.
* `-u` (Unpack): Unpack the contents of the archive.

### Example Usage
To display information about the archive:
```sh
PyInstaller-C++.exe -i executable_name
```

To unpack the contents of the archive:
```sh
PyInstaller-C++.exe -u executable_name
```
