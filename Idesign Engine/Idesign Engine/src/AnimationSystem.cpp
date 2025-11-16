#include "pch.h"
#include "AnimationSystem.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <iomanip>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/quaternion.hpp>
#include <gtx/quaternion.hpp>
#include <gtx/matrix_decompose.hpp> 

#include "Coordinator.h"
#include "ComponentList.h"
#include "System.h"
#include "Animation.h"
#include "MeshLoader.h"
#include "TextureLoader.h"
#include "AnimationProperties.h"
#include "MessageSystem.h"

extern Framework::Coordinator ecsInterface;

namespace Framework
{
    void AnimationSystem::Initialize()
    {
        ecsInterface.RegisterComponent<AnimationProperties>();
        Signature signature;
        signature.set(ecsInterface.GetComponentType<AnimationProperties>());
        ecsInterface.SetSystemSignature<AnimationSystem>(signature);
    }

    std::string AnimationSystem::GetName()
    {
        return "AnimationSystem";
    }

    void AnimationSystem::Update(float deltaTime)
    {
        for (auto const& entity : mEntities)
        {
            if (ecsInterface.HasComponent<AnimationProperties>(entity)) {
                auto& animator = ecsInterface.GetComponent<AnimationProperties>(entity);


                if (!animator.currentAnimation && animator.bPlayOnStart)
                {

                    animator.StartDefaultAnimation();
                }

                // Now, if an animation is (or just became) active, update its transforms
                if (animator.currentAnimation)
                {
                    CalculateBoneTransforms(animator, deltaTime);
                }
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


    void AnimationSystem::UpdateBoneTransformsRecursive(
        AnimationProperties& animator,
        const Skeleton& skeleton,
        const Animation& animation,
        int boneIndex,
        const glm::mat4& parentTransform,
        std::vector<glm::mat4>& globalTransforms)
    {
        const Bone& bone = skeleton.bones[boneIndex];
        const auto* channel = animation.FindChannel(bone.name);

        // 1a. Decompose the bone's default (T-pose) transform
        glm::vec3 scale;
        glm::quat rotation;
        glm::vec3 position;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(bone.defaultLocalTransform, scale, rotation, position, skew, perspective);

        // 1b. Overwrite with animated keys IF they exist
        if (channel)
        {
            // This logic will now run every frame as intended
            if (!channel->positions.empty())
                position = InterpolatePosition(channel, animator.currentTime);

            if (!channel->rotations.empty())
                rotation = InterpolateRotation(channel, animator.currentTime);

            if (!channel->scales.empty())
                scale = InterpolateScale(channel, animator.currentTime);
        }

        // 1c. Rebuild the final local transform matrix
        glm::mat4 localTransform = glm::translate(glm::mat4(1.0f), position) *
            glm::toMat4(rotation) *
            glm::scale(glm::mat4(1.0f), scale);


        // 2. Calculate Global Transform
        glm::mat4 globalTransform = parentTransform * localTransform;
        globalTransforms[boneIndex] = globalTransform; // Store it for children

        // 3. Store Final Matrix for the shader
        animator.finalBoneMatrices[boneIndex] = globalTransform * bone.inverseBindPoseMatrix;


        // 4. Recurse for Children
        for (int childIndex : bone.childrenIndices)
        {
            UpdateBoneTransformsRecursive(animator, skeleton, animation,
                childIndex, globalTransform, globalTransforms);
        }
    }

    void AnimationSystem::CalculateBoneTransforms(AnimationProperties& animator, float dt)
    {
        const Skeleton& skeleton = animator.skeleton;
        const Animation& animation = *animator.currentAnimation;
        // 1. Update Time
        float ticksPerSecond = (animation.ticksPerSecond > 0.0f) ? animation.ticksPerSecond : 24.0f;
        animator.currentTime += dt * animator.animationSpeed * ticksPerSecond;

        // Only loop the time if the duration is valid (greater than zero)
        if (animation.duration > 0.0f)
        {
            if (animator.bLoops)
            {
                animator.currentTime = fmod(animator.currentTime, animation.duration);
            }
            else
            {
                // We are not looping, so clamp the time to the end
                animator.currentTime = std::min(animator.currentTime, animation.duration);
            }
        }

        if (animator.finalBoneMatrices.size() != skeleton.bones.size())
        {
            animator.finalBoneMatrices.resize(skeleton.bones.size(), glm::mat4(1.0f));
        }

        // 2. Allocate space for this frame's global transforms
        std::vector<glm::mat4> globalTransforms(skeleton.bones.size(), glm::mat4(1.0f));

        // 3. Start recursion from the root bone
        int rootIndex = skeleton.rootBoneIndex;
        if (rootIndex != -1)
        {

            UpdateBoneTransformsRecursive(animator, skeleton, animation,
                rootIndex, skeleton.rootNodeTransform, globalTransforms);
        }
        else
        {
            static bool s_didError = false;
            if (!s_didError) {
                std::cerr << "Animation Error: No root bone found! (Index is -1)" << std::endl;
                MessageBus::Get().SendImmediate(ConsoleLogMessage(
                    "Animation Error: No root bone found! (Index is -1)",
                    ConsoleLogMessage::LogLevel::ERR,
                    "AnimationSystem"
                ));
                s_didError = true;
            }
        }
    }
}