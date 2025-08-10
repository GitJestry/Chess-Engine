#include "visuals/texture_table.hpp"

#include <constants.hpp>

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

sf::Texture makeAttackDotTexture(unsigned size) {
  sf::RenderTexture rt;
  rt.create(size, size);
  rt.clear(sf::Color::Transparent);

  float cx = size * 0.5f;
  float cy = size * 0.5f;

  // zentrale Farbe
  sf::Color centerColor(120, 120, 120, 200);

  // zeichne mehrere konzentrische Kreise (inner = stark, outer = schwach)
  const int layers = 8;
  for (int i = 0; i < layers; ++i) {
    float t = float(i) / float(layers - 1);             // 0..1
    float radius = (size * 0.2f) + t * (size * 0.35f);  // anpassen
    sf::Uint8 alpha = static_cast<sf::Uint8>(centerColor.a * (1.0f - t) * 0.9f);

    sf::CircleShape ring(radius);
    ring.setOrigin(radius, radius);
    ring.setPosition(cx, cy);
    ring.setFillColor(sf::Color(centerColor.r, centerColor.g, centerColor.b, alpha));
    rt.draw(ring, sf::BlendAlpha);
  }

  rt.display();
  return rt.getTexture();
}

// Preload commonly used textures without filename
void TextureTable::preloadTextures() {
  load("white", sf::Color(240, 217, 181));            // helles Beige, warm und klassisch
  load("black", sf::Color(181, 136, 99));             // dunkles Braun, harmonisch zu weiß
  load("sel_hlight", sf::Color(255, 255, 102, 180));  // leicht transparentes Gelb für Auswahl
  m_textures["att_hlight"] = makeAttackDotTexture(ATTACK_DOT_SIZE);
  load("transparent", sf::Color::Transparent);
}
