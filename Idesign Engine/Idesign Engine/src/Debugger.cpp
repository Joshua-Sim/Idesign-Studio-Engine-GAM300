///////////////////////////////////////////////////////////////////////////////
//
//	\file Debugger.cpp
//	Defines the DebugSystem class for tracking and displaying system performance.
//
//  Authors: Keegan Lim Beng Yang(l.keeganbengyang@digipen.edu)
//	Copyright 2010, Digipen Institute of Technology
//
///////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "Debugger.h"

namespace Framework {

    int currentFrame;  // Global frame counter

    /********************************************************************
    * DebugSystem: Constructor for the DebugSystem class.               *
    *              Displays a message when the system is created.       *
    ********************************************************************/
    DebugSystem::DebugSystem() {
       // std::cout << "DebugSystem created" << std::endl;
    }

    /********************************************************************
    * ~DebugSystem: Destructor for the DebugSystem class.               *
    *               Displays a message when the system is destroyed.    *
    ********************************************************************/
    DebugSystem::~DebugSystem() {
      //  std::cout << "DebugSystem destroyed" << std::endl;
    }

    /********************************************************************
    * SetSystemTimes: Stores the update times for each system.          *
    *                 - times: A map of system names to update times.   *
    ********************************************************************/
    void DebugSystem::SetSystemTimes(const std::unordered_map<std::string, double>& times) {
        systemTimes = times;
    }

    /********************************************************************
    * GetSystemUpdateTimes: Retrieves the last valid cached percentages.*
    *                       Returns an empty map if no data is cached.  *
    ********************************************************************/
    const std::unordered_map<std::string, double>& DebugSystem::GetSystemUpdateTimes() {
        if (!lastValidPercentages.empty()) {
        }
        return lastValidPercentages;
    }

    /********************************************************************
    * ProcessSystemUpdateTimes: Periodically updates and caches system  *
    *                           performance data every second.          *
    *                           - deltaTime: Time elapsed since last    *
    *                             update.                               *
    ********************************************************************/
    void DebugSystem::ProcessSystemUpdateTimes(float deltaTime) {
        elapsedTime += deltaTime;

        if (elapsedTime >= 1.0f) {
            PrintSystemUpdateTimes();
            elapsedTime = 0.0;
        }

        for (const auto& [checkName, checkFunc] : checks) {
            if (checkStates[checkName]) {
                checkFunc(deltaTime);
            }
        }

        systemTimes.clear();
    }

    /********************************************************************
    * GetInstance: Retrieves the singleton instance of DebugSystem.     *
    *              Prints the instance address for verification.        *
    ********************************************************************/
    DebugSystem* DebugSystem::GetInstance() {
        static DebugSystem instance;
        static bool isPrinted = false;

        if (!isPrinted) {
            std::cout << "DebugSystem::GetInstance - Address: " << &instance << std::endl;
            isPrinted = true;
        }

        return &instance;
    }

    /********************************************************************
    * PrintSystemUpdateTimes: Calculates and caches the performance     *
    *                         percentages for each system.              *
    ********************************************************************/
    void DebugSystem::PrintSystemUpdateTimes() {
        if (systemTimes.empty()) {
        //    std::cout << "Warning: systemTimes map is empty. Skipping cache update." << std::endl;
            lastValidPercentages.clear();
            return;
        }

        double totalUpdateTime = 0.0;

        for (const auto& [systemName, timeTaken] : systemTimes) {
            totalUpdateTime += timeTaken;
        }

        if (totalUpdateTime > tolerance) {
            lastValidPercentages.clear();
            for (const auto& [systemName, timeTaken] : systemTimes) {
                lastValidPercentages[systemName] = (timeTaken / totalUpdateTime) * 100.0;
            }

           // std::cout << "Updated Cached Percentages - Frame: " << currentFrame << std::endl;
            for (const auto& [systemName, percentage] : lastValidPercentages) {
      //          std::cout << systemName << ": " << percentage << "%" << std::endl;
            }
        }
        else {
            //std::cout << "Total update time below tolerance. No data cached." << std::endl;
            lastValidPercentages.clear();
        }
    }

    /********************************************************************
    * GetLastValidPercentages: Returns the cached system percentages.   *
    ********************************************************************/
    const std::unordered_map<std::string, double>& DebugSystem::GetLastValidPercentages() const {
        return lastValidPercentages;
    }

    /********************************************************************
    * AddCheck: Registers a new debug check function.                   *
    *           - checkName: The name of the check.                     *
    *           - checkFunc: The function to execute for the check.     *
    ********************************************************************/
    void DebugSystem::AddCheck(const std::string& checkName, std::function<void(float)> checkFunc) {
        checks.emplace_back(checkName, checkFunc);
        checkStates[checkName] = true;
      //  std::cout << "Added debug check: " << checkName << std::endl;
    }

    /********************************************************************
    * ListChecks: Lists all registered debug checks.                    *
    ********************************************************************/
    void DebugSystem::ListChecks() {
        std::cout << "Registered Debug Checks:" << std::endl;
        for (const auto& [checkName, checkFunc] : checks) {
      //      std::cout << "- " << checkName << std::endl;
        }
    }

    /********************************************************************
    * ToggleCheck: Enables or disables a specific debug check.          *
    *              - checkName: The name of the check.                  *
    *              - 
    : True to enable, false to disable.        *
    ********************************************************************/
    void DebugSystem::ToggleCheck(const std::string& checkName, bool isActive) {
        if (checkStates.find(checkName) != checkStates.end()) {
            checkStates[checkName] = isActive;
  //          std::cout << "Check " << checkName << (isActive ? " enabled" : " disabled") << std::endl;
        }
    }

} // namespace Framework

