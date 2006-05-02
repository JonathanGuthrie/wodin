#include <iostream>

#include <stdlib.h>

#include "imapserver.hpp"

#define PORT 143

int main()
{
    ImapServer *server = new ImapServer(INADDR_ANY, PORT);
    
    server->Run();
    std::cout << "Hit q and return to exit" << std::endl;
    char response;
    std::cin >> response;
    server->Shutdown();
    delete server;
   
    return EXIT_SUCCESS;
}
