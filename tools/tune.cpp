#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#include "lilia/constants.hpp"
#include "lilia/model/chess_game.hpp"

using namespace lilia;

struct Parameters {
  std::unordered_map<std::string, double> values;

  static Parameters load(const std::filesystem::path &path =
                             "include/lilia/engine/eval_tune_shared.hpp") {
    Parameters p;
    std::ifstream in(path);
    if (!in) {
      std::cerr << "Failed to open " << path << "\n";
      return p;
    }
    std::string content((std::istreambuf_iterator<char>(in)), {});
    std::regex pattern(R"(inline\s+int\s+(\w+)\s*=\s*([-0-9]+))");
    std::smatch m;
    auto begin = content.cbegin();
    while (std::regex_search(begin, content.cend(), m, pattern)) {
      p.values[m[1]] = std::stod(m[2]);
      begin = m.suffix().first;
    }
    return p;
  }

  void save_as_constexpr(const std::filesystem::path &out_path) const {
    std::ofstream out(out_path);
    for (const auto &kv : values) {
      out << "constexpr int " << kv.first << " = "
          << static_cast<int>(std::round(kv.second)) << ";\n";
    }
  }
};

double cp_to_wdl(double cp) { return 1.0 / (1.0 + std::exp(-cp / 400.0)); }

double evaluate_board(const std::string &fen, const Parameters &params) {
  const auto &p = params.values;
  double piece_values[6];
  piece_values[0] = p.at("PAWN_VALUE");
  piece_values[1] = p.at("KNIGHT_VALUE");
  piece_values[2] = p.at("BISHOP_VALUE");
  piece_values[3] = p.at("ROOK_VALUE");
  piece_values[4] = p.at("QUEEN_VALUE");
  piece_values[5] = 0.0; // King

  int counts[2][6] = {};
  std::string placement = fen.substr(0, fen.find(' '));
  for (char ch : placement) {
    if (ch == '/')
      continue;
    if (std::isdigit(static_cast<unsigned char>(ch))) {
      continue; // skip empty squares
    }
    bool white = std::isupper(static_cast<unsigned char>(ch));
    char piece = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    int idx = 5;
    switch (piece) {
    case 'p':
      idx = 0;
      break;
    case 'n':
      idx = 1;
      break;
    case 'b':
      idx = 2;
      break;
    case 'r':
      idx = 3;
      break;
    case 'q':
      idx = 4;
      break;
    case 'k':
    default:
      idx = 5;
      break;
    }
    counts[white ? 0 : 1][idx]++;
  }

  double score = 0.0;
  for (int i = 0; i < 5; ++i)
    score += piece_values[i] * (counts[0][i] - counts[1][i]);

  bool white_turn = fen.find(" w ") != std::string::npos;
  score += white_turn ? p.at("TEMPO_MG") : -p.at("TEMPO_MG");
  if (counts[0][2] >= 2)
    score += p.at("BISHOP_PAIR");
  if (counts[1][2] >= 2)
    score -= p.at("BISHOP_PAIR");
  return score;
}

std::vector<std::string> generate_fens(int samples) {
  model::ChessGame game;
  std::vector<std::string> fens;
  std::mt19937 rng(std::random_device{}());
  while (static_cast<int>(fens.size()) < samples) {
    const auto &moves = game.generateLegalMoves();
    if (moves.empty()) {
      game.setPosition(core::START_FEN);
      continue;
    }
    std::uniform_int_distribution<size_t> dist(0, moves.size() - 1);
    const auto &m = moves[dist(rng)];
    game.doMove(m.from(), m.to(), m.promotion());
    game.checkGameResult();
    fens.push_back(game.getFen());
    if (game.getResult() != core::GameResult::ONGOING) {
      game.setPosition(core::START_FEN);
    }
  }
  return fens;
}

std::vector<double> label_fens(const std::vector<std::string> &fens,
                               const Parameters &params) {
  std::vector<double> labels;
  labels.reserve(fens.size());
  for (const auto &fen : fens) {
    double cp = evaluate_board(fen, params);
    labels.push_back(cp_to_wdl(cp));
  }
  return labels;
}

double texel_loss(const std::vector<double> &pred,
                  const std::vector<double> &target) {
  double loss = 0.0;
  const double eps = 1e-12;
  for (size_t i = 0; i < pred.size(); ++i) {
    loss += target[i] * std::log(pred[i] + eps) +
            (1.0 - target[i]) * std::log(1.0 - pred[i] + eps);
  }
  return -loss / pred.size();
}

Parameters spsa_optimize(const Parameters &start_params,
                         const std::vector<std::string> &fens,
                         const std::vector<double> &labels, int iterations = 100,
                         double a = 0.1, double c = 0.1) {
  std::vector<std::string> keys;
  std::vector<double> theta;
  keys.reserve(start_params.values.size());
  theta.reserve(start_params.values.size());
  for (const auto &kv : start_params.values) {
    keys.push_back(kv.first);
    theta.push_back(kv.second);
  }
  std::mt19937 rng(std::random_device{}());

  auto evaluate = [&](const std::vector<double> &theta_vec) {
    Parameters p;
    for (size_t i = 0; i < keys.size(); ++i)
      p.values[keys[i]] = theta_vec[i];
    std::vector<double> preds;
    preds.reserve(fens.size());
    for (const auto &fen : fens) {
      double cp = evaluate_board(fen, p);
      preds.push_back(cp_to_wdl(cp));
    }
    return texel_loss(preds, labels);
  };

  for (int k = 1; k <= iterations; ++k) {
    double alpha = a / std::pow(k, 0.602);
    double gamma = c / std::pow(k, 0.101);
    std::vector<double> delta(theta.size());
    for (auto &d : delta)
      d = std::uniform_int_distribution<int>(0, 1)(rng) ? 1.0 : -1.0;
    std::vector<double> theta_plus(theta.size()), theta_minus(theta.size());
    for (size_t i = 0; i < theta.size(); ++i) {
      theta_plus[i] = theta[i] + gamma * delta[i];
      theta_minus[i] = theta[i] - gamma * delta[i];
    }
    double loss_plus = evaluate(theta_plus);
    double loss_minus = evaluate(theta_minus);
    for (size_t i = 0; i < theta.size(); ++i) {
      double grad = (loss_plus - loss_minus) / (2.0 * gamma * delta[i]);
      theta[i] -= alpha * grad;
    }
  }
  Parameters tuned;
  for (size_t i = 0; i < keys.size(); ++i)
    tuned.values[keys[i]] = theta[i];
  return tuned;
}

int main(int argc, char **argv) {
  int samples = 100;
  int iterations = 50;
  std::string export_path;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--samples" && i + 1 < argc) {
      samples = std::stoi(argv[++i]);
    } else if (arg == "--iterations" && i + 1 < argc) {
      iterations = std::stoi(argv[++i]);
    } else if (arg == "--export" && i + 1 < argc) {
      export_path = argv[++i];
    }
  }

  Parameters params = Parameters::load();
  auto fens = generate_fens(samples);
  auto labels = label_fens(fens, params);
  auto tuned = spsa_optimize(params, fens, labels, iterations);

  if (!export_path.empty()) {
    tuned.save_as_constexpr(export_path);
    std::cout << "Exported tuned parameters to " << export_path << "\n";
  } else {
    for (const auto &kv : tuned.values) {
      std::cout << kv.first << " = " << std::fixed << std::setprecision(2)
                << kv.second << "\n";
    }
  }
  return 0;
}

