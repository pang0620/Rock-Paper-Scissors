#include <iostream>
#include <string>
#include <limits> // Required for std::numeric_limits
#include "HandDetector.h" // Include our new HandDetector class

int main() {
    HandDetector detector; // Create an instance of our hand detector

    std::cout << "C++: Hand Gesture Detection Demo" << std::endl;
    std::cout << "C++: Press Enter to detect a hand gesture, or type 'q' and Enter to quit." << std::endl;

    // TODO where dependency check???

    std::string input_line;
    while (true) {
        // TODO: not here, get Input at python script?
        //std::cout << "\nC++: Waiting for input... ";
        //std::getline(std::cin, input_line);

        if (input_line == "q" || input_line == "Q") {
            std::cout << "C++: Exiting demo." << std::endl;
            break;
        }

        try {
            std::string gesture = detector.detect();
            std::cout << "C++ | Detected Gesture: [" << gesture << "]" << std::endl;
        } catch (const std::runtime_error& e) {
            std::cerr << "C++ | Error during detection: " << e.what() << std::endl;
            // Depending on the error, you might want to exit or continue
        }
    }

    return 0;
}
