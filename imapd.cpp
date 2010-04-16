#include <iostream>

#include <stdlib.h>

#include <libcppserver/internetserver.hpp>

#include "imapmaster.hpp"
#include "namespace.hpp"

#define PORT 143

int main() {
    uint32_t bind_address = INADDR_ANY;
    short port = PORT;
    ImapMaster master("husky.brokersys.com");

    Namespace::runtime_init();

    InternetServer *server = new InternetServer(bind_address, port, &master, 1);
    
    server->Run();
    std::cout << "Hit q and return to exit" << std::endl;
    char response;
    std::cin >> response;
    server->Shutdown();
    delete server;
   
    return EXIT_SUCCESS;
}
