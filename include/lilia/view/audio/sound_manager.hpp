#pragma once

#include <SFML/Audio.hpp>
#include <memory>
#include <string>
#include <unordered_map>

namespace lilia {

class SoundManager {
 public:
  SoundManager() = default;
  ~SoundManager() = default;

  /// Load all sound effects (move, capture, check, checkmate, promotion, etc.)
  void loadSounds();

  /// Play specific effects
  void playPlayerMove();
  void playEnemyMove();
  void playCapture();
  void playCheck();
  void playGameBegins();
  void playGameEnds();

  /// Music control
  void playBackgroundMusic(const std::string& filename, bool loop = true);
  void stopBackgroundMusic();
  void setMusicVolume(float volume);    // 0.0f - 100.0f
  void setEffectsVolume(float volume);  // 0.0f - 100.0f

 private:
  /// Helper to load a single sound buffer
  void loadEffect(const std::string& name, const std::string& filepath);

  std::unordered_map<std::string, sf::SoundBuffer> m_buffers;
  std::unordered_map<std::string, sf::Sound> m_sounds;

  sf::Music m_music;
  float m_effectsVolume = 100.f;
};

}  // namespace lilia
