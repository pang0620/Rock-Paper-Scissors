#ifndef CHECK_DEPENDENCY_H
#define CHECK_DEPENDENCY_H

// Checks if Python dependencies from requirements.txt are met.
// If not, it prompts the user to install them automatically.
// Returns true if dependencies are satisfied (either initially or after installation),
// false otherwise.
bool checkAndInstallDependencies();

#endif // CHECK_DEPENDENCY_H
