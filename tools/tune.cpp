#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <cstdint>

#include <cstdio>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdexcept>

#include "lilia/constants.hpp"
#include "lilia/model/chess_game.hpp"
#include "lilia/engine/eval.hpp"
#include "lilia/engine/eval_tune_shared.hpp"

using namespace lilia;

// -----------------------------------------------------------------------------
// Parameter wiring
// -----------------------------------------------------------------------------
namespace {
struct ScalarParam {
  const char* name;
  int* ptr;
};

struct ArrayParam {
  const char* name;
  int* ptr;
  std::size_t size;
};

// Generated from include/lilia/engine/eval_tune_shared.hpp
static ScalarParam scalarParams[] = {
  {"MAX_PHASE", &lilia::engine::MAX_PHASE},
  {"BLEND_SCALE", &lilia::engine::BLEND_SCALE},
  {"PAWN_VALUE", &lilia::engine::PAWN_VALUE},
  {"KNIGHT_VALUE", &lilia::engine::KNIGHT_VALUE},
  {"BISHOP_VALUE", &lilia::engine::BISHOP_VALUE},
  {"ROOK_VALUE", &lilia::engine::ROOK_VALUE},
  {"QUEEN_VALUE", &lilia::engine::QUEEN_VALUE},
  {"TEMPO_MG", &lilia::engine::TEMPO_MG},
  {"TEMPO_EG", &lilia::engine::TEMPO_EG},
  {"SPACE_EG_DEN", &lilia::engine::SPACE_EG_DEN},
  {"ISO_P", &lilia::engine::ISO_P},
  {"DOUBLED_P", &lilia::engine::DOUBLED_P},
  {"BACKWARD_P", &lilia::engine::BACKWARD_P},
  {"PHALANX", &lilia::engine::PHALANX},
  {"CANDIDATE_P", &lilia::engine::CANDIDATE_P},
  {"CONNECTED_PASSERS", &lilia::engine::CONNECTED_PASSERS},
  {"PASS_BLOCK", &lilia::engine::PASS_BLOCK},
  {"PASS_SUPP", &lilia::engine::PASS_SUPP},
  {"PASS_FREE", &lilia::engine::PASS_FREE},
  {"PASS_KBOOST", &lilia::engine::PASS_KBOOST},
  {"PASS_KBLOCK", &lilia::engine::PASS_KBLOCK},
  {"PASS_PIECE_SUPP", &lilia::engine::PASS_PIECE_SUPP},
  {"PASS_KPROX", &lilia::engine::PASS_KPROX},
  {"KS_W_N", &lilia::engine::KS_W_N},
  {"KS_W_B", &lilia::engine::KS_W_B},
  {"KS_W_R", &lilia::engine::KS_W_R},
  {"KS_W_Q", &lilia::engine::KS_W_Q},
  {"KS_RING_BONUS", &lilia::engine::KS_RING_BONUS},
  {"KS_MISS_SHIELD", &lilia::engine::KS_MISS_SHIELD},
  {"KS_OPEN_FILE", &lilia::engine::KS_OPEN_FILE},
  {"KS_RQ_LOS", &lilia::engine::KS_RQ_LOS},
  {"KS_CLAMP", &lilia::engine::KS_CLAMP},
  {"KING_RING_RADIUS", &lilia::engine::KING_RING_RADIUS},
  {"KING_SHIELD_DEPTH", &lilia::engine::KING_SHIELD_DEPTH},
  {"KS_POWER_COUNT_CLAMP", &lilia::engine::KS_POWER_COUNT_CLAMP},
  {"KS_MIX_MG_Q_ON", &lilia::engine::KS_MIX_MG_Q_ON},
  {"KS_MIX_MG_Q_OFF", &lilia::engine::KS_MIX_MG_Q_OFF},
  {"KS_MIX_EG_HEAVY_THRESHOLD", &lilia::engine::KS_MIX_EG_HEAVY_THRESHOLD},
  {"KS_MIX_EG_IF_HEAVY", &lilia::engine::KS_MIX_EG_IF_HEAVY},
  {"KS_MIX_EG_IF_LIGHT", &lilia::engine::KS_MIX_EG_IF_LIGHT},
  {"SHELTER_EG_DEN", &lilia::engine::SHELTER_EG_DEN},
  {"BISHOP_PAIR", &lilia::engine::BISHOP_PAIR},
  {"BAD_BISHOP_PER_PAWN", &lilia::engine::BAD_BISHOP_PER_PAWN},
  {"BAD_BISHOP_SAME_COLOR_THRESHOLD", &lilia::engine::BAD_BISHOP_SAME_COLOR_THRESHOLD},
  {"BAD_BISHOP_OPEN_NUM", &lilia::engine::BAD_BISHOP_OPEN_NUM},
  {"BAD_BISHOP_OPEN_DEN", &lilia::engine::BAD_BISHOP_OPEN_DEN},
  {"OUTPOST_KN", &lilia::engine::OUTPOST_KN},
  {"OUTPOST_DEEP_RANK_WHITE", &lilia::engine::OUTPOST_DEEP_RANK_WHITE},
  {"OUTPOST_DEEP_RANK_BLACK", &lilia::engine::OUTPOST_DEEP_RANK_BLACK},
  {"OUTPOST_DEEP_EXTRA", &lilia::engine::OUTPOST_DEEP_EXTRA},
  {"CENTER_CTRL", &lilia::engine::CENTER_CTRL},
  {"OUTPOST_CENTER_SQ_BONUS", &lilia::engine::OUTPOST_CENTER_SQ_BONUS},
  {"KNIGHT_RIM", &lilia::engine::KNIGHT_RIM},
  {"ROOK_OPEN", &lilia::engine::ROOK_OPEN},
  {"ROOK_SEMI", &lilia::engine::ROOK_SEMI},
  {"ROOK_ON_7TH", &lilia::engine::ROOK_ON_7TH},
  {"CONNECTED_ROOKS", &lilia::engine::CONNECTED_ROOKS},
  {"ROOK_BEHIND_PASSER", &lilia::engine::ROOK_BEHIND_PASSER},
  {"ROOK_SEMI_ON_KING_FILE", &lilia::engine::ROOK_SEMI_ON_KING_FILE},
  {"ROOK_OPEN_ON_KING_FILE", &lilia::engine::ROOK_OPEN_ON_KING_FILE},
  {"ROOK_PASSER_PROGRESS_START_RANK", &lilia::engine::ROOK_PASSER_PROGRESS_START_RANK},
  {"ROOK_CUT_MIN_SEPARATION", &lilia::engine::ROOK_CUT_MIN_SEPARATION},
  {"ROOK_CUT_BONUS", &lilia::engine::ROOK_CUT_BONUS},
  {"BLOCK_PASSER_STOP_KNIGHT", &lilia::engine::BLOCK_PASSER_STOP_KNIGHT},
  {"BLOCK_PASSER_STOP_BISHOP", &lilia::engine::BLOCK_PASSER_STOP_BISHOP},
  {"THR_PAWN_MINOR", &lilia::engine::THR_PAWN_MINOR},
  {"THR_PAWN_ROOK", &lilia::engine::THR_PAWN_ROOK},
  {"THR_PAWN_QUEEN", &lilia::engine::THR_PAWN_QUEEN},
  {"HANG_MINOR", &lilia::engine::HANG_MINOR},
  {"HANG_ROOK", &lilia::engine::HANG_ROOK},
  {"HANG_QUEEN", &lilia::engine::HANG_QUEEN},
  {"MINOR_ON_QUEEN", &lilia::engine::MINOR_ON_QUEEN},
  {"THREATS_MG_NUM", &lilia::engine::THREATS_MG_NUM},
  {"THREATS_MG_DEN", &lilia::engine::THREATS_MG_DEN},
  {"THREATS_EG_DEN", &lilia::engine::THREATS_EG_DEN},
  {"SPACE_BASE", &lilia::engine::SPACE_BASE},
  {"SPACE_SCALE_BASE", &lilia::engine::SPACE_SCALE_BASE},
  {"SPACE_MINOR_SATURATION", &lilia::engine::SPACE_MINOR_SATURATION},
  {"DEVELOPMENT_PIECE_ON_HOME_PENALTY", &lilia::engine::DEVELOPMENT_PIECE_ON_HOME_PENALTY},
  {"DEV_MG_PHASE_CUTOFF", &lilia::engine::DEV_MG_PHASE_CUTOFF},
  {"DEV_MG_PHASE_DEN", &lilia::engine::DEV_MG_PHASE_DEN},
  {"DEV_EG_DEN", &lilia::engine::DEV_EG_DEN},
  {"PIECE_BLOCKING_PENALTY", &lilia::engine::PIECE_BLOCKING_PENALTY},
  {"TROPISM_BASE_KN", &lilia::engine::TROPISM_BASE_KN},
  {"TROPISM_BASE_BI", &lilia::engine::TROPISM_BASE_BI},
  {"TROPISM_BASE_RO", &lilia::engine::TROPISM_BASE_RO},
  {"TROPISM_BASE_QU", &lilia::engine::TROPISM_BASE_QU},
  {"TROPISM_DIST_FACTOR", &lilia::engine::TROPISM_DIST_FACTOR},
  {"TROPISM_EG_DEN", &lilia::engine::TROPISM_EG_DEN},
  {"KING_ACTIVITY_EG_MULT", &lilia::engine::KING_ACTIVITY_EG_MULT},
  {"PASS_RACE_MAX_MINORMAJOR", &lilia::engine::PASS_RACE_MAX_MINORMAJOR},
  {"PASS_RACE_STM_ADJ", &lilia::engine::PASS_RACE_STM_ADJ},
  {"PASS_RACE_MULT", &lilia::engine::PASS_RACE_MULT},
  {"FULL_SCALE", &lilia::engine::FULL_SCALE},
  {"SCALE_DRAW", &lilia::engine::SCALE_DRAW},
  {"SCALE_VERY_DRAWISH", &lilia::engine::SCALE_VERY_DRAWISH},
  {"SCALE_REDUCED", &lilia::engine::SCALE_REDUCED},
  {"SCALE_MEDIUM", &lilia::engine::SCALE_MEDIUM},
  {"KN_CORNER_PAWN_SCALE", &lilia::engine::KN_CORNER_PAWN_SCALE},
  {"OPP_BISHOPS_SCALE", &lilia::engine::OPP_BISHOPS_SCALE},
  {"CASTLE_BONUS", &lilia::engine::CASTLE_BONUS},
  {"CENTER_BACK_PENALTY_Q_ON", &lilia::engine::CENTER_BACK_PENALTY_Q_ON},
  {"CENTER_BACK_PENALTY_Q_OFF", &lilia::engine::CENTER_BACK_PENALTY_Q_OFF},
  {"CENTER_BACK_OPEN_FILE_OPEN", &lilia::engine::CENTER_BACK_OPEN_FILE_OPEN},
  {"CENTER_BACK_OPEN_FILE_SEMI", &lilia::engine::CENTER_BACK_OPEN_FILE_SEMI},
  {"CENTER_BACK_OPEN_FILE_WEIGHT", &lilia::engine::CENTER_BACK_OPEN_FILE_WEIGHT},
  {"ROOK_KFILE_PRESS_FREE", &lilia::engine::ROOK_KFILE_PRESS_FREE},
  {"ROOK_KFILE_PRESS_PAWNATT", &lilia::engine::ROOK_KFILE_PRESS_PAWNATT},
  {"ROOK_LIFT_SAFE", &lilia::engine::ROOK_LIFT_SAFE},
  {"KS_ESCAPE_EMPTY", &lilia::engine::KS_ESCAPE_EMPTY},
  {"KS_ESCAPE_FACTOR", &lilia::engine::KS_ESCAPE_FACTOR},
  {"EARLY_QUEEN_MALUS", &lilia::engine::EARLY_QUEEN_MALUS},
  {"UNCASTLED_PENALTY_Q_ON", &lilia::engine::UNCASTLED_PENALTY_Q_ON},
  {"MOBILITY_CLAMP", &lilia::engine::MOBILITY_CLAMP},
};

static ArrayParam arrayParams[] = {
  {"PASSED_MG", lilia::engine::PASSED_MG, 8},
  {"PASSED_EG", lilia::engine::PASSED_EG, 8},
  {"SHELTER", lilia::engine::SHELTER, 8},
  {"STORM", lilia::engine::STORM, 8},
  {"KN_MOB_MG", lilia::engine::KN_MOB_MG, 9},
  {"KN_MOB_EG", lilia::engine::KN_MOB_EG, 9},
  {"BI_MOB_MG", lilia::engine::BI_MOB_MG, 14},
  {"BI_MOB_EG", lilia::engine::BI_MOB_EG, 14},
  {"RO_MOB_MG", lilia::engine::RO_MOB_MG, 15},
  {"RO_MOB_EG", lilia::engine::RO_MOB_EG, 15},
  {"QU_MOB_MG", lilia::engine::QU_MOB_MG, 28},
  {"QU_MOB_EG", lilia::engine::QU_MOB_EG, 28},
};

std::vector<int*> paramPtrs;
std::vector<std::string> paramNames;
std::vector<double> paramMins;
std::vector<double> paramMaxs;

struct ParamRange {
  const char* name;
  int min;
  int max;
};

static ParamRange paramRanges[] = {
    {"PAWN_VALUE", 50, 200},
    {"ISO_P", -50, 0},
};

void init_param_refs() {
  if (!paramPtrs.empty()) return;
  for (const auto& s : scalarParams) {
    paramNames.emplace_back(s.name);
    paramPtrs.push_back(s.ptr);
    int mn = -1000, mx = 1000;
    for (const auto& r : paramRanges) {
      if (std::strcmp(r.name, s.name) == 0) {
        mn = r.min;
        mx = r.max;
        break;
      }
    }
    paramMins.push_back(mn);
    paramMaxs.push_back(mx);
  }
  for (const auto& a : arrayParams) {
    for (std::size_t i = 0; i < a.size; ++i) {
      paramNames.emplace_back(std::string(a.name) + "[" + std::to_string(i) + "]");
      paramPtrs.push_back(a.ptr + i);
      paramMins.push_back(-1000);
      paramMaxs.push_back(1000);
    }
  }
}

std::vector<double> get_params() {
  init_param_refs();
  std::vector<double> vals(paramPtrs.size());
  for (std::size_t i = 0; i < paramPtrs.size(); ++i) vals[i] = *paramPtrs[i];
  return vals;
}

void set_params(const std::vector<double>& vals) {
  init_param_refs();
  for (std::size_t i = 0; i < paramPtrs.size() && i < vals.size(); ++i)
    *paramPtrs[i] = static_cast<int>(std::round(
        std::clamp(vals[i], paramMins[i], paramMaxs[i])));
}

void export_params(const std::filesystem::path& out_path) {
  std::ofstream out(out_path);
  out << "#pragma once\n\n";
  out << "namespace lilia::engine {\n";
  for (const auto& s : scalarParams)
    out << "inline constexpr int " << s.name << " = " << *s.ptr << ";\n";
  for (const auto& a : arrayParams) {
    out << "inline constexpr int " << a.name << "[" << a.size << "] = {";
    for (std::size_t i = 0; i < a.size; ++i) {
      if (i) out << ", ";
      out << a.ptr[i];
    }
    out << "};\n";
  }
  out << "} // namespace lilia::engine\n";
}

void print_params() {
  for (const auto& s : scalarParams)
    std::cout << s.name << " = " << *s.ptr << "\n";
  for (const auto& a : arrayParams)
    for (std::size_t i = 0; i < a.size; ++i)
      std::cout << a.name << "[" << i << "] = " << a.ptr[i] << "\n";
}
} // namespace

// -----------------------------------------------------------------------------
// UCI engine helper for labels
// -----------------------------------------------------------------------------
double cp_to_wdl(double cp) {
  constexpr double k = 0.004;  // Texel scaling
  return 1.0 / (1.0 + std::exp(-k * cp));
}

class UciEngine {
  pid_t pid_{};
  FILE* in_{};  // engine stdout
  FILE* out_{}; // engine stdin

 public:
  explicit UciEngine(const std::string& path, int threads = 1, int hash = 16) {
    int inpipe[2];
    int outpipe[2];
    if (pipe(inpipe) != 0 || pipe(outpipe) != 0) {
      throw std::runtime_error("pipe failed");
    }

    pid_ = fork();
    if (pid_ < 0) {
      throw std::runtime_error("fork failed");
    }

    if (pid_ == 0) {
      if (dup2(outpipe[0], STDIN_FILENO) == -1 ||
          dup2(inpipe[1], STDOUT_FILENO) == -1) {
        _exit(1);
      }
      close(outpipe[0]);
      close(outpipe[1]);
      close(inpipe[0]);
      close(inpipe[1]);
      execl(path.c_str(), path.c_str(), (char*)nullptr);
      _exit(1);
    }
    close(outpipe[0]);
    close(inpipe[1]);
    out_ = fdopen(outpipe[1], "w");
    in_ = fdopen(inpipe[0], "r");
    if (!out_ || !in_) {
      throw std::runtime_error("fdopen failed");
    }
    setvbuf(out_, nullptr, _IOLBF, 0);

    send("uci");
    if (!waitFor("uciok")) {
      throw std::runtime_error("uciok not received");
    }
    send("setoption name Threads value " + std::to_string(threads));
    send("setoption name Hash value " + std::to_string(hash));
    send("isready");
    if (!waitFor("readyok")) {
      throw std::runtime_error("readyok not received");
    }
  }

  ~UciEngine() {
    if (out_) {
      fprintf(out_, "quit\n");
      fflush(out_);
    }
    if (pid_ > 0) {
      waitpid(pid_, nullptr, 0);
    }
    if (in_) fclose(in_);
    if (out_) fclose(out_);
  }

  void send(const std::string& cmd) { fprintf(out_, "%s\n", cmd.c_str()); }

  std::string readLine() {
    char buf[4096];
    if (fgets(buf, sizeof(buf), in_)) return std::string(buf);
    throw std::runtime_error("engine stream closed");
  }

  bool waitFor(const std::string& token) {
    std::string line;
    while (true) {
      line = readLine();
      if (line.find(token) != std::string::npos) return true;
    }
  }

  double evaluateCp(const std::string& fen, int depth) {
    send("position fen " + fen);
    send("go depth " + std::to_string(depth));
    std::string line;
    double score = 0.0;
    while (true) {
      line = readLine();
      if (line.rfind("info", 0) == 0) {
        auto pos = line.find("score ");
        if (pos != std::string::npos) {
          double val;
          if (sscanf(line.c_str() + pos, "score cp %lf", &val) == 1) {
            score = val;
          } else if (sscanf(line.c_str() + pos, "score mate %lf", &val) == 1) {
            score = val > 0 ? 100000.0 : -100000.0;
          }
        }
      } else if (line.rfind("bestmove", 0) == 0) {
        break;
      }
    }
    return score;
  }

  std::string bestMove(const std::string& fen, int depth) {
    send("position fen " + fen);
    send("go depth " + std::to_string(depth));
    std::string line;
    char mv[16]{};
    while (true) {
      line = readLine();
      if (line.rfind("bestmove", 0) == 0) {
        if (sscanf(line.c_str(), "bestmove %15s", mv) == 1) {
          return std::string(mv);
        }
        break;
      }
    }
    return {};
  }
};

// -----------------------------------------------------------------------------
// Data generation & labeling
// -----------------------------------------------------------------------------
std::vector<std::string> generate_fens(int samples, const std::string& enginePath,
                                       int depth, int threads, int hash) {
  model::ChessGame game;
  std::vector<std::string> fens;
  fens.reserve(samples);
  UciEngine engine(enginePath, threads, hash);
  engine.send("ucinewgame");
  engine.send("isready");
  engine.waitFor("readyok");
  while (static_cast<int>(fens.size()) < samples) {
    std::string move = engine.bestMove(game.getFen(), depth);
    if (move.empty()) {
      game.setPosition(core::START_FEN);
      engine.send("ucinewgame");
      engine.send("isready");
      engine.waitFor("readyok");
      continue;
    }
    game.doMoveUCI(move);
    game.checkGameResult();
    fens.push_back(game.getFen());
    if (game.getResult() != core::GameResult::ONGOING) {
      game.setPosition(core::START_FEN);
      engine.send("ucinewgame");
      engine.send("isready");
      engine.waitFor("readyok");
    }
  }
  return fens;
}

std::vector<double> label_fens(const std::vector<std::string>& fens,
                               const std::string& enginePath,
                               int depth, int threads, int hash) {
  std::vector<double> labels;
  labels.reserve(fens.size());
  UciEngine engine(enginePath, threads, hash);
  engine.send("ucinewgame");
  engine.send("isready");
  engine.waitFor("readyok");
  for (const auto& fen : fens) {
    double cp = engine.evaluateCp(fen, depth);
    labels.push_back(cp_to_wdl(cp));
  }
  return labels;
}

double texel_loss(const std::vector<double>& pred,
                  const std::vector<double>& target) {
  double loss = 0.0;
  const double eps = 1e-12;
  for (std::size_t i = 0; i < pred.size(); ++i) {
    loss += target[i] * std::log(pred[i] + eps) +
            (1.0 - target[i]) * std::log(1.0 - pred[i] + eps);
  }
  return -loss / pred.size();
}

// -----------------------------------------------------------------------------
// SPSA optimizer
// -----------------------------------------------------------------------------
std::vector<double> spsa_optimize(const std::vector<std::string>& fens,
                                  const std::vector<double>& labels,
                                  std::vector<double> theta,
                                  int iterations, int batch, uint64_t seed,
                                  double a = 0.1, double c = 0.1) {
  std::size_t n = theta.size();
  int b = std::min(batch, static_cast<int>(fens.size()));
  std::mt19937 rng(seed);

  std::vector<double> baseC(n);
  for (std::size_t i = 0; i < n; ++i)
    baseC[i] = 0.05 * (paramMaxs[i] - paramMins[i]);

  auto evaluate = [&](const std::vector<double>& t,
                      const std::vector<std::size_t>& idxs) {
    set_params(t);
#ifdef LILIA_TUNE
    engine::rebuild_tune_masks();
#endif
    engine::Evaluator eval;
    std::vector<double> preds(b), lbl(b);
    model::ChessGame game;
    for (int i = 0; i < b; ++i) {
      std::size_t idx = idxs[i];
      game.setPosition(fens[idx]);
      int cp = eval.evaluate(game.getPositionRefForBot());
      preds[i] = cp_to_wdl(cp);
      lbl[i] = labels[idx];
    }
    return texel_loss(preds, lbl);
  };

  std::vector<double> delta(n);
  std::vector<double> theta_plus(n), theta_minus(n);
  for (int k = 1; k <= iterations; ++k) {
    double alpha = a / std::pow(k, 0.602);
    double gamma_k = c / std::pow(k, 0.101);
    for (std::size_t i = 0; i < n; ++i)
      delta[i] = std::uniform_int_distribution<int>(0, 1)(rng) ? 1.0 : -1.0;
    std::vector<std::size_t> idx(b);
    std::uniform_int_distribution<std::size_t> dist(0, fens.size() - 1);
    for (int i = 0; i < b; ++i) idx[i] = dist(rng);
    for (std::size_t i = 0; i < n; ++i) {
      double gamma_i = gamma_k * baseC[i];
      theta_plus[i] = std::clamp(theta[i] + gamma_i * delta[i], paramMins[i],
                                 paramMaxs[i]);
      theta_minus[i] = std::clamp(theta[i] - gamma_i * delta[i], paramMins[i],
                                  paramMaxs[i]);
    }
    double loss_plus = evaluate(theta_plus, idx);
    double loss_minus = evaluate(theta_minus, idx);
    for (std::size_t i = 0; i < n; ++i) {
      double gamma_i = gamma_k * baseC[i];
      double grad = (loss_plus - loss_minus) / (2.0 * gamma_i * delta[i]);
      theta[i] = std::clamp(theta[i] - alpha * grad, paramMins[i], paramMaxs[i]);
    }
  }
  set_params(theta);
  return theta;
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
  int samples = 200'000;
  int iterations = 200;
  int batch = 8'192;
  int gen_depth = 6;
  int label_depth = 14;
  int sf_threads = 1;
  int sf_hash = 16;
  uint64_t seed = 1;
  std::string export_path;
  std::string engine_path = "stockfish";
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--samples" && i + 1 < argc) {
      samples = std::stoi(argv[++i]);
    } else if (arg == "--iterations" && i + 1 < argc) {
      iterations = std::stoi(argv[++i]);
    } else if (arg == "--batch" && i + 1 < argc) {
      batch = std::stoi(argv[++i]);
    } else if (arg == "--gen-depth" && i + 1 < argc) {
      gen_depth = std::stoi(argv[++i]);
    } else if (arg == "--label-depth" && i + 1 < argc) {
      label_depth = std::stoi(argv[++i]);
    } else if (arg == "--export" && i + 1 < argc) {
      export_path = argv[++i];
    } else if (arg == "--engine" && i + 1 < argc) {
      engine_path = argv[++i];
    } else if (arg == "--sf-threads" && i + 1 < argc) {
      sf_threads = std::stoi(argv[++i]);
    } else if (arg == "--sf-hash" && i + 1 < argc) {
      sf_hash = std::stoi(argv[++i]);
    } else if (arg == "--seed" && i + 1 < argc) {
      seed = static_cast<uint64_t>(std::stoull(argv[++i]));
    }
  }

  if (samples < 1 || samples > 10'000'000) {
    std::cerr << "samples must be between 1 and 10,000,000\n";
    return 1;
  }

  auto init = get_params();
  auto fens = generate_fens(samples, engine_path, gen_depth, sf_threads, sf_hash);
  auto labels = label_fens(fens, engine_path, label_depth, sf_threads, sf_hash);
  auto tuned = spsa_optimize(fens, labels, init, iterations, batch, seed);

  if (!export_path.empty()) {
    export_params(export_path);
    std::cout << "Exported tuned parameters to " << export_path << "\n";
  } else {
    print_params();
  }
  return 0;
}

