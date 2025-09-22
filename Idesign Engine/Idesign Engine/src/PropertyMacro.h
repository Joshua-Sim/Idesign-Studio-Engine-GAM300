///////////////////////////////////////////////////////////////////////////////
///
///	@file  : PropertyMacro.h
/// @Brief : 
///	
///	@Authors: Edwin Leow
///	@Copyright 2025, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef _PROPERTY_MACRO_H_
#define _PROPERTY_MACRO_H_

#include "PropertySystem.h"

//------------------------------------------------------------------------------------------------------------------------------------
//  MACRO
//------------------------------------------------------------------------------------------------------------------------------------

// INT PROPERTY MACRO
#define PROP_INT(field, type) \
    Framework::prop{ #field, Framework::prop::INT, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const int*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<int*>(pData) = static_cast<const type*>(pObj)->field; } }

// FLOAT PROPERTY MACRO
#define PROP_FLOAT(field, type) \
    Framework::prop{ #field, Framework::prop::FLOAT, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const float*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<float*>(pData) = static_cast<const type*>(pObj)->field; } }

// DOUBLE PROPERTY MACRO
#define PROP_DOUBLE(field, type) \
    Framework::prop{ #field, Framework::prop::DOUBLE, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const double*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<double*>(pData) = static_cast<const type*>(pObj)->field; } }

// LONG PROPERTY MACRO
#define PROP_LONG(field, type) \
    Framework::prop{ #field, Framework::prop::LONG, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const long*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<long*>(pData) = static_cast<const type*>(pObj)->field; } }

// SHORT PROPERTY MACRO
#define PROP_SHORT(field, type) \
    Framework::prop{ #field, Framework::prop::SHORT, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const short*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<short*>(pData) = static_cast<const type*>(pObj)->field; } }

// STRING PROPERTY MACRO
#define PROP_STRING(field, type) \
    Framework::prop{ #field, Framework::prop::STRING, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const std::string*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<std::string*>(pData) = static_cast<const type*>(pObj)->field; } }

// CHAR PROPERTY MACRO
#define PROP_CHAR(field, type) \
    Framework::prop{ #field, Framework::prop::CHAR, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const char*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<char*>(pData) = static_cast<const type*>(pObj)->field; } }

// BOOL PROPERTY MACRO
#define PROP_BOOL(field, type) \
    Framework::prop{ #field, Framework::prop::BOOL, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const bool*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<bool*>(pData) = static_cast<const type*>(pObj)->field; } }


//------------------------------------------------------------------------------------------------------------------------------------
//  META MACRO
//------------------------------------------------------------------------------------------------------------------------------------

// INT PROPERTY META MACRO
#define PROP_INT_META(field, type, tooltip, category, min, max) \
    Framework::prop{ #field, Framework::prop::INT, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const int*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<int*>(pData) = static_cast<const type*>(pObj)->field; }, \
        tooltip, category, static_cast<float>(min), static_cast<float>(max), true }

// FLOAT PROPERTY META MACRO
#define PROP_FLOAT_META(field, type, tooltip, category, min, max) \
    Framework::prop{ #field, Framework::prop::FLOAT, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const float*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<float*>(pData) = static_cast<const type*>(pObj)->field; }, \
        tooltip, category, min, max, true }

// DOUBLE PROPERTY META MACRO
#define PROP_DOUBLE_META(field, type, tooltip, category, min, max) \
    Framework::prop{ #field, Framework::prop::DOUBLE, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const double*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<double*>(pData) = static_cast<const type*>(pObj)->field; }, \
        tooltip, category, static_cast<float>(min), static_cast<float>(max), true }

// LONG PROPERTY META MACRO
#define PROP_LONG_META(field, type, tooltip, category, min, max) \
    Framework::prop{ #field, Framework::prop::LONG, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const long*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<long*>(pData) = static_cast<const type*>(pObj)->field; }, \
        tooltip, category, static_cast<float>(min), static_cast<float>(max), true }

// SHORT PROPERTY META MACRO
#define PROP_SHORT_META(field, type, tooltip, category, min, max) \
    Framework::prop{ #field, Framework::prop::SHORT, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const short*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<short*>(pData) = static_cast<const type*>(pObj)->field; }, \
        tooltip, category, static_cast<float>(min), static_cast<float>(max), true }

// STRING PROPERTY META MACRO
#define PROP_STRING_META(field, type, tooltip, category) \
    Framework::prop{ #field, Framework::prop::STRING, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const std::string*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<std::string*>(pData) = static_cast<const type*>(pObj)->field; }, \
        tooltip, category, 0.0f, 0.0f, true }

// CHAR PROPERTY META MACRO
#define PROP_CHAR_META(field, type, tooltip, category, min, max) \
    Framework::prop{ #field, Framework::prop::CHAR, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const char*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<char*>(pData) = static_cast<const type*>(pObj)->field; }, \
        tooltip, category, static_cast<float>(min), static_cast<float>(max), true }

// BOOL PROPERTY META MACRO
#define PROP_BOOL_META(field, type, tooltip, category, min, max) \
    Framework::prop{ #field, Framework::prop::BOOL, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const bool*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<bool*>(pData) = static_cast<const type*>(pObj)->field; }, \
        tooltip, category, static_cast<float>(min), static_cast<float>(max), true }
#endif // !_PROPERTY_MACRO_H_

//------------------------------------------------------------------------------------------------------------------------------------
//  META MACRO
//------------------------------------------------------------------------------------------------------------------------------------

#define PROP_INT_ARRAY(field, type) \
    Framework::prop{ \
        #field, Framework::prop::INT_ARRAY, \
        [](void* pObj, const void* pData) { static_cast<type*>(pObj)->field = *static_cast<const std::vector<int>*>(pData); }, \
        [](const void* pObj, void* pData) { *static_cast<std::vector<int>*>(pData) = static_cast<const type*>(pObj)->field; } \
    }

#define PROP_FLOAT_ARRAY(field, type) \
    Framework::prop{ \
        #field, Framework::prop::FLOAT_ARRAY, \
        [](void* pObj, const void* pData) { static_cast<type*>(pObj)->field = *static_cast<const std::vector<float>*>(pData); }, \
        [](const void* pObj, void* pData) { *static_cast<std::vector<float>*>(pData) = static_cast<const type*>(pObj)->field; } \
    }

#define PROP_STRING_ARRAY(field, type) \
    Framework::prop{ \
        #field, Framework::prop::STRING_ARRAY, \
        [](void* pObj, const void* pData) { static_cast<type*>(pObj)->field = *static_cast<const std::vector<std::string>*>(pData); }, \
        [](const void* pObj, void* pData) { *static_cast<std::vector<std::string>*>(pData) = static_cast<const type*>(pObj)->field; } \
    }

//------------------------------------------------------------------------------------------------------------------------------------
//  TYPENAME MACRO
//------------------------------------------------------------------------------------------------------------------------------------

template<typename T>
struct TypeName;

#define REGISTER_TYPE_NAME(Type) \
    template<> struct TypeName<Type> { static constexpr const char* value = #Type; }


//------------------------------------------------------------------------------------------------------------------------------------
//  ECS COMPONENT MACRO
//------------------------------------------------------------------------------------------------------------------------------------

#define REGISTER_COMPONENT_WITH_PROPS(ComponentType, PropListRef) \
    ComponentManager::Instance().RegisterComponent<ComponentType>(PropListRef)

#define REGISTER_COMPONENT_AUTO(ComponentType) \
    ComponentManager::Instance().RegisterComponent<ComponentType>(&Framework::g_##ComponentType##Props)
