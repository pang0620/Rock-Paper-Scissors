#include "check_dependency.h"
#include <iostream>
#include <cstdlib> // For system()

bool checkAndInstallDependencies() {
    // Command to check for dependencies. Suppress output, we only need the exit code.
    const char* check_cmd = "python3 -c \"import pkg_resources; pkg_resources.require(open('requirements.txt').read().splitlines())\" > /dev/null 2>&1";

    std::cerr << "C++: Checking Python dependencies..." << std::endl;
    if (system(check_cmd) == 0) {
        std::cerr << "C++: All dependencies are satisfied." << std::endl << std::endl;
        return true;
    }

    // Dependencies are missing, installing automatically.
    std::cerr << "\nC++: Python dependencies are missing. Installing automatically..." << std::endl;
    std::cerr << "C++: This might take a few minutes." << std::endl;
    
    // Command to install dependencies. Use 'python3 -m pip' for robustness.
    const char* install_cmd = "python3 -m pip install -r requirements.txt";
    int install_result = system(install_cmd);

    if (install_result != 0) {
        std::cerr << "\nC++: Automatic installation failed." << std::endl;
        std::cerr << "C++: Please try running 'python3 -m pip install -r requirements.txt' manually." << std::endl;
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
