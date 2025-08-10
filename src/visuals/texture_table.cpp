#include "visuals/texture_table.hpp"

// Singleton instance getter
TextureTable& TextureTable::getInstance() {
  static TextureTable instance;
  return instance;
}

// Private constructor
TextureTable::TextureTable() = default;

// Destructor
TextureTable::~TextureTable() = default;

// Load a texture with a specified color and size
void TextureTable::load(const std::string& name, const sf::Color& color, sf::Vector2u size) {
  auto it = m_textures.find(name);
  if (it != m_textures.end()) return;

  sf::Texture texture;
  sf::Image image;
  image.create(size.x, size.y, color);  // Create a solid color image
  texture.loadFromImage(image);

  m_textures[name] = std::move(texture);
}

sf::Texture& TextureTable::get(const std::string& filename) {
  auto it = m_textures.find(filename);
  if (it != m_textures.end()) return it->second;

  // Laden, falls noch nicht vorhanden
  sf::Texture texture;
  if (!texture.loadFromFile(filename)) {
    throw std::runtime_error("Error when loading texture: " + filename);
  }
  m_textures[filename] = std::move(texture);
  return m_textures[filename];
}

// Preload commonly used textures without filename
void TextureTable::preloadTextures() {
  load("white", sf::Color::White);
  load("black", sf::Color::Green);
  load("transparent", sf::Color::Transparent);
}
