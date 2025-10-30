#pragma once

#include "System.h" // Your base System header
#include "Coordinator.h"
#include "AnimationProperties.h" // Use the renamed component
#include "Animation.h"
#include <glm.hpp>
#include <gtc/quaternion.hpp>

namespace Framework
{
    class AnimationSystem : public ISystem
    {
    public:

        void Initialize() override;
        void Update(float deltaTime) override;
        std::string GetName() override;

    private:
        // Calculates the final bone transforms for one entity
        void CalculateBoneTransforms(AnimationProperties& animator, float dt);

        // --- Interpolation Helpers ---

        // Finds the keyframes to interpolate between
        int FindPositionKeyframe(const BoneAnimation* channel, float time);
        int FindRotationKeyframe(const BoneAnimation* channel, float time);
        int FindScaleKeyframe(const BoneAnimation* channel, float time);

        // Calculates the interpolated value
        glm::vec3 InterpolatePosition(const BoneAnimation* channel, float time);
        glm::quat InterpolateRotation(const BoneAnimation* channel, float time);
        glm::vec3 InterpolateScale(const BoneAnimation* channel, float time);
    };
}