///////////////////////////////////////////////////////////////////////////////
//
//	\file Message.h
//	Defines Message system.
//
//	Authors: Keegan Lim Beng Yang (l.keeganbengyang@digipen.edu)
//	Copyright 2010, Digipen Institute of Technology
//
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include <string>

namespace Framework {

    /********************************************************************
    * Message: Base class for all messages/events that can be dispatched *
    *          in the system. All specific messages will inherit from    *
    *          this class.                                               *
    ********************************************************************/
    class Message {
    public:
        /********************************************************************
        * ~Message: Virtual destructor for the base Message class.          *
        *            Ensures derived class destructors are called properly. *
        ********************************************************************/
        virtual ~Message() = default;

        /********************************************************************
        * GetMessageName: Returns a string representing the message type.   *
        *                 This is useful for debugging or logging purposes. *
        *                 Can be overridden by derived messages.            *
        ********************************************************************/
        virtual std::string GetMessageName() const {
            return "BaseMessage";
        }
    };

    /********************************************************************
    * MessageQuit: Derived class for the "quit game" message/event.      *
    *              This message will signal that the game should quit.   *
    ********************************************************************/
    class MessageQuit : public Message {
    public:
        /********************************************************************
        * GetMessageName: Returns the message type as a string ("MessageQuit") *
        *                 This overrides the base class method.              *
        ********************************************************************/
        std::string GetMessageName() const override {
            return "MessageQuit";
        }
    };

    /********************************************************************
    * MessageKeyPressed: Derived class for key press events.             *
    *                    This message holds information about which     *
    *                    key was pressed.                               *
    ********************************************************************/
    class MessageKeyPressed : public Message {
    public:
        int keyCode;  // The key code that was pressed

        /********************************************************************
        * MessageKeyPressed: Constructor for the key press message.         *
        *                     - key: The code of the key that was pressed.  *
        ********************************************************************/
        MessageKeyPressed(int key) : keyCode(key) {}

        /********************************************************************
        * GetMessageName: Returns the message type as a string ("MessageKeyPressed") *
        *                 This overrides the base class method.             *
        ********************************************************************/
        std::string GetMessageName() const override {
            return "MessageKeyPressed";
        }
    };

    /********************************************************************
    * MessageMouseClick: Derived class for mouse click events.           *
    *                    This message holds information about which     *
    *                    mouse button was clicked.                      *
    ********************************************************************/
    class MessageMouseClick : public Message {
    public:
        int button;  // The mouse button that was clicked

        /********************************************************************
        * MessageMouseClick: Constructor for the mouse click message.       *
        *                     - btn: The code of the mouse button clicked.  *
        ********************************************************************/
        MessageMouseClick(int btn) : button(btn) {}

        /********************************************************************
        * GetMessageName: Returns the message type as a string ("MessageMouseClick") *
        *                 This overrides the base class method.             *
        ********************************************************************/
        std::string GetMessageName() const override {
            return "MessageMouseClick";
        }
    };

    /********************************************************************
    * MessageMouseMove: Derived class for mouse movement events.         *
    *                   This message holds information about the mouse's*
    *                   x and y positions.                              *
    ********************************************************************/
    class MessageMouseMove : public Message {
    public:
        double x, y;  // The x and y positions of the mouse

        /********************************************************************
        * MessageMouseMove: Constructor for the mouse move message.         *
        *                    - xpos: The x position of the mouse.           *
        *                    - ypos: The y position of the mouse.           *
        ********************************************************************/
        MessageMouseMove(double xpos, double ypos) : x(xpos), y(ypos) {}

        /********************************************************************
        * GetMessageName: Returns the message type as a string ("MessageMouseMove") *
        *                 This overrides the base class method.             *
        ********************************************************************/
        std::string GetMessageName() const override {
            return "MessageMouseMove";
        }
    };

}  // namespace Framework
