#pragma once

#include <glm.hpp>
#include <vector>
#include <string>
#include <map>
#include "Animation.h" // Include the data structures

namespace Framework
{
    struct AnimationProperties
    {
        Skeleton* skeleton = nullptr;
        Animation* currentAnimation = nullptr;
        float currentTime = 0.0f;

        std::vector<glm::mat4> finalBoneMatrices;
        std::map<std::string, Animation*> availableAnimations;

        void PlayAnimation(const std::string& name)
        {
            auto it = availableAnimations.find(name);
            if (it != availableAnimations.end())
            {
                if (currentAnimation != it->second)
                {
                    currentAnimation = it->second;
                    currentTime = 0.0f;
                }
            }
        }

        AnimationProperties() // Constructor remains the same name as the struct
        {
            finalBoneMatrices.resize(100);
        }
    };
}