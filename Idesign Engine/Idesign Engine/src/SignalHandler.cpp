/**
 * @file SignalHandler.cpp
 * @brief Checks for signala sent by the system if there were any faults (eg. Segfauult).
 *
 * Prints out the backtrace for tracking of errors, from HU_Backtrace, and can allow logging of errors.
 *
 * Author: Jarren (100%)
*/

#include "pch.h"
#include "SignalHandler.h"
#include <iostream>
#include <csignal>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
typedef DWORD pid_t; // On Windows, use DWORD for pid_t
#define getpid() GetCurrentProcessId()
#else
#include <unistd.h> // On Linux, use unistd for getpid
#endif

#include "backward.hpp"

/*
 * @brief Logs backtrace here
 */
#include <iostream>
#include <fstream>
#include <filesystem> // For creating directories

void logBackTrace() {
    using namespace backward;
    TraceResolver thisIsAWorkaround;
    StackTrace stackTrace;
    stackTrace.load_here(32); // Load up to 32 frames

    Printer printer;
    printer.print(stackTrace, std::cerr); // Print to std::cerr

    // Ask the user if they want to log the crash report
    // std::cout<< "\nDo you want to log the crash report? (y/n): ";
    char response;
    std::cin >> response;

    // Handle user input
    if (response == 'y' || response == 'Y') {
        // Check if the "error_log" directory exists, and create it if not
        std::filesystem::path logDir = "error_log";
        if (!std::filesystem::exists(logDir)) {
            std::filesystem::create_directory(logDir);
        }

        // Create and open the log file
        std::ofstream logFile(logDir / "crash_report.txt", std::ios::trunc);
        if (logFile.is_open()) {
            printer.print(stackTrace, logFile); // Save stack trace to a file
            logFile.close();
            // std::cout<< "Crash report saved to 'error_log/crash_report.txt'.\n";
        }
        else {
            std::cerr << "Failed to open log file.\n";
        }
    }
    else {
        // std::cout<< "Crash report not saved. Exiting...\n";
        std::exit(0); // Close the program or return if you want to return to a caller
    }
}


/*
 * @brief Based on the type of severity and what kind of fault it is, logs the backtrace and exits after
 *
 * @param sig: Signal number
 */
void SignalHandler(int sig) {
    std::string errorMessage;

    std::cerr << "Error: signal " << signal << std::endl;
    logBackTrace();
    exit(sig);
}

void SetupSignalHandlers() {
    std::signal(SIGFPE, SignalHandler);  // Handle floating-point exceptions
    std::signal(SIGSEGV, SignalHandler); // Handle segmentation faults
    std::signal(SIGABRT, SignalHandler); // Handle abort signals
    
    std::signal(SIGILL, SignalHandler);  // Handle illegal instructions
    std::signal(SIGTERM, SignalHandler); // Handle termination requests
}
