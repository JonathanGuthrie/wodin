#include <clotho/testserver.hpp>
#include <clotho/socket.hpp>

#include "locktestmaster.hpp"
#include "imapsession.hpp"
#include "mailstorelocktest.hpp"

/*
 * Reimplement these classes:  imapmaster, internetserver
 */

class TestSocket : public Socket {
public:
  TestSocket(void);
  virtual ~TestSocket();
  virtual ssize_t receive(uint8_t *buffer, size_t size);
  virtual ssize_t send(const uint8_t *data, size_t length);
};

TestSocket::TestSocket() : Socket(0, 0, 16384) {
}

TestSocket::~TestSocket() {
}

ssize_t TestSocket::receive(uint8_t *buffer, size_t size) {
  std::cout << "\"Sending\"1 logout\\r\\n" << std::endl;
  strcpy((char *)buffer, "1 logout\r\n");

  return strlen((char *)buffer);
}

ssize_t TestSocket::send(const uint8_t *data, size_t length) {
  std::cout << data << std::endl;

  return length;
}

int main() {
  LockTestMaster master("localhost", 60, 1800, 900, 5, 12, 5);
  TestServer server(&master);
  TestSocket *s = new TestSocket();

  server.test(s);
  return 0;
}
