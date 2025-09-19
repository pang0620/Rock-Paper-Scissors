#include <iostream>
#include <cstdio>
#include <string>
#include <memory>
#include <array>
#include <sys/wait.h> // For checking process exit status
#include "check_dependency.h"

// Function to execute a command and get its output
std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    // Use std::unique_ptr for automatic closing of the pipe
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

int main() {
    // Check and install dependencies if needed.
    if (!checkAndInstallDependencies()) {
        return 1; // Exit if dependencies are not met.
    }

    const char* python_command = "./venv_test/bin/python -u detection.py";

    std::cerr << "C++: Starting Python detection script..." << std::endl;
    std::cerr << "C++: Look at the camera and make a Rock, Paper, or Scissors gesture." << std::endl;

    // Open a pipe to the Python script
    FILE* pipe = popen(python_command, "r");
    if (!pipe) {
        std::cerr << "C++: Error! popen() failed." << std::endl;
        return 1;
    }

    // Buffer to read the output from Python
    std::array<char, 128> buffer;

    // Continuously read from the pipe
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        // Create a string and remove the newline character if it exists
        std::string output = buffer.data();
        output.erase(output.find_last_not_of("\n\r") + 1);
        std::cout << "C++ | Received: [" << output << "]" << std::endl;
    }

    // Close the pipe and get the exit status of the script
    int status = pclose(pipe);
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code == 0) {
            std::cerr << "C++: Python script finished successfully." << std::endl;
        } else {
            std::cerr << "\nC++: Python script exited with error code: " << exit_code << std::endl;
        }
        return exit_code;
    } else {
        std::cerr << "\nC++: Python script terminated abnormally." << std::endl;
        return 1; // Return a non-zero code for abnormal termination
    }
}
