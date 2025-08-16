#include "lilia/view/texture_table.hpp"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <stdexcept>

#include "lilia/view/render_constants.hpp"

namespace lilia {

TextureTable& TextureTable::getInstance() {
  static TextureTable instance;
  return instance;
}

TextureTable::TextureTable() = default;

TextureTable::~TextureTable() = default;

void TextureTable::load(const std::string& name, const sf::Color& color, sf::Vector2u size) {
  auto it = m_textures.find(name);
  if (it != m_textures.end()) return;

  sf::Texture texture;
  sf::Image image;
  image.create(size.x, size.y, color);
  texture.loadFromImage(image);

  m_textures[name] = std::move(texture);
}

const sf::Texture& TextureTable::get(const std::string& filename) {
  auto it = m_textures.find(filename);
  if (it != m_textures.end()) return it->second;

  // if filename hasnt been loaded yet
  sf::Texture texture;
  if (!texture.loadFromFile(filename)) {
    throw std::runtime_error("Error when loading texture: " + filename);
  }
  m_textures[filename] = std::move(texture);
  return m_textures[filename];
}

sf::Texture makeAttackDotTexture(unsigned int size) {
  sf::RenderTexture rt;
  rt.create(size, size);
  rt.clear(sf::Color::Transparent);

  float cx = size * 0.5f;
  float cy = size * 0.5f;

  sf::Color centerColor(120, 120, 120, 200);

  // draw multipled colored circles (inner = strong, outer = weak)
  const int layers = 8;
  for (int i = 0; i < layers; ++i) {
    float t = float(i) / float(layers - 1);  // 0..1
    float radius = (size * 0.2f) + t * (size * 0.35f);
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

sf::Texture makeSquareHoverTexture(unsigned int size) {
  sf::RenderTexture rt;
  rt.create(size, size);
  rt.clear(sf::Color::Transparent);

  float thickness = size / 6.f;
  sf::RectangleShape rect(sf::Vector2f(size - thickness, size - thickness));
  rect.setPosition(thickness / 2.f, thickness / 2.f);  // Innen verschieben
  rect.setFillColor(sf::Color::Transparent);
  rect.setOutlineColor(sf::Color(255, 200, 80));  // warmes Goldgelb
  rect.setOutlineThickness(thickness);

  rt.draw(rect);
  rt.display();

  return rt.getTexture();
}

void TextureTable::preLoad() {
  load(core::STR_TEXTURE_WHITE, sf::Color(240, 217, 181));              // light beige
  load(core::STR_TEXTURE_BLACK, sf::Color(181, 136, 99));               // dark brown
  load(core::STR_TEXTURE_SELECTHLIGHT, sf::Color(255, 255, 102, 100));  // light transparent yellow
  m_textures[core::STR_TEXTURE_ATTACKHLIGHT] =
      std::move(makeAttackDotTexture(core::ATTACK_DOT_PX_SIZE));
  m_textures[core::STR_TEXTURE_HOVERHLIGHT] =
      std::move(makeSquareHoverTexture(core::HOVER_PX_SIZE));
  load(core::STR_TEXTURE_TRANSPARENT, sf::Color::Transparent);
}

}  // namespace lilia
