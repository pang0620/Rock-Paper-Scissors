#include "HandDetector.h"
#include "check_dependency.h" // For checkAndInstallDependencies()
#include <iostream>
#include <cstdio>
#include <array>
#include <memory> // For std::unique_unique_ptr
#include <stdexcept> // For std::runtime_error
#include <sys/wait.h> // For WIFEXITED, WEXITSTATUS

// Constructor
HandDetector::HandDetector() {
    // Any initialization needed for the detector
    // Dependency check is now done in detect()
}

// Helper function to execute a shell command and capture its output
std::string HandDetector::exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    int status = pclose(pipe);
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code != 0) {
            std::string error_msg = "Python script exited with non-zero status: " + std::to_string(exit_code);
            // Print stderr from the script if available, though popen("r") only captures stdout
            // For more robust error capture, popen("r") would need to redirect stderr to stdout or use a different IPC.
            throw std::runtime_error(error_msg);
        }
    } else {
        throw std::runtime_error("Python script terminated abnormally.");
    }

    return result;
}


std::string HandDetector::detect() {
    // Check and install dependencies if needed.
    // This ensures dependencies are ready before the Python script is called.
    if (!checkAndInstallDependencies()) {
        std::cerr << "C++: Failed to meet Python dependencies." << std::endl;
        return "ERROR: Dependencies not met";
    }

    const char* python_command = "./venv_test/bin/python -u detection.py";

    std::cerr << "C++: Starting Python detection script for single shot..." << std::endl;

    std::string python_output;
    try {
        python_output = exec(python_command);
    } catch (const std::runtime_error& e) {
        std::cerr << "C++: Error during Python script execution: " << e.what() << std::endl;
        return "ERROR: Python script execution failed";
    }

    // Remove trailing newline characters from the output
    if (!python_output.empty() && python_output.back() == '\n') {
        python_output.pop_back();
    }
    if (!python_output.empty() && python_output.back() == '\r') {
        python_output.pop_back();
    }

    std::cerr << "C++: Python script finished." << std::endl;
    return python_output;
}
