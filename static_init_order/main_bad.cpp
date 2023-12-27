// mainSOIF1.cpp

#include <iostream>

extern int staticA;   // (1)
auto staticB = staticA;

int main() {
    
    std::cout << std::endl;
    
    std::cout << "staticB: " << staticB << std::endl;
    
    std::cout << std::endl;
    
}
