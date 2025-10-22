///////////////////////////////////////////////////////////////////////////////
///
/// \file MessageSystem.cpp
/// 
/// * This file implements both the legacy and modern messaging systems declared 
///   in `MessageSystem.h`.
/// * The legacy `MessageSystem` handles direct event dispatching for backward 
///   compatibility with older engine components.
/// * The modern `MessageBus` manages type-safe event routing, deferred message 
///   queuing, handler registration, and runtime statistics tracking.
/// 
/// -------------------------------
/// HOW TO USE THE MESSAGE SYSTEM
/// -------------------------------
///
/// 1. Register message handlers using `Subscribe()` for immediate handling,
///    or `SubscribeDeferred()` for queued delivery.
/// 2. Send messages instantly using `SendImmediate()` or queue them with 
///    `QueueMessage()` for later processing via `ProcessQueue()`.
/// 3. Use `Clear()`, `PrintStatistics()`, and `ResetStatistics()` to manage 
///    and debug the message system during runtime.
///
/// Example:
/// ---------
/// @code
/// // Subscribe to a message type
/// MessageBus::Get().Subscribe<DamageMessage>(
///     [](const DamageMessage& msg) {
///         std::cout << "Entity " << msg.target 
///                   << " took " << msg.amount << " damage." << std::endl;
///     });
///
/// // Send the message immediately
/// DamageMessage dmg(targetEntity, 10.0f, "fire");
/// MessageBus::Get().SendImmediate(dmg);
///
/// // Or queue the message for deferred delivery
/// MessageBus::Get().QueueMessage(DamageMessage(targetEntity, 5.0f, "ice"));
/// MessageBus::Get().ProcessQueue(); // Deliver queued messages
/// @endcode
///
/// Authors: Belinda Chua Jia Xuan   
/// Copyright 2025, Digipen Institute of Technology
///////////////////////////////////////////////////////////////////////////////



#include "pch.h"
#include "MessageSystem.h"

namespace Framework {

    // Initialize static instances
    MessageBus* MessageBus::instance = nullptr;
    MessageSystem* MessageSystem::instance = nullptr;

    MessageBus& MessageBus::Get() {
        if (!instance) {
            instance = new MessageBus();
        }
        return *instance;
    }

    void MessageBus::Destroy() {
        delete instance;
        instance = nullptr;
    }

    void MessageBus::ProcessQueue() {
        while (!messageQueue.empty()) {
            auto msg = messageQueue.front();
            messageQueue.pop();

            auto typeIndex = msg->GetTypeIndex();

            // Update statistics
            totalMessagesSent++;
            messageCountByType[typeIndex]++;

            // Call all deferred handlers
            auto it = deferredHandlers.find(typeIndex);
            if (it != deferredHandlers.end()) {
                for (auto& handler : it->second) {
                    handler(*msg);
                }
            }
        }
    }

    void MessageBus::Clear() {
        ClearHandlers();
        ClearQueue();
        ResetStatistics();
        std::cout << "[MessageBus] Cleared all data" << std::endl;
    }

    void MessageBus::ClearHandlers() {
        immediateHandlers.clear();
        deferredHandlers.clear();
        std::cout << "[MessageBus] All handlers cleared" << std::endl;
    }

    void MessageBus::ClearQueue() {
        while (!messageQueue.empty()) {
            messageQueue.pop();
        }
        std::cout << "[MessageBus] Message queue cleared" << std::endl;
    }

    void MessageBus::PrintStatistics() const {
        std::cout << "\n=== Message Bus Statistics ===" << std::endl;
        std::cout << "Total messages sent: " << totalMessagesSent << std::endl;

        if (!messageCountByType.empty()) {
            std::cout << "\nMessage counts by type:" << std::endl;
            for (const auto& pair : messageCountByType) {
                std::cout << "  " << pair.first.name() << ": " << pair.second << std::endl;
            }
        }

        std::cout << "\nHandlers registered:" << std::endl;
        std::cout << "  Immediate: " << immediateHandlers.size() << " types" << std::endl;
        std::cout << "  Deferred: " << deferredHandlers.size() << " types" << std::endl;

        size_t totalImmediateHandlers = 0;
        for (const auto& pair : immediateHandlers) {
            totalImmediateHandlers += pair.second.size();
        }
        std::cout << "  Total immediate handlers: " << totalImmediateHandlers << std::endl;

        size_t totalDeferredHandlers = 0;
        for (const auto& pair : deferredHandlers) {
            totalDeferredHandlers += pair.second.size();
        }
        std::cout << "  Total deferred handlers: " << totalDeferredHandlers << std::endl;

        std::cout << "\nMessages in queue: " << messageQueue.size() << std::endl;
        std::cout << "==============================\n" << std::endl;
    }

    void MessageBus::ResetStatistics() {
        totalMessagesSent = 0;
        messageCountByType.clear();
        std::cout << "[MessageBus] Statistics reset" << std::endl;
    }

    size_t MessageBus::GetMessageCount(std::type_index type) const {
        auto it = messageCountByType.find(type);
        return (it != messageCountByType.end()) ? it->second : 0;
    }

    size_t MessageBus::GetHandlerCount(std::type_index type, bool immediate) const {
        const auto& handlers = immediate ? immediateHandlers : deferredHandlers;
        auto it = handlers.find(type);
        return (it != handlers.end()) ? it->second.size() : 0;
    }

} // namespace Framework