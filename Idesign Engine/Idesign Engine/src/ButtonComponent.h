#pragma once
#ifndef _BUTTON_COMPONENT_H_
#define _BUTTON_COMPONENT_H_

#include "PropertySystem.h" 
#include "PropertyMacro.h" 
#include <glm.hpp>
#include <functional>
#include <iostream>
#include "AudioSystem.h"

namespace Framework
{
    struct ButtonComponent
    {
        glm::vec2 position;                  // Position of the button
        glm::vec2 size;                      // Size of the button
        bool isHovered;                      // Is the button hovered?
        std::function<void()> onClick;       // Function to be called when clicked
        bool isFrontButton = false;
        bool interactable = true;   // NEW: hard gate for clicking

        void TriggerClick(Entity e) const
        {
            if (!interactable) return;          // NEW
            if (onClick) 
            {
                g_AudioSystem->PlayAudio("event:/ButtonClick");
                onClick();
            }
            else
            {
                // Default debug behaviour if nothing is bound
                std::cout << "[Button] Entity " << e << " has been clicked!\n";
            }
        }
    };

    // Property registration for ButtonComponent
    inline const std::vector g_ButtonComponentPropsArray =
    {
        PROP_VEC2(position,   ButtonComponent),
        PROP_VEC2(size,       ButtonComponent),
        PROP_BOOL(isHovered,  ButtonComponent),
        PROP_BOOL(isFrontButton, ButtonComponent),
        // Note: onClick is a function, not a serializable property, so we don’t register it
    };

    inline const prop_list g_ButtonComponentProps{ "ButtonComponent", g_ButtonComponentPropsArray };
    inline global_registration g_ButtonComponentReg(&g_ButtonComponentProps);
}
#endif // !_BUTTON_COMPONENT_H_