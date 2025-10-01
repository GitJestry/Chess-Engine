#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include <chrono>
#include <iomanip>

#include "lilia/constants.hpp"
#include "lilia/engine/engine.hpp"
#include "lilia/engine/eval.hpp"
#include "lilia/engine/eval_shared.hpp"
#include "lilia/model/chess_game.hpp"
#include "lilia/model/core/model_types.hpp"

namespace fs = std::filesystem;
using namespace std::string_literals;

namespace lilia::tools::texel {

struct ProgressMeter {
  std::string label;
  std::size_t total = 0;
  std::size_t current = 0;
  int intervalMs = 750;

  std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
  std::chrono::steady_clock::time_point last = start;
  bool finished = false;

  ProgressMeter(std::string label_, std::size_t total_, int intervalMs_ = 750)
      : label(std::move(label_)), total(total_), intervalMs(intervalMs_) {}

  static std::string fmt_hms(std::chrono::seconds s) {
    long t = s.count();
    int h = static_cast<int>(t / 3600);
    int m = static_cast<int>((t % 3600) / 60);
    int sec = static_cast<int>(t % 60);
    std::ostringstream os;
    if (h > 0) {
      os << h << ":" << std::setw(2) << std::setfill('0') << m << ":" << std::setw(2) << sec;
    } else {
      os << m << ":" << std::setw(2) << std::setfill('0') << sec;
    }
    return os.str();
  }

  void update(std::size_t newCurrent) {
    if (finished) return;
    current = std::min(newCurrent, total);
    auto now = std::chrono::steady_clock::now();
    auto since = std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count();
    bool timeToPrint = since >= intervalMs || current == total;

    if (!timeToPrint) return;
    last = now;

    double pct = total ? (100.0 * static_cast<double>(current) / static_cast<double>(total)) : 0.0;

    // ETA
    double elapsedSec = std::chrono::duration<double>(now - start).count();
    double rate = elapsedSec > 0.0 ? current / elapsedSec : 0.0;
    double remainSec = (rate > 0.0 && total >= current) ? (total - current) / rate : 0.0;

    auto eta = std::chrono::seconds(static_cast<long long>(remainSec + 0.5));
    auto elapsed = std::chrono::seconds(static_cast<long long>(elapsedSec + 0.5));

    std::ostringstream line;
    line << "\r" << label << " " << std::fixed << std::setprecision(1) << pct << "% "
         << "(" << current << "/" << total << ")  "
         << "elapsed " << fmt_hms(elapsed) << "  ETA ~" << fmt_hms(eta);

    std::cout << line.str() << std::flush;
  }

  void finish() {
    if (finished) return;
    update(total);
    std::cout << "\n";
    finished = true;
  }
};

struct DefaultPaths {
  fs::path dataFile;
  fs::path weightsFile;
  std::optional<fs::path> stockfish;
};

struct Options {
  bool generateData = false;
  bool tune = false;

  std::string stockfishPath;
  int games = 8;
  int depth = 12;
  int maxPlies = 160;
  int sampleSkip = 6;
  int sampleStride = 4;

  std::string dataFile;
  int iterations = 200;
  double learningRate = 0.0005;
  double logisticScale = 256.0;
  double l2 = 0.0;  // L2 regularization strength (0 = off)

  std::optional<std::string> weightsOutput;
  std::optional<int> sampleLimit;
  bool shuffleBeforeTraining = true;
  int progressIntervalMs = 750;  // throttle console progress updates
};

struct StockfishResult {
  std::string bestmove;  // single UCI move token, e.g. "e2e4"
};

struct RawSample {
  std::string fen;
  double result = 0.5;  // from side-to-move POV
};

struct PreparedSample {
  double result = 0.5;
  double baseEval = 0.0;
  std::vector<double> gradients;  // dEval/dw_j at defaults
};

std::optional<fs::path> find_stockfish_in_dir(const fs::path& dir) {
  if (dir.empty()) return std::nullopt;
  std::error_code ec;
  if (!fs::exists(dir, ec)) return std::nullopt;
  ec.clear();

  const std::array<const char*, 2> names = {"stockfish", "stockfish.exe"};
  for (const auto* name : names) {
    const fs::path candidate = dir / name;
    std::error_code candidateEc;
    if (fs::exists(candidate, candidateEc) && fs::is_regular_file(candidate, candidateEc)) {
      return candidate;
    }
  }

  for (fs::directory_iterator it{dir, ec}; !ec && it != fs::directory_iterator{}; ++it) {
    const auto& entry = *it;
    bool isFile = false;
    std::error_code regularEc;
    if (entry.is_regular_file(regularEc)) {
      isFile = true;
    } else {
      std::error_code symlinkEc;
      if (entry.is_symlink(symlinkEc)) {
        isFile = true;
      }
    }
    if (!isFile) continue;
    const auto stem = entry.path().stem().string();
    if (stem.rfind("stockfish", 0) == 0) {
      return entry.path();
    }
  }
  return std::nullopt;
}

// Keep only the first 4 FEN fields (piece placement, active color, castling, en passant)
// so positions that differ only by clocks don't sneak in as "unique".
std::string fen_key(std::string_view fen) {
  std::array<std::string, 6> tok{};
  size_t i = 0, start = 0;
  for (; i < 6; ++i) {
    auto sp = fen.find(' ', start);
    if (sp == std::string_view::npos) {
      tok[i] = std::string(fen.substr(start));
      ++i;
      break;
    }
    tok[i] = std::string(fen.substr(start, sp - start));
    start = sp + 1;
  }
  // join first four
  std::ostringstream os;
  os << tok[0] << ' ' << tok[1] << ' ' << tok[2] << ' ' << tok[3];
  return os.str();
}

fs::path locate_project_root(fs::path start) {
  std::error_code ec;
  if (!start.is_absolute()) {
    start = fs::absolute(start, ec);
    if (ec) {
      start = fs::current_path();
    }
  }
  while (true) {
    if (fs::exists(start / "CMakeLists.txt")) {
      return start;
    }
    const auto parent = start.parent_path();
    if (parent.empty() || parent == start) {
      return fs::current_path();
    }
    start = parent;
  }
}

DefaultPaths compute_default_paths(const char* argv0) {
  fs::path exePath;
#ifdef _WIN32
  wchar_t buffer[MAX_PATH];
  DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
  if (len > 0) {
    exePath.assign(buffer, buffer + len);
  }
  if (exePath.empty() && argv0 && *argv0) {
    exePath = fs::path(argv0);
  }
#else
  std::error_code ec;
  exePath = fs::read_symlink("/proc/self/exe", ec);
  if (ec && argv0 && *argv0) {
    exePath = fs::absolute(fs::path(argv0), ec);
  }
  if (ec) {
    exePath.clear();
  }
#endif
  if (exePath.empty()) {
    exePath = fs::current_path();
  }
  fs::path exeDir = exePath;
  if (exeDir.has_filename()) {
    exeDir = exeDir.parent_path();
  }
  if (exeDir.empty()) {
    exeDir = fs::current_path();
  }

  const fs::path projectRoot = locate_project_root(exeDir);
  DefaultPaths defaults;
  defaults.dataFile = projectRoot / "texel_data" / "texel_dataset.txt";
  defaults.weightsFile = projectRoot / "texel_data" / "texel_weights.txt";

  defaults.stockfish = find_stockfish_in_dir(exeDir);
  if (!defaults.stockfish) {
    defaults.stockfish = find_stockfish_in_dir(projectRoot / "tools" / "texel");
  }
  return defaults;
}

[[noreturn]] void usage_and_exit(const DefaultPaths& defaults) {
  std::cerr
      << "Usage: texel_tuner [--generate-data] [--tune] [options]\n"
         "Options:\n"
         "  --stockfish <path>        Path to Stockfish binary (default autodetect)\n"
         "  --games <N>               Number of self-play games for data generation (default 8)\n"
         "  --depth <D>               Search depth for Stockfish (default 12)\n"
         "  --max-plies <N>           Maximum plies per game (default 160)\n"
         "  --sample-skip <N>         Skip first N plies before sampling (default 6)\n"
         "  --sample-stride <N>       Sample every N plies thereafter (default 4)\n"
      << "  --data <file>             Dataset path (default " << defaults.dataFile.string()
      << ")\n"
         "  --iterations <N>          Training iterations (default 200)\n"
         "  --learning-rate <value>   Gradient descent learning rate (default 5e-4)\n"
         "  --scale <value>           Logistic scale in centipawns (default 256)\n"
      << "  --l2 <value>              L2 regularization strength (default 0.0 = off)\n"
         "  --no-shuffle              Do not shuffle dataset before training\n"
      << "  --weights-output <file>   Write tuned weights to file (default "
      << defaults.weightsFile.string()
      << ")\n"
         "  --sample-limit <N>        Limit number of samples used for tuning\n"
         "  --progress-interval <ms>  Min milliseconds between progress updates (default 750)\n"
         "  --help                    Show this message\n";
  std::exit(1);
}

Options parse_args(int argc, char** argv, const DefaultPaths& defaults) {
  Options opts;
  opts.dataFile = defaults.dataFile.string();
  if (defaults.stockfish) {
    opts.stockfishPath = defaults.stockfish->string();
  }
  if (!defaults.weightsFile.empty()) {
    opts.weightsOutput = defaults.weightsFile.string();
  }
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto require_value = [&](const char* name) -> std::string {
      if (i + 1 >= argc) {
        std::cerr << "Missing value for " << name << "\n";
        usage_and_exit(defaults);
      }
      return argv[++i];
    };
    if (arg == "--generate-data") {
      opts.generateData = true;
    } else if (arg == "--tune") {
      opts.tune = true;
    } else if (arg == "--stockfish") {
      opts.stockfishPath = require_value("--stockfish");
    } else if (arg == "--games") {
      opts.games = std::stoi(require_value("--games"));
    } else if (arg == "--depth") {
      opts.depth = std::stoi(require_value("--depth"));
    } else if (arg == "--max-plies") {
      opts.maxPlies = std::stoi(require_value("--max-plies"));
    } else if (arg == "--sample-skip") {
      opts.sampleSkip = std::stoi(require_value("--sample-skip"));
    } else if (arg == "--sample-stride") {
      opts.sampleStride = std::stoi(require_value("--sample-stride"));
    } else if (arg == "--data") {
      opts.dataFile = require_value("--data");
    } else if (arg == "--iterations") {
      opts.iterations = std::stoi(require_value("--iterations"));
    } else if (arg == "--learning-rate") {
      opts.learningRate = std::stod(require_value("--learning-rate"));
    } else if (arg == "--scale") {
      opts.logisticScale = std::stod(require_value("--scale"));
    } else if (arg == "--l2") {
      opts.l2 = std::stod(require_value("--l2"));
    } else if (arg == "--no-shuffle") {
      opts.shuffleBeforeTraining = false;
    } else if (arg == "--weights-output") {
      opts.weightsOutput = require_value("--weights-output");
    } else if (arg == "--sample-limit") {
      opts.sampleLimit = std::stoi(require_value("--sample-limit"));
    } else if (arg == "--progress-interval") {
      opts.progressIntervalMs = std::stoi(require_value("--progress-interval"));
    } else if (arg == "--help" || arg == "-h") {
      usage_and_exit(defaults);
    } else {
      std::cerr << "Unknown option: " << arg << "\n";
      usage_and_exit(defaults);
    }
  }
  if (!opts.generateData && !opts.tune) {
    std::cerr << "Nothing to do: specify --generate-data and/or --tune.\n";
    usage_and_exit(defaults);
  }
  return opts;
}

StockfishResult run_stockfish(const Options& opts, const std::vector<std::string>& moves) {
  if (opts.stockfishPath.empty()) {
    throw std::runtime_error("Stockfish path required for data generation");
  }

  // Build the command script for Stockfish
  std::ostringstream cmdStream;
  cmdStream << "uci\n";
  cmdStream << "isready\n";
  cmdStream << "ucinewgame\n";
  cmdStream << "position startpos";
  if (!moves.empty()) {
    cmdStream << " moves";
    for (const auto& m : moves) cmdStream << ' ' << m;
  }
  cmdStream << "\n";
  if (opts.depth > 0) {
    cmdStream << "go depth " << opts.depth << "\n";
  } else {
    cmdStream << "go movetime 1000\n";
  }
  cmdStream << "quit\n";

  const auto tmpDir = fs::temp_directory_path();
  const auto cmdFile = tmpDir / fs::path("texel_sf_cmd.txt");
  const auto outFile = tmpDir / fs::path("texel_sf_out.txt");

  {
    std::ofstream out(cmdFile, std::ios::trunc | std::ios::binary);
    out << cmdStream.str();
  }

#ifdef _WIN32
  // ---- Windows: spawn without a shell, redirecting stdio to our files ----
  SECURITY_ATTRIBUTES sa{};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;  // allow child to inherit handles

  HANDLE hIn = CreateFileW(cmdFile.wstring().c_str(), GENERIC_READ, FILE_SHARE_READ, &sa,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hIn == INVALID_HANDLE_VALUE) {
    throw std::runtime_error("Failed to open temp stdin file for Stockfish");
  }

  HANDLE hOut = CreateFileW(outFile.wstring().c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hOut == INVALID_HANDLE_VALUE) {
    CloseHandle(hIn);
    throw std::runtime_error("Failed to open temp stdout file for Stockfish");
  }

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;
  si.hStdInput = hIn;
  si.hStdOutput = hOut;
  si.hStdError = hOut;

  PROCESS_INFORMATION pi{};
  std::wstring app = fs::path(opts.stockfishPath).wstring();

  BOOL ok = CreateProcessW(
      /*lpApplicationName=*/app.c_str(),
      /*lpCommandLine=*/nullptr,  // no args
      /*lpProcessAttributes=*/nullptr,
      /*lpThreadAttributes=*/nullptr,
      /*bInheritHandles=*/TRUE,  // inherit our std handles
      /*dwCreationFlags=*/CREATE_NO_WINDOW,
      /*lpEnvironment=*/nullptr,
      /*lpCurrentDirectory=*/nullptr,
      /*lpStartupInfo=*/&si,
      /*lpProcessInformation=*/&pi);

  // We can close our side of the handles now; child has inherited them.
  CloseHandle(hIn);
  CloseHandle(hOut);

  if (!ok) {
    DWORD err = GetLastError();
    throw std::runtime_error("CreateProcessW failed for Stockfish (Win32 error " +
                             std::to_string(err) + ")");
  }

  WaitForSingleObject(pi.hProcess, INFINITE);

  DWORD exitCode = 0;
  GetExitCodeProcess(pi.hProcess, &exitCode);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);

  if (exitCode != 0) {
    throw std::runtime_error("Stockfish exited with code " +
                             std::to_string(static_cast<int>(exitCode)));
  }
#else
  // ---- POSIX: keep simple shell redirection ----
  std::ostringstream execCmd;
  execCmd << '"' << opts.stockfishPath << '"' << " < \"" << cmdFile.string() << "\" > \""
          << outFile.string() << "\"";
  const int rc = std::system(execCmd.str().c_str());
  if (rc != 0) {
    throw std::runtime_error("Failed to run Stockfish command");
  }
#endif

  // Parse bestmove from the captured output
  StockfishResult result;
  {
    std::ifstream in(outFile, std::ios::binary);
    std::string line;
    while (std::getline(in, line)) {
      if (line.rfind("bestmove ", 0) == 0) {
        // Extract only the first token after "bestmove "
        std::istringstream ls(line.substr(9));
        ls >> result.bestmove;
        break;
      }
    }
  }

  std::error_code ec;
  fs::remove(cmdFile, ec);
  fs::remove(outFile, ec);

  return result;
}

core::Color flip_color(core::Color c) {
  return c == core::Color::White ? core::Color::Black : core::Color::White;
}

double result_from_pov(core::GameResult res, core::Color winner, core::Color pov) {
  switch (res) {
    case core::GameResult::CHECKMATE:
      return (winner == pov) ? 1.0 : 0.0;
    case core::GameResult::STALEMATE:
    case core::GameResult::REPETITION:
    case core::GameResult::MOVERULE:
    case core::GameResult::INSUFFICIENT:
      return 0.5;
    default:
      return 0.5;  // treat unknown/ongoing as draw
  }
}

std::vector<RawSample> generate_samples(const Options& opts) {
  if (!opts.generateData) return {};
  std::vector<RawSample> samples;
  std::vector<std::string> moveHistory;
  const size_t maxSamples = opts.sampleLimit ? static_cast<size_t>(*opts.sampleLimit)
                                             : std::numeric_limits<size_t>::max();
  samples.reserve(std::min(static_cast<size_t>(opts.games) * 32u, maxSamples));

  // Deduplicate sampled FENs across all games
  std::unordered_set<std::string> seen;

  const int stride = std::max(1, opts.sampleStride);
  ProgressMeter gamePM("Generating self-play games", static_cast<std::size_t>(opts.games),
                       opts.progressIntervalMs);
  for (int gameIdx = 0; gameIdx < opts.games; ++gameIdx) {
    if (samples.size() >= maxSamples) break;
    model::ChessGame game;
    game.setPosition(core::START_FEN);
    moveHistory.clear();
    std::vector<std::pair<std::string, core::Color>> gamePositions;
    // Maintain individual cadence counters per side so that both colours are sampled even
    // when the stride or skip values would otherwise only hit a single ply parity.
    std::array<int, 2> sideSampleCounters{0, 0};

    for (int ply = 0; ply < opts.maxPlies; ++ply) {
      // If the current position is already terminal, stop the game before sampling.
      game.checkGameResult();
      if (game.getResult() != core::GameResult::ONGOING) {
        break;
      }
      // Sample current position (FEN + POV) periodically
      if (ply >= opts.sampleSkip) {
        const auto sideToMove = game.getGameState().sideToMove;
        auto& counter = sideSampleCounters[static_cast<std::size_t>(sideToMove)];
        if (counter % stride == 0) {
          const auto fen = game.getFen();
          const auto key = fen_key(fen);
          if (seen.insert(key).second) {  // only record new positions
            gamePositions.emplace_back(fen, sideToMove);
          }
        }
        ++counter;
      }

      StockfishResult res = run_stockfish(opts, moveHistory);
      if (res.bestmove.empty() || res.bestmove == "(none)") {
        // Update result for the *current* position (mate/stalemate) before bailing.
        game.checkGameResult();
        // Side to move here has no legal moves -> terminal.
        break;
      }
      if (!game.doMoveUCI(res.bestmove)) {
        // Defensive: if the move fails to parse/play, stop this game.
        break;
      }
      moveHistory.push_back(res.bestmove);

      game.checkGameResult();
      if (game.getResult() != core::GameResult::ONGOING) {
        break;
      }
    }

    const core::GameResult finalRes = game.getResult();

    // If the game ended by checkmate, winner is opposite of side to move.
    // For draws/non-checkmates, targets are 0.5 regardless of 'winner'.
    core::Color winner = flip_color(game.getGameState().sideToMove);

    for (const auto& [fen, pov] : gamePositions) {
      RawSample sample;
      sample.fen = fen;
      sample.result = result_from_pov(finalRes, winner, pov);
      samples.push_back(std::move(sample));
      if (samples.size() >= maxSamples) break;
    }
    if (samples.size() >= maxSamples) break;
    gamePM.update(static_cast<std::size_t>(gameIdx + 1));
  }
  gamePM.finish();
  return samples;
}

void write_dataset(const std::vector<RawSample>& samples, const std::string& path) {
  if (samples.empty()) return;
  fs::path p{path};
  if (p.has_parent_path()) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
  }
  std::ofstream out(path, std::ios::trunc);
  out << "# FEN|result\n";
  for (const auto& s : samples) {
    out << s.fen << '|' << s.result << '\n';
  }
  std::cout << "Wrote " << samples.size() << " unique samples to " << path << "\n";
}

std::vector<RawSample> read_dataset(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("Unable to open dataset: " + path);
  std::vector<RawSample> samples;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    const auto bar = line.find_last_of('|');
    if (bar == std::string::npos) continue;
    RawSample sample;
    sample.fen = line.substr(0, bar);
    sample.result = std::stod(line.substr(bar + 1));
    samples.push_back(std::move(sample));
  }
  return samples;
}

PreparedSample prepare_sample(const RawSample& sample, engine::Evaluator& evaluator,
                              const std::vector<int>& defaults,
                              const std::span<const engine::EvalParamEntry>& entries) {
  model::ChessGame game;
  game.setPosition(sample.fen);
  auto& pos = game.getPositionRefForBot();
  pos.rebuildEvalAcc();

  PreparedSample prepared;
  prepared.result = sample.result;
  prepared.gradients.resize(entries.size());

  evaluator.clearCaches();
  const auto pov = game.getGameState().sideToMove;
  const double sgn = (pov == core::Color::White) ? 1.0 : -1.0;
  prepared.baseEval = sgn * static_cast<double>(evaluator.evaluate(pos));

  constexpr int delta = 1;
  for (size_t i = 0; i < entries.size(); ++i) {
    int* ptr = entries[i].value;
    const int orig = defaults[i];

    *ptr = orig + delta;
    evaluator.clearCaches();
    const double plus = sgn * evaluator.evaluate(pos);

    *ptr = orig - delta;
    evaluator.clearCaches();
    const double minus = sgn * evaluator.evaluate(pos);

    *ptr = orig;
    prepared.gradients[i] = (plus - minus) / (2.0 * delta);
  }
  evaluator.clearCaches();
  return prepared;
}

std::vector<PreparedSample> prepare_samples(std::vector<RawSample> rawSamples,
                                            engine::Evaluator& evaluator,
                                            const std::vector<int>& defaults,
                                            const std::span<const engine::EvalParamEntry>& entries,
                                            const Options& opts) {
  if (opts.sampleLimit && rawSamples.size() > static_cast<size_t>(*opts.sampleLimit)) {
    rawSamples.resize(static_cast<size_t>(*opts.sampleLimit));
  }

  // (Optional) Shuffle to better mix classes and avoid ordering artifacts
  if (opts.shuffleBeforeTraining) {
    std::mt19937_64 rng{std::random_device{}()};
    std::shuffle(rawSamples.begin(), rawSamples.end(), rng);
  }

  std::vector<PreparedSample> prepared;
  prepared.reserve(rawSamples.size());

  ProgressMeter prepPM("Preparing samples", rawSamples.size(), opts.progressIntervalMs);
  std::size_t processed = 0;
  for (const auto& sample : rawSamples) {
    auto ps = prepare_sample(sample, evaluator, defaults, entries);
    prepared.push_back(std::move(ps));
    ++processed;
    prepPM.update(processed);
  }
  prepPM.finish();
  return prepared;
}

struct TrainingResult {
  std::vector<double> weights;
  double finalLoss = 0.0;
};

TrainingResult train(const std::vector<PreparedSample>& samples, const std::vector<int>& defaults,
                     const std::span<const engine::EvalParamEntry>& entries, const Options& opts) {
  if (samples.empty()) throw std::runtime_error("No samples to train on");
  const size_t paramCount = entries.size();
  std::vector<double> weights(defaults.begin(), defaults.end());
  std::vector<double> defaultsD(defaults.begin(), defaults.end());
  std::vector<double> gradient(paramCount, 0.0);

  const double invN = 1.0 / static_cast<double>(samples.size());
  ProgressMeter trainPM("Training (Texel)", static_cast<std::size_t>(opts.iterations),
                        opts.progressIntervalMs);

  for (int iter = 0; iter < opts.iterations; ++iter) {
    std::fill(gradient.begin(), gradient.end(), 0.0);
    double loss = 0.0;

    for (const auto& sample : samples) {
      // Linearized eval around defaults
      double eval = sample.baseEval;
      for (size_t j = 0; j < paramCount; ++j) {
        eval += (weights[j] - defaultsD[j]) * sample.gradients[j];
      }

      // Logistic probability (clamped for numerical safety)
      const double scaled = std::clamp(eval / opts.logisticScale, -500.0, 500.0);
      const double prob = 1.0 / (1.0 + std::exp(-scaled));
      const double target = sample.result;

      // Cross entropy
      const double eps = 1e-12;
      loss += -(target * std::log(std::max(prob, eps)) +
                (1.0 - target) * std::log(std::max(1.0 - prob, eps)));

      // dL/deval = (prob - target) / scale
      const double diff = (prob - target) / opts.logisticScale;
      for (size_t j = 0; j < paramCount; ++j) {
        gradient[j] += diff * sample.gradients[j];
      }
    }

    // Average gradients
    for (size_t j = 0; j < paramCount; ++j) {
      gradient[j] *= invN;
    }

    // L2 regularization around defaults: add lambda * (w - w0) to gradient, and lambda *
    // ||w-w0||^2/2 to loss
    if (opts.l2 > 0.0) {
      for (size_t j = 0; j < paramCount; ++j) {
        const double diff = (weights[j] - defaultsD[j]);
        gradient[j] += opts.l2 * diff;
        loss += 0.5 * opts.l2 * diff * diff;
      }
    }

    // Gradient descent step
    for (size_t j = 0; j < paramCount; ++j) {
      weights[j] -= opts.learningRate * gradient[j];
    }

    // keep output sparse; meter handles the heartbeat, print a few checkpoints
    if ((iter + 1) % std::max(1, opts.iterations / 5) == 0 || iter == opts.iterations - 1) {
      std::cout << "\nIter " << (iter + 1) << "/" << opts.iterations << ": loss=" << (loss * invN)
                << "\n";
    }
    trainPM.update(static_cast<std::size_t>(iter + 1));
  }
  trainPM.finish();

  // Final loss recompute for reporting
  double loss = 0.0;
  for (const auto& sample : samples) {
    double eval = sample.baseEval;
    for (size_t j = 0; j < paramCount; ++j) {
      eval += (weights[j] - defaults[j]) * sample.gradients[j];
    }
    const double scaled = std::clamp(eval / opts.logisticScale, -500.0, 500.0);
    const double prob = 1.0 / (1.0 + std::exp(-scaled));
    const double target = sample.result;
    const double eps = 1e-12;
    loss += -(target * std::log(std::max(prob, eps)) +
              (1.0 - target) * std::log(std::max(1.0 - prob, eps)));
  }
  loss /= static_cast<double>(samples.size());

  if (opts.l2 > 0.0) {
    double reg = 0.0;
    for (size_t j = 0; j < paramCount; ++j) {
      const double diff = (weights[j] - defaults[j]);
      reg += 0.5 * opts.l2 * diff * diff;
    }
    loss += reg;
  }

  TrainingResult tr;
  tr.weights = std::move(weights);
  tr.finalLoss = loss;
  return tr;
}

void emit_weights(const TrainingResult& result, const std::vector<int>& defaults,
                  const std::span<const engine::EvalParamEntry>& entries, const Options& opts,
                  const Options& originalOptsForHeader = Options{}) {
  std::vector<int> tuned;
  tuned.reserve(result.weights.size());
  for (double w : result.weights) tuned.push_back(static_cast<int>(std::llround(w)));

  engine::set_eval_param_values(tuned);

  std::ostream* out = &std::cout;
  std::ofstream file;
  if (opts.weightsOutput) {
    fs::path p{*opts.weightsOutput};
    if (p.has_parent_path()) {
      std::error_code ec;
      fs::create_directories(p.parent_path(), ec);
    }
    file.open(p, std::ios::trunc);
    if (!file) throw std::runtime_error("Unable to open weights output file");
    out = &file;
  }

  *out << "# Tuned evaluation parameters\n";
  *out << "# Texel training loss: " << result.finalLoss << "\n";
  *out << "# scale=" << originalOptsForHeader.logisticScale
       << " lr=" << originalOptsForHeader.learningRate
       << " iters=" << originalOptsForHeader.iterations << " l2=" << originalOptsForHeader.l2
       << " sample_limit="
       << (originalOptsForHeader.sampleLimit ? std::to_string(*originalOptsForHeader.sampleLimit)
                                             : "none")
       << " shuffled=" << (originalOptsForHeader.shuffleBeforeTraining ? "yes" : "no") << "\n";

  for (size_t i = 0; i < entries.size(); ++i) {
    *out << entries[i].name << "=" << tuned[i] << "  # default=" << defaults[i]
         << " tuned=" << result.weights[i] << "\n";
  }
  if (file) {
    std::cout << "Wrote tuned weights to " << *opts.weightsOutput << "\n";
  }
}

}  // namespace lilia::tools::texel

int main(int argc, char** argv) {
  using namespace lilia::tools::texel;
  try {
    lilia::engine::Engine::init();
    const DefaultPaths defaults = compute_default_paths(argc > 0 ? argv[0] : nullptr);
    Options opts = parse_args(argc, argv, defaults);

    if (opts.generateData && opts.stockfishPath.empty()) {
      std::ostringstream err;
      err << "Stockfish executable not found. Place it in tools/texel, next to texel_tuner, or"
          << " provide --stockfish.";
      throw std::runtime_error(err.str());
    }

    if (opts.generateData) {
      std::cout << "Using Stockfish at " << opts.stockfishPath << "\n";
    }

    std::cout << "Dataset path: " << opts.dataFile << "\n";
    if (opts.weightsOutput) {
      std::cout << "Weights output path: " << *opts.weightsOutput << "\n";
    }

    if (opts.generateData) {
      auto samples = generate_samples(opts);
      if (samples.empty()) {
        std::cerr << "No samples generated.\n";
      } else {
        write_dataset(samples, opts.dataFile);
      }
    }

    if (opts.tune) {
      auto rawSamples = read_dataset(opts.dataFile);
      if (rawSamples.empty()) {
        throw std::runtime_error("Dataset is empty");
      }
      lilia::engine::Evaluator evaluator;
      lilia::engine::reset_eval_params();
      auto defaultsVals = lilia::engine::get_eval_param_values();
      auto entriesSpan = lilia::engine::eval_param_entries();
      auto prepared =
          prepare_samples(std::move(rawSamples), evaluator, defaultsVals, entriesSpan, opts);
      std::cout << "Prepared " << prepared.size() << " samples for tuning\n";
      auto result = train(prepared, defaultsVals, entriesSpan, opts);
      emit_weights(result, defaultsVals, entriesSpan, opts, opts);
    }
  } catch (const std::exception& ex) {
    std::cerr << "Error: " << ex.what() << "\n";
    return 1;
  }
  return 0;
}
