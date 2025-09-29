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
#ifndef _PROPERTY_SYSTEM_H_
#define _PROPERTY_SYSTEM_H_

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
    Serialization       - Tested
    Deserialization     - Tested
    ECS Components      - 1 Prop per Component (Nid Test)
    Prefab System       - 1 Prop per Prefab
    Scripting           - In the future
    Font System?
    Physics System?

    - Create a struct for each Object
    - With that struct, create an array for that Object's properties
    - Tie all the properties up into the same Object
    - Register Object into Global Registry

    Current Idea of Property System 12/09/2025
    - Create a new .h object property file
    - Include PropertySystem.h + PropertyMacro.h in new file
    - Create reflection metadata in the new file
    - Include the new files in other .h files that requires its data
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
            LONG,
            SHORT,
            STRING,
            CHAR,
            BOOL,
            STRING_ARRAY,
            VEC3,
            UINT
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

        // New metadata fields
        const char* m_pTooltip = nullptr;
        const char* m_pCategory = nullptr;
        float       m_Min = 0.0f;
        float       m_Max = 0.0f;
        bool        m_HasRange = false;
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

    inline const prop_list* FindPropListByName(const std::string& typeName)
    {
        global_registration* current = global_registration::m_pHead;
        while (current)
        {
            if (current->m_pPopList && current->m_pPopList->m_pName == typeName)
                return current->m_pPopList;

            current = current->m_pNext;
        }
        return nullptr;
    }
}
#endif // !_PROPERTY_SYSTEM_H_