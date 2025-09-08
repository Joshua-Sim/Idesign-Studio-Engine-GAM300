///////////////////////////////////////////////////////////////////////////////
///
///	@File  : EntityAsset.cpp
/// @Brief : This file defines the EntityAsset class, which is responsible for 
///          loading and managing entity configurations for the application. 
///          The class deserializes entity data from JSON files and integrates 
///          them into the Entity-Component System (ECS), allowing the application 
///          to load and organize entities with their associated components, 
///          such as transformations, rendering properties, and behavior functions.
///
///	@Authors : Edwin
///	@Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#include "pch.h"
#include "EntityAsset.h"
#include "LogicManager.h"
#include "AssetManager.h"

EntityAsset GlobalEntityAsset;

EntityAsset::EntityAsset() {}

EntityAsset::EntityAsset(const std::string& filePath)
{
    DeserializeEntities(filePath);
}

EntityAsset::~EntityAsset() {}

void EntityAsset::DeserializeEntities(const std::string& filename)
{
    // Read JSON file
    std::ifstream ifs(filename);
    rapidjson::IStreamWrapper isw(ifs);
    rapidjson::Document document;
    document.ParseStream(isw);

    // Check for errors in parsing
    if (document.HasParseError()) 
    {
        std::cerr << "Error parsing JSON file!" << std::endl;
        return;
    }

    // Check if "entities" array exists
    if (!document.HasMember("entities") || !document["entities"].IsArray()) 
    {
        std::cerr << "Invalid or missing 'entities' array!" << std::endl;
        return;
    }

    // Iterate over each entity in the "entities" array
    const rapidjson::Value& entities = document["entities"];
    for (rapidjson::SizeType i = 0; i < entities.Size(); ++i) 
    {
        const rapidjson::Value& entity = entities[i];

        // Create a new entity
        Framework::Entity newEntity = ecsInterface.CreateEntity();

        // Check if the entity has components
        if (entity.HasMember("components") && entity["components"].IsObject()) 
        {
            const rapidjson::Value& components = entity["components"];

            // Check and add TransformComponent
            if (components.HasMember("TransformComponent") && components["TransformComponent"].IsObject()) 
            {
                const rapidjson::Value& transform = components["TransformComponent"];
                TransformComponent transformComponent;

                if (transform.HasMember("x")) transformComponent.position.x = transform["x"].GetFloat();
                if (transform.HasMember("y")) transformComponent.position.y = transform["y"].GetFloat();
                if (transform.HasMember("scaleX")) transformComponent.scale.x = transform["scaleX"].GetFloat();
                if (transform.HasMember("scaleY")) transformComponent.scale.y = transform["scaleY"].GetFloat();
                if (transform.HasMember("rotation")) transformComponent.rotation = transform["rotation"].GetFloat();

                ecsInterface.AddComponent<TransformComponent>(newEntity, transformComponent);
                //std::cout << "ADDED TRANSFORM COMPONENT\n";
            }

            // Check and add RenderComponent
            if (components.HasMember("RenderComponent") && components["RenderComponent"].IsObject()) 
            {
                const rapidjson::Value& render = components["RenderComponent"];
                RenderComponent renderComponent;

                if (render.HasMember("textureID")) renderComponent.textureID = render["textureID"].GetString();

                // Parse color array if it exists
                if (render.HasMember("color") && render["color"].IsArray()) 
                {
                    const rapidjson::Value& colorArray = render["color"];
                    if (colorArray.Size() == 3) 
                    {
                        renderComponent.color = glm::vec3
                        (
                            colorArray[0].GetFloat(),
                            colorArray[1].GetFloat(),
                            colorArray[2].GetFloat()
                        );
                    }
                }

                if (render.HasMember("alpha")) renderComponent.alpha = render["alpha"].GetFloat();

                // Parse renderType and convert to enum
                if (render.HasMember("renderType") && render["renderType"].IsString()) 
                {
                    std::string typeStr = render["renderType"].GetString();
                    if (typeStr == "Sprite") renderComponent.renderType = RenderType::Sprite;
                    else if (typeStr == "Particle") renderComponent.renderType = RenderType::Particle;
                    else if (typeStr == "Text") renderComponent.renderType = RenderType::Text;
                }

                ecsInterface.AddComponent<RenderComponent>(newEntity, renderComponent);
                //std::cout << "ADDED RENDER COMPONENT\n";
            }

            // Check and add LayerComponent
            if (components.HasMember("LayerComponent") && components["LayerComponent"].IsObject()) 
            {
                const rapidjson::Value& layer = components["LayerComponent"];
                LayerComponent layerComponent;

                // Check if LayerID is a string and map to enum
                if (layer.HasMember("LayerID")) 
                {
                    if (layer["LayerID"].IsString()) 
                    {
                        std::string layerStr = layer["LayerID"].GetString();
                        if (layerStr == "Background") layerComponent.layerID = Layer::Background;
                        else if (layerStr == "Character") layerComponent.layerID = Layer::Character;
                        else if (layerStr == "Foreground") layerComponent.layerID = Layer::Foreground;
                        else if (layerStr == "UI") layerComponent.layerID = Layer::UI;
                        else if (layerStr == "Debug") layerComponent.layerID = Layer::Debug;
                        else layerComponent.layerID = Layer::Background; // Default or unknown
                    }
                    // Check if LayerID is an integer and assign directly
                    else if (layer["LayerID"].IsInt()) 
                    {
                        int layerInt = layer["LayerID"].GetInt();
                        // Ensure the integer is within the valid enum range
                        if (layerInt >= static_cast<int>(Layer::Background) && layerInt <= static_cast<int>(Layer::Debug)) 
                        {
                            layerComponent.layerID = static_cast<Layer>(layerInt);
                        }
                        else 
                        {
                            layerComponent.layerID = Layer::Background; // Default if out of range
                            std::cerr << "Warning: LayerID out of range. Defaulting to Background layer.\n";
                        }
                    }
                }

                // Assign SortID if present
                if (layer.HasMember("SortID") && layer["SortID"].IsUint()) 
                {
                    layerComponent.sortID = layer["SortID"].GetUint();
                }

                ecsInterface.AddComponent<LayerComponent>(newEntity, layerComponent);
                // std::cout << "ADDED LAYER COMPONENT\n";
            }

            // Check and add TextComponent
            if (components.HasMember("TextComponent") && components["TextComponent"].IsObject()) 
            {
                const rapidjson::Value& textComp = components["TextComponent"];
                TextComponent textComponent;

                // Read "text" field
                if (textComp.HasMember("text") && textComp["text"].IsString()) 
                {
                    textComponent.text = textComp["text"].GetString();
                }

                // Read "fontSize" field
                if (textComp.HasMember("fontSize") && textComp["fontSize"].IsInt()) 
                {
                    textComponent.fontSize = textComp["fontSize"].GetInt();
                }

                // Read "color" array
                if (textComp.HasMember("color") && textComp["color"].IsArray()) 
                {
                    const rapidjson::Value& colorArray = textComp["color"];
                    if (colorArray.Size() == 3) 
                    {
                        textComponent.color = glm::vec3
                        (
                            colorArray[0].GetFloat(),
                            colorArray[1].GetFloat(),
                            colorArray[2].GetFloat()
                        );
                    }
                }

                // Read "fontName" field
                if (textComp.HasMember("fontName") && textComp["fontName"].IsString()) 
                {
                    textComponent.fontName = textComp["fontName"].GetString();
                }

                // Read "offset" array
                if (textComp.HasMember("offset") && textComp["offset"].IsArray()) 
                {
                    const rapidjson::Value& offsetArray = textComp["offset"];
                    if (offsetArray.Size() == 2) 
                    {
                        textComponent.offset = glm::vec2
                        (
                            offsetArray[0].GetFloat(),
                            offsetArray[1].GetFloat()
                        );
                    }
                }

                // Add TextComponent to entity
                ecsInterface.AddComponent<TextComponent>(newEntity, textComponent);
                // std::cout << "ADDED TEXT COMPONENT\n";
            }

            // Check and add PlayerComponent
            if (components.HasMember("PlayerComponent") && components["PlayerComponent"].IsObject())
            {
                const rapidjson::Value& playerComp = components["PlayerComponent"];
                PlayerComponent playerComponent;

                // Read "CurrentText" field
                if (playerComp.HasMember("CurrentText") && playerComp["CurrentText"].IsString())
                {
                    playerComponent.CurrentText = playerComp["CurrentText"].GetString();
                }

                // Add PlayerComponent to entity
                ecsInterface.AddComponent<PlayerComponent>(newEntity, playerComponent);
                // std::cout << "ADDED PLAYER COMPONENT\n";
            }
            //Check if it has movement component
            if (components.HasMember("MovementComponent") && components["MovementComponent"].IsObject()) {

                const rapidjson::Value& movement = components["MovementComponent"];

                MovementComponent movementComponent;

                if (movement.HasMember("x")) movementComponent.velocity.x = movement["x"].GetFloat();
                if (movement.HasMember("y")) movementComponent.velocity.y = movement["y"].GetFloat();

                ecsInterface.AddComponent<MovementComponent>(newEntity, movementComponent);

                //std::cout << "ADDED MOVEMENT COMPONENT\n";
            }

            //Check if it has collision component
            if (components.HasMember("CollisionComponent") && components["CollisionComponent"].IsObject()) {
                const rapidjson::Value& collision = components["CollisionComponent"];
                CollisionComponent collisionComponent;
                if (collision.HasMember("type")) {
                    std::string typeStr = collision["type"].GetString();
                    if (typeStr == "Player") {

                        collisionComponent.type = Player;
                    }
                    else if (typeStr == "Enemy") {

                        collisionComponent.type = Enemy;
                    }
                    else if (typeStr == "CollidableObject") {
                        collisionComponent.type = CollidableObject;
                    }
                }
                if (collision.HasMember("collided")) collisionComponent.collided = collision["collided"].GetBool();
                if (collision.HasMember("radius")) collisionComponent.radius = collision["radius"].GetFloat();

                ecsInterface.AddComponent<CollisionComponent>(newEntity, collisionComponent);
                //std::cout << "ADDED COLLISION COMPONENT\n";
            }

            // Check and add Enemy Component
            if (components.HasMember("EnemyComponent") && components["EnemyComponent"].IsObject()) {
                const rapidjson::Value& enemy = components["EnemyComponent"];
                EnemyComponent enemyComponent;

                // Load and set the enemy type
                if (enemy.HasMember("type") && enemy["type"].IsString()) {
                    std::string typeStr = enemy["type"].GetString();
                    if (typeStr == "Minion") {
                        enemyComponent.type = Minion;
                    }
                    else if (typeStr == "Boss") {
                        enemyComponent.type = Boss;
                    }
                }

                // Load health value
                if (enemy.HasMember("health") && enemy["health"].IsFloat()) {
                    enemyComponent.health = enemy["health"].GetFloat();
                }

                // Load the UpdateFunctionName
                if (enemy.HasMember("UpdateFunctionName") && enemy["UpdateFunctionName"].IsString()) {
                    enemyComponent.UpdateFunctionName = enemy["UpdateFunctionName"].GetString();

                    // Retrieve the behavior function from LogicManager
                    BehaviorFunction behaviorFunction = GlobalLogicManager.GetFunction(enemyComponent.UpdateFunctionName);

                    if (behaviorFunction) {
                        enemyComponent.behavior = behaviorFunction;
                        std::cout << "Assigned behavior function '" << enemyComponent.UpdateFunctionName
                            << "' to entity " << newEntity << std::endl;
                    }
                    else {
                        std::cerr << "Warning: Behavior function '" << enemyComponent.UpdateFunctionName
                            << "' not found for entity " << newEntity << std::endl;
                    }
                }

                // Add EnemyComponent to the entity
                ecsInterface.AddComponent<EnemyComponent>(newEntity, enemyComponent);
                std::cout << "ADDED ENEMY COMPONENT to entity " << newEntity << std::endl;
            }

            // Check and add Animation Component
            if (components.HasMember("AnimationComponent") && components["AnimationComponent"].IsObject()) {
                const rapidjson::Value& animation = components["AnimationComponent"];
                AnimationComponent animationComponent;
                
                if (animation.HasMember("interval")) animationComponent.interval = animation["interval"].GetFloat();
                if (animation.HasMember("animationIndex")) animationComponent.animationIndex = animation["animationIndex"].GetInt();
                if (animation.HasMember("totalFrameTime")) animationComponent.totalFrameTime = animation["totalFrameTime"].GetFloat();
                if (animation.HasMember("key")) animationComponent.key = animation["key"].GetString();

                ecsInterface.AddComponent<AnimationComponent>(newEntity, animationComponent);
                //std::cout << "ADDED ANIMATION COMPONENT\n";
            }
        }
    }
}

void EntityAsset::SerializeEntities(const std::string& filename)
{
    std::cout << "Serializing to: " << filename << std::endl;

    // Create a JSON document
    rapidjson::Document document;
    document.SetObject();

    // Create the "entities" array
    rapidjson::Value entities(rapidjson::kArrayType);

    auto& entityAsset = ecsInterface.GetEntities(); // Get all entities
    std::cout << "Found " << entityAsset.size() << " entities." << std::endl;

    // Iterate through all entities
    for (const auto& entity : entityAsset)
    {
        std::cout << "Serializing entity: " << entity << std::endl;

        rapidjson::Value entityObj(rapidjson::kObjectType);

        // Add type of the entity (use GetEntityName if applicable)
        std::string entityName = ecsInterface.GetEntityName(entity);
        std::cout << "Entity name: " << entityName << std::endl;

        // Use "type" instead of "name" for entity type
        entityObj.AddMember("type", rapidjson::Value(entityName.c_str(), document.GetAllocator()), document.GetAllocator());

        // Create a "components" object for the entity
        rapidjson::Value components(rapidjson::kObjectType);

        // Serialize TransformComponent
        if (ecsInterface.HasComponent<TransformComponent>(entity)) {
            const auto& transform = ecsInterface.GetComponent<TransformComponent>(entity);
            std::cout << "Serializing TransformComponent for entity: " << entity << std::endl;

            rapidjson::Value transformComp(rapidjson::kObjectType);
            transformComp.AddMember("x", transform.position.x, document.GetAllocator());
            transformComp.AddMember("y", transform.position.y, document.GetAllocator());
            transformComp.AddMember("scaleX", transform.scale.x, document.GetAllocator());
            transformComp.AddMember("scaleY", transform.scale.y, document.GetAllocator());
            transformComp.AddMember("rotation", transform.rotation, document.GetAllocator());
            components.AddMember("TransformComponent", transformComp, document.GetAllocator());
        }

        // Serialize RenderComponent
        if (ecsInterface.HasComponent<RenderComponent>(entity)) {
            const auto& render = ecsInterface.GetComponent<RenderComponent>(entity);
            std::cout << "Serializing RenderComponent for entity: " << entity << std::endl;

            rapidjson::Value renderComp(rapidjson::kObjectType);
            renderComp.AddMember("textureID", rapidjson::Value(render.textureID.c_str(), document.GetAllocator()), document.GetAllocator());

            // Serialize color as an array
            rapidjson::Value colorArray(rapidjson::kArrayType);
            colorArray.PushBack(render.color.r, document.GetAllocator());
            colorArray.PushBack(render.color.g, document.GetAllocator());
            colorArray.PushBack(render.color.b, document.GetAllocator());
            renderComp.AddMember("color", colorArray, document.GetAllocator());

            renderComp.AddMember("alpha", render.alpha, document.GetAllocator());

            // Serialize renderType as a string
            std::string renderTypeStr;
            switch (render.renderType) {
            case RenderType::Sprite: renderTypeStr = "Sprite"; break;
            case RenderType::Particle: renderTypeStr = "Particle"; break;
            case RenderType::Text: renderTypeStr = "Text"; break;
            }
            renderComp.AddMember("renderType", rapidjson::Value(renderTypeStr.c_str(), document.GetAllocator()), document.GetAllocator());

            components.AddMember("RenderComponent", renderComp, document.GetAllocator());
        }

        // Serialize TextComponent
        if (ecsInterface.HasComponent<TextComponent>(entity)) {
            const auto& text = ecsInterface.GetComponent<TextComponent>(entity);
            std::cout << "Serializing TextComponent for entity: " << entity << std::endl;

            rapidjson::Value textComp(rapidjson::kObjectType);

            textComp.AddMember("text", rapidjson::Value(text.text.c_str(), document.GetAllocator()), document.GetAllocator());
            textComp.AddMember("fontSize", text.fontSize, document.GetAllocator());

            // Serialize color as an array
            rapidjson::Value colorArray(rapidjson::kArrayType);
            colorArray.PushBack(text.color.r, document.GetAllocator());
            colorArray.PushBack(text.color.g, document.GetAllocator());
            colorArray.PushBack(text.color.b, document.GetAllocator());
            textComp.AddMember("color", colorArray, document.GetAllocator());

            textComp.AddMember("fontName", rapidjson::Value(text.fontName.c_str(), document.GetAllocator()), document.GetAllocator());

            // Serialize offset as an array
            rapidjson::Value offsetArray(rapidjson::kArrayType);
            offsetArray.PushBack(text.offset.x, document.GetAllocator());
            offsetArray.PushBack(text.offset.y, document.GetAllocator());
            textComp.AddMember("offset", offsetArray, document.GetAllocator());

            components.AddMember("TextComponent", textComp, document.GetAllocator());
        }

        // Serialize LayerComponent
        if (ecsInterface.HasComponent<LayerComponent>(entity)) {
            const auto& layer = ecsInterface.GetComponent<LayerComponent>(entity);
            std::cout << "Serializing LayerComponent for entity: " << entity << std::endl;

            rapidjson::Value layerComp(rapidjson::kObjectType);

            layerComp.AddMember("LayerID", static_cast<int>(layer.layerID), document.GetAllocator());
            layerComp.AddMember("SortID", layer.sortID, document.GetAllocator());

            components.AddMember("LayerComponent", layerComp, document.GetAllocator());
        }

        // Add the components to the entity
        entityObj.AddMember("components", components, document.GetAllocator());

        // Add the entity to the "entities" array
        entities.PushBack(entityObj, document.GetAllocator());
    }

    // Add "entities" array to the document
    document.AddMember("entities", entities, document.GetAllocator());

    std::cout << "Writing JSON to file: " << filename << std::endl;

    // Convert document to JSON string and write to file
    std::ofstream ofs(filename);
    if (!ofs.is_open()) {
        std::cerr << "Error: Unable to open file " << filename << " for writing." << std::endl;
        return;
    }

    rapidjson::OStreamWrapper osw(ofs);
    rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
    document.Accept(writer);

    std::cout << "Entities serialized successfully to " << filename << std::endl;
}
