///////////////////////////////////////////////////////////////////////////////
//
//	\file MessageSystem.h
//	Defines MessageSystem system.
//
//	Authors: Keegan Lim Beng Yang (l.keeganbengyang@digipen.edu)
//	Copyright 2010, Digipen Institute of Technology
//
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Message.h"
#include "System.h"  // Include ISystem
#include <unordered_map>
#include <vector>
#include <functional>
#include <typeindex>

namespace Framework {

    /********************************************************************
    * MessageSystem: This class handles the messaging system used to    *
    *                dispatch messages/events to various subscribers.   *
    *                Inherits from ISystem to work as part of the ECS   *
    *                architecture.                                      *
    ********************************************************************/
    class MessageSystem : public ISystem {
    public:
        /********************************************************************
        * MessageSystem: Constructor for the MessageSystem class.           *
        ********************************************************************/
        MessageSystem();

        /********************************************************************
        * ~MessageSystem: Destructor for the MessageSystem class.           *
        ********************************************************************/
        ~MessageSystem();

        /********************************************************************
        * Initialize: Initializes the MessageSystem.                        *
        *             Required override from the ISystem base class.        *
        ********************************************************************/
        void Initialize() override;

        /********************************************************************
        * Update: Updates the MessageSystem every frame.                    *
        *         This method is called every frame and allows for any      *
        *         per-frame logic related to the messaging system.          *
        *         Required override from the ISystem base class.            *
        *         - deltaTime: Time elapsed since the last frame.           *
        ********************************************************************/
        void Update(float deltaTime) override;

        /********************************************************************
        * GetName: Returns the name of the system as a string.              *
        *         Required override from the ISystem base class.            *
        ********************************************************************/
        std::string GetName() override;

        /********************************************************************
        * Dispatch: Sends a message/event to all subscribers registered for *
        *           that particular message type.                           *
        *           - msg: Pointer to the message/event to be dispatched.   *
        ********************************************************************/
        void Dispatch(Message* msg);

        /********************************************************************
        * Subscribe: Allows a system or component to subscribe to a         *
        *            specific message/event type. The handler function      *
        *            will be called whenever that message type is dispatched*
        *            - T: The type of the message/event to subscribe to.    *
        *            - handler: The function that will handle the message.  *
        ********************************************************************/
        template <typename T>
        void Subscribe(std::function<void(T*)> handler) {
            auto& handlers = subscribers[typeid(T)];
            handlers.push_back([handler](Message* msg) {
                handler(static_cast<T*>(msg));
                });
        }

        /********************************************************************
        * Unsubscribe: Removes a previously subscribed handler for a        *
        *              specific message/event type.                         *
        *              - T: The type of the message/event to unsubscribe    *
        *              - handler: The function that was handling the message*
        ********************************************************************/
        template <typename T>
        void Unsubscribe(std::function<void(T*)> handler) {
            auto& handlers = subscribers[typeid(T)];
            handlers.erase(std::remove_if(handlers.begin(), handlers.end(), [&](const std::function<void(Message*)>& existingHandler) {
                return existingHandler.target_type() == handler.target_type();
                }), handlers.end());
        }

    private:
        /********************************************************************
        * subscribers: An unordered map that links message/event types to   *
        *              a list of subscriber functions. Each key in the map   *
        *              is a type_index representing a message type, and the  *
        *              value is a vector of functions that handle that type. *
        ********************************************************************/
        std::unordered_map<std::type_index, std::vector<std::function<void(Message*)>>> subscribers;
    };

}  // namespace Framework
