#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "lilia/constants.hpp"
#include "lilia/engine/eval.hpp"
#include "lilia/engine/eval_shared.hpp"
#include "lilia/model/chess_game.hpp"
#include "lilia/model/core/model_types.hpp"

namespace fs = std::filesystem;
using namespace std::string_literals;

namespace lilia::tools::texel {

struct Options {
  bool generateData = false;
  bool tune = false;
  std::string stockfishPath;
  int games = 8;
  int depth = 12;
  int maxPlies = 160;
  int sampleSkip = 6;
  int sampleStride = 4;
  std::string dataFile = "texel_dataset.txt";
  int iterations = 200;
  double learningRate = 0.0005;
  double logisticScale = 256.0;
  std::optional<std::string> weightsOutput;
  std::optional<int> sampleLimit;
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

[[noreturn]] void usage_and_exit() {
  std::cerr << "Usage: texel_tuner [--generate-data] [--tune] [options]\n"
               "Options:\n"
               "  --stockfish <path>        Path to Stockfish binary (required for generation)\n"
               "  --games <N>               Number of self-play games for data generation (default 8)\n"
               "  --depth <D>               Search depth for Stockfish (default 12)\n"
               "  --max-plies <N>           Maximum plies per game (default 160)\n"
               "  --sample-skip <N>         Skip first N plies before sampling (default 6)\n"
               "  --sample-stride <N>       Sample every N plies thereafter (default 4)\n"
               "  --data <file>             Dataset path (default texel_dataset.txt)\n"
               "  --iterations <N>          Training iterations (default 200)\n"
               "  --learning-rate <value>   Gradient descent learning rate (default 5e-4)\n"
               "  --scale <value>           Logistic scale in centipawns (default 256)\n"
               "  --weights-output <file>   Write tuned weights to file\n"
               "  --sample-limit <N>        Limit number of samples used for tuning\n"
               "  --help                    Show this message\n";
  std::exit(1);
}

Options parse_args(int argc, char** argv) {
  Options opts;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto require_value = [&](const char* name) -> std::string {
      if (i + 1 >= argc) {
        std::cerr << "Missing value for " << name << "\n";
        usage_and_exit();
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
    } else if (arg == "--weights-output") {
      opts.weightsOutput = require_value("--weights-output");
    } else if (arg == "--sample-limit") {
      opts.sampleLimit = std::stoi(require_value("--sample-limit"));
    } else if (arg == "--help" || arg == "-h") {
      usage_and_exit();
    } else {
      std::cerr << "Unknown option: " << arg << "\n";
      usage_and_exit();
    }
  }
  if (!opts.generateData && !opts.tune) {
    std::cerr << "Nothing to do: specify --generate-data and/or --tune.\n";
    usage_and_exit();
  }
  return opts;
}

StockfishResult run_stockfish(const Options& opts, const std::vector<std::string>& moves) {
  if (opts.stockfishPath.empty()) {
    throw std::runtime_error("Stockfish path required for data generation");
  }
  std::ostringstream cmdStream;
  cmdStream << "uci\n";
  cmdStream << "isready\n";
  cmdStream << "position startpos";
  if (!moves.empty()) {
    cmdStream << " moves";
    for (const auto& m : moves) cmdStream << ' ' << m;
  }
  cmdStream << "\n";
  if (opts.depth > 0) {
    cmdStream << "go depth " << opts.depth << "\n";
  } else {
    cmdStream << "go movetime 1000\n";  // fallback 1s
  }
  cmdStream << "quit\n";

  const auto tmpDir = fs::temp_directory_path();
  const auto cmdFile = tmpDir / fs::path("texel_sf_cmd.txt");
  const auto outFile = tmpDir / fs::path("texel_sf_out.txt");
  {
    std::ofstream out(cmdFile, std::ios::trunc);
    out << cmdStream.str();
  }

  std::ostringstream execCmd;
  execCmd << '"' << opts.stockfishPath << '"'
          << " < \"" << cmdFile.string() << "\" > \"" << outFile.string() << "\"";
  const int rc = std::system(execCmd.str().c_str());
  if (rc != 0) {
    throw std::runtime_error("Failed to run Stockfish command");
  }

  StockfishResult result;
  {
    std::ifstream in(outFile);
    std::string line;
    while (std::getline(in, line)) {
      if (line.rfind("bestmove ", 0) == 0) {
        result.bestmove = line.substr(9);
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
  samples.reserve(static_cast<size_t>(opts.games) * 32u);

  for (int gameIdx = 0; gameIdx < opts.games; ++gameIdx) {
    model::ChessGame game;
    game.setPosition(core::START_FEN);
    moveHistory.clear();
    std::vector<std::pair<std::string, core::Color>> gamePositions;

    for (int ply = 0; ply < opts.maxPlies; ++ply) {
      if (ply >= opts.sampleSkip && ((ply - opts.sampleSkip) % std::max(1, opts.sampleStride) == 0)) {
        gamePositions.emplace_back(game.getFen(), game.getGameState().sideToMove);
      }

      StockfishResult res = run_stockfish(opts, moveHistory);
      if (res.bestmove.empty() || res.bestmove == "(none)") {
        break;
      }
      moveHistory.push_back(res.bestmove);
      game.doMoveUCI(res.bestmove);
      game.checkGameResult();
      if (game.getResult() != core::GameResult::ONGOING) {
        break;
      }
    }

    const core::GameResult finalRes = game.getResult();
    core::Color winner = flip_color(game.getGameState().sideToMove);
    if (finalRes != core::GameResult::CHECKMATE) {
      winner = core::Color::White;  // dummy for draws
    }

    for (const auto& [fen, pov] : gamePositions) {
      RawSample sample;
      sample.fen = fen;
      sample.result = result_from_pov(finalRes, winner, pov);
      samples.push_back(std::move(sample));
    }
  }
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
  std::cout << "Wrote " << samples.size() << " samples to " << path << "\n";
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
  prepared.baseEval = static_cast<double>(evaluator.evaluate(pos));

  constexpr int delta = 1;
  for (size_t i = 0; i < entries.size(); ++i) {
    int* ptr = entries[i].value;
    const int orig = defaults[i];

    *ptr = orig + delta;
    evaluator.clearCaches();
    const double plus = evaluator.evaluate(pos);

    *ptr = orig - delta;
    evaluator.clearCaches();
    const double minus = evaluator.evaluate(pos);

    *ptr = orig;
    prepared.gradients[i] = (plus - minus) / (2.0 * delta);
  }
  evaluator.clearCaches();
  return prepared;
}

std::vector<PreparedSample> prepare_samples(const std::vector<RawSample>& rawSamples,
                                            engine::Evaluator& evaluator,
                                            const std::vector<int>& defaults,
                                            const std::span<const engine::EvalParamEntry>& entries,
                                            const Options& opts) {
  std::vector<PreparedSample> prepared;
  prepared.reserve(rawSamples.size());

  int processed = 0;
  for (const auto& sample : rawSamples) {
    if (opts.sampleLimit && processed >= *opts.sampleLimit) break;
    auto ps = prepare_sample(sample, evaluator, defaults, entries);
    prepared.push_back(std::move(ps));
    ++processed;
    if (processed % 10 == 0) {
      std::cout << "Prepared " << processed << " samples...\n";
    }
  }
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
    const double invN = 1.0 / static_cast<double>(samples.size());
    for (size_t j = 0; j < paramCount; ++j) {
      weights[j] -= opts.learningRate * gradient[j] * invN;
    }
    if ((iter + 1) % std::max(1, opts.iterations / 10) == 0 || iter == opts.iterations - 1) {
      std::cout << "Iter " << (iter + 1) << "/" << opts.iterations << ": loss="
                << (loss * invN) << "\n";
    }
  }
  TrainingResult tr;
  tr.weights = std::move(weights);
  // recompute loss for reporting
  double loss = 0.0;
  for (const auto& sample : samples) {
    double eval = sample.baseEval;
    for (size_t j = 0; j < paramCount; ++j) {
      eval += (tr.weights[j] - defaultsD[j]) * sample.gradients[j];
    }
    const double scaled = std::clamp(eval / opts.logisticScale, -500.0, 500.0);
    const double prob = 1.0 / (1.0 + std::exp(-scaled));
    const double target = sample.result;
    const double eps = 1e-12;
    loss += -(target * std::log(std::max(prob, eps)) +
              (1.0 - target) * std::log(std::max(1.0 - prob, eps)));
  }
  tr.finalLoss = loss / static_cast<double>(samples.size());
  return tr;
}

void emit_weights(const TrainingResult& result, const std::vector<int>& defaults,
                  const std::span<const engine::EvalParamEntry>& entries, const Options& opts) {
  std::vector<int> tuned;
  tuned.reserve(result.weights.size());
  for (double w : result.weights) tuned.push_back(static_cast<int>(std::llround(w)));

  engine::set_eval_param_values(tuned);

  std::ostream* out = &std::cout;
  std::ofstream file;
  if (opts.weightsOutput) {
    file.open(*opts.weightsOutput, std::ios::trunc);
    if (!file) throw std::runtime_error("Unable to open weights output file");
    out = &file;
  }

  *out << "# Tuned evaluation parameters (Texel training loss: " << result.finalLoss << ")\n";
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
    Options opts = parse_args(argc, argv);

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
      auto defaults = lilia::engine::get_eval_param_values();
      auto entriesSpan = lilia::engine::eval_param_entries();
      auto prepared = prepare_samples(rawSamples, evaluator, defaults, entriesSpan, opts);
      std::cout << "Prepared " << prepared.size() << " samples for tuning\n";
      auto result = train(prepared, defaults, entriesSpan, opts);
      emit_weights(result, defaults, entriesSpan, opts);
    }
  } catch (const std::exception& ex) {
    std::cerr << "Error: " << ex.what() << "\n";
    return 1;
  }
  return 0;
}
