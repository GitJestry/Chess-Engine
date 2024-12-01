#include <texture_manager.hpp>

bool TextureManager::loadTexture(const std::string &key, const std::string &filepath)
{
  // Check if the texture already exists to avoid duplication
  if (textures.find(key) != textures.end())
  {
    return false; // or true if re-adding is acceptable
  }
  sf::Texture texture;
  if (!texture.loadFromFile(filepath))
    return false;
  textures[key] = std::move(texture);
  return true;
}
bool TextureManager::loadTexture(const std::string &key, sf::Color color)
{
  if (textures.find(key) != textures.end())
  {
    return false;
  }
  sf::Texture texture;
  sf::Image image;
  image.create(1, 1, color);
  texture.loadFromImage(image);

  textures[key] = std::move(texture);
  return true;
}

sf::Texture &TextureManager::getTexture(const std::string &key)
{
  return textures.at(key);
}
