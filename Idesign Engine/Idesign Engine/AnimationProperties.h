#pragma once

#include <glew.h> // For GLuint
#include <glm.hpp>
#include <vector>
#include <map>
#include <string>
#include "animation.h"

namespace Framework
{
    /**
         * @brief ECS Component for animated entities.
         */
    struct AnimationProperties
    {
        Animation* currentAnimation = nullptr;
        float currentTime = 0.0f;

        unsigned int vbo_skinning = 0; // VBO for bone IDs/weights

        std::vector<glm::mat4> finalBoneMatrices;
        std::map<std::string, Animation*> availableAnimations;

        // This component OWNS the skeleton data.
        // This line will now work because Skeleton is defined in "AnimationData.h"
        Skeleton skeleton;

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
    };
}