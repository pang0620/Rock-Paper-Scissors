#include "check_dependency.h"
#include <iostream>
#include <cstdlib> // For system()
#include <string>
#include <sys/stat.h> // For stat()

// Helper function to check if a directory exists
bool directoryExists(const char* path) {
    struct stat info;
    if (stat(path, &info) != 0) {
        return false;
    }
    return (info.st_mode & S_IFDIR) != 0;
}


bool checkAndInstallDependencies() {
    const char* venv_path = "venv_test";
    const char* venv_python = "venv_test/bin/python";
    const char* venv_pip = "venv_test/bin/pip";

    // 1. Check if venv_test directory exists
    if (!directoryExists(venv_path)) {
        std::cerr << "C++: Virtual environment 'venv_test' not found. Creating it..." << std::endl;
        // Use system's python3 to create the venv
        int venv_create_result = system("python3 -m venv venv_test");
        if (venv_create_result != 0) {
            std::cerr << "C++: Failed to create virtual environment. Please ensure python3 is installed and accessible." << std::endl;
            return false;
        }
        std::cerr << "C++: Virtual environment created successfully." << std::endl;
    }

    // Command to check for dependencies. Suppress output, we only need the exit code.
    std::string check_cmd_str = std::string(venv_python) + " -c \"import pkg_resources; pkg_resources.require(open('requirements.txt').read().splitlines())\" > /dev/null 2>&1";
    const char* check_cmd = check_cmd_str.c_str();

    std::cerr << "C++: Checking Python dependencies in venv..." << std::endl;
    if (system(check_cmd) == 0) {
        std::cerr << "C++: All dependencies are satisfied." << std::endl << std::endl;
        return true;
    }

    // Dependencies are missing, installing automatically.
    std::cerr << "\nC++: Python dependencies are missing. Installing automatically into venv..." << std::endl;
    std::cerr << "C++: This might take a few minutes." << std::endl;
    
    // Command to install dependencies. Use venv's pip.
    std::string install_cmd_str = std::string(venv_pip) + " install -r requirements.txt";
    const char* install_cmd = install_cmd_str.c_str();
    int install_result = system(install_cmd);

    if (install_result != 0) {
        std::cerr << "\nC++: Automatic installation failed." << std::endl;
        std::cerr << "C++: Please try running '" << venv_pip << " install -r requirements.txt' manually." << std::endl;
        return false;
    }

    std::cerr << "\nC++: Installation complete. Re-checking dependencies..." << std::endl;
    if (system(check_cmd) == 0) {
        std::cerr << "C++: All dependencies are now satisfied." << std::endl << std::endl;
        return true;
    } else {
        std::cerr << "C++: Dependency check still failed after installation." << std::endl;
        return false;
    }
}
