
///////////////////////////////////////////////////////////////////////////////
//
//	\file InputHandler.cpp
//	
//  * This system uses GLFW callbacks to capture key presses, mouse clicks, and 
//  * cursor movement.It provides functions to check the state of input events,
//  * such as whether a key or mouse button is pressed, held, or released.
//	
//	Authors: Joshua Sim Yue Chen (joshuayuechen.sim@digipen.edu)
//	Copyright 2010, Digipen Institute of Technology
//
///////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "InputHandler.h"
#include <iostream>
#include <EngineState.h>
#include <imgui.h>
#include <Camera.h>

static bool sFirstMouse = true;
static double sLastX = 0.0, sLastY = 0.0;

namespace Framework {

	// Initialize the static instance to nullptr
	InputHandler* InputHandler::instance = nullptr;

	// Private constructor for the singleton
	InputHandler::InputHandler(GLFWwindow* window)
		: window(window), mouseX(0), mouseY(0) {}

	void InputHandler::SetMessageSystem(MessageSystem* msgSystem) {
		messageSystem = msgSystem;
	}

	InputHandler::~InputHandler()
	{
		// Check if an instance exists before trying to delete it
		if (instance)
		{
			delete instance;
			instance = nullptr; // Set to null to prevent dangling pointers
			std::cout << " System: Input Handler Shutdown" << std::endl;
		}
	}

	void InputHandler::Initialize() {
		// Set this InputHandler instance as the user pointer for the GLFW window
		glfwSetWindowUserPointer(window, this);

		// Set GLFW callbacks
		glfwSetKeyCallback(window, KeyCallback);
		glfwSetMouseButtonCallback(window, MouseButtonCallback);
		glfwSetCursorPosCallback(window, CursorPositionCallback);

	}

	/**
* \brief Updates the input states by polling GLFW events and checking for key or mouse inputs.
*
* \param deltaTime The time elapsed since the last update (unused in this function).
*/
	void InputHandler::Update(float deltaTime) {

		(void)deltaTime;

		glfwPollEvents();  // Poll events for GLFW, key pressed and key released

		if (IsKeyPressed(GLFW_KEY_TAB)) {
			isUIFocused = !isUIFocused; // Flip the state

			if (isUIFocused) {
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			}
			else {
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
				sFirstMouse = true;  // avoid first-frame mouse jump
			}
		}

		ImGuiIO& io = ImGui::GetIO();

		// Conditionally enable/disable ImGui input capturing
		// When UI is focused, ImGui behaves normally.
		// When game is focused, we tell ImGui to ignore all inputs.
		if (!isUIFocused) {
			io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
			io.ConfigFlags |= ImGuiConfigFlags_NoKeyboard;
		}
		else {
			io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
			io.ConfigFlags &= ~ImGuiConfigFlags_NoKeyboard;
		}

		if (!isUIFocused) {
			// polling for key held example
			if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
				//std::cout << "W key is being held down (move forward)" << std::endl;
			}

			if (IsKeyPressed(GLFW_KEY_F1))
			{
				engineState.SetPlay(!engineState.IsPlay());

			}

			if (IsKeyPressed(GLFW_KEY_F4))
			{ // Set Engine state to debug mode (Displays bounding box, gizmos, system info , etc)p
				engineState.SetInDebugMode(!engineState.IsInDebugMode());
			}


			// BGM Audio
			if (IsKeyPressed(GLFW_KEY_5))
			{
				//GlobalAudio.UE_PlaySound("piano");
			}

			if (IsKeyPressed(GLFW_KEY_6))
			{
				//GlobalAudio.UE_PauseSound("piano");
			}

			// Sound Effects Audio
			if (IsKeyPressed(GLFW_KEY_7))
			{
				//GlobalAudio.UE_PlaySound("buttonclick");
			}

			if (IsKeyPressed(GLFW_KEY_8))
			{
				//GlobalAudio.UE_PlaySound("selectbutton");
			}

			if (IsKeyPressed(GLFW_KEY_9))
			{
				//GlobalAudio.UE_PlaySound("previousbutton");
			}

			// Audio Volume Control
			if (IsKeyPressed(GLFW_KEY_MINUS))
			{
				//GlobalAudio.DecrementGroupVol("BackgroundMusic");
				//GlobalAudio.DecrementGroupVol("SoundEffects");
			}

			if (IsKeyPressed(GLFW_KEY_EQUAL))
			{
				//GlobalAudio.IncrementGroupVol("BackgroundMusic");
				//GlobalAudio.IncrementGroupVol("SoundEffects");
			}


			if (IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
				//std::cout << "Left mouse button clicked" << std::endl;
				if (messageSystem) {
					MessageMouseClick* msg = new MessageMouseClick(GLFW_MOUSE_BUTTON_LEFT);
					messageSystem->Dispatch(msg);
				}
			}
		}
	}

	void InputHandler::SetCamera(Camera* camera) {
		this->mainCamera = camera;
	}

	/*
	* \brief Checks if a key was pressed (transition from released to pressed state).
	*
	* \param key The GLFW key code to check.
	* \return True if the key was pressed, false otherwise.
	*/
	bool InputHandler::IsKeyPressed(int key)
	{
		// 1. Safely search for the key in the map
		auto it = keyStates.find(key);

		// 2. If the iterator is not at the end, the key was found
		if (it != keyStates.end())
		{
			// 3. Now it's safe to check the value
			return it->second == KEY_PRESSED;
		}

		// 4. If the key was not found, it can't be pressed
		return false;
	}

	/**
  * \brief Checks if a key is being held down.
  *
  * \param key The GLFW key code to check.
  * \return True if the key is held, false otherwise.
  */
	bool InputHandler::IsKeyHeld(int key) {
		return keyStates[key] == KEY_PRESSED || keyStates[key] == KEY_HELD;
	}

	/**
	* \brief Checks if a key was released.
	*
	* \param key The GLFW key code to check.
	* \return True if the key was released, false otherwise.
	*/
	bool InputHandler::IsKeyReleased(int key) {
		return keyStates[key] == KEY_RELEASED;
	}

	/**
	* \brief Checks if a mouse button was pressed (transition from released to pressed state).
	*
	* \param button The GLFW mouse button code to check.
	* \return True if the mouse button was pressed, false otherwise.
	*/
	bool InputHandler::IsMouseButtonPressed(int button) {
		if (mouseButtonStates[button] == KEY_PRESSED) {
			mouseButtonStates[button] = KEY_HELD;  // Mark the button as held to prevent repeated actions
			return true;
		}
		return false;
	}

	/**
	* \brief Checks if a mouse button is being held down.
	*
	* \param button The GLFW mouse button code to check.
	* \return True if the mouse button is held, false otherwise.
	*/
	bool InputHandler::IsMouseButtonHeld(int button) {
		return mouseButtonStates[button] == KEY_HELD || mouseButtonStates[button] == KEY_PRESSED;
	}

	/**
	* \brief Checks if a mouse button was released.
	*
	* \param button The GLFW mouse button code to check.
	* \return True if the mouse button was released, false otherwise.
	*/
	bool InputHandler::IsMouseButtonReleased(int button) {
		return mouseButtonStates[button] == KEY_RELEASED;
	}

	/**
	* \brief Gets the current X position of the mouse cursor.
	*
	* \return The X position of the mouse cursor.
	*/
	double InputHandler::GetMouseX() const {
		return mouseX;
	}

	/**
	* \brief Gets the current Y position of the mouse cursor.
	*
	* \return The Y position of the mouse cursor.
	*/
	double InputHandler::GetMouseY() const {
		return mouseY;
	}

	/**
	* \brief Static function that handles GLFW key input events and updates key states.
	*
	* \param window Pointer to the GLFW window.
	* \param key The GLFW key code.
	* \param scancode The platform-specific scancode of the key.
	* \param action The action performed (press, release, repeat).
	* \param mods Bitfield describing which modifier keys were held.
	*/
	void InputHandler::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
		(void)mods; // warning handling
		(void)scancode; // warning handling

		InputHandler* inputSystem = static_cast<InputHandler*>(glfwGetWindowUserPointer(window));
		if (inputSystem) {
			KeyState currentKeyState{};

			if (action == GLFW_PRESS) {
				currentKeyState = KEY_PRESSED;

			}
			else if (action == GLFW_RELEASE) {
				currentKeyState = KEY_RELEASED;
			}
			else if (action == GLFW_REPEAT) {
				currentKeyState = KEY_HELD;
			}

			 // Store the state of the key in the map
			inputSystem->keyStates[key] = currentKeyState;

			// Dispatch a message for key press/release events
			if (currentKeyState == KEY_PRESSED || currentKeyState == KEY_RELEASED) {
				MessageKeyPressed* message = new MessageKeyPressed(key);
				inputSystem->messageSystem->Dispatch(message);  // Dispatch the message
			}
		}
	}

	/**
	* \brief Static function that handles GLFW mouse button events and updates button states.
	*
	* \param window Pointer to the GLFW window.
	* \param button The GLFW mouse button code.
	* \param action The action performed (press, release).
	* \param mods Bitfield describing which modifier keys were held.
	*/

	void InputHandler::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
		(void)mods; // Suppress unused parameter warning

		InputHandler* inputSystem = static_cast<InputHandler*>(glfwGetWindowUserPointer(window));
		if (inputSystem) {
			KeyState currentMouseState{};

			// Set the current state based on the action
			if (action == GLFW_PRESS) {
				currentMouseState = KEY_PRESSED;
			}
			else if (action == GLFW_RELEASE) {
				currentMouseState = KEY_RELEASED;
			}

			// Store the state of the mouse button in the map
			inputSystem->mouseButtonStates[button] = currentMouseState;  // << was keyStates[button]

			// Dispatch a message for mouse button press/release events
			if (currentMouseState == KEY_PRESSED || currentMouseState == KEY_RELEASED) {
				//MessageMouseClick* message = new MessageMouseClick(button);
				//inputSystem->messageSystem->Dispatch(message);  // Dispatch the message
			}
		}
	}



	/**
	 * \brief Static function that handles GLFW cursor position events and updates the mouse position.
	 *
	 * \param window Pointer to the GLFW window.
	 * \param xpos The new X position of the cursor.
	 * \param ypos The new Y position of the cursor.
	 */
	void InputHandler::CursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
		InputHandler* inputSystem = static_cast<InputHandler*>(glfwGetWindowUserPointer(window));
		if (!inputSystem) return;

		inputSystem->mouseX = xpos;
		inputSystem->mouseY = ypos;

		// Only rotate camera when game has focus (TAB toggled off UI)
		if (!inputSystem->isUIFocused && inputSystem->mainCamera) {
			if (sFirstMouse) { sLastX = xpos; sLastY = ypos; sFirstMouse = false; }
			float xoffset = static_cast<float>(xpos - sLastX);
			float yoffset = static_cast<float>(sLastY - ypos); // y inverted
			sLastX = xpos; sLastY = ypos;

			inputSystem->mainCamera->processMouseMovement(xoffset, yoffset);
		}
	}
}

