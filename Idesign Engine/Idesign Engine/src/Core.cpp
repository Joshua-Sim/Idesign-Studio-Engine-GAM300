///////////////////////////////////////////////////////////////////////////////////////
//
//	Core.cpp
//	Authors: Chris Peters, Joshua Sim, Keegan Lim
// 
// This file defines CoreEngine class that handles the management of systems in Unnamed engine.
// 
//	Copyright 2024, Digipen Institute of Technology
//
///////////////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "Core.h"
#include "EngineState.h"
#include "Debugger.h"
#include "InputHandler.h"
#include <chrono>
#include <thread>
#include <cmath>
#include <algorithm>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#endif
namespace Framework
{
	//A global pointer to the core
	CoreEngine* CORE;

	extern Framework::EngineState engineState;
	/**
 * \brief CoreEngine constructor.
 *
 * Initializes the CoreEngine, sets the initial time to 0, and marks the game as active.
 * Also sets the global CORE pointer to this instance.
 */
	CoreEngine::CoreEngine()
	{
		LastTime = 0;
		GameActive = true;
		CORE = this; //Set the global pointer
#ifdef _WIN32
		//Ask Windows for 1 ms timer resolution so sleep_for() in the frame limiter is accurate
		//(the default granularity is ~15.6 ms, which makes frame times uneven).
		timeBeginPeriod(1);
#endif
	}

	/**
 * \brief CoreEngine destructor.
 *
 * The destructor doesn't perform any actions since no dynamic resources are directly managed.
 */
	CoreEngine::~CoreEngine()
	{
#ifdef _WIN32
		timeEndPeriod(1);
#endif
	}

	/**
	 * \brief Initializes all systems added to the CoreEngine.
	 *
	 * Calls the Initialize() method on each system in the order they were added.
	 */
	void CoreEngine::Initialize()
	{
		for (unsigned i = 0; i < Systems.size(); ++i)
			Systems[i]->Initialize();
	}

	/**
* \brief The main game loop.
*
* This loop continuously runs while the game is active. It calculates the delta time (dt),
* which represents the time passed since the last frame, and updates all systems using this dt.
* It continues until the game is marked as inactive (GameActive is set to false).
*/

    namespace
    {
        using Clock = std::chrono::steady_clock;

        // Largest frame time fed into the simulation. A long hitch (scene load, breakpoint, window drag)
        // would otherwise make the loop run dozens of catch-up steps in one frame.
        constexpr double kMaxFrameTime = 0.1;

        // If a frame took (nearly) a whole number of fixed steps, snap to exactly that many.
        // Without this, a 60 Hz display (16.68 ms frames) against a 60 Hz simulation (16.67 ms steps)
        // alternates between 0 and 2 steps every few seconds, which is visible as a stutter.
        double SnapFrameTime(double frameTime, double fixedDt)
        {
            constexpr double kTolerance = 0.001; // 1 ms
            for (int k = 1; k <= 4; ++k)
            {
                if (std::abs(frameTime - k * fixedDt) < kTolerance)
                    return k * fixedDt;
            }
            return frameTime;
        }

        // Sleeps for most of the remaining time, then spin-waits the last ~1.5 ms for precision.
        void WaitUntil(Clock::time_point target)
        {
            constexpr auto kSpinMargin = std::chrono::microseconds(1500);
            auto now = Clock::now();
            if (target - now > kSpinMargin)
                std::this_thread::sleep_for(target - now - kSpinMargin);
            while (Clock::now() < target)
                std::this_thread::yield();
        }
    }

    void CoreEngine::GameLoop()
    {
        DebugSystem* debugSystem = DebugSystem::GetInstance();

        // Logic runs at a fixed 60 Hz. Rendering runs once per frame, AFTER the logic steps,
        // so every presented frame shows the newest simulation state and a frame that needs
        // 0 or 2 logic steps still renders exactly once.
        const double fixedDt = 1.0 / 60.0;

        double lastTime = glfwGetTime();
        double accumulator = 0.0;

        while (GameActive)
        {
            const auto frameStart = Clock::now();

            // --- Measure frame time ---
            const double currentTime = glfwGetTime();
            const double rawFrameTime = currentTime - lastTime; // real time, used for FPS stats
            lastTime = currentTime;

            const double frameTime = SnapFrameTime((std::min)(rawFrameTime, kMaxFrameTime), fixedDt);
            accumulator += frameTime;

            std::unordered_map<std::string, double> systemTimes;

            auto timedUpdate = [&](ISystem* system, float dt)
            {
                auto start = std::chrono::high_resolution_clock::now();
                system->Update(dt);
                auto end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::milli> elapsed = end - start;
                systemTimes[system->GetName()] += elapsed.count(); // summed over all steps this frame
            };

            // --- Pump OS events ONCE per frame (previously done in InputHandler AND ImguiManager) ---
            glfwPollEvents();

            // --- Fixed timestep: game logic ---
            while (accumulator >= fixedDt)
            {
                const bool playing = engineState.Is(EngineMode::Play);

                for (auto* system : Systems)
                {
                    if (system->GetUpdateStage() != UpdateStage::Fixed) continue;
                    timedUpdate(system, playing ? static_cast<float>(fixedDt) : 0.0f);
                }

                // "Pressed" edges are consumed by this step; demote them to "held".
                if (InputHandler* input = InputHandler::GetInstance())
                    input->EndStep();

                accumulator -= fixedDt;
            }

            // --- Once per frame: rendering / editor UI (includes the buffer swap) ---
            {
                const float frameDt = engineState.Is(EngineMode::Play) ? static_cast<float>(frameTime) : 0.0f;
                for (auto* system : Systems)
                {
                    if (system->GetUpdateStage() != UpdateStage::Frame) continue;
                    timedUpdate(system, frameDt);
                }
            }

            if (debugSystem)
            {
                debugSystem->SetSystemTimes(systemTimes);
                // Pass the real frame time for accurate FPS calculation
                debugSystem->ProcessSystemUpdateTimes(static_cast<float>(rawFrameTime));
            }

            // --- Frame limiter (safety net when VSync is on, the real limiter when it is off) ---
            if (frameRateCap > 0.0)
            {
                WaitUntil(frameStart + std::chrono::duration_cast<Clock::duration>(
                    std::chrono::duration<double>(1.0 / frameRateCap)));
            }
        }
    }

	/**
 * \brief Adds a system to the CoreEngine.
 *
 * Adds a new system to the list of systems managed by the CoreEngine. These systems
 * will be updated every frame in the game loop.
 *
 * \param system Pointer to the system to be added to the CoreEngine.
 */
	void CoreEngine::AddSystem(ISystem* system)
	{
		//Add a system to the core to be updated
		//every frame
		Systems.push_back(system);
	}

	/**
 * \brief Destroys and deallocates all systems.
 *
 * Deletes all systems in reverse order of their addition to minimize
 * any dependency issues between systems. After this function, all systems are removed.
 */
void CoreEngine::DestroySystems()
{
    // Just clear the vector; smart pointers handle the memory cleanup automatically
    Systems.clear();

	// Flush input handler
	delete Framework::InputHandler::instance;
	InputHandler::instance = nullptr;
}

	/**
 * \brief Ends the game loop.
 *
 * Sets the GameActive flag to false, causing the game loop to stop on the next iteration.
 */
	void CoreEngine::EndGameLoop() {
		GameActive = false;
	}
}
