#pragma once

// forward declaration
namespace sf {
class Color;
}  // namespace sf
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>
#include <string>
#include <unordered_map>

namespace lilia::view {

/**
 * @brief Singleton design
 *
 */
class TextureTable {
 public:
  static TextureTable& getInstance();

  // Retrieve a texture by name
  [[nodiscard]] const sf::Texture& get(const std::string& name);

  // Function to preload common textures (e.g., white, black)
  void preLoad();

 private:
  // Load a texture with a specific color. Only for preload
  void load(const std::string& name, const sf::Color& color, sf::Vector2u size = {1, 1});

  TextureTable();
  ~TextureTable();
  TextureTable(const TextureTable&) = delete;
  TextureTable& operator=(const TextureTable&) = delete;

  static TextureTable instance;
  std::unordered_map<std::string, sf::Texture> m_textures;
};

}  // namespace lilia::view
