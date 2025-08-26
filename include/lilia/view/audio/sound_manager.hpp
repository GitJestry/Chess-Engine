#pragma once

#include <SFML/Audio.hpp>
#include <memory>
#include <string>
#include <unordered_map>

namespace lilia::view::sound {

class SoundManager {
 public:
  SoundManager() = default;
  ~SoundManager() = default;

  
  void loadSounds();

  
  void playPlayerMove();
  void playEnemyMove();
  void playCapture();
  void playCheck();
  void playWarning();
  void playCastle();
  void playPromotion();
  void playGameBegins();
  void playGameEnds();

  
  void playBackgroundMusic(const std::string& filename, bool loop = true);
  void stopBackgroundMusic();
  void setMusicVolume(float volume);    
  void setEffectsVolume(float volume);  

 private:
  
  void loadEffect(const std::string& name, const std::string& filepath);

  std::unordered_map<std::string, sf::SoundBuffer> m_buffers;
  std::unordered_map<std::string, sf::Sound> m_sounds;

  sf::Music m_music;
  float m_effectsVolume = 100.f;
};

}  
