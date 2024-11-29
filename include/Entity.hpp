#pragma once
#include <SFML/System/Vector2.hpp>

// interface 
class Entity {

    virtual ~Entity() = default;
    virtual void SetPosition(const sf::Vector2f& position) = 0;
    virtual sf::Vector2f GetPosition() = 0;

};