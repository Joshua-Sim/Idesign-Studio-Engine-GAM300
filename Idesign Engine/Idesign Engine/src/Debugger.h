///////////////////////////////////////////////////////////////////////////////
//
//	\file Debugger.h
//	Defines the DebugSystem class for tracking and debugging system performance.
//
//	Authors: Keegan Lim Beng Yang(l.keeganbengyang@digipen.edu)
//	Copyright 2010, Digipen Institute of Technology
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <unordered_map>
#include <string>
#include <functional>
#include <vector>
#include <iostream>

namespace Framework {

    extern int currentFrame;  // Global frame counter declaration

    /********************************************************************
    * \class DebugSystem
    * The DebugSystem class is responsible for tracking the performance
    * of various systems in the engine, providing tools for debugging,
    * and caching performance data for analysis.
    ********************************************************************/
    class DebugSystem {
    public:
        /********************************************************************
        * DebugSystem: Constructor for the DebugSystem class.                *
        ********************************************************************/
        DebugSystem();

        /********************************************************************
        * ~DebugSystem: Destructor for the DebugSystem class.               *
        ********************************************************************/
        ~DebugSystem();

        /********************************************************************
        * SetSystemTimes: Sets the update times for each system.            *
        *                 - times: A map of system names to their update    *
        *                   times in milliseconds.                          *
        ********************************************************************/
        void SetSystemTimes(const std::unordered_map<std::string, double>& times);

        /********************************************************************
        * GetSystemUpdateTimes: Retrieves the last valid cached percentages.*
        *                       Returns an empty map if no data is cached.  *
        ********************************************************************/
        const std::unordered_map<std::string, double>& GetSystemUpdateTimes();

        /********************************************************************
        * ProcessSystemUpdateTimes: Periodically processes the system times *
        *                           and caches the results. Called every    *
        *                           second based on delta time.             *
        *                           - deltaTime: Time elapsed since the     *
        *                             last update, in seconds.              *
        ********************************************************************/
        void ProcessSystemUpdateTimes(float deltaTime);

        /********************************************************************
        * AddCheck: Registers a new debug check function.                   *
        *           - checkName: The name of the debug check.               *
        *           - checkFunc: A function to execute the debug check.     *
        ********************************************************************/
        void AddCheck(const std::string& checkName, std::function<void(float)> checkFunc);

        /********************************************************************
        * ListChecks: Displays all registered debug checks in the console.  *
        ********************************************************************/
        void ListChecks();

        /********************************************************************
        * ToggleCheck: Enables or disables a specific debug check.          *
        *              - checkName: The name of the debug check.            *
        *              - 
        : Set to true to enable, false to disable. *
        ********************************************************************/
        void ToggleCheck(const std::string& checkName, bool isActive);

        /********************************************************************
        * GetInstance: Retrieves the singleton instance of DebugSystem.     *
        ********************************************************************/
        static DebugSystem* GetInstance();

        /********************************************************************
        * GetLastValidPercentages: Returns the cached performance           *
        *                          percentages of systems.                  *
        ********************************************************************/
        const std::unordered_map<std::string, double>& GetLastValidPercentages() const;

    private:
        /********************************************************************
        * System update time tracking                                      *
        * - systemTimes: A map storing the update times for each system.    *
        * - lastValidPercentages: A map storing the cached performance      *
        *   percentages for each system.                                    *
        * - tolerance: The threshold for caching performance data.          *
        ********************************************************************/
        std::unordered_map<std::string, double> systemTimes;
        std::unordered_map<std::string, double> lastValidPercentages;
        const double tolerance = 1e-120;

        /********************************************************************
        * Debug checks tracking                                             *
        * - checks: A list of registered debug check functions.             *
        * - checkStates: A map tracking the enabled/disabled state of       *
        *   each debug check.                                               *
        ********************************************************************/
        std::vector<std::pair<std::string, std::function<void(float)>>> checks;
        std::unordered_map<std::string, bool> checkStates;

        /********************************************************************
        * elapsedTime: Accumulates delta time for periodic processing.      *
        ********************************************************************/
        double elapsedTime = 0.0;

        /********************************************************************
        * PrintSystemUpdateTimes: Calculates and caches the performance     *
        *                         percentages of systems based on update    *
        *                         times.                                    *
        ********************************************************************/
        void PrintSystemUpdateTimes();
    };

} // namespace Framework
