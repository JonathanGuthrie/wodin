#include <iostream>

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
  // std::cout << data << std::endl;
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

bool test(TestServer &server, TestSocket *s, const std::string commands[],
	  int command_count, const std::string &match, int response,
	  int initial_retry_count, int final_retry_count) {
  bool result = true;
  const std::string *o;

  g_lockState.retryCount(initial_retry_count);
  s->reset();
  for (int i=0; i<command_count; ++i) {
    s->in(commands[i]);
  }
  server.test(s);
  for (int i=0; i< response; ++i) {
    o = s->out();
  }
  if ((NULL == o) || (*o != match)) {
    std::cout << "Response doesn't match. \"";
    if (NULL != o) {
      std::cout << *o;
    }
    else {
      std::cout << "NULL";
    }
    std::cout << "\" expecting \"" << match << "\"" << std::endl;
    result = false;
  }
  if (final_retry_count != g_lockState.retryCount()) {
    std:: cout << "The retry count is " << g_lockState.retryCount() << " and I was expecting " << final_retry_count << std::endl;
    result = false;
  }
  return result;
}

typedef struct {
  const std::string commands[10];
  int command_count;
  const std::string match;
  int response_number;
  int initial_retry_count;
  int final_retry_count;
  const std::string test_name;
} test_descriptor;

test_descriptor descriptors[] = {
  {{
    "1 login foo bar\r\n",
    "2 create inbox\r\n",
    "3 logout\r\n"
    }, 3, "2 NO create Locking Error:  Too Many Retries\r\n", 3, 1000, 987,
   "Creation failure"},
  {{
    "1 login foo bar\r\n",
    "2 create inbox\r\n",
    "3 logout\r\n"
    }, 3, "2 OK create Completed\r\n", 3, 0, -1,
   "Successful creation"},
  {{
      "1 login foo bar\r\n",
      "2 delete inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 NO delete Locking Error:  Too Many Retries\r\n", 3, 1000, 987,
   "Deletion failure"},
  {{
      "1 login foo bar\r\n",
      "2 delete inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 OK delete Completed\r\n", 3, 0, -1,
   "Successful deletion"},
  {{
      "1 login foo bar\r\n",
      "2 rename inbox outbox\r\n",
      "3 logout\r\n"
    }, 3, "2 NO rename Locking Error:  Too Many Retries\r\n", 3, 1000, 987,
   "Renaming failure"},
  {{
      "1 login foo bar\r\n",
      "2 rename inbox outbox\r\n",
      "3 logout\r\n"
    }, 3, "2 OK rename Completed\r\n", 3, 0, -1,
   "Successful renaming"},
  {{
      "1 login foo bar\r\n",
      "2 subscribe inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 NO subscribe Locking Error:  Too Many Retries\r\n", 3, 1000, 987,
   "Subscribing failure"},
  {{
      "1 login foo bar\r\n",
      "2 subscribe inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 OK subscribe Completed\r\n", 3, 0, -1,
   "Successful subscribing"},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 NO select Locking Error:  Too Many Retries\r\n", 3, 1000, 987,
   "Selecting failure"},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 OK [READ-WRITE] select Completed\r\n", 4, 0, -2,  // -2 instead of -1 because it also does a close
   "Successful selecting"},
  {{
      "1 login foo bar\r\n",
      "2 examine inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 NO examine Locking Error:  Too Many Retries\r\n", 3, 1000, 987,
   "Examining failure"},
  {{
      "1 login foo bar\r\n",
      "2 examine inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 OK [READ-ONLY] examine Completed\r\n", 4, 0, -2, // -2 instead of -1 b3ecause it also does a close
   "Successful examining"},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 close\r\n",
      "4 close\r\n",
      "5 logout\r\n"
    }, 5, "3 NO close Locking Error:  Too Many Retries\r\n", 5, 1, 4,
   "Selecting failure"},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 close\r\n",
      "4 logout\r\n"
    }, 4, "3 OK close Completed\r\n", 5, 0, -3,  // -3 instead of -1 because it also does a close
   "Successful selecting"},
};

int main() {
  LockTestMaster master("localhost", 60, 1800, 900, 5, 12, 5);
  TestServer server(&master);
  TestSocket *s = new TestSocket();
  int pass_count = 0;
  int fail_count = 0;

  for (int i=0; i<(sizeof(descriptors)/sizeof(test_descriptor)); ++i) {
    bool result = test(server, s, descriptors[i].commands, descriptors[i].command_count, descriptors[i].match, descriptors[i].response_number, descriptors[i].initial_retry_count, descriptors[i].final_retry_count);
    std::cout << descriptors[i].test_name;
    if (result) {
      std::cout << " success" << std::endl;
      ++pass_count;
    }
    else {
      std::cout << " failure" << std::endl;
      ++fail_count;
    }
  }

  return 0;
}
