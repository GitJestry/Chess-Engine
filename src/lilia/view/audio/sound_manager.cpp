#include "lilia/view/audio/sound_manager.hpp"

#include "lilia/view/render_constants.hpp"

namespace lilia::view::sound {

void SoundManager::loadSounds() {
  loadEffect(constant::SFX_CAPTURE_NAME, constant::ASSET_SFX_FILE_PATH);
  loadEffect(constant::SFX_CASTLE_NAME, constant::ASSET_SFX_FILE_PATH);
  loadEffect(constant::SFX_CHECK_NAME, constant::ASSET_SFX_FILE_PATH);
  loadEffect(constant::SFX_ENEMY_MOVE_NAME, constant::ASSET_SFX_FILE_PATH);
  loadEffect(constant::SFX_GAME_BEGINS_NAME, constant::ASSET_SFX_FILE_PATH);
  loadEffect(constant::SFX_GAME_ENDS_NAME, constant::ASSET_SFX_FILE_PATH);
  loadEffect(constant::SFX_PLAYER_MOVE_NAME, constant::ASSET_SFX_FILE_PATH);
}

/// Play specific effects
void SoundManager::playPlayerMove() {
  m_sounds[constant::SFX_PLAYER_MOVE_NAME].play();
}
void SoundManager::playEnemyMove() {
  m_sounds[constant::SFX_ENEMY_MOVE_NAME].play();
}
void SoundManager::playCapture() {
  m_sounds[constant::SFX_CAPTURE_NAME].play();
}
void SoundManager::playCheck() {
  m_sounds[constant::SFX_CHECK_NAME].play();
}
void SoundManager::playGameBegins() {
  m_sounds[constant::SFX_GAME_BEGINS_NAME].play();
}
void SoundManager::playGameEnds() {
  m_sounds[constant::SFX_GAME_ENDS_NAME].play();
}
void SoundManager::playCastle() {
  m_sounds[constant::SFX_CASTLE_NAME].play();
}

/// Music control
void SoundManager::playBackgroundMusic(const std::string& filename, bool loop) {
  if (!m_music.openFromFile(filename)) {
    throw std::runtime_error("Failed to open music file: " + filename);
  }
  m_music.setLoop(loop);
  m_music.play();
}

void SoundManager::stopBackgroundMusic() {
  m_music.stop();
}

void SoundManager::setMusicVolume(float volume) {
  m_music.setVolume(volume);
}
void SoundManager::setEffectsVolume(float volume) {}

void SoundManager::loadEffect(const std::string& name, const std::string& filepath) {
  sf::SoundBuffer buffer;
  if (!buffer.loadFromFile(filepath + "/" + name + ".wav")) {
    throw std::runtime_error("Failed to load sound effect: " + filepath + "/" + name + ".wav");
  }

  // Move buffer into map
  auto [it, inserted] = m_buffers.emplace(name, std::move(buffer));

  // Bind sound to buffer
  sf::Sound sound;
  sound.setBuffer(it->second);
  sound.setVolume(m_effectsVolume);
  m_sounds[name] = std::move(sound);
}

}  // namespace lilia::view::sound
