#include <list>
#include <string>

#include <clotho/testserver.hpp>
#include <clotho/socket.hpp>

#include "locktestmaster.hpp"
#include "imapsession.hpp"
#include "mailstorelocktest.hpp"

/*
 * Reimplement these classes:  imapmaster, internetserver
 */

typedef std::list<std::string *> StringList;

class TestSocket : public Socket {
public:
  TestSocket(void);
  virtual ~TestSocket();
  virtual ssize_t receive(uint8_t *buffer, size_t size);
  virtual ssize_t send(const uint8_t *data, size_t length);
  void in(const std::string &s);
  const std::string *out(void);
  void reset(void);

private:
  StringList *m_inputs;
  StringList *m_outputs;
  size_t m_inputOffset;
  StringList::iterator m_pIn;
  StringList::iterator m_pOut;
};

TestSocket::TestSocket() : Socket(0, 0, 16384) {
  m_inputs = NULL;
  m_outputs = NULL;
}

TestSocket::~TestSocket() {
}

ssize_t TestSocket::receive(uint8_t *buffer, size_t size) {
  ssize_t result = -1;
  if ((NULL != m_inputs) && (m_inputs->end() != m_pIn)) {
    strcpy((char *)buffer, (*m_pIn)->c_str());
    result = (*m_pIn)->length();
    m_pIn++;
  }

  return result;
}

ssize_t TestSocket::send(const uint8_t *data, size_t length) {
  if (NULL == m_outputs) {
    m_outputs = new StringList;
  }

  m_outputs->push_back(new std::string((char *)data, length));
  std::cout << data << std::endl;
  m_pOut = m_outputs->begin();

  return length;
}

void TestSocket::in(const std::string &s) {
  if (NULL == m_inputs) {
    m_inputs = new StringList;
  }
  m_inputs->push_back(new std::string(s));
  m_pIn = m_inputs->begin();
}

const std::string *TestSocket::out(void) {
  std::string *result = NULL;
  if ((NULL != m_outputs) && (m_outputs->end() != m_pOut)) {
    result = *m_pOut;
    m_pOut++;
  }
  return result;
}

void TestSocket::reset(void) {
  delete m_inputs;
  delete m_outputs;
  m_inputs = NULL;
  m_outputs = NULL;
}

int main() {
  LockTestMaster master("localhost", 60, 1800, 900, 5, 12, 5);
  TestServer server(&master);
  TestSocket *s = new TestSocket();

  g_lockState.retryCount(1000);
  s->in("1 login foo bar\r\n");
  s->in("2 create inbox\r\n");
  s->in("3 logout\r\n");
  server.test(s);
  std::cout << "Has " << g_lockState.retryCount() << " retries left" << std::endl;

  g_lockState.retryCount(0);
  s->reset();
  s->in("1 login foo bar\r\n");
  s->in("2 create inbox\r\n");
  s->in("3 logout\r\n");
  server.test(s);
  std::cout << "Has " << g_lockState.retryCount() << " retries left" << std::endl;
  return 0;
}
