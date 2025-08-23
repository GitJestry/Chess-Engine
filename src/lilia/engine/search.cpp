// FILE: src/search.cpp
#include "lilia/engine/search.hpp"

#include <algorithm>
#include <limits>

#include "lilia/model/move_generator.hpp"

namespace lilia {

static const int INF = 30000;
static const int MATE = 29000;

static inline bool same_move(const model::Move& a, const model::Move& b) {
  return a.from == b.from && a.to == b.to && a.promotion == b.promotion;
}

// convert evaluator result (white - black) to negamax sign convention:
// return value from side-to-move viewpoint
static inline int signed_eval(Evaluator& eval, const model::Position& pos) {
  int v = eval.evaluate(pos);
  // if black to move, invert so value is from side-to-move perspective
  using core::Color;
  if (pos.state().sideToMove == Color::Black) return -v;
  return v;
}

Search::Search(model::TT4& tt_, Evaluator& eval_, const EngineConfig& cfg_)
    : tt(tt_), eval(eval_), cfg(cfg_) {
  killers.fill(model::Move{});
  for (auto& h : history) h.fill(0);
  stopFlag = nullptr;
}

int Search::quiescence(model::Position& pos, int alpha, int beta, int ply) {
  stats.nodes++;
  if (stopFlag && stopFlag->load()) return 0;

  int stand = signed_eval(eval, pos);
  if (stand >= beta) return beta;
  if (alpha < stand) alpha = stand;

  model::MoveGenerator mg;
  auto moves = mg.generatePseudoLegalMoves(pos.board(), pos.state());
  std::vector<model::Move> caps;
  caps.reserve(moves.size());
  for (auto& m : moves) {
    // include captures and promotions in q-search
    if (m.isCapture || m.promotion != core::PieceType::None) caps.push_back(m);
  }
  std::sort(caps.begin(), caps.end(), [&pos](const model::Move& a, const model::Move& b) {
    return mvv_lva_score(pos, a) > mvv_lva_score(pos, b);
  });

  for (auto& m : caps) {
    if (stopFlag && stopFlag->load()) return 0;
    if (!pos.doMove(m)) continue;  // illegal
    int score = -quiescence(pos, -beta, -alpha, ply + 1);
    pos.undoMove();
    if (score >= beta) return beta;
    if (score > alpha) alpha = score;
  }
  return alpha;
}

/*
  PVS (NegaScout) implementation:
  - First move searched with full window.
  - Subsequent moves searched with null-window (alpha+1); if they exceed alpha,
    re-search with full window.
*/
int Search::negamax(model::Position& pos, int depth, int alpha, int beta, int ply,
                    model::Move& refBest) {
  if (pos.checkInsufficientMaterial() || pos.checkMoveRule() || pos.checkRepitition()) return 0;

  stats.nodes++;
  if (stopFlag && stopFlag->load()) return 0;  // oder origAlpha
  if (depth <= 0) return quiescence(pos, alpha, beta, ply);

  const int origAlpha = alpha;
  const int origBeta = beta;

  // TT probe
  auto key = pos.hash();
  model::Move ttMove{};
  bool haveTT = false;
  if (auto* e = tt.probe(key)) {
    haveTT = true;
    ttMove = e->best;
    if (e->depth >= depth) {
      if (e->bound == model::Bound::Exact) return e->value;
      if (e->bound == model::Bound::Lower && e->value > alpha) alpha = e->value;
      if (e->bound == model::Bound::Upper && e->value < beta) beta = e->value;
    }
    if (alpha >= beta) return e->value;
  }

  // occasional stop-check in heavy nodes
  if ((stats.nodes & 1023) == 0 && stopFlag && stopFlag->load()) return 0;

  model::MoveGenerator mg;
  auto moves = mg.generatePseudoLegalMoves(pos.board(), pos.state());

  // build legal move list (use doMove/undoMove)
  std::vector<model::Move> legal;
  legal.reserve(moves.size());
  for (auto& m : moves) {
    if (!pos.doMove(m)) continue;
    pos.undoMove();
    legal.push_back(m);
  }

  if (legal.empty()) {
    // terminal
    auto stm = pos.state().sideToMove;
    auto kbb = pos.board().pieces(stm, core::PieceType::King);
    core::Square ksq = static_cast<core::Square>(model::bb::ctz64(kbb));
    if (pos.isSquareAttacked(ksq, ~stm)) return -MATE + ply;
    return 0;
  }

  // score & order moves: prioritize TT best, captures (MVV-LVA), promotions, killers, history
  std::vector<std::pair<int, model::Move>> scored;
  scored.reserve(legal.size());
  for (auto& m : legal) {
    int score = 0;
    if (haveTT && same_move(ttMove, m)) {
      score = 20000;  // highest priority
    } else if (m.isCapture) {
      score = 10000 + mvv_lva_score(pos, m);
    } else if (m.promotion != core::PieceType::None) {
      score = 9000;
    } else {
      // killer / history
      if (same_move(killers[0], m))
        score = 8000;
      else if (same_move(killers[1], m))
        score = 7000;
      else
        score = history[m.from][m.to];
    }
    scored.push_back({score, m});
  }
  std::sort(scored.begin(), scored.end(), [](auto& a, auto& b) { return a.first > b.first; });

  int best = -INF;
  model::Move bestLocal{};
  int moveCount = 0;

  for (auto& sm : scored) {
    if (stopFlag && stopFlag->load()) break;
    model::Move m = sm.second;
    if (!pos.doMove(m)) continue;  // defensive
    int value;

    // PVS:
    if (moveCount == 0) {
      // first move: full window
      value = -negamax(pos, depth - 1, -beta, -alpha, ply + 1, refBest);
    } else {
      // non-first: null-window search
      value = -negamax(pos, depth - 1, -alpha - 1, -alpha, ply + 1, refBest);
      // if it looks promising, research full window
      if (value > alpha && value < beta) {
        value = -negamax(pos, depth - 1, -beta, -alpha, ply + 1, refBest);
      }
    }

    pos.undoMove();

    if (value > best) {
      best = value;
      bestLocal = m;
    }
    if (value > alpha) alpha = value;

    if (alpha >= beta) {
      // beta cutoff -> update killers/history
      if (!m.isCapture && m.promotion == core::PieceType::None) {
        killers[1] = killers[0];
        killers[0] = m;
        history[m.from][m.to] += depth * depth;
      }

      break;
    }
    ++moveCount;
  }

  // store in TT with correct bound
  model::Bound bound;
  if (best <= origAlpha)
    bound = model::Bound::Upper;
  else if (best >= origBeta)
    bound = model::Bound::Lower;
  else
    bound = model::Bound::Exact;
  tt.store(key, best, static_cast<int16_t>(depth), bound, bestLocal);

  refBest = bestLocal;
  return best;
}

int Search::search_root(model::Position& pos, int depth, std::optional<model::Move>& bestOut,
                        std::atomic<bool>* stop) {
  this->stopFlag = stop;
  int lastScore = 0;
  model::Move bestMove{};
  int alpha = -INF, beta = INF;

  auto rootKey = pos.hash();
  // try iterative deepening with aspiration windows and TT root move
  for (int d = 1; d <= depth; ++d) {
    if (stopFlag && stopFlag->load()) break;

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

    // gather legal root moves
    model::MoveGenerator mg;
    auto moves = mg.generatePseudoLegalMoves(pos.board(), pos.state());
    std::vector<model::Move> legal;
    legal.reserve(moves.size());
    for (auto& m : moves) {
      if (!pos.doMove(m)) continue;
      pos.undoMove();
      legal.push_back(m);
    }

    if (legal.empty()) break;

    // order root moves: prefer TT.best if present
    if (auto* e = tt.probe(rootKey)) {
      if (e->key == rootKey) {
        // try to place e->best first if legal
        for (size_t i = 0; i < legal.size(); ++i) {
          if (same_move(legal[i], e->best)) {
            std::swap(legal[0], legal[i]);
            break;
          }
        }
      }
    }

    int bestScore = -INF;
    // search moves (first full, others PVS)
    bool first = true;
    for (auto& m : legal) {
      if (stopFlag && stopFlag->load()) break;
      if (!pos.doMove(m)) continue;
      model::Move refBest;
      int score;
      if (first) {
        score = -negamax(pos, d - 1, -beta, -alpha, 1, refBest);
        first = false;
      } else {
        score = -negamax(pos, d - 1, -alpha - 1, -alpha, 1, refBest);
        if (score > alpha && score < beta) {
          // research full window
          score = -negamax(pos, d - 1, -beta, -alpha, 1, refBest);
        }
      }
      pos.undoMove();
      if (score > bestScore) {
        bestScore = score;
        bestMove = m;
      }
      if (score > alpha) alpha = score;
      if (alpha >= beta) {
        // beta cutoff at root: nothing more
        break;
      }
    }

    if (stopFlag && stopFlag->load()) break;
    lastScore = bestScore;

    // aspiration fail handling: if result outside window, widen and retry current depth once
    if (cfg.useAspiration && (lastScore <= a0 || lastScore >= b0)) {
      // widen and re-search this depth once with full window
      alpha = -INF;
      beta = INF;
      int bestScore2 = -INF;
      for (auto& m : legal) {
        if (!pos.doMove(m)) continue;
        model::Move refBest;
        int score = -negamax(pos, d - 1, -beta, -alpha, 1, refBest);
        pos.undoMove();
        if (score > bestScore2) {
          bestScore2 = score;
          bestMove = m;
        }
      }
      lastScore = bestScore2;
    }
  }

  this->stopFlag = nullptr;

  bestOut = bestMove;
  return lastScore;
}

}  // namespace lilia
