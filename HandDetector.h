#ifndef HAND_DETECTOR_H
#define HAND_DETECTOR_H

#include <string>
#include <stdexcept>

class HandDetector {
public:
    HandDetector();
    std::string detect();

private:
    // Helper function to execute a shell command and capture its output
    std::string exec(const char* cmd);
};

#endif // HAND_DETECTOR_H
