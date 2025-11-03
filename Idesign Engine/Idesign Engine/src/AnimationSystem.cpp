#include "pch.h"
#include "AnimationSystem.h"
#include <iostream>
#include <cmath>
#include <algorithm>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/quaternion.hpp>
#include <gtx/quaternion.hpp>

#include "Coordinator.h"
#include "ComponentList.h"
#include "System.h"
#include "Animation.h"
#include "MeshLoader.h"
#include "TextureLoader.h"
#include "AnimationProperties.h" // Make sure this is included

extern Framework::Coordinator ecsInterface;

namespace Framework
{
    void AnimationSystem::Initialize()
    {
        // This registration should ideally be in your main engine setup,
        // not in the system's Initialize()
        ecsInterface.RegisterComponent<AnimationProperties>();
        std::cout << "AnimationSystem Initialized\n";
    }

    std::string AnimationSystem::GetName()
    {
        return "AnimationSystem";
    }

    void AnimationSystem::Update(float deltaTime)
    {
        // mEntities list is already filtered to only contain entities
        // with the component(s) in this system's signature.
        for (auto const& entity : mEntities)
        {
            if (ecsInterface.HasComponent<AnimationProperties>(entity)) {
                auto& animator = ecsInterface.GetComponent<AnimationProperties>(entity);

                // If an animation is assigned, calculate transforms
                if (animator.currentAnimation)
                {
                    CalculateBoneTransforms(animator, deltaTime);
                }
            }
          
        }
    }

    void AnimationSystem::CalculateBoneTransforms(AnimationProperties& animator, float dt)
    {
        const Skeleton& skeleton = animator.skeleton;
        const Animation& animation = *animator.currentAnimation;

        // Update animation time
        float ticksPerSecond = (animation.ticksPerSecond > 0.0f) ? animation.ticksPerSecond : 24.0f;
        animator.currentTime += dt * ticksPerSecond;
        if (animation.duration > 0.0f) {
            animator.currentTime = fmod(animator.currentTime, animation.duration);
        }
        else {
            animator.currentTime = 0.0f;
        }

        std::vector<glm::mat4> globalTransforms(skeleton.bones.size());

        for (int i = 0; i < skeleton.bones.size(); ++i)
        {
            const auto& bone = skeleton.bones[i];
            const auto* channel = animation.FindChannel(bone.name);

            glm::mat4 localTransform = bone.defaultLocalTransform; // Default pose

            if (channel)
            {
                // Interpolate keyframes to get new local transform
                glm::vec3 pos = InterpolatePosition(channel, animator.currentTime);
                glm::quat rot = InterpolateRotation(channel, animator.currentTime);
                glm::vec3 scale = InterpolateScale(channel, animator.currentTime);

                localTransform = glm::translate(glm::mat4(1.0f), pos) *
                    glm::toMat4(rot) *
                    glm::scale(glm::mat4(1.0f), scale);
            }

            if (bone.parentIndex == -1) // Root bone
            {
                globalTransforms[i] = localTransform;
            }
            else // Child bone
            {
                globalTransforms[i] = globalTransforms[bone.parentIndex] * localTransform;
            }

            // Apply inverse bind pose matrix
            if (i < animator.finalBoneMatrices.size()) {
                animator.finalBoneMatrices[i] = globalTransforms[i] * bone.inverseBindPoseMatrix;
            }
        }
    }


    int AnimationSystem::FindPositionKeyframe(const BoneAnimation* channel, float time)
    {
        if (channel->positions.empty()) return -1;
        for (int i = 0; i < channel->positions.size() - 1; ++i) {
            if (time < channel->positions[i + 1].timeStamp)
                return i;
        }
        return std::max(0, (int)channel->positions.size() - 2);
    }

    int AnimationSystem::FindRotationKeyframe(const BoneAnimation* channel, float time)
    {
        if (channel->rotations.empty()) return -1;
        for (int i = 0; i < channel->rotations.size() - 1; ++i) {
            if (time < channel->rotations[i + 1].timeStamp)
                return i;
        }
        return std::max(0, (int)channel->rotations.size() - 2);
    }

    int AnimationSystem::FindScaleKeyframe(const BoneAnimation* channel, float time)
    {
        if (channel->scales.empty()) return -1;
        for (int i = 0; i < channel->scales.size() - 1; ++i) {
            if (time < channel->scales[i + 1].timeStamp)
                return i;
        }
        return std::max(0, (int)channel->scales.size() - 2);
    }

    glm::vec3 AnimationSystem::InterpolatePosition(const BoneAnimation* channel, float time)
    {
        if (channel->positions.empty()) return glm::vec3(0.0f);
        if (channel->positions.size() == 1) return channel->positions[0].position;

        int p0_idx = FindPositionKeyframe(channel, time);
        if (p0_idx == -1) return glm::vec3(0.0f);
        int p1_idx = p0_idx + 1;

        float t0 = channel->positions[p0_idx].timeStamp;
        float t1 = channel->positions[p1_idx].timeStamp;
        float factor = (t1 - t0 > 0.0f) ? glm::clamp((time - t0) / (t1 - t0), 0.0f, 1.0f) : 0.0f;

        return glm::mix(channel->positions[p0_idx].position,
            channel->positions[p1_idx].position, factor);
    }

    glm::quat AnimationSystem::InterpolateRotation(const BoneAnimation* channel, float time)
    {
        if (channel->rotations.empty()) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        if (channel->rotations.size() == 1) return channel->rotations[0].rotation;

        int r0_idx = FindRotationKeyframe(channel, time);
        if (r0_idx == -1) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        int r1_idx = r0_idx + 1;

        float t0 = channel->rotations[r0_idx].timeStamp;
        float t1 = channel->rotations[r1_idx].timeStamp;
        float factor = (t1 - t0 > 0.0f) ? glm::clamp((time - t0) / (t1 - t0), 0.0f, 1.0f) : 0.0f;

        return glm::slerp(channel->rotations[r0_idx].rotation,
            channel->rotations[r1_idx].rotation, factor);
    }

    glm::vec3 AnimationSystem::InterpolateScale(const BoneAnimation* channel, float time)
    {
        if (channel->scales.empty()) return glm::vec3(1.0f);
        if (channel->scales.size() == 1) return channel->scales[0].scale;

        int s0_idx = FindScaleKeyframe(channel, time);
        if (s0_idx == -1) return glm::vec3(1.0f);
        int s1_idx = s0_idx + 1;

        float t0 = channel->scales[s0_idx].timeStamp;
        float t1 = channel->scales[s1_idx].timeStamp;
        float factor = (t1 - t0 > 0.0f) ? glm::clamp((time - t0) / (t1 - t0), 0.0f, 1.0f) : 0.0f;

        return glm::mix(channel->scales[s0_idx].scale,
            channel->scales[s1_idx].scale, factor);
    }
}