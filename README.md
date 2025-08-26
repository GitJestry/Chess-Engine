# Lilia Chess Engine

Lilia is a Windows-only chess project written in modern **C++20**. It is split into the SFML-based **LiliaApp** graphical client and the standalone **liliaengine** core, which is fully UCI compatible and can be used with any UCI front-end.

## Design Highlights
- Clear separation between *engine* and *app* modules.
- Modular MVC layout (`model`, `engine`, `uci` / `view`, `controller`, `app`).
- Bitboard-based board representation for fast move generation.
- Multithreaded search designed for maintainability and experimentation.

## Algorithms & Techniques
- Negamax search with alpha-beta pruning.
- Iterative deepening with aspiration windows.
- Quiescence search to reduce horizon effects.
- Transposition table (TT4) using Zobrist hashing.
- Move ordering heuristics (killer moves, history heuristic, null-move pruning).

## Project Structure
```
.
├── assets/        # textures and audio for the GUI
├── examples/      # example entry point
├── include/
│   └── lilia/     # public headers
└── src/lilia/
    ├── app/       # LiliaApp front-end
    ├── controller/ # MVC controllers for the GUI
    ├── engine/    # search, evaluation & transposition table
    ├── model/     # board representation & move generation
    ├── uci/       # UCI protocol implementation
    └── view/      # SFML-based rendering
```

## Building (Windows 64-bit)
1. **Prerequisites**
   - [CMake](https://cmake.org/)
   - A C++20 compiler (tested with MinGW-w64)
   - Git (for fetching SFML via `FetchContent`)

2. **Configure and build**
   ```bash
   cmake -S . -B build
   cmake --build build --config Release
   ```
   The Release binaries will be placed in `build/bin/Release/`.

3. **Run**
   - `build/bin/Release/lilia_engine.exe` – console UCI engine
   - `build/bin/Release/lilia_app.exe` – SFML GUI

## Using the Engine
- `liliaengine` speaks the [UCI protocol](https://en.wikipedia.org/wiki/Universal_Chess_Interface) and can be plugged into any UCI-compatible GUI.
- The engine can also be linked as a library; see `examples/main.cpp` for a minimal integration example.

## Acknowledgements
- Graphics, windowing and audio are provided by [SFML](https://www.sfml-dev.org/).
