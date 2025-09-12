///////////////////////////////////////////////////////////////////////////////
///
///	@file  : PropertySystem.h
/// @Brief : 
///	
///	@Authors: Edwin Leow
///	@Copyright 2025, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#pragma once
#include <iostream>
#include <vector>
#include <cassert>
#include <span>
#include <array>

// Property system backbone
// Things to do
/*
    Audio               - All Cleared
    Editor (IMGUI)      - Wait for 3D Graphics to be implemented first
    Serialization       - 1 Generic Load + 1 Generic LoadArray Cleared
    Deserialization     - 1 Generic Unload (Nid Test)
    ECS Components      - 1 Prop per Component (Nid Test)
    Prefab System       - 1 Prop per Prefab
    Scripting           - In the future
    Font System?
    Physics System?

    - Create a struct for each Object
    - With that struct, create an array for that Object's properties
    - Tie all the properties up into the same Object
    - Register Object into Global Registry
*/
namespace Framework
{
    // A single property descriptor (like "field metadata")
    struct prop
    {
        // The kinds of primitive types we support right now
        enum atomic_type
        {
            INT,
            FLOAT,
            DOUBLE,
            STRING,
            CHAR,
            BOOL
        };

        // Function pointers to *set* and *get* a value on an object.
        // Lambdas to read/write a field of an object
        using fn_set = void(*)(void* pObject, const void* pData);
        using fn_get = void(*)(const void* pObject, void* pData);

        const char* m_pName;   // Property name ("x", "y", etc.)
        atomic_type m_Type;    // What kind of data is this (int, float, …)?

        // To interact with the fields
        fn_set      m_fnSet;   // Setter function for this property, to write a new value
        fn_get      m_fnGet;   // Getter function for this property, to read the value
    };

    // Collection of Properties for one type
    struct prop_list
    {
        const char* m_pName;                // Type name
        std::span<const prop> m_Props;      // Span of all its properties
    };

    // Global registry of all reflected types
    // So you can iterate over *all* types at runtime without knowing them at compile time
    struct global_registration
    {
        global_registration* m_pNext;       // Linked-list pointer
        const prop_list* m_pPopList;        // The type's property list

        // Constructor registers this type into the global linked list
        global_registration(const prop_list* pPropList)
        {
            assert(pPropList && "global_registration: prop_list is null");
            assert(pPropList->m_pName && "global_registration: type name is null");
            assert(pPropList->m_Props.size() > 0 && "global_registration: property list is empty");

            m_pPopList = pPropList;
            m_pNext = m_pHead;
            m_pHead = this;
        }
        // Global static head of linked list
        inline static global_registration* m_pHead = nullptr;
    };

    /////////////////////////////////////////////
    // Reflected structs
    /////////////////////////////////////////////

    struct AudioData
    {
        std::string customName;
        std::string filePath;
        std::string mode;
        std::string soundType;
    };

    inline constexpr std::array g_AudioDataPropsArray = 
    {
        prop{ "customName", prop::STRING,
            [](void* pObject, const void* pData) { assert(pObject && pData); static_cast<AudioData*>(pObject)->customName = *static_cast<const std::string*>(pData); },
            [](const void* pObject, void* pData) { assert(pObject && pData); *static_cast<std::string*>(pData) = static_cast<const AudioData*>(pObject)->customName; } },

        prop{ "filePath", prop::STRING,
            [](void* pObject, const void* pData) { assert(pObject && pData); static_cast<AudioData*>(pObject)->filePath = *static_cast<const std::string*>(pData); },
            [](const void* pObject, void* pData) { assert(pObject && pData); *static_cast<std::string*>(pData) = static_cast<const AudioData*>(pObject)->filePath; } },

        prop{ "mode", prop::STRING,
            [](void* pObject, const void* pData) { assert(pObject && pData); static_cast<AudioData*>(pObject)->mode = *static_cast<const std::string*>(pData); },
            [](const void* pObject, void* pData) { assert(pObject && pData); *static_cast<std::string*>(pData) = static_cast<const AudioData*>(pObject)->mode; } },
        
        prop{ "soundType", prop::STRING,
            [](void* pObject, const void* pData) { assert(pObject && pData); static_cast<AudioData*>(pObject)->soundType = *static_cast<const std::string*>(pData); },
            [](const void* pObject, void* pData) { assert(pObject && pData); *static_cast<std::string*>(pData) = static_cast<const AudioData*>(pObject)->soundType; } },
    };


    inline constexpr prop_list g_AudioDataProps{ "AudioData", g_AudioDataPropsArray };

    inline global_registration g_AudioDataPropsReg(&g_AudioDataProps);


    //----------------------------------------------------------

    struct Prefab
    {
        std::string name;
        int entityCount;
    };

    inline constexpr std::array g_PrefabPropsArray
    {
        prop{ "name", prop::STRING,
            [](void* pObject, const void* pData) { assert(pObject && pData); static_cast<Prefab*>(pObject)->name = *static_cast<const std::string*>(pData); },
            [](const void* pObject, void* pData) { assert(pObject && pData); *static_cast<std::string*>(pData) = static_cast<const Prefab*>(pObject)->name; } },

        prop{ "entityCount", prop::INT,
            [](void* pObject, const void* pData) { assert(pObject && pData); static_cast<Prefab*>(pObject)->entityCount = *static_cast<const int*>(pData); },
            [](const void* pObject, void* pData) { assert(pObject && pData); *static_cast<int*>(pData) = static_cast<const Prefab*>(pObject)->entityCount; } },
    };

    inline constexpr prop_list g_PrefabProps{ "Prefab", g_PrefabPropsArray };
    inline global_registration g_PrefabPropsReg(&g_PrefabProps);

    //----------------------------------------------------------

    struct WindowConfig
    {
        int x;
        int y;
        std::string programName;
    };

    inline constexpr std::array g_WindowConfigPropsArray
    {
        prop { "x", prop::INT,
            [](void* pObject, const void* pData) { assert(pObject && pData); static_cast<WindowConfig*>(pObject)->x = *static_cast<const int*>(pData); },
            [](const void* pObject, void* pData) { assert(pObject && pData); *static_cast<int*>(pData) = static_cast<const WindowConfig*>(pObject)->x; } },

        prop { "y", prop::INT,
            [](void* pObject, const void* pData) { assert(pObject && pData); static_cast<WindowConfig*>(pObject)->y = *static_cast<const int*>(pData); },
            [](const void* pObject, void* pData) { assert(pObject && pData); *static_cast<int*>(pData) = static_cast<const WindowConfig*>(pObject)->y; } },

        prop { "program_name", prop::STRING,
            [](void* pObject, const void* pData) { assert(pObject && pData); static_cast<WindowConfig*>(pObject)->programName = *static_cast<const std::string*>(pData); },
            [](const void* pObject, void* pData) { assert(pObject && pData); *static_cast<std::string*>(pData) = static_cast<const WindowConfig*>(pObject)->programName; } }
    };

    // Bundle all properties of `windowconfig` together
    inline constexpr prop_list g_WindowConfigProps{ "WindowConfig", g_WindowConfigPropsArray };

    // Auto-register `windowconfig` in the global registry
    inline global_registration g_WindowConfigReg(&g_WindowConfigProps);
}