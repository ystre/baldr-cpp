#include <iostream>

int main(int argc, char** argv) {
    std::cout << "Hello from the mini CMake-based project!\n";
#ifdef BALDR_TEST_DEFINE
    std::cout << "BALDR_TEST_DEFINE=" << BALDR_TEST_DEFINE << "\n";
#endif
    for (int i = 1; i < argc; ++i) {
        std::cout << "arg[" << i << "]=" << argv[i] << "\n";
    }
    return 0;
}
