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

/**
 * @brief GameManager: trennt Spiel-Lifecycle & Bot-Orchestrierung vom Controller.
 *
 * Responsibilities:
 * - Start/Stop/Restart eines Spiels
 * - Aufnahme von User-Move-Requests und Anwendung auf das Model
 * - Asynchrone Anforderung von Bot-Zügen und Anwendung auf das Model (main-thread)
 * - Promotion-Flow: feuert einen Callback, damit die View die Promotion auswähler
 * - Gibt Events/Callbacks für den Controller (Animate/Sound/etc.) aus
 */
class GameManager {
 public:
  using MoveCallback = std::function<void(const model::Move& mv, bool isPlayerMove, bool onClick)>;
  using PromotionCallback = std::function<void(core::Square promotionSquare)>;
  using EndCallback = std::function<void(core::GameResult)>;

  explicit GameManager(model::ChessGame& model);
  ~GameManager();

  // Lifecycle
  void startGame(core::Color playerColor, const std::string& fen = core::START_FEN,
                 bool vsBot = true);
  void stopGame();

  // Called each frame from main loop: Polls bot futures and applies moves when ready.
  void update(float dt);

  // Called by controller on user-move (drag/drop / click)
  // returns true wenn move angewendet wurde (bei Promotion -> false und PromotionEvent feuern)
  bool requestUserMove(core::Square from, core::Square to, bool onClick);

  // Called when user selected promotion piece after onPromotionRequested
  void completePendingPromotion(core::PieceType promotion);

  // Register callbacks
  void setOnMoveExecuted(MoveCallback cb) { onMoveExecuted_ = std::move(cb); }
  void setOnPromotionRequested(PromotionCallback cb) { onPromotionRequested_ = std::move(cb); }
  void setOnGameEnd(EndCallback cb) { onGameEnd_ = std::move(cb); }

  // Optional: set bot for specific color
  void setBotForColor(core::Color color, std::unique_ptr<IPlayer> bot);

 private:
  model::ChessGame& m_game;
  core::Color m_player_color = core::Color::White;

  // Players: nullptr bedeutet menschlicher Spieler
  std::unique_ptr<IPlayer> m_white_player;
  std::unique_ptr<IPlayer> m_black_player;

  // Bot future & cancel token
  std::future<model::Move> m_bot_future;
  IPlayer* m_pending_bot_player = nullptr;  // roher pointer auf den aktiven Player
  std::atomic<bool> m_cancel_bot{false};

  // pending promotion info
  bool m_waiting_promotion = false;
  core::Square m_promotion_from = core::NO_SQUARE;
  core::Square m_promotion_to = core::NO_SQUARE;

  std::mutex m_mutex;

  // callbacks
  MoveCallback onMoveExecuted_;
  PromotionCallback onPromotionRequested_;
  EndCallback onGameEnd_;

  // intern
  void applyMoveAndNotify(const model::Move& mv, bool onClick);
  void startBotIfNeeded();
};

}  // namespace lilia::controller
