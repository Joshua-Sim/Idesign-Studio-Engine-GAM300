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
#include <gtx/matrix_decompose.hpp> // <-- ADD THIS LINE

#include "Coordinator.h"
#include "ComponentList.h"
#include "System.h"
#include "Animation.h"
#include "MeshLoader.h"
#include "TextureLoader.h"
#include "AnimationProperties.h" // Make sure this is included

extern Framework::Coordinator ecsInterface;

#include <iomanip> // To format the matrix output

// Helper function to print a glm::mat4
void printMat4(const glm::mat4& mat)
{
    std::cout << std::fixed << std::setprecision(2);
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            std::cout << std::setw(8) << mat[j][i] << " ";
        }
        std::cout << std::endl;
    }
    std::cout << "---------------------------------" << std::endl;


}

namespace Framework
{
    void AnimationSystem::Initialize()
    {
        // This registration should ideally be in your main engine setup,
        // not in the system's Initialize()
        ecsInterface.RegisterComponent<AnimationProperties>();
        std::cout << "AnimationSystem Initialized\n";
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
        std::vector<glm::mat4>& globalTransforms) // Pass globalTransforms as a reference
    {
        const Bone& bone = skeleton.bones[boneIndex];
        const auto* channel = animation.FindChannel(bone.name);
        // --- REPLACE WITH THIS NEW DEBUG BLOCK ---
        static bool didDebugPrint = false;
        if (!didDebugPrint)
        {
            //// This part runs ONLY for the root bone to print a header
            //if (boneIndex == skeleton.rootBoneIndex)
            //{
            //    std::cout << "--- BONE & CHANNEL NAME DEBUG ---" << std::endl;
            //    std::cout << "Available Animation Channels:" << std::endl;
            //    for (auto const& [name, anim] : animation.boneChannels)
            //    {
            //        std::cout << "  - [\"" << name << "\"]" << std::endl;
            //    }
            //    std::cout << "\nMatching Skeleton Bones to Channels:" << std::endl;
            //}

            //// This part runs for EVERY bone
            //std::string boneName = "  - Bone: [\"" + bone.name + "\"]";
            //if (channel)
            //{
            //    std::cout << boneName << " ... OK (Found Channel)" << std::endl;
            //}
            //else
            //{
            //    std::cout << boneName << " ... FAILED (Channel Not Found)" << std::endl;
            //}

            //// After the last bone, stop printing
            //if (boneIndex == skeleton.bones.size() - 1)
            //{
            //    didDebugPrint = true;
            //    std::cout << "---------------------------------" << std::endl;
            //}
        }

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
            // Only use animated position if keyframes exist
            if (!channel->positions.empty())
                position = InterpolatePosition(channel, animator.currentTime);

            // Only use animated rotation if keyframes exist
            if (!channel->rotations.empty())
                rotation = InterpolateRotation(channel, animator.currentTime);

            // Only use animated scale if keyframes exist
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
        // This is correct: (AnimatedBoneToModelSpace) * (ModelSpaceToBindPoseBoneSpace)
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
        animator.currentTime += dt * ticksPerSecond;
        animator.currentTime = fmod(animator.currentTime, animation.duration);

        // 2. Allocate space for this frame's global transforms
        //    (This is used by the recursive helper)
        std::vector<glm::mat4> globalTransforms(skeleton.bones.size(), glm::mat4(1.0f));

        // 3. Start recursion from the root bone
        int rootIndex = skeleton.rootBoneIndex;
        if (rootIndex != -1)
        {
            // --- FIX: Pass the skeleton's rootNodeTransform ---
            // The root node's transform (loaded from Assimp) must be the
            // starting parentTransform, otherwise the entire animation
            // may be mis-oriented or mis-scaled.
            UpdateBoneTransformsRecursive(animator, skeleton, animation,
                rootIndex, skeleton.rootNodeTransform, globalTransforms);
            // --- END FIX ---

            // --- ADD THIS DEBUG CODE ---
            static int frameCount = 0;
            if (frameCount % 60 == 0) // Print once every 60 frames
            {
               // std::cout << "=== Root Bone Matrix (Final) ===\n";
                // Print the final shader matrix for the *root bone*
                printMat4(animator.finalBoneMatrices[rootIndex]);
            }
            frameCount++;
            // --- END OF DEBUG CODE ---
        }
        else
        {
            // This error will stop if you fixed ProcessNode in MeshLoader.cpp
            std::cerr << "Animation Error: No root bone found! (Index is -1)" << std::endl;
        }
    }
}