#pragma once

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <mutex>

#include "../constants.hpp"
#include "../model/chess_game.hpp"
#include "bot_player.hpp"
#include "player.hpp"

namespace lilia::controller {

class GameManager {
 public:
  using MoveCallback = std::function<void(const model::Move& mv, bool isPlayerMove, bool onClick)>;
  using PromotionCallback = std::function<void(core::Square promotionSquare)>;
  using EndCallback = std::function<void(core::GameResult)>;

  explicit GameManager(model::ChessGame& model);
  ~GameManager();

  
  void startGame(core::Color playerColor, const std::string& fen = core::START_FEN,
                 bool vsBot = true, int thinkTimeMs = 1000, int depth = 5);
  void stopGame();

  
  void update(float dt);

  
  
  bool requestUserMove(core::Square from, core::Square to, bool onClick);

  
  void completePendingPromotion(core::PieceType promotion);

  
  void setOnMoveExecuted(MoveCallback cb) { onMoveExecuted_ = std::move(cb); }
  void setOnPromotionRequested(PromotionCallback cb) { onPromotionRequested_ = std::move(cb); }
  void setOnGameEnd(EndCallback cb) { onGameEnd_ = std::move(cb); }

  
  void setBotForColor(core::Color color, std::unique_ptr<IPlayer> bot);

 private:
  model::ChessGame& m_game;
  core::Color m_playerColor = core::Color::White;

  
  std::unique_ptr<IPlayer> m_whitePlayer;
  std::unique_ptr<IPlayer> m_blackPlayer;

  
  std::future<model::Move> m_botFuture;
  IPlayer* m_pendingBotPlayer = nullptr;  
  std::atomic<bool> m_cancelBot{false};

  
  bool m_waitingPromotion = false;
  core::Square m_promotionFrom = core::NO_SQUARE;
  core::Square m_promotionTo = core::NO_SQUARE;

  std::mutex m_mutex;

  
  MoveCallback onMoveExecuted_;
  PromotionCallback onPromotionRequested_;
  EndCallback onGameEnd_;

  
  void applyMoveAndNotify(const model::Move& mv, bool onClick);
  void startBotIfNeeded();
};

}  
