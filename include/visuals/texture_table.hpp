#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include <string>
#include <unordered_map>

class TextureTable {
 public:
  // Singleton instance for global access
  static TextureTable& getInstance();

  // Retrieve a texture by name
  sf::Texture& get(const std::string& name);

  // Function to preload common textures (e.g., white, black)
  void preloadTextures();

 private:
  // Load a texture with a specific color. Only for preload
  void load(const std::string& name, const sf::Color& color, sf::Vector2u size = {1, 1});
  // Private constructor and destructor for Singleton
  TextureTable();
  ~TextureTable();
  TextureTable(const TextureTable&) = delete;
  TextureTable& operator=(const TextureTable&) = delete;

  // Storage for textures
  std::unordered_map<std::string, sf::Texture> m_textures;
};
