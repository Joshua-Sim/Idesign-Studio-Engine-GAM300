///////////////////////////////////////////////////////////////////////////////
///
/// \file MessageSystem.h
/// 
/// * The MessageSystem defines both the legacy and modern messaging frameworks 
///   for communication between systems in the Unnamed Engine.
/// * The legacy `MessageSystem` provides backward compatibility with old 
///   input and event-based components using polymorphic messages.
/// * The modern `MessageBus` introduces a centralized, type-safe, and flexible 
///   event dispatch model with immediate and deferred message support.
/// 
/// -------------------------------
/// HOW TO USE THE MESSAGE SYSTEM
/// -------------------------------
///
/// Legacy System Example:
/// -----------------------
/// @code
/// // Subscribe to a key press event
/// MessageSystem::GetInstance()->Subscribe<MessageKeyPressed>(
///     [](MessageKeyPressed* msg) {
///         std::cout << "Key pressed: " << msg->keyCode << std::endl;
///     });
///
/// // Dispatch a key press message
/// MessageSystem::GetInstance()->Dispatch(new MessageKeyPressed(GLFW_KEY_SPACE));
/// @endcode
///
///
/// Modern System Example:
/// -----------------------
/// @code
/// // Define a new message type
/// struct PlayerScoredMessage : public TypedMessage<PlayerScoredMessage> {
///     Entity player;
///     int points;
///     PlayerScoredMessage(Entity p, int pts) : player(p), points(pts) {}
/// };
///
/// // Subscribe to the message
/// MessageBus::Get().Subscribe<PlayerScoredMessage>(
///     [](const PlayerScoredMessage& msg) {
///         std::cout << "Player " << msg.player << " scored " << msg.points << " points!" << std::endl;
///     });
///
/// // Send a message immediately
/// MessageBus::Get().SendImmediate(PlayerScoredMessage(playerID, 100));
///
/// // Queue a message for later processing
/// MessageBus::Get().QueueMessage(PlayerScoredMessage(playerID, 50));
/// MessageBus::Get().ProcessQueue(); // Deliver queued messages
/// @endcode
///
/// Authors: Belinda Chua Jia Xuam 
/// Copyright 2025, Digipen Institute of Technology
///////////////////////////////////////////////////////////////////////////////


#pragma once
#include <functional>
#include <vector>
#include <unordered_map>
#include <queue>
#include <memory>
#include <typeindex>
#include <glm.hpp>
#include <string>
#include <iostream>
#include <GLFW/glfw3.h>

namespace Framework {

    // Forward declarations
    using Entity = uint32_t;

    // ==================== OLD SYSTEM COMPATIBILITY ====================
    // These classes must come first as they're used by existing code

    /**
     * Base message class for old system compatibility
     */
    class Message {
    public:
        virtual ~Message() = default;
        virtual std::string GetMessageName() const { return "BaseMessage"; }
    };

    /**
     * Mouse click message for old InputHandler compatibility
     */
    class MessageMouseClick : public Message {
    public:
        int button;

        MessageMouseClick(int btn) : button(btn) {}
        std::string GetMessageName() const override { return "MessageMouseClick"; }
    };

    /**
     * Key pressed message for old system
     */
    class MessageKeyPressed : public Message {
    public:
        int keyCode;

        MessageKeyPressed(int key) : keyCode(key) {}
        std::string GetMessageName() const override { return "MessageKeyPressed"; }
    };

    /**
     * Mouse move message for old system
     */
    class MessageMouseMove : public Message {
    public:
        double x, y;

        MessageMouseMove(double xPos, double yPos) : x(xPos), y(yPos) {}
        std::string GetMessageName() const override { return "MessageMouseMove"; }
    };

    /**
     * MessageSystem class - wrapper for compatibility with old code
     */
    class MessageSystem {
    private:
        static MessageSystem* instance;

        // Store handlers for old system
        std::vector<std::function<void(MessageKeyPressed*)>> keyPressHandlers;
        std::vector<std::function<void(MessageMouseClick*)>> mouseClickHandlers;
        std::vector<std::function<void(MessageMouseMove*)>> mouseMoveHandlers;

    public:
        static MessageSystem* GetInstance() {
            if (!instance) {
                instance = new MessageSystem();
            }
            return instance;
        }

        static void Destroy() {
            delete instance;
            instance = nullptr;
        }

        // Old dispatch method that InputHandler expects
        void Dispatch(Message* msg) {
            if (auto* mouseClick = dynamic_cast<MessageMouseClick*>(msg)) {
                for (auto& handler : mouseClickHandlers) {
                    handler(mouseClick);
                }
            }
            else if (auto* keyPress = dynamic_cast<MessageKeyPressed*>(msg)) {
                for (auto& handler : keyPressHandlers) {
                    handler(keyPress);
                }
            }
            else if (auto* mouseMove = dynamic_cast<MessageMouseMove*>(msg)) {
                for (auto& handler : mouseMoveHandlers) {
                    handler(mouseMove);
                }
            }

            // Clean up the message
            delete msg;
        }

        // Template specializations for subscribing
        template<typename MessageType>
        void Subscribe(std::function<void(MessageType*)> handler);
    };

    // Template specializations for each message type
    template<>
    inline void MessageSystem::Subscribe<MessageKeyPressed>(std::function<void(MessageKeyPressed*)> handler) {
        keyPressHandlers.push_back(handler);
    }

    template<>
    inline void MessageSystem::Subscribe<MessageMouseClick>(std::function<void(MessageMouseClick*)> handler) {
        mouseClickHandlers.push_back(handler);
    }

    template<>
    inline void MessageSystem::Subscribe<MessageMouseMove>(std::function<void(MessageMouseMove*)> handler) {
        mouseMoveHandlers.push_back(handler);
    }

    // ==================== NEW MESSAGE SYSTEM ====================

    /**
     * Base class for new message system
     */
    class NewMessage {
    public:
        virtual ~NewMessage() = default;
        virtual std::type_index GetTypeIndex() const = 0;

        float timestamp = 0.0f;
        Entity sender = std::numeric_limits<Entity>::max();
    };

    /**
     * Template for new concrete message types
     */
    template<typename Derived>
    class TypedMessage : public NewMessage {
    public:
        std::type_index GetTypeIndex() const override {
            return std::type_index(typeid(Derived));
        }

        static std::type_index GetStaticTypeIndex() {
            return std::type_index(typeid(Derived));
        }
    };

    /**
     * Message handler function types
     */
    template<typename MessageType>
    using MessageHandler = std::function<void(const MessageType&)>;
    using GenericHandler = std::function<void(const NewMessage&)>;

    /**
     * Message Bus - central hub for all new messaging
     */
    class MessageBus {
    private:
        std::unordered_map<std::type_index, std::vector<GenericHandler>> immediateHandlers;
        std::queue<std::shared_ptr<NewMessage>> messageQueue;
        std::unordered_map<std::type_index, std::vector<GenericHandler>> deferredHandlers;

        size_t totalMessagesSent = 0;
        std::unordered_map<std::type_index, size_t> messageCountByType;

        static MessageBus* instance;

        MessageBus() = default;
        ~MessageBus() = default;
        MessageBus(const MessageBus&) = delete;
        MessageBus& operator=(const MessageBus&) = delete;

    public:
        static MessageBus& Get();
        static void Destroy();

        template<typename MessageType>
        void Subscribe(MessageHandler<MessageType> handler) {
            auto typeIndex = MessageType::GetStaticTypeIndex();
            immediateHandlers[typeIndex].push_back(
                [handler](const NewMessage& msg) {
                    handler(static_cast<const MessageType&>(msg));
                }
            );
        }

        template<typename MessageType>
        void SubscribeDeferred(MessageHandler<MessageType> handler) {
            auto typeIndex = MessageType::GetStaticTypeIndex();
            deferredHandlers[typeIndex].push_back(
                [handler](const NewMessage& msg) {
                    handler(static_cast<const MessageType&>(msg));
                }
            );
        }

        template<typename MessageType>
        void SendImmediate(const MessageType& message) {
            auto typeIndex = message.GetTypeIndex();
            totalMessagesSent++;
            messageCountByType[typeIndex]++;

            auto it = immediateHandlers.find(typeIndex);
            if (it != immediateHandlers.end()) {
                for (auto& handler : it->second) {
                    handler(message);
                }
            }
        }

        template<typename MessageType>
        void QueueMessage(const MessageType& message) {
            messageQueue.push(std::make_shared<MessageType>(message));
        }

        void ProcessQueue();
        void Clear();
        void ClearHandlers();
        void ClearQueue();
        void PrintStatistics() const;
        void ResetStatistics();

        size_t GetTotalMessagesSent() const { return totalMessagesSent; }
        size_t GetMessageCount(std::type_index type) const;
        size_t GetQueueSize() const { return messageQueue.size(); }
        size_t GetHandlerCount(std::type_index type, bool immediate = true) const;
    };

    // ==================== NEW GAME MESSAGE TYPES ====================

    /**
     * Collision Event Message
     */
    struct CollisionMessage : TypedMessage<CollisionMessage> {
        Entity entity1;
        Entity entity2;
        glm::vec3 contactPoint;
        glm::vec3 contactNormal;
        float penetrationDepth;

        CollisionMessage(Entity e1, Entity e2, const glm::vec3& point,
            const glm::vec3& normal, float depth = 0.0f)
            : entity1(e1), entity2(e2), contactPoint(point),
            contactNormal(normal), penetrationDepth(depth) {
        }
    };

    /**
     * Entity Damage Message
     */
    struct DamageMessage : TypedMessage<DamageMessage> {
        Entity target;
        Entity source;
        float amount;
        std::string damageType;

        DamageMessage(Entity tgt, float dmg, const std::string& type = "physical",
            Entity src = std::numeric_limits<Entity>::max())
            : target(tgt), source(src), amount(dmg), damageType(type) {
        }
    };

    /**
     * Entity Destroyed Message
     */
    struct EntityDestroyedMessage : TypedMessage<EntityDestroyedMessage> {
        Entity entity;
        std::string reason;

        EntityDestroyedMessage(Entity e, const std::string& r = "")
            : entity(e), reason(r) {
        }
    };

    /**
     * Input Movement Message
     */
    struct InputMoveMessage : TypedMessage<InputMoveMessage> {
        Entity player;
        glm::vec3 direction;
        float speed;

        InputMoveMessage(Entity p, const glm::vec3& dir, float s = 1.0f)
            : player(p), direction(dir), speed(s) {
        }
    };

    /**
     * Input Action Message
     */
    struct InputActionMessage : TypedMessage<InputActionMessage> {
        enum Action {
            JUMP,
            ATTACK,
            INTERACT,
            USE_ABILITY,
            RELOAD,
            CROUCH,
            SPRINT
        };

        Entity player;
        Action action;
        bool pressed;

        InputActionMessage(Entity p, Action a, bool pr = true)
            : player(p), action(a), pressed(pr) {
        }
    };

    /**
     * Item Pickup Message
     */
    struct ItemPickupMessage : TypedMessage<ItemPickupMessage> {
        Entity collector;
        Entity item;
        std::string itemType;
        int quantity;

        ItemPickupMessage(Entity c, Entity i, const std::string& type, int qty = 1)
            : collector(c), item(i), itemType(type), quantity(qty) {
        }
    };

    /**
     * Play Sound Message
     */
    struct PlaySoundMessage : TypedMessage<PlaySoundMessage> {
        std::string soundId;
        glm::vec3 position;
        float volume;
        bool is3D;
        bool loop;

        PlaySoundMessage(const std::string& id, float vol = 1.0f,
            const glm::vec3& pos = glm::vec3(0),
            bool threeD = false, bool l = false)
            : soundId(id), position(pos), volume(vol), is3D(threeD), loop(l) {
        }
    };

    /**
     * UI Update Message
     */
    struct UIUpdateMessage : TypedMessage<UIUpdateMessage> {
        enum UIElement {
            HEALTH_BAR,
            MANA_BAR,
            SCORE,
            TIMER,
            AMMO_COUNT,
            INVENTORY,
            MINIMAP
        };

        UIElement element;
        float value;
        float maxValue;
        std::string text;

        UIUpdateMessage(UIElement elem, float val, float max = 100.0f, const std::string& t = "")
            : element(elem), value(val), maxValue(max), text(t) {
        }
    };

    /**
     * Apply Force Message
     */
    struct ApplyForceMessage : TypedMessage<ApplyForceMessage> {
        Entity entity;
        glm::vec3 force;
        glm::vec3 point;
        bool isImpulse;

        ApplyForceMessage(Entity e, const glm::vec3& f,
            const glm::vec3& p = glm::vec3(0), bool impulse = false)
            : entity(e), force(f), point(p), isImpulse(impulse) {
        }
    };

    /**
     * Trigger Enter Message
     */
    struct TriggerEnterMessage : TypedMessage<TriggerEnterMessage> {
        Entity trigger;
        Entity other;

        TriggerEnterMessage(Entity t, Entity o)
            : trigger(t), other(o) {
        }
    };

    /**
     * Trigger Exit Message
     */
    struct TriggerExitMessage : TypedMessage<TriggerExitMessage> {
        Entity trigger;
        Entity other;

        TriggerExitMessage(Entity t, Entity o)
            : trigger(t), other(o) {
        }
    };

    /**
     * Console Log Message
     */
    struct ConsoleLogMessage : TypedMessage<ConsoleLogMessage> {
        enum class LogLevel {
            INFO,
            WARNING,
            ERR,
            SUCCESS
        };

        std::string message;
        LogLevel level;
        std::string timestamp;
        std::string category;

        ConsoleLogMessage(const std::string& msg,
            LogLevel lvl = LogLevel::INFO,
            const std::string& cat = "APP")
            : message(msg), level(lvl), category(cat), timestamp("--:--:--") {
            // Timestamp will be generated by ImguiManager to avoid header compilation issues
        }
    };

} // namespace Framework