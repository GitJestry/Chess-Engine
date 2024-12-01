#pragma once
#include <SFML/Graphics/Texture.hpp>
#include <map>
#include <string>

class TextureManager
{
public:
  // Load a texture from file and store it with a key
  bool loadTexture(const std::string &key, const std::string &filepath);
  bool loadTexture(const std::string &key, sf::Color color);

  // Get a reference to a texture by key
  sf::Texture &getTexture(const std::string &key);

private:
  std::map<std::string, sf::Texture> textures;
};
