#include "server.h"

#include <iostream>

int main(int argc, char * const argv[])
{   
    Server server("0.0.0.0", 10001); 
    
    std::cout << "Server Start" << std::endl;
    
    server.StartServer(); //Start From Here :)
    
    return 0;
}
