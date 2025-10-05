// texel_tuner.cpp — fast parallel self-play, Adam optimizer, cached prepared samples
// Drop-in replacement for your previous file. Compatible CLI + extra flags documented below.

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <optional>
#include <random>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "lilia/constants.hpp"
#include "lilia/engine/engine.hpp"
#include "lilia/engine/eval.hpp"
#include "lilia/engine/eval_shared.hpp"
#include "lilia/model/chess_game.hpp"
#include "lilia/model/core/model_types.hpp"

namespace fs = std::filesystem;
using namespace std::string_literals;

namespace lilia::tools::texel {

// ------------------------ Progress meter ------------------------
struct ProgressMeter {
  std::string label;
  std::size_t total = 0;
  std::atomic<std::size_t> current{0};
  int intervalMs = 750;

  std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
  std::chrono::steady_clock::time_point last = start;
  std::atomic<bool> finished{false};
  bool threadSafe = false;
  mutable std::mutex mutex_;

  ProgressMeter(std::string label_, std::size_t total_, int intervalMs_ = 750,
                bool threadSafe_ = false)
      : label(std::move(label_)), total(total_), intervalMs(intervalMs_), threadSafe(threadSafe_) {}

  static std::string fmt_hms(std::chrono::seconds s) {
    long t = s.count();
    int h = static_cast<int>(t / 3600);
    int m = static_cast<int>((t % 3600) / 60);
    int sec = static_cast<int>(t % 60);
    std::ostringstream os;
    if (h > 0)
      os << h << ":" << std::setw(2) << std::setfill('0') << m << ":" << std::setw(2) << sec;
    else
      os << m << ":" << std::setw(2) << std::setfill('0') << sec;
    return os.str();
  }

  void add(std::size_t delta = 1) {
    if (finished.load(std::memory_order_acquire)) return;
    if (threadSafe) {
      current.fetch_add(delta, std::memory_order_relaxed);
    } else {
      auto cur = current.load(std::memory_order_relaxed);
      cur = std::min(cur + delta, total);
      current.store(cur, std::memory_order_relaxed);
    }
    tick();
  }

  void update(std::size_t newCurrent) {
    if (finished.load(std::memory_order_acquire)) return;
    auto clamped = std::min(newCurrent, total);
    current.store(clamped, std::memory_order_relaxed);
    tick();
  }

  void tick(bool force = false) {
    if (!force && finished.load(std::memory_order_acquire)) return;

    auto now = std::chrono::steady_clock::now();
    std::size_t cur = current.load(std::memory_order_relaxed);
    if (cur > total) cur = total;

    std::unique_lock<std::mutex> lock(mutex_, std::defer_lock);
    if (threadSafe) lock.lock();

    auto since = std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count();
    bool timeToPrint = force || since >= intervalMs || cur == total;
    if (!timeToPrint) return;
    last = now;

    double pct = total ? (100.0 * double(cur) / double(total)) : 0.0;
    double elapsedSec = std::chrono::duration<double>(now - start).count();
    double rate = elapsedSec > 0.0 ? cur / elapsedSec : 0.0;
    double remainSec = (rate > 0.0 && total >= cur) ? (total - cur) / rate : 0.0;

    auto eta = std::chrono::seconds((long long)(remainSec + 0.5));
    auto elapsed = std::chrono::seconds((long long)(elapsedSec + 0.5));

    std::ostringstream line;
    line << "\r" << label << " " << std::fixed << std::setprecision(1) << pct << "% "
         << "(" << cur << "/" << total << ")  "
         << "elapsed " << fmt_hms(elapsed) << "  ETA ~" << fmt_hms(eta);
    std::cout << line.str() << std::flush;
  }

  void finish() {
    if (finished.exchange(true, std::memory_order_acq_rel)) return;
    current.store(total, std::memory_order_relaxed);
    tick(true);
    if (threadSafe) {
      std::lock_guard<std::mutex> lk(mutex_);
      std::cout << "\n";
    } else {
      std::cout << "\n";
    }
  }
};

// ------------------------ Defaults & CLI ------------------------
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
  int depth = 12;  // used when movetimeMs == 0
  int maxPlies = 160;
  int sampleSkip = 6;
  int sampleStride = 4;

  std::string dataFile;
  int iterations = 200;
  double learningRate = 0.0005;
  double logisticScale = 256.0;
  double l2 = 0.0;

  std::optional<std::string> weightsOutput;
  std::optional<int> sampleLimit;
  bool shuffleBeforeTraining = true;
  int progressIntervalMs = 750;

  // ---- Engine / self-play options ----
  int threads = 10;               // Stockfish Threads
  int multipv = 4;                // >= 1
  double tempCp = 80.0;           // softmax temperature in centipawns
  int movetimeMs = 0;             // if >0 use movetime instead of depth
  int movetimeJitterMs = 0;       // +/- jitter added to movetime
  std::optional<int> skillLevel;  // 0..20
  std::optional<int> elo;         // activates UCI_LimitStrength
  std::optional<int> contempt;    // e.g. 20

  // ---- New fast-path options ----
  int genWorkers =
      std::max(1, int(std::thread::hardware_concurrency()));  // parallel self-play workers
  int trainWorkers =
      std::max(1, int(std::thread::hardware_concurrency()));  // parallel training workers
  bool useAdam = true;
  double adamBeta1 = 0.9;
  double adamBeta2 = 0.999;
  double adamEps = 1e-8;
  int logEvery = 0;  // 0 => auto
  // optional cache of prepared samples
  std::optional<std::string> preparedCache;  // path to .bin (read or write)
  bool loadPreparedIfExists = true;
  bool savePrepared = true;
};

struct StockfishResult {
  std::string bestmove;
};

struct RawSample {
  std::string fen;
  double result = 0.5;  // from side-to-move POV
};

struct PreparedSample {
  float result = 0.5f;
  float baseEval = 0.0f;
  std::vector<float> gradients;  // dEval/dw_j at defaults (float to reduce bandwidth)
};

// --- Utility to find Stockfish near exe / project ---
std::optional<fs::path> find_stockfish_in_dir(const fs::path& dir) {
  if (dir.empty()) return std::nullopt;
  std::error_code ec;
  if (!fs::exists(dir, ec)) return std::nullopt;

  const std::array<const char*, 2> names = {"stockfish", "stockfish.exe"};
  for (const auto* name : names) {
    const fs::path candidate = dir / name;
    std::error_code e2;
    if (fs::exists(candidate, e2) && fs::is_regular_file(candidate, e2)) return candidate;
  }
  for (fs::directory_iterator it{dir, ec}; !ec && it != fs::directory_iterator{}; ++it) {
    std::error_code rf, sl;
    bool isFile = it->is_regular_file(rf) || it->is_symlink(sl);
    if (!isFile) continue;
    if (it->path().stem().string().rfind("stockfish", 0) == 0) return it->path();
  }
  return std::nullopt;
}

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
  std::ostringstream os;
  os << tok[0] << ' ' << tok[1] << ' ' << tok[2] << ' ' << tok[3];
  return os.str();
}

fs::path locate_project_root(fs::path start) {
  std::error_code ec;
  if (!start.is_absolute()) start = fs::absolute(start, ec), void(ec);
  while (true) {
    if (fs::exists(start / "CMakeLists.txt")) return start;
    const auto parent = start.parent_path();
    if (parent.empty() || parent == start) return fs::current_path();
    start = parent;
  }
}

DefaultPaths compute_default_paths(const char* argv0) {
  fs::path exePath;
#ifdef _WIN32
  wchar_t buffer[MAX_PATH];
  DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
  if (len > 0) exePath.assign(buffer, buffer + len);
  if (exePath.empty() && argv0 && *argv0) exePath = fs::path(argv0);
#else
  std::error_code ec;
  exePath = fs::read_symlink("/proc/self/exe", ec);
  if (ec && argv0 && *argv0) exePath = fs::absolute(fs::path(argv0), ec);
  if (ec) exePath.clear();
#endif
  if (exePath.empty()) exePath = fs::current_path();
  fs::path exeDir = exePath.has_filename() ? exePath.parent_path() : exePath;
  if (exeDir.empty()) exeDir = fs::current_path();

  const fs::path projectRoot = locate_project_root(exeDir);
  DefaultPaths defaults;
  defaults.dataFile = projectRoot / "texel_data" / "texel_dataset.txt";
  defaults.weightsFile = projectRoot / "texel_data" / "texel_weights.txt";
  defaults.stockfish = find_stockfish_in_dir(exeDir);
  if (!defaults.stockfish)
    defaults.stockfish = find_stockfish_in_dir(projectRoot / "tools" / "texel");
  return defaults;
}

[[noreturn]] void usage_and_exit(const DefaultPaths& d) {
  std::cerr << "Usage: texel_tuner [--generate-data] [--tune] [options]\n"
               "Options:\n"
               "  --stockfish <path>        Path to Stockfish binary (default autodetect)\n"
               "  --games <N>               Self-play games (default 8)\n"
               "  --depth <D>               Stockfish depth (default 12)\n"
               "  --movetime <ms>           Use movetime instead of depth (default off)\n"
               "  --jitter <ms>             +/- movetime jitter (default 0)\n"
               "  --threads <N>             Stockfish Threads (default 10)\n"
               "  --multipv <N>             MultiPV for sampling (default 4)\n"
               "  --temp <cp>               Softmax temperature in centipawns (default 80)\n"
               "  --skill <0..20>           Stockfish Skill Level (optional)\n"
               "  --elo <E>                 UCI_LimitStrength with UCI_Elo=E (optional)\n"
               "  --contempt <C>            Engine Contempt (e.g. 20)\n"
               "  --max-plies <N>           Max plies per game (default 160)\n"
               "  --sample-skip <N>         Skip first N plies before sampling (default 6)\n"
               "  --sample-stride <N>       Sample every N plies thereafter (default 4)\n"
               "  --data <file>             Dataset path (default "
            << d.dataFile.string()
            << ")\n"
               "  --iterations <N>          Training iterations (default 200)\n"
               "  --learning-rate <v>       Learning rate (default 5e-4)\n"
               "  --scale <v>               Logistic scale in centipawns (default 256)\n"
               "  --l2 <v>                  L2 regularization (default 0)\n"
               "  --no-shuffle              Do not shuffle dataset before training\n"
               "  --weights-output <file>   Write tuned weights (default "
            << d.weightsFile.string()
            << ")\n"
               "  --sample-limit <N>        Limit training samples\n"
               "  --progress-interval <ms>  Progress update interval (default 750)\n"
               "  --help                    Show this message\n"
               "\nFast-mode additions:\n"
               "  --gen-workers <N>         Parallel self-play workers (default = hw threads)\n"
               "  --train-workers <N>       Parallel training workers (default = hw threads)\n"
               "  --adam 0|1                Use Adam optimizer (default 1)\n"
               "  --adam-b1 <v>             Adam beta1 (default 0.9)\n"
               "  --adam-b2 <v>             Adam beta2 (default 0.999)\n"
               "  --adam-eps <v>            Adam epsilon (default 1e-8)\n"
               "  --log-every <N>           Log every N iterations (auto if 0)\n"
               "  --prepared-cache <file>   Binary cache for prepared samples (.bin)\n"
               "  --no-load-prepared        Do not load cache even if exists\n"
               "  --no-save-prepared        Do not write cache\n";
  std::exit(1);
}

Options parse_args(int argc, char** argv, const DefaultPaths& defaults) {
  Options o;
  o.dataFile = defaults.dataFile.string();
  if (defaults.stockfish) o.stockfishPath = defaults.stockfish->string();
  if (!defaults.weightsFile.empty()) o.weightsOutput = defaults.weightsFile.string();

  auto require_value = [&](int& i, const char* name, int argc, char** argv) -> std::string {
    if (i + 1 >= argc) {
      std::cerr << "Missing value for " << name << "\n";
      usage_and_exit(defaults);
    }
    return argv[++i];
  };

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--generate-data")
      o.generateData = true;
    else if (arg == "--tune")
      o.tune = true;
    else if (arg == "--stockfish")
      o.stockfishPath = require_value(i, "--stockfish", argc, argv);
    else if (arg == "--games")
      o.games = std::stoi(require_value(i, "--games", argc, argv));
    else if (arg == "--depth")
      o.depth = std::stoi(require_value(i, "--depth", argc, argv));
    else if (arg == "--movetime")
      o.movetimeMs = std::stoi(require_value(i, "--movetime", argc, argv));
    else if (arg == "--jitter")
      o.movetimeJitterMs = std::stoi(require_value(i, "--jitter", argc, argv));
    else if (arg == "--threads")
      o.threads = std::max(1, std::stoi(require_value(i, "--threads", argc, argv)));
    else if (arg == "--multipv")
      o.multipv = std::max(1, std::stoi(require_value(i, "--multipv", argc, argv)));
    else if (arg == "--temp")
      o.tempCp = std::stod(require_value(i, "--temp", argc, argv));
    else if (arg == "--skill")
      o.skillLevel = std::stoi(require_value(i, "--skill", argc, argv));
    else if (arg == "--elo")
      o.elo = std::stoi(require_value(i, "--elo", argc, argv));
    else if (arg == "--contempt")
      o.contempt = std::stoi(require_value(i, "--contempt", argc, argv));
    else if (arg == "--max-plies")
      o.maxPlies = std::stoi(require_value(i, "--max-plies", argc, argv));
    else if (arg == "--sample-skip")
      o.sampleSkip = std::stoi(require_value(i, "--sample-skip", argc, argv));
    else if (arg == "--sample-stride")
      o.sampleStride = std::stoi(require_value(i, "--sample-stride", argc, argv));
    else if (arg == "--data")
      o.dataFile = require_value(i, "--data", argc, argv);
    else if (arg == "--iterations")
      o.iterations = std::stoi(require_value(i, "--iterations", argc, argv));
    else if (arg == "--learning-rate")
      o.learningRate = std::stod(require_value(i, "--learning-rate", argc, argv));
    else if (arg == "--scale")
      o.logisticScale = std::stod(require_value(i, "--scale", argc, argv));
    else if (arg == "--l2")
      o.l2 = std::stod(require_value(i, "--l2", argc, argv));
    else if (arg == "--no-shuffle")
      o.shuffleBeforeTraining = false;
    else if (arg == "--weights-output")
      o.weightsOutput = require_value(i, "--weights-output", argc, argv);
    else if (arg == "--sample-limit")
      o.sampleLimit = std::stoi(require_value(i, "--sample-limit", argc, argv));
    else if (arg == "--progress-interval")
      o.progressIntervalMs = std::stoi(require_value(i, "--progress-interval", argc, argv));
    else if (arg == "--gen-workers")
      o.genWorkers = std::max(1, std::stoi(require_value(i, "--gen-workers", argc, argv)));
    else if (arg == "--train-workers")
      o.trainWorkers = std::max(1, std::stoi(require_value(i, "--train-workers", argc, argv)));
    else if (arg == "--adam")
      o.useAdam = std::stoi(require_value(i, "--adam", argc, argv)) != 0;
    else if (arg == "--adam-b1")
      o.adamBeta1 = std::stod(require_value(i, "--adam-b1", argc, argv));
    else if (arg == "--adam-b2")
      o.adamBeta2 = std::stod(require_value(i, "--adam-b2", argc, argv));
    else if (arg == "--adam-eps")
      o.adamEps = std::stod(require_value(i, "--adam-eps", argc, argv));
    else if (arg == "--log-every")
      o.logEvery = std::stoi(require_value(i, "--log-every", argc, argv));
    else if (arg == "--prepared-cache")
      o.preparedCache = require_value(i, "--prepared-cache", argc, argv);
    else if (arg == "--no-load-prepared")
      o.loadPreparedIfExists = false;
    else if (arg == "--no-save-prepared")
      o.savePrepared = false;
    else if (arg == "--help" || arg == "-h")
      usage_and_exit(defaults);
    else {
      std::cerr << "Unknown option: " << arg << "\n";
      usage_and_exit(defaults);
    }
  }
  if (!o.generateData && !o.tune) {
    std::cerr << "Nothing to do: specify --generate-data and/or --tune.\n";
    usage_and_exit(defaults);
  }
  return o;
}

// ------------------------ Helpers ------------------------
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
      return 0.5;
  }
}

// ------------------------ Persistent UCI Engine ------------------------
class UciEngine {
 public:
  explicit UciEngine(const std::string& exe, const Options& opts)
      : exePath_(exe), opts_(opts), rng_(std::random_device{}()) {
    if (exePath_.empty()) throw std::runtime_error("UCI engine path is empty");
    spawn();
    uci_handshake();
    apply_options();
  }
  ~UciEngine() { terminate(); }

  void ucinewgame() {
    sendln("ucinewgame");
    isready();
  }

  // Choose move for "position startpos [moves ...]" using MultiPV sampling
  std::string pick_move_from_startpos(const std::vector<std::string>& moves) {
    {
      std::ostringstream os;
      os << "position startpos";
      if (!moves.empty()) {
        os << " moves";
        for (const auto& m : moves) os << ' ' << m;
      }
      sendln(os.str());
    }

    // Build go command
    std::string goCmd;
    if (opts_.movetimeMs > 0) {
      int mt = opts_.movetimeMs;
      if (opts_.movetimeJitterMs > 0) {
        std::uniform_int_distribution<int> dist(-opts_.movetimeJitterMs, opts_.movetimeJitterMs);
        mt = std::max(5, mt + dist(rng_));
      }
      goCmd = "go movetime " + std::to_string(mt);
    } else if (opts_.depth > 0) {
      goCmd = "go depth " + std::to_string(opts_.depth);
    } else {
      goCmd = "go movetime 1000";
    }
    sendln(goCmd);

    struct Cand {
      std::string move;
      double scoreCp = 0.0;
      int multipv = 1;
    };
    std::vector<Cand> cands;
    int bestDepth = -1;

    for (;;) {
      std::string line = readline_blocking();
      if (line.empty()) continue;

      if (starts_with(line, "info ")) {
        auto tok = tokenize(line);
        int depth = -1, mpv = 1;
        bool haveScore = false, isMate = false;
        int scoreCp = 0, matePly = 0;
        std::string firstMove;
        for (size_t i = 0; i + 1 < tok.size(); ++i) {
          if (tok[i] == "depth")
            depth = to_int(tok[i + 1]);
          else if (tok[i] == "multipv")
            mpv = std::max(1, to_int(tok[i + 1]));
          else if (tok[i] == "score" && i + 2 < tok.size()) {
            if (tok[i + 1] == "cp") {
              haveScore = true;
              scoreCp = to_int(tok[i + 2]);
            } else if (tok[i + 1] == "mate") {
              haveScore = true;
              isMate = true;
              matePly = to_int(tok[i + 2]);
            }
          } else if (tok[i] == "pv" && i + 1 < tok.size()) {
            firstMove = tok[i + 1];
            break;
          }
        }
        if (depth >= 0 && haveScore && !firstMove.empty()) {
          if (depth > bestDepth) {
            bestDepth = depth;
            cands.clear();
          }
          if (depth == bestDepth) {
            double cp = isMate ? (matePly >= 0 ? 30000.0 : -30000.0) : double(scoreCp);
            cands.push_back(Cand{firstMove, cp, mpv});
          }
        }
        continue;
      }

      if (starts_with(line, "bestmove ")) {
        std::string best = word_after(line, "bestmove");
        if (cands.empty() || opts_.multipv <= 1) return best.empty() ? "(none)" : best;

        std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {
          if (a.multipv != b.multipv) return a.multipv < b.multipv;
          if (a.scoreCp != b.scoreCp) return a.scoreCp > b.scoreCp;
          return a.move < b.move;
        });
        cands.erase(std::unique(cands.begin(), cands.end(),
                                [](const Cand& a, const Cand& b) { return a.move == b.move; }),
                    cands.end());

        const double T = std::max(1e-3, opts_.tempCp);
        double maxCp = -1e300;
        for (const auto& c : cands) maxCp = std::max(maxCp, c.scoreCp);
        std::vector<double> w;
        w.reserve(cands.size());
        double sum = 0.0;
        for (const auto& c : cands) {
          double wi = std::exp((c.scoreCp - maxCp) / T);
          w.push_back(wi);
          sum += wi;
        }
        if (sum <= 0.0) return best.empty() ? "(none)" : best;

        std::uniform_real_distribution<double> U(0.0, sum);
        double r = U(rng_), acc = 0.0;
        for (size_t i = 0; i < cands.size(); ++i) {
          acc += w[i];
          if (r <= acc) return cands[i].move;
        }
        return cands.back().move;
      }
    }
  }

 private:
#ifdef _WIN32
  PROCESS_INFORMATION pi_{};                // child
  HANDLE hInWrite_{NULL}, hOutRead_{NULL};  // our handles
#else
  pid_t pid_ = -1;
  int in_w_ = -1, out_r_ = -1;
#endif
  FILE* fin_ = nullptr;   // read from engine stdout
  FILE* fout_ = nullptr;  // write to engine stdin
  std::string exePath_;
  Options opts_;
  std::mt19937_64 rng_;

  static bool starts_with(const std::string& s, const char* pfx) { return s.rfind(pfx, 0) == 0; }
  static int to_int(const std::string& s) {
    try {
      return std::stoi(s);
    } catch (...) {
      return 0;
    }
  }
  static std::vector<std::string> tokenize(const std::string& s) {
    std::vector<std::string> v;
    std::istringstream is(s);
    std::string t;
    while (is >> t) v.push_back(std::move(t));
    return v;
  }
  static std::string word_after(const std::string& s, const char* key) {
    std::istringstream is(s);
    std::string w;
    is >> w;
    if (w != key) return {};
    return (is >> w) ? w : std::string();
  }

  void sendln(const std::string& s) {
    if (!fout_) throw std::runtime_error("UCI engine stdin closed");
    std::fputs(s.c_str(), fout_);
    std::fputc('\n', fout_);
    std::fflush(fout_);
  }

  std::string readline_blocking() {
    if (!fin_) throw std::runtime_error("UCI engine stdout closed");
    std::string line;
    int ch;
    while ((ch = std::fgetc(fin_)) != EOF) {
      if (ch == '\r') continue;
      if (ch == '\n') break;
      line.push_back((char)ch);
    }
    return line;
  }

  void isready() {
    sendln("isready");
    for (;;) {
      std::string l = readline_blocking();
      if (l == "readyok") break;
    }
  }

  void uci_handshake() {
    sendln("uci");
    for (;;) {
      std::string l = readline_blocking();
      if (l == "uciok") break;
    }
    isready();
  }

  void apply_options() {
    sendln("setoption name Threads value " + std::to_string(std::max(1, opts_.threads)));
    if (opts_.skillLevel)
      sendln("setoption name Skill Level value " + std::to_string(*opts_.skillLevel));
    if (opts_.elo) {
      sendln("setoption name UCI_LimitStrength value true");
      sendln("setoption name UCI_Elo value " + std::to_string(*opts_.elo));
    }
    if (opts_.contempt) sendln("setoption name Contempt value " + std::to_string(*opts_.contempt));
    // Set MultiPV ONCE (avoid per-move chatter)
    sendln("setoption name MultiPV value " + std::to_string(std::max(1, opts_.multipv)));
    // Ensure engine is settled
    isready();
  }

  void spawn() {
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE hOutWrite = NULL, hInReadLocal = NULL;
    if (!CreatePipe(&hOutRead_, &hOutWrite, &sa, 0))
      throw std::runtime_error("CreatePipe stdout failed");
    if (!SetHandleInformation(hOutRead_, HANDLE_FLAG_INHERIT, 0))
      throw std::runtime_error("stdout SetHandleInformation failed");
    HANDLE hInRead = NULL;
    if (!CreatePipe(&hInRead, &hInWrite_, &sa, 0))
      throw std::runtime_error("CreatePipe stdin failed");
    if (!SetHandleInformation(hInWrite_, HANDLE_FLAG_INHERIT, 0))
      throw std::runtime_error("stdin SetHandleInformation failed");
    hInReadLocal = hInRead;

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = hInReadLocal;
    si.hStdOutput = hOutWrite;
    si.hStdError = hOutWrite;

    std::wstring app = fs::path(exePath_).wstring();
    if (!CreateProcessW(app.c_str(), nullptr, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                        nullptr, &si, &pi_))
      throw std::runtime_error("CreateProcessW failed for Stockfish");
    CloseHandle(hOutWrite);
    CloseHandle(hInReadLocal);

    int fdIn = _open_osfhandle(reinterpret_cast<intptr_t>(hInWrite_), _O_WRONLY | _O_BINARY);
    int fdOut = _open_osfhandle(reinterpret_cast<intptr_t>(hOutRead_), _O_RDONLY | _O_BINARY);
    if (fdIn == -1 || fdOut == -1) throw std::runtime_error("_open_osfhandle failed");
    fout_ = _fdopen(fdIn, "wb");
    fin_ = _fdopen(fdOut, "rb");
    if (!fin_ || !fout_) throw std::runtime_error("_fdopen failed");
    setvbuf(fout_, nullptr, _IONBF, 0);
#else
    int inpipe[2]{}, outpipe[2]{};
    if (pipe(inpipe) != 0 || pipe(outpipe) != 0) throw std::runtime_error("pipe() failed");
    pid_ = fork();
    if (pid_ == -1) throw std::runtime_error("fork() failed");
    if (pid_ == 0) {
      dup2(inpipe[0], STDIN_FILENO);
      dup2(outpipe[1], STDOUT_FILENO);
      dup2(outpipe[1], STDERR_FILENO);
      close(inpipe[0]);
      close(inpipe[1]);
      close(outpipe[0]);
      close(outpipe[1]);
      execl(exePath_.c_str(), exePath_.c_str(), (char*)nullptr);
      _exit(127);
    }
    close(inpipe[0]);
    close(outpipe[1]);
    in_w_ = inpipe[1];
    out_r_ = outpipe[0];
    fout_ = fdopen(in_w_, "w");
    fin_ = fdopen(out_r_, "r");
    if (!fin_ || !fout_) throw std::runtime_error("fdopen failed");
    setvbuf(fout_, nullptr, _IONBF, 0);
#endif
  }

  void terminate() {
#ifdef _WIN32
    if (fout_) {
      std::fputs("quit\n", fout_);
      std::fflush(fout_);
    }
    if (pi_.hProcess) {
      WaitForSingleObject(pi_.hProcess, 500);
      CloseHandle(pi_.hThread);
      CloseHandle(pi_.hProcess);
      pi_.hThread = pi_.hProcess = NULL;
    }
#else
    if (fout_) {
      std::fputs("quit\n", fout_);
      std::fflush(fout_);
    }
    if (pid_ > 0) {
      int status = 0;
      waitpid(pid_, &status, 0);
      pid_ = -1;
    }
#endif
    if (fin_) {
      std::fclose(fin_);
      fin_ = nullptr;
    }
    if (fout_) {
      std::fclose(fout_);
      fout_ = nullptr;
    }
  }
};

// ------------------------ Data generation (parallel self-play) ------------------------
struct GameBatchCfg {
  int games = 0;
  int maxPlies = 160;
  int sampleSkip = 6;
  int sampleStride = 4;
  int movetimeMs = 0;
  int movetimeJitterMs = 0;
};

static void run_games_worker(int workerId, const Options& opts, std::atomic<int>& nextGame,
                             int totalGames, std::vector<RawSample>& outSamples,
                             std::mutex& outMutex, ProgressMeter& pm) {
  // Each worker its own engine and RNG
  UciEngine engine(opts.stockfishPath, opts);
  std::vector<RawSample> local;
  local.reserve(8192);
  std::vector<std::string> moveHistory;

  for (;;) {
    int g = nextGame.fetch_add(1, std::memory_order_relaxed);
    if (g >= totalGames) break;

    engine.ucinewgame();
    model::ChessGame game;
    game.setPosition(core::START_FEN);
    moveHistory.clear();

    std::vector<std::pair<std::string, core::Color>> gamePositions;
    std::array<int, 2> sideSampleCounters{0, 0};

    for (int ply = 0; ply < opts.maxPlies; ++ply) {
      game.checkGameResult();
      if (game.getResult() != core::GameResult::ONGOING) break;

      if (ply >= opts.sampleSkip) {
        const auto sideToMove = game.getGameState().sideToMove;
        auto& counter = sideSampleCounters[(size_t)sideToMove];
        if (counter % std::max(1, opts.sampleStride) == 0) {
          const auto fen = game.getFen();
          gamePositions.emplace_back(fen, sideToMove);
        }
        ++counter;
      }

      std::string mv = engine.pick_move_from_startpos(moveHistory);
      if (mv.empty() || mv == "(none)") {
        game.checkGameResult();
        break;
      }
      if (!game.doMoveUCI(mv)) {
        break;
      }
      moveHistory.push_back(mv);

      game.checkGameResult();
      if (game.getResult() != core::GameResult::ONGOING) break;
    }

    const core::GameResult finalRes = game.getResult();
    core::Color winner = flip_color(game.getGameState().sideToMove);

    for (const auto& [fen, pov] : gamePositions) {
      RawSample s;
      s.fen = fen;
      s.result = result_from_pov(finalRes, winner, pov);
      local.push_back(std::move(s));
    }

    pm.add(1);
  }

  // Merge local samples (we dedup after merge to reduce contention)
  {
    std::lock_guard<std::mutex> lk(outMutex);
    outSamples.insert(outSamples.end(), std::make_move_iterator(local.begin()),
                      std::make_move_iterator(local.end()));
  }
}

std::vector<RawSample> generate_samples_parallel(const Options& opts) {
  if (!opts.generateData) return {};
  if (opts.stockfishPath.empty()) {
    throw std::runtime_error("Stockfish path required for data generation");
  }

  const int W = std::max(1, opts.genWorkers);
  std::vector<std::thread> threads;
  std::vector<RawSample> samples;
  samples.reserve(size_t(opts.games) * 32u);
  std::mutex samplesMutex;
  std::atomic<int> nextGame{0};

  ProgressMeter pm("Generating self-play games (parallel)", (std::size_t)opts.games,
                   opts.progressIntervalMs, true);

  threads.reserve(W);
  for (int w = 0; w < W; ++w) {
    threads.emplace_back(run_games_worker, w, std::cref(opts), std::ref(nextGame), opts.games,
                         std::ref(samples), std::ref(samplesMutex), std::ref(pm));
  }
  for (auto& t : threads) t.join();
  pm.finish();

  // Deduplicate FEN keys globally (keep first occurrence)
  std::unordered_set<std::string> seen;
  seen.reserve(samples.size() * 2 + 16);
  std::vector<RawSample> unique;
  unique.reserve(samples.size());
  for (auto& s : samples) {
    auto key = fen_key(s.fen);
    if (seen.insert(key).second) unique.push_back(std::move(s));
  }

  // Enforce sampleLimit if set
  if (opts.sampleLimit && unique.size() > (size_t)*opts.sampleLimit) {
    unique.resize((size_t)*opts.sampleLimit);
  }
  return unique;
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
  for (const auto& s : samples) out << s.fen << '|' << s.result << '\n';
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

// ------------------------ Prepared cache I/O (binary, fast) ------------------------
struct PreparedCacheHeader {
  uint32_t magic = 0x54455845u;  // 'TEXE'
  uint32_t version = 1;
  uint32_t paramCount = 0;
  uint64_t sampleCount = 0;
  double logisticScale = 256.0;
};

bool load_prepared_cache(const std::string& path, std::vector<PreparedSample>& out,
                         uint32_t expectedParams, double expectedScale) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  PreparedCacheHeader h{};
  f.read(reinterpret_cast<char*>(&h), sizeof(h));
  if (!f || h.magic != 0x54455845u || h.version != 1) return false;
  if (h.paramCount != expectedParams) return false;
  if (std::abs(h.logisticScale - expectedScale) > 1e-9) return false;

  out.clear();
  out.resize(h.sampleCount);
  for (uint64_t i = 0; i < h.sampleCount; ++i) {
    float res, base;
    f.read(reinterpret_cast<char*>(&res), sizeof(float));
    f.read(reinterpret_cast<char*>(&base), sizeof(float));
    out[i].result = res;
    out[i].baseEval = base;
  }
  // gradients blob
  for (uint64_t i = 0; i < h.sampleCount; ++i) {
    out[i].gradients.resize(h.paramCount);
    f.read(reinterpret_cast<char*>(out[i].gradients.data()), sizeof(float) * h.paramCount);
  }
  return (bool)f;
}

bool save_prepared_cache(const std::string& path, const std::vector<PreparedSample>& samples,
                         uint32_t paramCount, double logisticScale) {
  fs::path p{path};
  if (p.has_parent_path()) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
  }
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) return false;
  PreparedCacheHeader h{};
  h.paramCount = paramCount;
  h.sampleCount = samples.size();
  h.logisticScale = logisticScale;
  f.write(reinterpret_cast<const char*>(&h), sizeof(h));
  for (const auto& s : samples) {
    f.write(reinterpret_cast<const char*>(&s.result), sizeof(float));
    f.write(reinterpret_cast<const char*>(&s.baseEval), sizeof(float));
  }
  for (const auto& s : samples) {
    f.write(reinterpret_cast<const char*>(s.gradients.data()), sizeof(float) * s.gradients.size());
  }
  return (bool)f;
}

// ------------------------ Texel preparation & training ------------------------
PreparedSample prepare_sample(const RawSample& sample, engine::Evaluator& evaluator,
                              const std::vector<int>& defaults,
                              const std::span<const engine::EvalParamEntry>& entries) {
  model::ChessGame game;
  game.setPosition(sample.fen);
  auto& pos = game.getPositionRefForBot();
  pos.rebuildEvalAcc();

  PreparedSample prepared;
  prepared.result = (float)sample.result;
  prepared.gradients.resize(entries.size());

  evaluator.clearCaches();
  const auto pov = game.getGameState().sideToMove;
  const double sgn = (pov == core::Color::White) ? 1.0 : -1.0;
  prepared.baseEval = (float)(sgn * (double)evaluator.evaluate(pos));

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

    prepared.gradients[i] = (float)((plus - minus) / (2.0 * delta));
  }
  evaluator.clearCaches();
  return prepared;
}

std::vector<PreparedSample> prepare_samples(std::vector<RawSample> rawSamples,
                                            engine::Evaluator& evaluator,
                                            const std::vector<int>& defaults,
                                            const std::span<const engine::EvalParamEntry>& entries,
                                            const Options& opts) {
  if (opts.sampleLimit && rawSamples.size() > (size_t)*opts.sampleLimit)
    rawSamples.resize((size_t)*opts.sampleLimit);

  if (opts.shuffleBeforeTraining) {
    std::mt19937_64 rng{std::random_device{}()};
    std::shuffle(rawSamples.begin(), rawSamples.end(), rng);
  }

  std::vector<PreparedSample> prepared;
  prepared.resize(rawSamples.size());

  ProgressMeter prepPM("Preparing samples (finite-diff)", rawSamples.size(),
                       opts.progressIntervalMs);
  for (size_t i = 0; i < rawSamples.size(); ++i) {
    prepared[i] = prepare_sample(rawSamples[i], evaluator, defaults, entries);
    prepPM.add(1);
  }
  prepPM.finish();
  return prepared;
}

// ------------------------ Parallel training (Adam or SGD) ------------------------
struct TrainingResult {
  std::vector<double> weights;
  double finalLoss = 0.0;
};

TrainingResult train_parallel(const std::vector<PreparedSample>& samples,
                              const std::vector<int>& defaults,
                              const std::span<const engine::EvalParamEntry>& entries,
                              const Options& opts) {
  if (samples.empty()) throw std::runtime_error("No samples to train on");
  const size_t P = entries.size();
  const size_t N = samples.size();

  std::vector<double> w(defaults.begin(), defaults.end());
  std::vector<double> w0(defaults.begin(), defaults.end());
  std::vector<double> g(P, 0.0);

  // Adam state
  std::vector<double> m(P, 0.0), v(P, 0.0);
  double b1 = opts.adamBeta1, b2 = opts.adamBeta2, eps = opts.adamEps;
  double b1t = 1.0, b2t = 1.0;

  const double invN = 1.0 / (double)N;
  const int logEvery = (opts.logEvery > 0) ? opts.logEvery : std::max(1, opts.iterations / 5);
  ProgressMeter trainPM("Training (Texel, parallel)", (std::size_t)opts.iterations,
                        opts.progressIntervalMs);

  // Thread-local accumulators
  const int TW = std::max(1, opts.trainWorkers);
  std::vector<std::vector<double>> tg(TW, std::vector<double>(P, 0.0));
  std::vector<double> threadLoss(TW, 0.0);

  std::vector<size_t> rangesB(TW + 1, 0);
  for (int t = 0; t < TW; ++t) {
    rangesB[t] = (N * t) / TW;
  }
  rangesB[TW] = N;

  for (int iter = 0; iter < opts.iterations; ++iter) {
    // zero grads
    for (int t = 0; t < TW; ++t) std::fill(tg[t].begin(), tg[t].end(), 0.0);
    std::fill(threadLoss.begin(), threadLoss.end(), 0.0);

    // parallel over samples (read-only sample data, write to thread-local grads)
    std::vector<std::thread> threads;
    threads.reserve(TW);
    for (int t = 0; t < TW; ++t) {
      threads.emplace_back([&, t]() {
        size_t start = rangesB[t], end = rangesB[t + 1];
        auto& G = tg[t];
        double lossLocal = 0.0;

        for (size_t i = start; i < end; ++i) {
          const auto& s = samples[i];
          double eval = s.baseEval;
          // (w - w0) dot grad
          for (size_t j = 0; j < P; ++j) eval += (w[j] - w0[j]) * (double)s.gradients[j];
          double scaled = std::clamp(eval / opts.logisticScale, -500.0, 500.0);
          double prob = 1.0 / (1.0 + std::exp(-scaled));
          double target = s.result;

          const double epsStab = 1e-12;
          lossLocal += -(target * std::log(std::max(prob, epsStab)) +
                         (1.0 - target) * std::log(std::max(1.0 - prob, epsStab)));

          double diff = (prob - target) / opts.logisticScale;
          // accumulate gradient: G += diff * grad_i
          for (size_t j = 0; j < P; ++j) G[j] += diff * (double)s.gradients[j];
        }
        threadLoss[t] = lossLocal;
      });
    }
    for (auto& th : threads) th.join();

    // reduce
    std::fill(g.begin(), g.end(), 0.0);
    double loss = 0.0;
    for (int t = 0; t < TW; ++t) {
      for (size_t j = 0; j < P; ++j) g[j] += tg[t][j];
      loss += threadLoss[t];
    }
    for (size_t j = 0; j < P; ++j) g[j] *= invN;
    loss *= invN;

    if (opts.l2 > 0.0) {
      for (size_t j = 0; j < P; ++j) {
        const double d = (w[j] - w0[j]);
        g[j] += opts.l2 * d;
        loss += 0.5 * opts.l2 * d * d;
      }
    }

    // update
    if (opts.useAdam) {
      b1t *= b1;
      b2t *= b2;
      for (size_t j = 0; j < P; ++j) {
        m[j] = b1 * m[j] + (1.0 - b1) * g[j];
        v[j] = b2 * v[j] + (1.0 - b2) * (g[j] * g[j]);
        double mhat = m[j] / (1.0 - b1t);
        double vhat = v[j] / (1.0 - b2t);
        w[j] -= opts.learningRate * mhat / (std::sqrt(vhat) + eps);
      }
    } else {
      for (size_t j = 0; j < P; ++j) w[j] -= opts.learningRate * g[j];
    }

    if ((iter + 1) % logEvery == 0 || iter == opts.iterations - 1) {
      std::cout << "\nIter " << (iter + 1) << "/" << opts.iterations << ": loss=" << loss << "\n";
    }
    trainPM.add(1);
  }
  trainPM.finish();

  // final exact loss report (same reduction)
  double finalLoss = 0.0;
  {
    std::vector<std::thread> threads2;
    std::vector<double> tLoss(TW, 0.0);
    for (int t = 0; t < TW; ++t) {
      threads2.emplace_back([&, t]() {
        size_t start = rangesB[t], end = rangesB[t + 1];
        double lossLocal = 0.0;
        for (size_t i = start; i < end; ++i) {
          const auto& s = samples[i];
          double eval = s.baseEval;
          for (size_t j = 0; j < P; ++j) eval += (w[j] - w0[j]) * (double)s.gradients[j];
          double scaled = std::clamp(eval / opts.logisticScale, -500.0, 500.0);
          double prob = 1.0 / (1.0 + std::exp(-scaled));
          double target = s.result;
          const double epsStab = 1e-12;
          lossLocal += -(target * std::log(std::max(prob, epsStab)) +
                         (1.0 - target) * std::log(std::max(1.0 - prob, epsStab)));
        }
        tLoss[t] = lossLocal;
      });
    }
    for (auto& th : threads2) th.join();
    for (int t = 0; t < TW; ++t) finalLoss += tLoss[t];
    finalLoss /= (double)N;
    if (opts.l2 > 0.0) {
      double reg = 0.0;
      for (size_t j = 0; j < P; ++j) {
        double d = (w[j] - w0[j]);
        reg += 0.5 * opts.l2 * d * d;
      }
      finalLoss += reg;
    }
  }

  TrainingResult tr;
  tr.weights = std::move(w);
  tr.finalLoss = finalLoss;
  return tr;
}

void emit_weights(const TrainingResult& result, const std::vector<int>& defaults,
                  const std::span<const engine::EvalParamEntry>& entries, const Options& opts,
                  const Options& originalOptsForHeader = Options{}) {
  std::vector<int> tuned;
  tuned.reserve(result.weights.size());
  for (double w : result.weights) tuned.push_back((int)std::llround(w));

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
       << " shuffled=" << (originalOptsForHeader.shuffleBeforeTraining ? "yes" : "no")
       << " adam=" << (originalOptsForHeader.useAdam ? "yes" : "no")
       << " train_workers=" << originalOptsForHeader.trainWorkers
       << " gen_workers=" << originalOptsForHeader.genWorkers << "\n";

  for (size_t i = 0; i < entries.size(); ++i) {
    *out << entries[i].name << "=" << tuned[i] << "  # default=" << defaults[i]
         << " tuned=" << result.weights[i] << "\n";
  }
  if (file) std::cout << "Wrote tuned weights to " << *opts.weightsOutput << "\n";
}

}  // namespace lilia::tools::texel

// ------------------------ main ------------------------
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
      std::cout << "Threads=" << opts.threads << " MultiPV=" << opts.multipv
                << " temp(cp)=" << opts.tempCp
                << (opts.movetimeMs > 0
                        ? (" movetime=" + std::to_string(opts.movetimeMs) +
                           "ms jitter=" + std::to_string(opts.movetimeJitterMs) + "ms")
                        : (" depth=" + std::to_string(opts.depth)))
                << (opts.skillLevel ? (" skill=" + std::to_string(*opts.skillLevel)) : "")
                << (opts.elo ? (" elo=" + std::to_string(*opts.elo)) : "")
                << (opts.contempt ? (" contempt=" + std::to_string(*opts.contempt)) : "")
                << " gen_workers=" << opts.genWorkers << "\n";
    }

    std::cout << "Dataset path: " << opts.dataFile << "\n";
    if (opts.weightsOutput) std::cout << "Weights output path: " << *opts.weightsOutput << "\n";

    if (opts.generateData) {
      auto samples = generate_samples_parallel(opts);
      if (samples.empty()) {
        std::cerr << "No samples generated.\n";
      } else {
        write_dataset(samples, opts.dataFile);
      }
    }

    if (opts.tune) {
      auto rawSamples = read_dataset(opts.dataFile);
      if (rawSamples.empty()) throw std::runtime_error("Dataset is empty");

      lilia::engine::Evaluator evaluator;
      lilia::engine::reset_eval_params();
      auto defaultsVals = lilia::engine::get_eval_param_values();
      auto entriesSpan = lilia::engine::eval_param_entries();

      std::vector<PreparedSample> prepared;

      // Optional: load cached prepared samples if compatible
      bool loadedFromCache = false;
      if (opts.preparedCache && opts.loadPreparedIfExists) {
        loadedFromCache = load_prepared_cache(*opts.preparedCache, prepared,
                                              (uint32_t)entriesSpan.size(), opts.logisticScale);
        if (loadedFromCache)
          std::cout << "Loaded prepared samples from cache: " << *opts.preparedCache << "\n";
      }
      if (!loadedFromCache) {
        prepared =
            prepare_samples(std::move(rawSamples), evaluator, defaultsVals, entriesSpan, opts);
        std::cout << "Prepared " << prepared.size() << " samples for tuning\n";
        if (opts.preparedCache && opts.savePrepared) {
          if (save_prepared_cache(*opts.preparedCache, prepared, (uint32_t)entriesSpan.size(),
                                  opts.logisticScale))
            std::cout << "Saved prepared cache to " << *opts.preparedCache << "\n";
          else
            std::cout << "Warning: failed to save prepared cache to " << *opts.preparedCache
                      << "\n";
        }
      } else {
        std::cout << "Prepared " << prepared.size() << " samples (from cache)\n";
      }

      auto result = train_parallel(prepared, defaultsVals, entriesSpan, opts);
      emit_weights(result, defaultsVals, entriesSpan, opts, opts);
    }
  } catch (const std::exception& ex) {
    std::cerr << "Error: " << ex.what() << "\n";
    return 1;
  }
  return 0;
}
