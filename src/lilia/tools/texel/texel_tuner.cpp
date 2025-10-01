#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
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

  // ---- Engine randomness / options ----
  int threads = 10;
  int multipv = 4;                // >= 1
  double tempCp = 80.0;           // softmax temperature in centipawns
  int movetimeMs = 0;             // if >0 use movetime instead of depth
  int movetimeJitterMs = 0;       // +/- jitter added to movetime
  std::optional<int> skillLevel;  // 0..20
  std::optional<int> elo;         // activates UCI_LimitStrength
  std::optional<int> contempt;    // e.g. 20
};

struct StockfishResult {
  std::string bestmove;
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

// --- Utility to find Stockfish near exe / project ---
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
         "  --movetime <ms>           Use movetime in ms instead of depth (default off)\n"
         "  --jitter <ms>             +/- movetime jitter in ms (default 0)\n"
         "  --threads <N>             Engine Threads (default 1)\n"
         "  --multipv <N>             MultiPV count for sampling (default 4)\n"
         "  --temp <cp>               Softmax temperature in centipawns (default 80)\n"
         "  --skill <0..20>           Stockfish Skill Level (optional)\n"
         "  --elo <E>                 Enable UCI_LimitStrength with UCI_Elo=E (optional)\n"
         "  --contempt <C>            Engine Contempt (e.g. 20) to reduce drawish lines\n"
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
    } else if (arg == "--movetime") {
      opts.movetimeMs = std::stoi(require_value("--movetime"));
    } else if (arg == "--jitter") {
      opts.movetimeJitterMs = std::stoi(require_value("--jitter"));
    } else if (arg == "--threads") {
      opts.threads = std::max(1, std::stoi(require_value("--threads")));
    } else if (arg == "--multipv") {
      opts.multipv = std::max(1, std::stoi(require_value("--multipv")));
    } else if (arg == "--temp") {
      opts.tempCp = std::stod(require_value("--temp"));
    } else if (arg == "--skill") {
      opts.skillLevel = std::stoi(require_value("--skill"));
    } else if (arg == "--elo") {
      opts.elo = std::stoi(require_value("--elo"));
    } else if (arg == "--contempt") {
      opts.contempt = std::stoi(require_value("--contempt"));
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

    // Ensure MultiPV is set (>=1)
    {
      std::ostringstream os;
      os << "setoption name MultiPV value " << std::max(1, opts_.multipv);
      sendln(os.str());
      isready();  // make sure option is applied
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
        // coarse parse of tokens
        auto tok = tokenize(line);
        int depth = -1, mpv = 1;
        bool haveScore = false, isMate = false;
        int scoreCp = 0, matePly = 0;
        std::string firstMove;

        for (size_t i = 0; i + 1 < tok.size(); ++i) {
          if (tok[i] == "depth") {
            depth = to_int(tok[i + 1]);
          } else if (tok[i] == "multipv") {
            mpv = std::max(1, to_int(tok[i + 1]));
          } else if (tok[i] == "score" && i + 2 < tok.size()) {
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
            double cp = isMate ? (matePly >= 0 ? 30000.0 : -30000.0) : static_cast<double>(scoreCp);
            cands.push_back(Cand{firstMove, cp, mpv});
          }
        }
        continue;
      }

      if (starts_with(line, "bestmove ")) {
        std::string best = word_after(line, "bestmove");
        if (cands.empty() || opts_.multipv <= 1) {
          return best.empty() ? "(none)" : best;
        }

        // Sort by MultiPV (1..N), break ties by scoreCp desc
        std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {
          if (a.multipv != b.multipv) return a.multipv < b.multipv;
          if (a.scoreCp != b.scoreCp) return a.scoreCp > b.scoreCp;
          return a.move < b.move;
        });
        // Unique by move
        cands.erase(std::unique(cands.begin(), cands.end(),
                                [](const Cand& a, const Cand& b) { return a.move == b.move; }),
                    cands.end());

        // Stable softmax over CP with temperature in centipawns
        const double T = std::max(1e-3, opts_.tempCp);
        double maxCp = -1e300;
        for (const auto& c : cands) maxCp = std::max(maxCp, c.scoreCp);
        std::vector<double> w;
        w.reserve(cands.size());
        double sum = 0.0;
        for (const auto& c : cands) {
          // exp((cp - max)/T) for numerical stability
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

  // ---- helpers ----
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
    is >> w;  // key
    if (w != key) return {};
    if (is >> w) return w;
    return {};
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
      line.push_back(static_cast<char>(ch));
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
    {
      std::ostringstream os;
      os << "setoption name Threads value " << std::max(1, opts_.threads);
      sendln(os.str());
    }
    if (opts_.skillLevel) {
      std::ostringstream os;
      os << "setoption name Skill Level value " << *opts_.skillLevel;
      sendln(os.str());
    }
    if (opts_.elo) {
      sendln("setoption name UCI_LimitStrength value true");
      std::ostringstream os;
      os << "setoption name UCI_Elo value " << *opts_.elo;
      sendln(os.str());
    }
    if (opts_.contempt) {
      std::ostringstream os;
      os << "setoption name Contempt value " << *opts_.contempt;
      sendln(os.str());
    }
    // Ensure engine is settled
    isready();
  }

  void spawn() {
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hOutWrite = NULL, hInReadLocal = NULL;
    // Child stdout/stderr -> our read end
    if (!CreatePipe(&hOutRead_, &hOutWrite, &sa, 0))
      throw std::runtime_error("CreatePipe stdout failed");
    if (!SetHandleInformation(hOutRead_, HANDLE_FLAG_INHERIT, 0))
      throw std::runtime_error("stdout SetHandleInformation failed");

    // Our write end -> child stdin
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
                        nullptr, &si, &pi_)) {
      throw std::runtime_error("CreateProcessW failed for Stockfish");
    }
    // Parent: close the child-side handles we don't need
    CloseHandle(hOutWrite);
    CloseHandle(hInReadLocal);

    // Wrap to FILE*
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
      // child
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
    // parent
    close(inpipe[0]);   // parent doesn't read stdin
    close(outpipe[1]);  // parent doesn't write stdout
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

// ------------------------ Data generation (self-play) ------------------------
std::vector<RawSample> generate_samples(const Options& opts) {
  if (!opts.generateData) return {};
  if (opts.stockfishPath.empty()) {
    throw std::runtime_error("Stockfish path required for data generation");
  }

  UciEngine engine(opts.stockfishPath, opts);  // persistent engine

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

    engine.ucinewgame();
    model::ChessGame game;
    game.setPosition(core::START_FEN);
    moveHistory.clear();

    std::vector<std::pair<std::string, core::Color>> gamePositions;
    std::array<int, 2> sideSampleCounters{0, 0};

    for (int ply = 0; ply < opts.maxPlies; ++ply) {
      game.checkGameResult();
      if (game.getResult() != core::GameResult::ONGOING) break;

      // Sample current position periodically
      if (ply >= opts.sampleSkip) {
        const auto sideToMove = game.getGameState().sideToMove;
        auto& counter = sideSampleCounters[static_cast<std::size_t>(sideToMove)];
        if (counter % stride == 0) {
          const auto fen = game.getFen();
          const auto key = fen_key(fen);
          if (seen.insert(key).second) {  // only record new positions globally
            gamePositions.emplace_back(fen, sideToMove);
          }
        }
        ++counter;
      }

      // Engine move (with MultiPV sampling & randomness)
      std::string mv = engine.pick_move_from_startpos(moveHistory);
      if (mv.empty() || mv == "(none)") {
        // no legal moves from engine pov -> terminal
        game.checkGameResult();
        break;
      }
      if (!game.doMoveUCI(mv)) {
        // Defensive: if move can't be played in our model, stop this game
        break;
      }
      moveHistory.push_back(mv);

      game.checkGameResult();
      if (game.getResult() != core::GameResult::ONGOING) break;
    }

    // Assign results to the positions from this game
    const core::GameResult finalRes = game.getResult();
    core::Color winner = flip_color(game.getGameState().sideToMove);

    for (const auto& [fen, pov] : gamePositions) {
      RawSample s;
      s.fen = fen;
      s.result = result_from_pov(finalRes, winner, pov);
      samples.push_back(std::move(s));
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

// ------------------------ Texel preparation & training ------------------------
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
      double eval = sample.baseEval;
      for (size_t j = 0; j < paramCount; ++j) {
        eval += (weights[j] - defaultsD[j]) * sample.gradients[j];
      }

      const double scaled = std::clamp(eval / opts.logisticScale, -500.0, 500.0);
      const double prob = 1.0 / (1.0 + std::exp(-scaled));
      const double target = sample.result;

      const double eps = 1e-12;
      loss += -(target * std::log(std::max(prob, eps)) +
                (1.0 - target) * std::log(std::max(1.0 - prob, eps)));

      const double diff = (prob - target) / opts.logisticScale;
      for (size_t j = 0; j < paramCount; ++j) {
        gradient[j] += diff * sample.gradients[j];
      }
    }

    for (size_t j = 0; j < paramCount; ++j) {
      gradient[j] *= invN;
    }

    if (opts.l2 > 0.0) {
      for (size_t j = 0; j < paramCount; ++j) {
        const double d = (weights[j] - defaultsD[j]);
        gradient[j] += opts.l2 * d;
        loss += 0.5 * opts.l2 * d * d;
      }
    }

    for (size_t j = 0; j < paramCount; ++j) {
      weights[j] -= opts.learningRate * gradient[j];
    }

    if ((iter + 1) % std::max(1, opts.iterations / 5) == 0 || iter == opts.iterations - 1) {
      std::cout << "\nIter " << (iter + 1) << "/" << opts.iterations << ": loss=" << (loss * invN)
                << "\n";
    }
    trainPM.update(static_cast<std::size_t>(iter + 1));
  }
  trainPM.finish();

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
      const double d = (weights[j] - defaults[j]);
      reg += 0.5 * opts.l2 * d * d;
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
                << (opts.contempt ? (" contempt=" + std::to_string(*opts.contempt)) : "") << "\n";
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
