///////////////////////////////////////////////////////////////////////////////
///
///@file TagManager.h
/// 
/// @brief This file contains the declaration of the Tag Manager. It handles
///       the assigning and removal of tags in an entity.
///  
///@Authors: Teo Joel
///Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////


#pragma once
#include "Types.h"
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <cassert>

namespace Framework 
{
	class TagManager
	{
		public:
			// Takes in an entity and a tag name, then assigns the tag to that entity
			void AddTag(Entity entity, const std::string& tag);

			// Takes in an entity and a tag name, then removes the tag from that entity
			void RemoveTag(Entity entity, const std::string& tag);

			// Takes in an entity and a tag name, then returns true if the tag exists, or false if it does not
			bool HasTag(Entity entity, const std::string& tag) const;

			// Takes in a tag name, and returns the entities that contain said tag
			const std::unordered_set<Entity>& GetEntitiesByTag(const std::string& tag) const;

			// Takes in an entity and removes all tags from it 
			void RemoveAllTags(Entity entity);

			/// Clears all stored tag associations.
			void Clear();

		private:
			std::unordered_map<std::string, std::unordered_set<Entity>> tagToEntities;
			std::unordered_map<Entity, std::unordered_set<std::string>> entityToTags;
	};
}

