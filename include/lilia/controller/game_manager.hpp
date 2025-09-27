#pragma once

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <mutex>

#include "../chess_types.hpp"
#include "../constants.hpp"
#include "../model/move.hpp"

namespace lilia::model {
class ChessGame;
} // namespace lilia::model

namespace lilia::controller {
struct IPlayer;
}

namespace lilia::controller {

class GameManager {
public:
  using MoveCallback = std::function<void(const model::Move &mv,
                                          bool isPlayerMove, bool onClick)>;
  using PromotionCallback = std::function<void(core::Square promotionSquare)>;
  using EndCallback = std::function<void(core::GameResult)>;

  explicit GameManager(model::ChessGame &model);
  ~GameManager();

  void startGame(const std::string &fen = core::START_FEN,
                 bool whiteIsBot = false, bool blackIsBot = true,
                 int whiteThinkTimeMs = 1000, int whiteDepth = 5,
                 int blackThinkTimeMs = 1000, int blackDepth = 5,
                 bool startBotImmediately = true);

  bool applyMoveUci(const std::string &uciMove, bool suppressBotStart = false);
  void resumeBots();
  void stopGame();

  void update(float dt);

  bool requestUserMove(core::Square from, core::Square to, bool onClick,
                       core::PieceType promotion = core::PieceType::None);

  void completePendingPromotion(core::PieceType promotion);

  void setOnMoveExecuted(MoveCallback cb) { onMoveExecuted_ = std::move(cb); }
  void setOnPromotionRequested(PromotionCallback cb) {
    onPromotionRequested_ = std::move(cb);
  }
  void setOnGameEnd(EndCallback cb) { onGameEnd_ = std::move(cb); }

  void setBotForColor(core::Color color, std::unique_ptr<IPlayer> bot);

  [[nodiscard]] bool isHuman(core::Color color) const;
  [[nodiscard]] bool isHumanTurn() const;

private:
  model::ChessGame &m_game;
  // Players: nullptr bedeutet menschlicher Spieler
  std::unique_ptr<IPlayer> m_white_player;
  std::unique_ptr<IPlayer> m_black_player;

  // Bot future & cancel token
  std::future<model::Move> m_bot_future;
  IPlayer *m_pending_bot_player =
      nullptr; // roher pointer auf den aktiven Player
  std::atomic<bool> m_cancel_bot{false};

  // pending promotion info
  bool m_waiting_promotion = false;
  core::Square m_promotion_from = core::NO_SQUARE;
  core::Square m_promotion_to = core::NO_SQUARE;

  std::mutex m_mutex;

  MoveCallback onMoveExecuted_;
  PromotionCallback onPromotionRequested_;
  EndCallback onGameEnd_;

  void applyMoveAndNotify(const model::Move &mv, bool onClick);
  void startBotIfNeeded();
};

} // namespace lilia::controller
