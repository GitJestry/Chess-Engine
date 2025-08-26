#include "lilia/view/texture_table.hpp"

#include <SFML/Graphics.hpp>
#include <stdexcept>

#include "lilia/view/render_constants.hpp"

namespace lilia::view {

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

[[nodiscard]] const sf::Texture& TextureTable::get(const std::string& filename) {
  auto it = m_textures.find(filename);
  if (it != m_textures.end()) return it->second;

  
  sf::Texture texture;
  if (!texture.loadFromFile(filename)) {
    throw std::runtime_error("Error when loading texture: " + filename);
  }
  m_textures[filename] = std::move(texture);
  return m_textures[filename];
}

static const char* captureFrag = R"(
uniform vec2 resolution;
uniform vec4 color;    
uniform float centerR; 
uniform float halfThickness; 
uniform float softness; 
uniform float innerShade; 

void main()
{
    vec2 uv = gl_FragCoord.xy / resolution;
    vec2 c = vec2(0.5, 0.5);
    float d = distance(uv, c);

    
    
    float distFromRing = abs(d - centerR);

    
    float edge = smoothstep(halfThickness, halfThickness - softness, distFromRing);

    
    float ringMask = clamp(edge, 0.0, 1.0);

    
    float shade = mix(1.0, innerShade, smoothstep(0.0, halfThickness, (centerR - d)));

    float alpha = color.a * ringMask;
    vec3 rgb = color.rgb * shade;

    gl_FragColor = vec4(rgb, alpha);
}
)";

[[nodiscard]] sf::Texture makeCaptureCircleTexture(unsigned int size) {
  sf::RenderTexture rt;
  rt.create(size, size);
  rt.clear(sf::Color::Transparent);

  sf::Shader shader;
  bool shaderOk = shader.loadFromMemory(captureFrag, sf::Shader::Fragment);

  if (!shaderOk) {
    
    float radius = size * 0.45f;
    float thickness = size * 0.1f;
    sf::CircleShape ring(radius);
    ring.setOrigin(radius, radius);
    ring.setPosition(size * 0.5f, size * 0.5f);
    ring.setFillColor(sf::Color::Transparent);
    ring.setOutlineThickness(-thickness);
    ring.setOutlineColor(sf::Color(120, 120, 120, 65));  
    rt.draw(ring, sf::BlendAlpha);
    rt.display();
    return rt.getTexture();
  }

  
  
  float outerR_px = size * 0.45f;
  float thickness_px = size * 0.11f;
  float centerR = outerR_px / (float)size;                    
  float halfThickness = (thickness_px * 0.5f) / (float)size;  
  float softness = 3.0f / (float)size;  

  
  sf::Glsl::Vec4 col(120.f / 255.f, 120.f / 255.f, 120.f / 255.f, 65.f / 255.f);

  shader.setUniform("resolution", sf::Glsl::Vec2((float)size, (float)size));
  shader.setUniform("color", col);
  shader.setUniform("centerR", centerR);
  shader.setUniform("halfThickness", halfThickness);
  shader.setUniform("softness", softness);
  shader.setUniform("innerShade", 0.92f);

  sf::RectangleShape quad(sf::Vector2f((float)size, (float)size));
  quad.setPosition(0.f, 0.f);
  rt.draw(quad, &shader);

  rt.display();
  return rt.getTexture();
}

static const char* dotFrag = R"(
uniform vec2 resolution;
uniform vec4 color;   
uniform float radius; 
uniform float softness; 
uniform float coreBoost; 
uniform float highlight; 

void main()
{
    vec2 uv = gl_FragCoord.xy / resolution;
    vec2 c = vec2(0.5, 0.5);
    float d = distance(uv, c);

    
    
    float a = 1.0 - smoothstep(radius - softness, radius + softness, d);

    
    
    a = pow(a, 1.2);

    
    float core = 1.0 + coreBoost * (1.0 - smoothstep(0.0, radius * 0.9, d));

    
    float h = 1.0 - smoothstep(0.0, radius * 0.5, d);
    float highlightMask = pow(h, 3.0) * highlight;

    vec3 rgb = color.rgb * core + vec3(highlightMask);
    float alpha = color.a * a;

    gl_FragColor = vec4(rgb, alpha);
}
)";

[[nodiscard]] sf::Texture makeAttackDotTexture(unsigned int size) {
  sf::RenderTexture rt;
  rt.create(size, size);
  rt.clear(sf::Color::Transparent);

  sf::Shader shader;
  bool shaderOk = shader.loadFromMemory(dotFrag, sf::Shader::Fragment);

  if (!shaderOk) {
    
    float maxRadius = size * 0.35f;
    sf::CircleShape core(maxRadius);
    core.setOrigin(maxRadius, maxRadius);
    core.setPosition(size * 0.5f, size * 0.5f);
    
    core.setFillColor(sf::Color(120, 120, 120, 65));
    rt.draw(core, sf::BlendAlpha);
    rt.display();
    return rt.getTexture();
  }

  
  float maxRadius_px = size * 0.35f;
  float radius_frac = maxRadius_px / (float)size;  
  float softness = 3.0f / (float)size;             

  
  sf::Glsl::Vec4 col(120.f / 255.f, 120.f / 255.f, 120.f / 255.f, 65.f / 255.f);

  shader.setUniform("resolution", sf::Glsl::Vec2((float)size, (float)size));
  shader.setUniform("color", col);
  shader.setUniform("radius", radius_frac);
  shader.setUniform("softness", softness);
  shader.setUniform("coreBoost", 0.08f);
  shader.setUniform("highlight", 0.18f);

  sf::RectangleShape quad(sf::Vector2f((float)size, (float)size));
  quad.setPosition(0.f, 0.f);
  rt.draw(quad, &shader);

  rt.display();
  return rt.getTexture();
}

[[nodiscard]] sf::Texture makeSquareHoverTexture(unsigned int size) {
  sf::RenderTexture rt;
  rt.create(size, size);
  rt.clear(sf::Color::Transparent);

  float thickness = size / 6.f;
  sf::RectangleShape rect(sf::Vector2f(size - thickness, size - thickness));
  rect.setPosition(thickness / 2.f, thickness / 2.f);  
  rect.setFillColor(sf::Color::Transparent);
  rect.setOutlineColor(sf::Color(255, 200, 80));  
  rect.setOutlineThickness(thickness);

  rt.draw(rect);
  rt.display();

  return rt.getTexture();
}

static const char* roundedRectFrag = R"SHADER(
#version 120
uniform vec2 resolution;    
uniform float radius;       
uniform float softness;     
uniform vec4 color;         

void main()
{
    
    vec2 coord = gl_TexCoord[0].xy; 
    vec2 uv = coord / resolution;   

    
    vec2 pos = uv * resolution - 0.5 * resolution;
    vec2 halfSize = 0.5 * resolution;

    
    vec2 q = abs(pos) - (halfSize - vec2(radius));
    vec2 qpos = max(q, vec2(0.0));
    float dist = length(qpos) - radius; 

    
    
    float edge0 = -softness;
    float edge1 = softness;
    float a = 1.0 - smoothstep(edge0, edge1, dist);
    a = clamp(a, 0.0, 1.0);

    
    float innerShade = mix(1.0, 0.98, smoothstep(-radius*0.6, 0.0, dist));

    vec3 rgb = color.rgb * innerShade;
    float alpha = color.a * a;

    gl_FragColor = vec4(rgb, alpha);
}
)SHADER";

static const char* shadowFrag = R"SHADER(
#version 120
uniform vec2 resolution;   
uniform vec2 rectSize;     
uniform float radius;      
uniform float blur;        
uniform float offsetY;     
uniform vec4 shadowColor;  

void main()
{
    vec2 coord = gl_TexCoord[0].xy;
    vec2 uv = coord / resolution;
    vec2 pos = uv * resolution - 0.5 * resolution - vec2(0.0, -offsetY);

    
    vec2 halfSize = 0.5 * rectSize;
    vec2 q = abs(pos) - (halfSize - vec2(radius));
    vec2 qpos = max(q, vec2(0.0));
    float dist = length(qpos) - radius;

    
    float a = 1.0 - smoothstep(0.0, blur, dist);
    a = clamp(pow(a, 1.1), 0.0, 1.0);

    vec3 rgb = shadowColor.rgb;
    float alpha = shadowColor.a * a;

    gl_FragColor = vec4(rgb, alpha);
}
)SHADER";

static sf::VertexArray makeFullQuadVA(unsigned int width, unsigned int height) {
  sf::VertexArray va(sf::Quads, 4);
  va[0].position = sf::Vector2f(0.f, 0.f);
  va[1].position = sf::Vector2f((float)width, 0.f);
  va[2].position = sf::Vector2f((float)width, (float)height);
  va[3].position = sf::Vector2f(0.f, (float)height);

  
  va[0].texCoords = sf::Vector2f(0.f, 0.f);
  va[1].texCoords = sf::Vector2f((float)width, 0.f);
  va[2].texCoords = sf::Vector2f((float)width, (float)height);
  va[3].texCoords = sf::Vector2f(0.f, (float)height);
  return va;
}

[[nodiscard]] sf::Texture makeRoundedRectTexture(
    unsigned int width, unsigned int height, float radius_px = 6.f,
    sf::Color fillColor = sf::Color(255, 255, 255, 255), float softness_px = 1.0f) {
  sf::RenderTexture rt;
  rt.create(width, height);
  rt.clear(sf::Color::Transparent);

  sf::Shader shader;
  bool ok = shader.loadFromMemory(roundedRectFrag, sf::Shader::Fragment);

  if (ok) {
    shader.setUniform("resolution", sf::Glsl::Vec2((float)width, (float)height));
    shader.setUniform("radius", radius_px);
    shader.setUniform("softness", softness_px);
    shader.setUniform("color", sf::Glsl::Vec4(fillColor.r / 255.f, fillColor.g / 255.f,
                                              fillColor.b / 255.f, fillColor.a / 255.f));

    sf::VertexArray quad = makeFullQuadVA(width, height);
    rt.draw(quad, &shader);
    rt.display();

    sf::Texture tex = rt.getTexture();
    tex.setSmooth(false);  
    tex.setRepeated(false);
    return tex;
  }

  
  
  float cx = width * 0.5f, cy = height * 0.5f;
  sf::RectangleShape body(
      sf::Vector2f((float)width - 2.f * radius_px, (float)height - 2.f * radius_px));
  body.setOrigin(body.getSize() * 0.5f);
  body.setPosition(cx, cy);
  body.setFillColor(fillColor);
  rt.draw(body);

  
  sf::CircleShape corner(radius_px);
  corner.setFillColor(fillColor);
  corner.setOrigin(radius_px, radius_px);
  corner.setPosition(radius_px, radius_px);
  rt.draw(corner);
  corner.setPosition((float)width - radius_px, radius_px);
  rt.draw(corner);
  corner.setPosition(radius_px, (float)height - radius_px);
  rt.draw(corner);
  corner.setPosition((float)width - radius_px, (float)height - radius_px);
  rt.draw(corner);

  rt.display();
  sf::Texture tex = rt.getTexture();
  tex.setSmooth(false);
  tex.setRepeated(false);
  return tex;
}

[[nodiscard]] sf::Texture makeRoundedRectShadowTexture(
    unsigned int width, unsigned int height, float rectWidth_px, float rectHeight_px,
    float radius_px = 6.f, float blur_px = 12.f, sf::Color shadowColor = sf::Color(0, 0, 0, 140),
    float offsetY_px = 4.f) {
  
  sf::RenderTexture rt;
  rt.create(width, height);
  rt.clear(sf::Color::Transparent);

  sf::Shader shader;
  bool ok = shader.loadFromMemory(shadowFrag, sf::Shader::Fragment);

  if (ok) {
    shader.setUniform("resolution", sf::Glsl::Vec2((float)width, (float)height));
    shader.setUniform("rectSize", sf::Glsl::Vec2(rectWidth_px, rectHeight_px));
    shader.setUniform("radius", radius_px);
    shader.setUniform("blur", blur_px);
    shader.setUniform("offsetY", offsetY_px);
    shader.setUniform("shadowColor", sf::Glsl::Vec4(shadowColor.r / 255.f, shadowColor.g / 255.f,
                                                    shadowColor.b / 255.f, shadowColor.a / 255.f));

    sf::VertexArray quad = makeFullQuadVA(width, height);
    rt.draw(quad, &shader);
    rt.display();

    sf::Texture tex = rt.getTexture();
    tex.setSmooth(true);  
    tex.setRepeated(false);
    return tex;
  }

  
  int steps = 16;
  for (int i = steps - 1; i >= 0; --i) {
    float t = (float)i / (float)(steps - 1);
    float grow = blur_px * (1.0f - t);
    sf::Color c(shadowColor.r, shadowColor.g, shadowColor.b,
                static_cast<sf::Uint8>(shadowColor.a * t));
    float rw = rectWidth_px + grow * 2.f;
    float rh = rectHeight_px + grow * 2.f;
    sf::RectangleShape body(sf::Vector2f(rw - 2.f * radius_px, rh - 2.f * radius_px));
    body.setOrigin(body.getSize() * 0.5f);
    body.setPosition((float)width * 0.5f, (float)height * 0.5f + offsetY_px);
    body.setFillColor(c);
    rt.draw(body);
    sf::CircleShape corner(radius_px + grow);
    corner.setFillColor(c);
    corner.setOrigin(radius_px + grow, radius_px + grow);
    corner.setPosition((float)(width * 0.5f - rw * 0.5f) + radius_px + grow,
                       (float)(height * 0.5f - rh * 0.5f) + radius_px + grow + offsetY_px);
    rt.draw(corner);
    
  }

  rt.display();
  sf::Texture tex = rt.getTexture();
  tex.setSmooth(true);
  tex.setRepeated(false);
  return tex;
}

void TextureTable::preLoad() {
  load(constant::STR_TEXTURE_WHITE, sf::Color(240, 240, 210));
  load(constant::STR_TEXTURE_BLACK, sf::Color(120, 150, 86));
  load(constant::STR_TEXTURE_SELECTHLIGHT, sf::Color(240, 240, 50, 160));
  load(constant::STR_TEXTURE_WARNINGHLIGHT, sf::Color(255, 50, 50, 160));

  m_textures[constant::STR_TEXTURE_ATTACKHLIGHT] =
      std::move(makeAttackDotTexture(constant::ATTACK_DOT_PX_SIZE));
  m_textures[constant::STR_TEXTURE_HOVERHLIGHT] =
      std::move(makeSquareHoverTexture(constant::HOVER_PX_SIZE));
  m_textures[constant::STR_TEXTURE_CAPTUREHLIGHT] =
      std::move(makeCaptureCircleTexture(constant::CAPTURE_CIRCLE_PX_SIZE));
  m_textures[constant::STR_TEXTURE_PROMOTION] =
      std::move(makeRoundedRectTexture(constant::SQUARE_PX_SIZE, 4 * constant::SQUARE_PX_SIZE));
  m_textures[constant::STR_TEXTURE_PROMOTION_SHADOW] = std::move(
      makeRoundedRectShadowTexture(constant::SQUARE_PX_SIZE * 1.1f, 4 * constant::SQUARE_PX_SIZE,
                                   constant::SQUARE_PX_SIZE * 1.1f, 4 * constant::SQUARE_PX_SIZE));
  load(constant::STR_TEXTURE_TRANSPARENT, sf::Color::Transparent);
}

}  
