# Lilia Chess Engine

Lilia is a Windows‑focused chess sandbox written in modern **C++20**. The repository hosts both the SFML‑based **LiliaApp** GUI and the standalone **liliaengine** core. The engine speaks [UCI](https://en.wikipedia.org/wiki/Universal_Chess_Interface) and can be plugged into any UCI front‑end.

👉 **Get prebuilt binaries** from the [Releases tab](https://github.com/JustAnoAim/Lilia/releases) to try the latest version of the engine and GUI.

## Project Idea & Design
Lilia is first and foremost a playground to experiment with chess‑engine ideas. The code base is deliberately modular so that algorithms can be swapped in and out with minimal friction.

- Clear separation between *engine* and *app* modules.
- Modular MVC layout (`model`, `engine`, `uci` / `view`, `controller`, `app`).
- Bitboard board representation for fast move generation.
- Multithreaded search designed for maintainability and experimentation.

## Search Algorithms
The search is a classic negamax with alpha‑beta pruning augmented by many heuristics and pruning ideas from modern engines:

- Iterative deepening with aspiration windows and principal variation search.
- Late move reductions with a pre‑tuned reduction table.
- Null‑move pruning, razoring and multiple futility pruning stages.
- SEE pruning, ProbCut and light check extensions.
- Rich move ordering: transposition table, killer moves, quiet/capture histories, counter‑moves and history pruning.

## Evaluation
The handcrafted evaluator mixes material, mobility and many structural features:

- Piece‑square tables and mobility profiles for each piece.
- Pawn structure: isolated, doubled and backward pawns, phalanxes, candidates and connected or passed pawns.
- King safety: pawn shields, king rings and storm/shelter tables.
- Piece‑specific motifs such as bishop pair, outposts, rooks on open files or behind passers, connected rooks and more.
- Threat detection, space and material imbalance terms.

## Transposition Table
The engine currently uses **TT5**, a compact 16‑byte entry table with two‑stage key verification and generation‑based aging for fast lookups.

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

## Using the Engine
- `liliaengine` speaks the [UCI protocol](https://en.wikipedia.org/wiki/Universal_Chess_Interface) and can be plugged into any UCI-compatible GUI.
- The engine can also be linked as a library; see `examples/main.cpp` for a minimal integration example.

## Acknowledgements
- Graphics, windowing and audio are provided by [SFML](https://www.sfml-dev.org/).
- This setup is currently optimized for **Windows 64-bit** architecture.

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

## Future Work
- Integrate an NNUE evaluation backend.
- Continue exploring new pruning techniques and search ideas.
- Improve cross-platform support.
