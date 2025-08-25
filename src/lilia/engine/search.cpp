#include "lilia/engine/search.hpp"

#include <algorithm>
#include <chrono>
#include <exception>  // für std::exception
#include <iostream>
#include <limits>

namespace lilia::engine {

static const int INF = 30000;
static const int MATE = 29000;

static inline bool same_move(const model::Move& a, const model::Move& b) {
  return a.from == b.from && a.to == b.to && a.promotion == b.promotion;
}

// convert evaluator result (white - black) to negamax sign convention:
// return value from side-to-move viewpoint
static inline int signed_eval(Evaluator& eval, model::Position& pos) {
  int v = eval.evaluate(pos);
  if (pos.state().sideToMove == core::Color::Black) return -v;
  return v;
}

Search::Search(model::TT4& tt_, Evaluator& eval_, const EngineConfig& cfg_)
    : tt(tt_), eval(eval_), cfg(cfg_) {
  killers.fill(model::Move{});
  for (auto& h : history) h.fill(0);
  stopFlag = nullptr;
  stats = SearchStats{};
}

int Search::quiescence(model::Position& pos, int alpha, int beta, int ply) {
  stats.nodes++;
  if (stopFlag && stopFlag->load()) {
    // Stop: safe evaluation, innerhalb von [-MATE, MATE]
    int safeEval = signed_eval(eval, pos);
    return std::clamp(safeEval, -MATE, MATE);
  }

  int stand = std::clamp(signed_eval(eval, pos), -MATE, MATE);

  if (stand >= beta) return beta;
  if (alpha < stand) alpha = stand;

  std::vector<model::Move> moves;
  try {
    moves = mg.generatePseudoLegalMoves(pos.board(), pos.state());
  } catch (...) {
    return stand;  // move generator failed
  }

  std::vector<model::Move> caps;
  caps.reserve(moves.size());
  for (auto& m : moves) {
    if (m.isCapture || m.promotion != core::PieceType::None) caps.push_back(m);
  }

  std::sort(caps.begin(), caps.end(), [&pos](const model::Move& a, const model::Move& b) {
    return mvv_lva_score(pos, a) > mvv_lva_score(pos, b);
  });

  int best = stand;
  for (auto& m : caps) {
    if (stopFlag && stopFlag->load()) {
      break;
    }

    if (!pos.doMove(m)) continue;

    int score = -quiescence(pos, -beta, -alpha, ply + 1);
    pos.undoMove();

    if (stopFlag && stopFlag->load()) {
      break;
    }

    score = std::clamp(score, -MATE, MATE);

    if (score >= beta) return beta;
    if (score > alpha) alpha = score;
    if (score > best) best = score;
  }

  return best;
}
int Search::negamax(model::Position& pos, int depth, int alpha, int beta, int ply,
                    model::Move& refBest) {
  stats.nodes++;
  if (stopFlag && stopFlag->load()) {
    return 0;
  }

  if (pos.checkInsufficientMaterial() || pos.checkMoveRule() || pos.checkRepitition()) return 0;

  // --- Quiescence ---
  if (depth <= 0) return quiescence(pos, alpha, beta, ply);

  const int origAlpha = alpha;
  const int origBeta = beta;
  int best = -MATE - 1;
  model::Move bestLocal{};

  // --- TT probe ---
  model::Move ttMove{};
  bool haveTT = false;
  try {
    if (auto* e = tt.probe(pos.hash())) {
      haveTT = true;
      ttMove = e->best;
      if (e->depth >= depth) {
        if (e->bound == model::Bound::Exact) return std::clamp(e->value, -MATE, MATE);
        if (e->bound == model::Bound::Lower) alpha = std::max(alpha, e->value);
        if (e->bound == model::Bound::Upper) beta = std::min(beta, e->value);
        if (alpha >= beta) return std::clamp(e->value, -MATE, MATE);
      }
    }
  } catch (...) {
    haveTT = false;
  }

  // --- Null Move Pruning ---
  if (depth >= 3 && !pos.inCheck()) {
    pos.doNullMove();
    int nullScore = -negamax(pos, depth - 1 - 2, -beta, -beta + 1, ply + 1, refBest);
    pos.undoNullMove();
    if (nullScore >= beta) return beta;
  }

  // --- Generate legal moves ---
  std::vector<model::Move> moves;
  try {
    moves = mg.generatePseudoLegalMoves(pos.board(), pos.state());
  } catch (...) {
    moves.clear();
  }

  std::vector<model::Move> legal;
  legal.reserve(moves.size());
  for (auto& m : moves) {
    if (!pos.doMove(m)) continue;
    pos.undoMove();
    legal.push_back(m);
  }

  if (legal.empty()) {
    if (pos.inCheck()) return -MATE + ply;
    return 0;
  }

  // --- Move ordering ---
  std::vector<std::pair<int, model::Move>> scored;
  scored.reserve(legal.size());
  for (auto& m : legal) {
    int score = 0;
    if (haveTT && same_move(ttMove, m))
      score = 20000;
    else if (m.isCapture)
      score = 10000 + mvv_lva_score(pos, m);
    else if (m.promotion != core::PieceType::None)
      score = 9000;
    else if (same_move(m, killers[0]) || same_move(m, killers[1]))
      score = 8000;  // killer bonus
    else
      score = history[m.from][m.to];
    scored.push_back({score, m});
  }
  std::sort(scored.begin(), scored.end(), [](auto& a, auto& b) { return a.first > b.first; });

  int moveCount = 0;
  for (auto& sm : scored) {
    if (stopFlag && stopFlag->load()) break;

    model::Move m = sm.second;
    if (!pos.doMove(m)) continue;

    int value;
    model::Move childBest{};

    // --- Check extension ---
    int newDepth = depth - 1;
    if (pos.inCheck()) {
      newDepth += 1;  // extend check
    }

    if (moveCount == 0) {
      value = -negamax(pos, newDepth, -beta, -alpha, ply + 1, childBest);
    } else {
      // --- Late Move Reductions ---
      int reduction = 0;
      if (depth >= 3 && moveCount >= 4 && !m.isCapture && m.promotion == core::PieceType::None) {
        reduction = 1;
      }
      value = -negamax(pos, newDepth - reduction, -alpha - 1, -alpha, ply + 1, childBest);
      if (value > alpha && value < beta) {
        value = -negamax(pos, newDepth, -beta, -alpha, ply + 1, childBest);
      }
    }

    pos.undoMove();
    value = std::clamp(value, -MATE, MATE);

    if (value > best) {
      best = value;
      bestLocal = m;
    }
    if (value > alpha) alpha = value;

    if (alpha >= beta) {
      // killer + history update
      if (!m.isCapture && m.promotion == core::PieceType::None) {
        killers[1] = killers[0];
        killers[0] = m;
        history[m.from][m.to] += depth * depth;
      }
      break;
    }
    ++moveCount;
  }

  // --- TT store ---
  if (!(stopFlag && stopFlag->load())) {
    model::Bound bound;
    if (best <= origAlpha)
      bound = model::Bound::Upper;
    else if (best >= origBeta)
      bound = model::Bound::Lower;
    else
      bound = model::Bound::Exact;
    try {
      tt.store(pos.hash(), best, static_cast<int16_t>(depth), bound, bestLocal);
    } catch (...) {
    }
  }

  refBest = bestLocal;
  return best;
}

std::vector<model::Move> Search::build_pv_from_tt(model::Position pos, int max_len) {
  std::vector<model::Move> pv;
  try {
    for (int i = 0; i < max_len; ++i) {
      model::TTEntry4 const* e = nullptr;
      try {
        e = tt.probe(pos.hash());
      } catch (...) {
        break;
      }
      if (!e) break;
      model::Move m = e->best;
      if (m.from < 0 || m.to < 0) break;
      bool moved = false;
      try {
        moved = pos.doMove(m);
      } catch (...) {
        break;
      }
      if (!moved) break;
      pv.push_back(m);
    }
  } catch (...) {
  }
  return pv;
}

int Search::search_root(model::Position& pos, int depth, std::atomic<bool>* stop) {
  this->stopFlag = stop;
  stats = SearchStats{};             // reset stats
  SearchStats lastCompletedStats{};  // snapshot of last finished depth

  using steady_clock = std::chrono::steady_clock;
  auto start = steady_clock::now();

  int lastScore = 0;
  model::Move bestMove{};
  int alpha = -INF, beta = INF;

  auto rootKey = pos.hash();

  try {
    for (int d = 1; d <= depth; ++d) {
      if (stopFlag && stopFlag->load()) {
        break;
      }

      // --- aspiration window setup ---
      int a0 = -INF, b0 = INF;
      if (cfg.useAspiration && d > 1) {
        a0 = lastScore - cfg.aspirationWindow;
        b0 = lastScore + cfg.aspirationWindow;
        alpha = a0;
        beta = b0;
      } else {
        alpha = -INF;
        beta = INF;
      }

      // --- generate legal moves ---
      std::vector<model::Move> moves;
      try {
        moves = mg.generatePseudoLegalMoves(pos.board(), pos.state());
      } catch (...) {
        moves.clear();
      }

      std::vector<model::Move> legal;
      legal.reserve(moves.size());
      for (auto& m : moves) {
        if (!pos.doMove(m)) continue;
        pos.undoMove();
        legal.push_back(m);
      }
      if (legal.empty()) break;

      // --- move ordering: TT first ---
      try {
        if (auto* e = tt.probe(rootKey)) {
          if (e->key == rootKey) {
            for (size_t i = 0; i < legal.size(); ++i) {
              if (same_move(legal[i], e->best)) {
                std::swap(legal[0], legal[i]);
                break;
              }
            }
          }
        }
      } catch (...) {
      }

      int bestScore = -MATE - 1;  // sichere Initialisierung
      std::vector<std::pair<int, model::Move>> rootCandidates;
      rootCandidates.reserve(legal.size());

      bool first = true;
      for (auto& m : legal) {
        if (stopFlag && stopFlag->load()) goto stop_search;  // clean abort

        if (!pos.doMove(m)) continue;

        model::Move refBest;
        int score;
        if (first) {
          score = -negamax(pos, d - 1, -beta, -alpha, 1, refBest);
          first = false;
        } else {
          score = -negamax(pos, d - 1, -alpha - 1, -alpha, 1, refBest);
          if (score > alpha && score < beta) {
            score = -negamax(pos, d - 1, -beta, -alpha, 1, refBest);
          }
        }

        pos.undoMove();

        score = std::clamp(score, -MATE, MATE);

        rootCandidates.push_back({score, m});

        if (score > bestScore) {
          bestScore = score;
          bestMove = m;
        }
        if (score > alpha) alpha = score;
        if (alpha >= beta) break;  // beta cutoff
      }

      lastScore = bestScore;

      // --- aspiration re-search if needed ---
      if (cfg.useAspiration && (lastScore <= a0 || lastScore >= b0)) {
        alpha = -INF;
        beta = INF;
        bestScore = -MATE - 1;
        rootCandidates.clear();

        for (auto& m : legal) {
          if (!pos.doMove(m)) continue;
          model::Move refBest;
          int score = -negamax(pos, d - 1, -beta, -alpha, 1, refBest);
          pos.undoMove();
          score = std::clamp(score, -MATE, MATE);
          rootCandidates.push_back({score, m});
          if (score > bestScore) {
            bestScore = score;
            bestMove = m;
          }
        }
        lastScore = bestScore;
      }

      // --- update stats ---
      auto now = steady_clock::now();
      long long elapsedMs =
          std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
      stats.elapsedMs = elapsedMs;
      stats.nps =
          (elapsedMs > 0) ? (double)stats.nodes / (elapsedMs / 1000.0) : (double)stats.nodes;
      stats.bestScore = lastScore;
      stats.bestMove = bestMove;

      // --- top-K moves ---
      std::sort(rootCandidates.begin(), rootCandidates.end(),
                [](const auto& a, const auto& b) { return a.first > b.first; });
      const size_t K = 5;
      stats.topMoves.clear();
      for (size_t i = 0; i < rootCandidates.size() && i < K; ++i) {
        stats.topMoves.push_back({rootCandidates[i].second, rootCandidates[i].first});
      }

      // --- PV line ---
      stats.bestPV.clear();
      if (stats.bestMove.has_value()) {
        model::Position tmp = pos;
        if (tmp.doMove(stats.bestMove.value())) {
          stats.bestPV.push_back(stats.bestMove.value());
          auto rest = build_pv_from_tt(tmp, 32);
          for (auto& mv : rest) stats.bestPV.push_back(mv);
        }
      }

      lastCompletedStats = stats;
    }  // end depth loop

  } catch (const std::exception& e) {
    std::cerr << "[Search] exception: " << e.what() << "\n";
  } catch (...) {
    std::cerr << "[Search] unknown exception\n";
  }

stop_search:
  if (stopFlag && stopFlag->load()) {
    auto now = steady_clock::now();
    long long elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
    std::cerr << "[Search] stopFlag raised at " << elapsedMs << " ms\n";

    // restore safe snapshot
    stats = lastCompletedStats;
    stats.elapsedMs = elapsedMs;
    stats.nps = (elapsedMs > 0) ? (double)stats.nodes / (elapsedMs / 1000.0) : (double)stats.nodes;
  }

  this->stopFlag = nullptr;
  return stats.bestScore;
}

// Return a copy of stats (thread-safe snapshot) instead of a reference
SearchStats Search::getStatsCopy() const {
  return stats;
}

}  // namespace lilia::engine
