#pragma once

namespace sf {
class Color;
}
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>
#include <string>
#include <unordered_map>

namespace lilia::view {

class TextureTable {
 public:
  static TextureTable& getInstance();

  [[nodiscard]] const sf::Texture& get(const std::string& name);

  void preLoad();

 private:
  void load(const std::string& name, const sf::Color& color, sf::Vector2u size = {1, 1});

  TextureTable();
  ~TextureTable();
  TextureTable(const TextureTable&) = delete;
  TextureTable& operator=(const TextureTable&) = delete;

  static TextureTable instance;
  std::unordered_map<std::string, sf::Texture> m_textures;
};

}  // namespace lilia::view
