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
#include "Jolt.h"
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>

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

// Hierarchy component for engine side sorting and management.
struct HierarchyComponent { // Left here since property serialization not necessary as a property panel, can move out if needed
    std::string parentGUID;                  // Empty if root
    std::vector<std::string> childrenGUIDs;  // Stable identifiers for inspector and serialization
    int sortIndex = 0; // Default to 0, unique among siblings
};

// Tag component for engine side tagging. Kept simple and serializable via vector O(n) lookup
struct TagComponent
{
    std::vector<std::string> tags; // e.g., "Enemy", "Boss", "Lootable"
};

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

// In ComponentList.h or wherever PhysicsBodyComponent is defined
struct PhysicsBodyComponent {
    std::vector<JPH::BodyID> IDs;

    JPH::ShapeRefC shape; 
    bool isInPhysicsSystem = false;
};