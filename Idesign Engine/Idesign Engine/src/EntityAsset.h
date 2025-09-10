///////////////////////////////////////////////////////////////////////////////
///
///	@File  : EntityAsset.h
/// @Brief : Defines the EntityAsset class, responsible for loading and managing
///          entity configurations for the application. This includes deserializing 
///          entity data from files and integrating with the ECS (Entity Component System).
///
///	@Authors : Edwin
///	@Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef _ENTITY_H_
#define _ENTITY_H_
#include "pch.h"
#include "JsonSerialize.h"
#include "Coordinator.h"
#include "ComponentList.h"

extern Framework::Coordinator ecsInterface;

/**
 * @class EntityAsset
 * @brief Manages the loading and configuration of entities within the ECS, allowing
 *        for entity creation and setup from configuration files.
 */
class EntityAsset
{
public:

    EntityAsset();

    /**
     * @brief Constructs an EntityAsset and loads entity configurations from a file.
     * @param filePath Path to the file containing entity data.
     */
    EntityAsset(const std::string& filePath);
    
    /**
     * @brief Destructor for the EntityAsset class.
     */
    ~EntityAsset();
	
    /**
     * @brief Deserializes and loads entity configurations from a specified file.
     * @param filePath Path to the file containing entity data.
     */
    void DeserializeEntities(const std::string& filePath);

    void SerializeEntities(const std::string& filePath);

private:
};

extern EntityAsset GlobalEntityAsset;

#endif // !_ENTITY_H_