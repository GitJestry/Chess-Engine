```
# Chess Engine

This is a chess engine written from scratch in C++ with a graphical user interface (GUI) built using the SFML library.

---

## Building the Project (Windows 64-bit Only)

### Prerequisites

1. **Install MinGW64**: 
   - You need to install MinGW64. We recommend following the official Microsoft guide for setting up `msys64`. Here's a helpful video to guide you through the process:
     - [Microsoft Guide to MSYS64 Setup](https://www.youtube.com/watch?v=oC69vlWofJQ)

2. **Directory Structure**:
   - Ensure your directory looks like this:
     ```
     C:\path\to\your\project\Chess-Engine
     ```

### Steps to Build

1. **Run CMake Commands**:
   - Open a terminal/command prompt.
   - Navigate to your project directory.
   
   Run the following commands to configure and build the project:
   
   ```bash
   cmake -S . -B build
   cmake --build build --config Release
   ```
   
   This will:
   - Generate the build configuration in the `build/` directory.
   - Compile the project in release mode.
   
2. **Locate the Executable**:
   - After the build process completes, the executable file `ChessEngine_Lilia-VERSION.exe` will be created in the `build/Release/` directory.

3. **Run the Engine**:
   - You can now run the latest version of the chess engine, which will be located in the `build/Release/` directory.

---

## Notes

- This setup is currently optimized for **Windows 64-bit** architecture.
- Make sure you have all dependencies (like SFML) correctly installed before building.
```
