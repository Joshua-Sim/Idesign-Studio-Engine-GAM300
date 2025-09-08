///////////////////////////////////////////////////////////////////////////////
///
///@file TagManager.cpp
/// 
/// @brief This file contains the definition of the Tag Manager. It handles
///       the assigning and removal of tags in an entity.
///  
///@Authors: Teo Joel
///Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "TagManager.h"

namespace Framework
{
	void TagManager::AddTag(Entity entity, const std::string& tag)
	{
		assert(tagToEntities.find(tag) == tagToEntities.end() || tagToEntities[tag].find(entity) == tagToEntities[tag].end() && "Entity is already tagged with this tag!");

		tagToEntities[tag].insert(entity);
		entityToTags[entity].insert(tag);
	}

	// Takes in an entity and a tag name, then removes the tag from that entity
	void TagManager::RemoveTag(Entity entity, const std::string& tag)
	{
		assert(tagToEntities.find(tag) != tagToEntities.end() && tagToEntities[tag].find(entity) != tagToEntities[tag].end() && "Entity is not tagged with this tag!");

		tagToEntities[tag].erase(entity);
		entityToTags[entity].erase(tag);
	}

	// Takes in an entity and a tag name, then returns true if the tag exists, or false if it does not
	bool TagManager::HasTag(Entity entity, const std::string& tag) const
	{
		auto it = entityToTags.find(entity);
		if (it == entityToTags.end())
		{
			return false;
		}

		return it->second.find(tag) != it->second.end();
	}

	// Takes in a tag name, and returns the entities that contain said tag
	const std::unordered_set<Entity>& TagManager::GetEntitiesByTag(const std::string& tag) const
	{
		// Safely handle tags that do not exist
		static const std::unordered_set<Entity> emptySet{};
		auto it = tagToEntities.find(tag);
		
		if (it != tagToEntities.end()) 
		{
			return it->second;
		}

		return emptySet;  // Return empty set if the tag is not found
	}

	// Takes in an entity and removes all tags from it 
	void TagManager::RemoveAllTags(Entity entity)
	{
		auto it = entityToTags.find(entity);
		if (it != entityToTags.end())
		{
			for (const auto& tag : it->second)
			{
				tagToEntities[tag].erase(entity);
			}
			entityToTags.erase(it);
		}
	}
}

