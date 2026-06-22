#include <iostream>
#include <unistd.h>

int main() {
    std::cout << "Compile time: " << __DATE__ << " " << __TIME__ << std::endl;
    std::cout << "Process ID: " << getpid() << std::endl;
    return 0;
}
