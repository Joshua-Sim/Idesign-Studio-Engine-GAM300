//#include "PropertySystem.h" 
//#include "PropertyMacro.h" 
//
//#include <glm.hpp>
//
//namespace Framework
//{
//
//    struct ButtonComponent
//    {
//        glm::vec2 position;                  // Position of the button
//        glm::vec2 size;                      // Size of the button
//        bool isHovered;                      // Is the button hovered?
//        std::function<void()> onClick;       // Function to be called when clicked
//
//        void TriggerClick() const {
//            if (onClick) {
//                onClick();
//            }
//        }
//    };
//
//    // Property registration for ButtonComponent
//    inline const std::vector g_ButtonComponentPropsArray =
//    {
//        PROP_VEC2(position,   ButtonComponent),
//        PROP_VEC2(size,       ButtonComponent),
//        PROP_BOOL(isHovered,  ButtonComponent),
//        // Note: onClick is a function, not a serializable property, so we don’t register it
//    };
//
//    inline const prop_list g_ButtonComponentProps{ "ButtonComponent", g_ButtonComponentPropsArray };
//    inline global_registration g_ButtonComponentReg(&g_ButtonComponentProps);
//}