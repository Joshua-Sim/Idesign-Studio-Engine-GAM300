
#pragma once
#ifndef _PROPERTY_WIDGET_H_
#define _PROPERTY_WIDGET_H_

// STL Library 
#include <algorithm>  // for std::copy, std::sort, etc.
#include <iterator>   // for std::begin, std::end, etc.

// ImGui Stuff
#include "imgui.h"
#include <gtc/type_ptr.hpp>

// Files
#include "Coordinator.h"
#include "TypeRegistry.h"
#include "PropertySystem.h"

// All Components || Properties
#include "TextureProperties.h"
#include "MeshProperties.h"
#include "AudioProperties.h"
#include "TransformProperties.h"

extern Framework::Coordinator ecsInterface;
namespace Framework
{
	// For Entity Inspector
	void DrawInspector(void* pObject, const prop_list& propList);
	void DrawEntityInspector(Entity entity, Coordinator& ecs);
	
	// For Component Registration with reflection
	void RegisterComponentTypes();
	void AutoRegisterAllReflectedComponents(Coordinator& ecs);
}
#endif // !_PROPERTY_WIDGET_H_