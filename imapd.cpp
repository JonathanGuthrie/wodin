#include <iostream>

#include <stdlib.h>

#include <clotho/internetserver.hpp>

#include "imapmaster.hpp"
#include "namespace.hpp"

#define PORT 143

int main() {
  uint32_t bind_address = INADDR_ANY;
  short port = PORT;
  ImapMaster master("husky.brokersys.com");

  Namespace::runtime_init();

  InternetServer *server = new InternetServer(bind_address, port, &master, 1);
    
  server->run();
  std::cout << "Hit q and return to exit" << std::endl;
  char response;
  std::cin >> response;
  server->shutdown();
  delete server;
   
  return EXIT_SUCCESS;
}
