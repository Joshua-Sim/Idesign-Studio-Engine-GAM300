#include "pch.h"
#include "AnimationSystem.h"
#include <glm.hpp>
#include "Coordinator.h"
#include "ComponentList.h"
#include "System.h"
#include "Animation.h"
#define GLM_ENABLE_EXPERIMENTAL
#include "gtx/quaternion.hpp"


extern Framework::Coordinator ecsInterface;

namespace Framework
{


    void AnimationSystem::Initialize()
    {
        // Add any setup code needed for the animation system here
        ecsInterface.RegisterComponent<AnimationProperties>();
        ecsInterface.RegisterComponent<AnimationComponent>();

        std::cout << "AnimationSystem Initialized\n";
    }



    std::string AnimationSystem::GetName() 
    {
        return "AnimationSystem";
    }

    void AnimationSystem::Update(float deltaTime)
    {
        // Loop over all entities that this system cares about
        // (i.e., entities with an AnimationProperties component)
        for (auto const& entity : mEntities)
        {
            if (ecsInterface.HasComponent<AnimationProperties>(entity)) {

 
                auto& animator = ecsInterface.GetComponent<AnimationProperties>(entity); // Use renamed component

                // Only update if it has a valid animation and skeleton
                if (animator.currentAnimation && animator.skeleton)
                {
                    CalculateBoneTransforms(animator, deltaTime);
                }
            }
        }
    }

    void AnimationSystem::CalculateBoneTransforms(AnimationProperties& animator, float dt) // Use renamed component
    {
        const Skeleton& skeleton = *animator.skeleton;
        const Animation& animation = *animator.currentAnimation;

        // 1. Advance and loop the animation time
        float ticksPerSecond = (animation.ticksPerSecond > 0.0f) ? animation.ticksPerSecond : 24.0f; // Default Ticks Per Second
        animator.currentTime += dt * ticksPerSecond;
        animator.currentTime = fmod(animator.currentTime, animation.duration);

        // 2. Calculate the local and global transforms for each bone
        std::vector<glm::mat4> globalTransforms(skeleton.bones.size());

        // This loop MUST iterate in parent-first order for hierarchy calculation
        // Assumes skeleton.bones is already sorted this way (usually loaded recursively)
        for (int i = 0; i < skeleton.bones.size(); ++i)
        {
            const auto& bone = skeleton.bones[i];
            const auto* channel = animation.FindChannel(bone.name);

            glm::mat4 localTransform;

            if (channel)
            {
                // Find interpolated transforms based on current time
                glm::vec3 pos = InterpolatePosition(channel, animator.currentTime);
                glm::quat rot = InterpolateRotation(channel, animator.currentTime);
                glm::vec3 scale = InterpolateScale(channel, animator.currentTime);

                // Combine them into a local matrix (T * R * S)
                localTransform = glm::translate(glm::mat4(1.0f), pos) *
                    glm::toMat4(rot) *
                    glm::scale(glm::mat4(1.0f), scale);
            }
            else
            {
                // No animation channel for this bone, use its default pose
                localTransform = bone.defaultLocalTransform;
            }

            // 3. Apply parent transform to get global transform
            if (bone.parentIndex == -1) // Root bone
            {
                globalTransforms[i] = localTransform;
            }
            else // Child bone
            {
                // Ensure parent index is valid
                if (bone.parentIndex >= 0 && bone.parentIndex < globalTransforms.size()) {
                    globalTransforms[i] = globalTransforms[bone.parentIndex] * localTransform;
                }
                else {
                    // Handle error case: invalid parent index
                    globalTransforms[i] = localTransform; // Fallback to local
                }
            }

            // 4. Calculate final matrix for the GPU shader
            // Global Transform * Inverse Bind Pose Matrix
            if (i < animator.finalBoneMatrices.size()) { // Bounds check
                animator.finalBoneMatrices[i] = globalTransforms[i] * bone.inverseBindPoseMatrix;
            }
        }
    }

    // --- INTERPOLATION FUNCTIONS ---

    // Find the index of the keyframe *before* the current time
    int AnimationSystem::FindPositionKeyframe(const BoneAnimation* channel, float time)
    {
        // Simple linear scan, could be optimized with binary search if needed
        for (int i = 0; i < channel->positions.size() - 1; ++i) {
            if (time < channel->positions[i + 1].timeStamp)
                return i;
        }
        // If time is past the last keyframe (due to looping), return the second to last index
        return std::max(0, (int)channel->positions.size() - 2);
    }

    int AnimationSystem::FindRotationKeyframe(const BoneAnimation* channel, float time)
    {
        for (int i = 0; i < channel->rotations.size() - 1; ++i) {
            if (time < channel->rotations[i + 1].timeStamp)
                return i;
        }
        return std::max(0, (int)channel->rotations.size() - 2);
    }

    int AnimationSystem::FindScaleKeyframe(const BoneAnimation* channel, float time)
    {
        for (int i = 0; i < channel->scales.size() - 1; ++i) {
            if (time < channel->scales[i + 1].timeStamp)
                return i;
        }
        return std::max(0, (int)channel->scales.size() - 2);
    }

    // Interpolate between two position keyframes
    glm::vec3 AnimationSystem::InterpolatePosition(const BoneAnimation* channel, float time)
    {
        if (channel->positions.empty()) return glm::vec3(0.0f); // Safety check
        if (channel->positions.size() == 1) return channel->positions[0].position; // Only one keyframe

        int p0_idx = FindPositionKeyframe(channel, time);
        int p1_idx = p0_idx + 1;

        float t0 = channel->positions[p0_idx].timeStamp;
        float t1 = channel->positions[p1_idx].timeStamp;
        float factor = (t1 - t0 > 0.0f) ? glm::clamp((time - t0) / (t1 - t0), 0.0f, 1.0f) : 0.0f; // Calculate blend factor

        // Linear interpolation (lerp)
        return glm::mix(channel->positions[p0_idx].position,
            channel->positions[p1_idx].position, factor);
    }

    // Interpolate between two rotation keyframes (using spherical linear interpolation)
    glm::quat AnimationSystem::InterpolateRotation(const BoneAnimation* channel, float time)
    {
        if (channel->rotations.empty()) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity quaternion
        if (channel->rotations.size() == 1) return channel->rotations[0].rotation;

        int r0_idx = FindRotationKeyframe(channel, time);
        int r1_idx = r0_idx + 1;

        float t0 = channel->rotations[r0_idx].timeStamp;
        float t1 = channel->rotations[r1_idx].timeStamp;
        float factor = (t1 - t0 > 0.0f) ? glm::clamp((time - t0) / (t1 - t0), 0.0f, 1.0f) : 0.0f;

        // Spherical linear interpolation (slerp) for smooth rotation
        return glm::slerp(channel->rotations[r0_idx].rotation,
            channel->rotations[r1_idx].rotation, factor);
    }

    // Interpolate between two scale keyframes
    glm::vec3 AnimationSystem::InterpolateScale(const BoneAnimation* channel, float time)
    {
        if (channel->scales.empty()) return glm::vec3(1.0f); // Default scale
        if (channel->scales.size() == 1) return channel->scales[0].scale;

        int s0_idx = FindScaleKeyframe(channel, time);
        int s1_idx = s0_idx + 1;

        float t0 = channel->scales[s0_idx].timeStamp;
        float t1 = channel->scales[s1_idx].timeStamp;
        float factor = (t1 - t0 > 0.0f) ? glm::clamp((time - t0) / (t1 - t0), 0.0f, 1.0f) : 0.0f;

        // Linear interpolation (lerp)
        return glm::mix(channel->scales[s0_idx].scale,
            channel->scales[s1_idx].scale, factor);
    }
}