#include <iostream>

#include <stdlib.h>

#include "imapserver.hpp"
#include "imapsessionfactory.hpp"

#define PORT 143

int main() {
    uint32_t bind_address = INADDR_ANY;
    short port = PORT;
    ImapSessionFactory factory;

    Namespace::runtime_init();

    ImapServer *server = new ImapServer(bind_address, port, &factory, "husky.brokersys.com");
    
    server->Run();
    std::cout << "Hit q and return to exit" << std::endl;
    char response;
    std::cin >> response;
    server->Shutdown();
    delete server;
   
    return EXIT_SUCCESS;
}
