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

1. **Open the MinGW64 shell** and navigate to the folder where you want to place the project.

   ```bash
   git clone <repo-url>
   cd Chess-Engine
   ```

2. **Configure and build** the engine in release mode with optimisation flags:

   ```bash
   cmake -S . -B build
   cmake --build build --config Release
   ```

   These commands generate a `build/` directory and compile an optimised release binary.

3. **Run the engine**:

   ```bash
   build/Release/ChessEngine_Lilia-VERSION.exe
   ```

   The executable lives in `build/Release/` after a successful build.

---

## Notes

- This setup is currently optimized for **Windows 64-bit** architecture.

---

## Architecture & Design Patterns

The project is structured around a clear separation of responsibilities:

- **Model** – core chess logic such as the board representation, move generation and evaluation (`include/lilia/model`, `src/lilia/model`).
- **View** – SFML-based rendering layer (`include/lilia/view`, `src/lilia/view`). `GameView` acts as a façade over SFML, while textures are managed by the Singleton `TextureTable`.
- **Controller** – orchestrates user input and the game loop (`include/lilia/controller`, `src/lilia/controller`). `GameManager` exposes callbacks for moves, promotions and game end events, enabling an observer-style communication with the view.
- **Engine** – search and evaluation algorithms for AI play (`include/lilia/engine`, `src/lilia/engine`).

Design patterns used include:

- **Model–View–Controller** to separate state, presentation and interaction.
- **Strategy** via the `IPlayer` interface, allowing interchangeable bot or human move providers.
- **Singleton** for central texture management (`TextureTable`).
- **Observer-like callbacks** from `GameManager` to notify other components of state changes.
- **Factory** to generate fresh evaluator instances for each search thread (`EvalFactory`).
- **Facade** through `GameView`, which offers a simplified interface to the rendering subsystem.
