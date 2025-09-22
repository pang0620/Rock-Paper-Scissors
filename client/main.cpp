#include <iostream>
#include <string>
#include <limits> // Required for std::numeric_limits
#include "HandDetector.h" // Include our new HandDetector class

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <video_source_url>" << std::endl;
        return 1;
    }

    HandDetector detector; // Create an instance of our hand detector
    std::string video_source = argv[1];

    std::cout << "C++: Performing single hand gesture detection on " << video_source << std::endl;

    try {
        // Call detect with the specified video source
        std::string gesture = detector.detect(video_source);
        std::cout << "C++ | Detected Gesture: [" << gesture << "]" << std::endl;
    } catch (const std::runtime_error& e) {
        std::cerr << "C++ | Error during detection: " << e.what() << std::endl;
        return 1; // Return an error code
    }

    return 0;
}
