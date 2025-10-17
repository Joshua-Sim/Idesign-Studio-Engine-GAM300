///////////////////////////////////////////////////////////////////////////////
///
///@file ComponentList.h
/// 
/// @brief This file contains all the components that will be registered and used for
///       the engine.
///  
///@Authors: Dylan Lau 
///       Teo Joel
///         Joshua Sim Yue Chen
///Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "MainMath.h"
#include "glm.hpp"
#include "Types.h"
#include "lua.hpp"
#include "TransformProperties.h"

// ----- ENUMS & Structs for components ----

enum class RenderType {
    Sprite,    // Regular renderable object
    Text,     // UI button
    Particle        // Text renderable
};

enum ObjectType { // If object and not player, do collision checks.
    Player,
    CollidableObject,
    Enemy
};

// Enum for predefined layers, used for rendering and logic optimization
enum Layer {
    Background = 0,
    Character = 1,
    Foreground = 2,
    UI = 3,
    Debug = 4
};

enum EnemyType {
    Boss,
    Minion
};

// ----- Define Components ----

// Transform component for position, rotation, and scale // Graphics System
//struct TransformComponent 
//{
//    glm::vec3 position{ 0.0f, 0.0f, 0.0f};
//    glm::vec3 rotation{ 0.0f, 0.0f, 0.0f };
//    glm::vec3 scale{ 1.0f, 1.0f, 1.0f };
//};

// Transform component for position, rotation, and scale // Graphics System rendering
struct RenderComponent
{
    std::string textureID;        // Texture or sprite name to render
    glm::vec3 color{ 1.0f, 1.0f, 1.0f }; // Tint color (default to white)
    float alpha = 1.0f;           // Opacity
    RenderType renderType = RenderType::Sprite; // Type of renderable entity, can leave for now to toggle between 2d or 3d
};

// Layering System for rendering and logic optimization
struct LayerComponent {
    int sortID;   // Sort ID for ordering entities within the same layer
    Layer layerID = Layer::Background;  // Layer for rendering order, default to 'First'
};

// Text component for text rendering entities
struct TextComponent {
    std::string text;
    int fontSize = 16;
    glm::vec3 color{ 1.0f, 1.0f, 1.0f };
    std::string fontName;
    glm::vec2 offset{ 0.0f, 0.0f };  // Position offset relative to TransformComponent's position
};

// Player component for updating text and holding player utilized information
struct PlayerComponent {
    std::string CurrentText;  // Updates the text component of entity to reflect this.
};



struct AABB {
    glm::vec2 max{0.0,0.0};
    glm::vec2 min{0.0,0.0};
};

struct Circle
{
    UE_Vector::Vector2D  m_center;
    float	m_radius;
};

struct AABB3 {
    UE_Vector::Vector3D min;
    UE_Vector::Vector3D max;
};

struct OBB3 {
    UE_Vector::Vector3D center;
    UE_Vector::Vector3D orientation;
    UE_Vector::Vector3D half;
};


struct Render {
    std::string textureAddress{};
};

struct MovementComponent
{
    glm::vec2 velocity{0.0f, 0.0f};
};

struct CollisionComponent
{
    ObjectType type{};
    AABB aabb{};
    bool collided = false;
    float radius = 0;

};

struct EnemyComponent
{
    EnemyType type{};
    float health = 0.0;
    std::string UpdateFunctionName;

    // Function pointer to store the behavior function
    std::function<void(Framework::Entity, float)> behavior;
};

struct AnimationComponent 
{
    float interval = 0.0;
    int animationIndex = 0;
    float totalFrameTime = 0.0;
    std::string key;

};

class ButtonComponent {
public:
    glm::vec2 position;  // Position of the button
    glm::vec2 size;      // Size of the button
    bool isHovered;      // Is the button hovered?
    std::function<void()> onClick;  // Function to be called when the button is clicked

    ButtonComponent(glm::vec2 pos, glm::vec2 sz, std::function<void()> clickFunc)
        : position(pos), size(sz), isHovered(false), onClick(clickFunc) {}

    // Trigger click event
    void const TriggerClick() {
        if (onClick) {
            onClick();
        }
    }
};

struct ScriptComponent
{
    std::string scriptPath;   // e.g. "scripts/player.lua"
    int tableRef = LUA_NOREF; // reference to the Lua table (instance)
    bool initialized = false; // if on_init() has been called
    bool hotReload = true;    // enable/disable live reload
    uint64_t lastWriteTicks = 0; // file modification time cache

    ScriptComponent() = default;
    ScriptComponent(const std::string& path)
        : scriptPath(path)
    {}
};