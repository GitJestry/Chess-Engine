#include "lilia/controller/game_manager.hpp"

#include <chrono>

namespace lilia::controller {

GameManager::GameManager(model::ChessGame& model) : m_game(model) {}

GameManager::~GameManager() {
  stopGame();
}

void GameManager::startGame(core::Color playerColor, const std::string& fen, bool vsBot) {
  std::lock_guard lock(m_mutex);
  m_playerColor = playerColor;
  m_game.setPosition(fen);
  m_cancelBot.store(false);
  m_waitingPromotion = false;
  int thinkTime = 20000;  // ms
  int depth = 9;

  // default: human for player color, bot for opponent (if vsBot)
  if (vsBot) {
    if (playerColor == core::Color::White) {
      m_whitePlayer.reset();  // human
      m_blackPlayer = std::make_unique<BotPlayer>(thinkTime, depth);
    } else {
      m_blackPlayer.reset();
      m_whitePlayer = std::make_unique<BotPlayer>(thinkTime, depth);
    }

  } else {
    m_whitePlayer.reset();
    m_blackPlayer.reset();
  }

  // Start bot if opponent to move
  startBotIfNeeded();
}

void GameManager::stopGame() {
  std::lock_guard lock(m_mutex);
  m_cancelBot.store(true);
  // Let the future finish naturally or be ignored. We don't block in destructor.
}

void GameManager::update(float /*dt*/) {
  std::lock_guard lock(m_mutex);
  using namespace std::chrono_literals;

  if (m_botFuture.valid()) {
    if (m_botFuture.wait_for(0ms) == std::future_status::ready) {
      model::Move mv = m_botFuture.get();
      // reset future
      m_botFuture = std::future<model::Move>();
      // If move is valid -> apply
      if (!(mv.from == core::NO_SQUARE && mv.to == core::NO_SQUARE)) {
        applyMoveAndNotify(mv, /*onClick=*/false);
      }
      // maybe chain another bot move
      startBotIfNeeded();
    }
  }
}

bool GameManager::requestUserMove(core::Square from, core::Square to, bool onClick) {
  std::lock_guard lock(m_mutex);
  if (m_waitingPromotion) return false;  // waiting on previous promotion

  const auto& moves = m_game.generateLegalMoves();
  for (const auto& m : moves) {
    if (m.from == from && m.to == to) {
      if (m.promotion != core::PieceType::None) {
        // request UI promotion selection
        m_waitingPromotion = true;
        m_promotionFrom = from;
        m_promotionTo = to;
        if (onPromotionRequested_) onPromotionRequested_(to);
        return false;  // not yet applied
      }

      // apply move immediately
      applyMoveAndNotify(m, onClick);
      // start bot if needed after applying
      startBotIfNeeded();
      return true;
    }
  }
  return false;  // illegal move
}

void GameManager::completePendingPromotion(core::PieceType promotion) {
  std::lock_guard lock(m_mutex);
  if (!m_waitingPromotion) return;

  const auto& moves = m_game.generateLegalMoves();
  for (const auto& m : moves) {
    if (m.from == m_promotionFrom && m.to == m_promotionTo && m.promotion == promotion) {
      applyMoveAndNotify(m, /*onClick=*/true);
      m_waitingPromotion = false;
      startBotIfNeeded();
      return;
    }
  }

  // if we reach here, the promotion selection did not match available moves -> cancel
  m_waitingPromotion = false;
}

void GameManager::applyMoveAndNotify(const model::Move& mv, bool onClick) {
  // Apply move to model (must be main-thread). GameController will animate and play
  // sounds using the callbacks.
  m_game.doMove(mv.from, mv.to, mv.promotion);

  // Determine if that move was executed by the player.
  bool wasPlayerMove = (m_game.getGameState().sideToMove != m_playerColor);

  if (onMoveExecuted_) onMoveExecuted_(mv, wasPlayerMove, onClick);

  // Check for end state
  auto result = m_game.getResult();
  if (result != core::GameResult::ONGOING) {
    if (onGameEnd_) onGameEnd_(result);
    // cancel any running bot
    m_cancelBot.store(true);
  }
}

void GameManager::startBotIfNeeded() {
  core::Color stm = m_game.getGameState().sideToMove;
  IPlayer* p = nullptr;
  if (stm == core::Color::White)
    p = m_whitePlayer.get();
  else
    p = m_blackPlayer.get();

  if (p && !p->isHuman()) {
    // cancel any running bot
    m_cancelBot.store(true);
    // small window to allow previous future to see cancel and exit
    m_cancelBot.store(false);

    m_pendingBotPlayer = p;
    m_botFuture = p->requestMove(m_game, m_cancelBot);
  }
}

void GameManager::setBotForColor(core::Color color, std::unique_ptr<IPlayer> bot) {
  std::lock_guard lock(m_mutex);
  if (color == core::Color::White)
    m_whitePlayer = std::move(bot);
  else
    m_blackPlayer = std::move(bot);
}

}  // namespace lilia::controller
