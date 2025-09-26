#include "lilia/controller/game_manager.hpp"

#include <chrono>
#include <cctype>

#include "lilia/controller/bot_player.hpp"
#include "lilia/controller/player.hpp"
#include "lilia/model/chess_game.hpp"
#include "lilia/uci/uci_helper.hpp"

namespace lilia::controller {

GameManager::GameManager(model::ChessGame &model) : m_game(model) {}

GameManager::~GameManager() {
  stopGame();
}

void GameManager::startGame(const std::string &fen, bool whiteIsBot, bool blackIsBot,
                            int whiteThinkTimeMs, int whiteDepth, int blackThinkTimeMs,
                            int blackDepth) {
  std::lock_guard lock(m_mutex);
  m_game.setPosition(fen);
  m_cancel_bot.store(false);
  m_waiting_promotion = false;
  m_suspendBots = false;

  if (whiteIsBot)
    m_white_player = std::make_unique<BotPlayer>(whiteThinkTimeMs, whiteDepth);
  else
    m_white_player.reset();

  if (blackIsBot)
    m_black_player = std::make_unique<BotPlayer>(blackThinkTimeMs, blackDepth);
  else
    m_black_player.reset();

  startBotIfNeeded();
}

void GameManager::stopGame() {
  std::lock_guard lock(m_mutex);
  m_cancel_bot.store(true);
}

namespace {
core::Square parseSquare(char file, char rank) {
  if (file < 'a' || file > 'h') return core::NO_SQUARE;
  if (rank < '1' || rank > '8') return core::NO_SQUARE;
  const int f = file - 'a';
  const int r = rank - '1';
  return static_cast<core::Square>(f + r * 8);
}

core::PieceType promotionFromChar(char c) {
  switch (static_cast<char>(std::tolower(static_cast<unsigned char>(c)))) {
    case 'q':
      return core::PieceType::Queen;
    case 'r':
      return core::PieceType::Rook;
    case 'b':
      return core::PieceType::Bishop;
    case 'n':
      return core::PieceType::Knight;
    default:
      return core::PieceType::None;
  }
}
}  // namespace

bool GameManager::applyImportedMove(const std::string &uciMove) {
  std::lock_guard lock(m_mutex);
  if (uciMove.size() < 4) return false;

  m_cancel_bot.store(true);
  m_suspendBots = true;

  core::Square from = parseSquare(uciMove[0], uciMove[1]);
  core::Square to = parseSquare(uciMove[2], uciMove[3]);
  if (from == core::NO_SQUARE || to == core::NO_SQUARE) return false;

  core::PieceType promotion = core::PieceType::None;
  if (uciMove.size() >= 5) promotion = promotionFromChar(uciMove[4]);

  const auto &moves = m_game.generateLegalMoves();
  for (const auto &m : moves) {
    if (m.from() != from || m.to() != to) continue;
    if (m.promotion() != promotion) continue;
    applyMoveAndNotify(m, false);
    return true;
  }
  return false;
}

void GameManager::resumeBotsAfterImport() {
  std::lock_guard lock(m_mutex);
  if (m_bot_future.valid()) {
    m_bot_future.wait();
    m_bot_future = std::future<model::Move>();
  }
  m_cancel_bot.store(false);
  m_suspendBots = false;
  startBotIfNeeded();
}

void GameManager::update([[maybe_unused]] float dt) {
  std::lock_guard lock(m_mutex);
  using namespace std::chrono_literals;
  if (m_bot_future.valid()) {
    if (m_bot_future.wait_for(1ms) == std::future_status::ready) {
      model::Move mv = m_bot_future.get();

      m_bot_future = std::future<model::Move>();
      if (!(mv.from() == core::NO_SQUARE && mv.to() == core::NO_SQUARE)) {
        applyMoveAndNotify(mv, false);
      }
      startBotIfNeeded();
    }
  }
}

bool GameManager::requestUserMove(core::Square from, core::Square to, bool onClick,
                                  core::PieceType promotion) {
  std::lock_guard lock(m_mutex);
  if (m_waiting_promotion) return false;  // waiting on previous promotion
  if (!isHuman(m_game.getGameState().sideToMove)) return false;

  const auto &moves = m_game.generateLegalMoves();
  for (const auto &m : moves) {
    if (m.from() == from && m.to() == to) {
      if (m.promotion() != core::PieceType::None) {
        // If caller already provided promotion piece, apply immediately.
        if (promotion != core::PieceType::None && promotion == m.promotion()) {
          applyMoveAndNotify(m, onClick);
          startBotIfNeeded();
          return true;
        }
        // Otherwise, request UI selection as before.
        m_waiting_promotion = true;
        m_promotion_from = from;
        m_promotion_to = to;
        if (onPromotionRequested_) onPromotionRequested_(to);
        return false;
      }

      applyMoveAndNotify(m, onClick);

      startBotIfNeeded();
      return true;
    }
  }
  return false;
}

void GameManager::completePendingPromotion(core::PieceType promotion) {
  std::lock_guard lock(m_mutex);
  if (!m_waiting_promotion) return;

  const auto &moves = m_game.generateLegalMoves();
  for (const auto &m : moves) {
    if (m.from() == m_promotion_from && m.to() == m_promotion_to && m.promotion() == promotion) {
      applyMoveAndNotify(m, true);
      m_waiting_promotion = false;
      startBotIfNeeded();
      return;
    }
  }

  // if we reach here, the promotion selection did not match available moves ->
  // cancel
  m_waiting_promotion = false;
}

void GameManager::applyMoveAndNotify(const model::Move &mv, bool onClick) {
  const core::Color mover = m_game.getGameState().sideToMove;
  m_game.doMove(mv.from(), mv.to(), mv.promotion());

  bool wasPlayerMove = isHuman(mover);

  if (onMoveExecuted_) onMoveExecuted_(mv, wasPlayerMove, onClick);

  auto result = m_game.getResult();
  if (result != core::GameResult::ONGOING) {
    if (onGameEnd_) onGameEnd_(result);
    // cancel any running bot
    m_cancel_bot.store(true);
  }
}

void GameManager::startBotIfNeeded() {
  core::Color stm = m_game.getGameState().sideToMove;
  IPlayer *p = nullptr;
  if (stm == core::Color::White)
    p = m_white_player.get();
  else
    p = m_black_player.get();

  if (m_suspendBots) return;
  if (m_bot_future.valid()) return;

  if (p && !p->isHuman()) {
    // cancel any running bot
    m_cancel_bot.store(true);
    // small window to allow previous future to see cancel and exit
    m_cancel_bot.store(false);

    m_pending_bot_player = p;
    m_bot_future = p->requestMove(m_game, m_cancel_bot);
  }
}

void GameManager::setBotForColor(core::Color color, std::unique_ptr<IPlayer> bot) {
  std::lock_guard lock(m_mutex);
  if (color == core::Color::White)
    m_white_player = std::move(bot);
  else
    m_black_player = std::move(bot);
}

bool GameManager::isHuman(core::Color color) const {
  const IPlayer *p = (color == core::Color::White) ? m_white_player.get() : m_black_player.get();
  return !p || p->isHuman();
}

bool GameManager::isHumanTurn() const {
  return isHuman(m_game.getGameState().sideToMove);
}

}  // namespace lilia::controller
