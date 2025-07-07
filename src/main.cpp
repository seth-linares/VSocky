#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "Hello, testing!" << std::endl;

    std::cout << "This is the argc: " << argc << std::endl;

    if(argc > 1) {
        std::cout << "This is the second (first real arg) from argv: " << argv[1] << std::endl;
    }

    
}

