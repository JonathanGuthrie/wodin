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

typedef struct {
  std::string s;
  int retry_count;
} commands;

typedef std::list<commands> StringList;

class TestSocket : public Socket {
public:
  TestSocket(void);
  virtual ~TestSocket();
  virtual ssize_t receive(uint8_t *buffer, size_t size);
  virtual ssize_t send(const uint8_t *data, size_t length);
  void in(const std::string &s, int count);
  const std::string *out(void);
  int retry_out(void);
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
    strcpy((char *)buffer, (*m_pIn).s.c_str());
    result = (*m_pIn).s.length();
    g_lockState.retryCount((*m_pIn).retry_count);
    m_pIn++;
  }

  return result;
}

ssize_t TestSocket::send(const uint8_t *data, size_t length) {
  if (NULL == m_outputs) {
    m_outputs = new StringList;
  }

  commands command = { std::string((char *)data, length), g_lockState.retryCount() };
  m_outputs->push_back(command);
  // m_outputs->push_back(new std::string((char *)data, length));
  // std::cout << "retries: " << g_lockState.retryCount() << "=+=+=+=" << data << std::endl;
  m_pOut = m_outputs->begin();

  return length;
}

void TestSocket::in(const std::string &s, int count) {
  if (NULL == m_inputs) {
    m_inputs = new StringList;
  }
  commands command = { s, count };
  m_inputs->push_back(command);
  m_pIn = m_inputs->begin();
}

const std::string *TestSocket::out(void) {
  std::string *result = NULL;
  if ((NULL != m_outputs) && (m_outputs->end() != m_pOut)) {
    result = new std::string ((*m_pOut).s);
    m_pOut++;
  }
  return result;
}

int TestSocket::retry_out(void) {
  if ((NULL != m_outputs) && (m_outputs->end() != m_pOut)) {
    return (*m_pOut).retry_count;
  }
  return -999;
}

void TestSocket::reset(void) {
  delete m_inputs;
  delete m_outputs;
  m_inputs = NULL;
  m_outputs = NULL;
}

bool test(TestServer &server, TestSocket *s, const std::string commands[],
	  int command_count, const std::string &match, int test_step,
	  int response, int initial_retry_count, int final_retry_count) {
  bool result = true;
  const std::string *o;

  s->reset();
  for (int i=0; i<command_count; ++i) {
    if (i == test_step) {
      s->in(commands[i], initial_retry_count);
    }
    else {
      s->in(commands[i], 0);
    }
  }
  server.test(s);
  int retries;
  for (int i=0; i< response; ++i) {
    retries = s->retry_out();
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
  if (final_retry_count != retries) {
    std:: cout << "The retry count is " << retries << " and I was expecting " << final_retry_count << std::endl;
    result = false;
  }
  return result;
}

typedef struct {
  const std::string commands[10];
  int command_count;
  const std::string match;
  int test_step;
  int response_number;
  int initial_retry_count;
  int final_retry_count;
  const std::string test_name;
} test_descriptor;

test_descriptor descriptors[] = {
#if 0
#endif // 0
  {{
    "1 login foo bar\r\n",
    "2 create inbox\r\n",
    "3 logout\r\n"
    }, 3, "2 NO create Locking Error:  Too Many Retries\r\n", 1, 3, 1000, 987,
   "Creation failure"},
  {{
    "1 login foo bar\r\n",
    "2 create inbox\r\n",
    "3 logout\r\n"
    }, 3, "2 OK create Completed\r\n", 1, 3, 0, -1,
   "Successful creation"},
  {{
      "1 login foo bar\r\n",
      "2 delete inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 NO delete Locking Error:  Too Many Retries\r\n", 1, 3, 1000, 987,
   "Deletion failure"},
  {{
      "1 login foo bar\r\n",
      "2 delete inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 OK delete Completed\r\n", 1, 3, 0, -1,
   "Successful deletion"},
  {{
      "1 login foo bar\r\n",
      "2 rename inbox outbox\r\n",
      "3 logout\r\n"
    }, 3, "2 NO rename Locking Error:  Too Many Retries\r\n", 1, 3, 1000, 987,
   "Renaming failure"},
  {{
      "1 login foo bar\r\n",
      "2 rename inbox outbox\r\n",
      "3 logout\r\n"
    }, 3, "2 OK rename Completed\r\n", 1, 3, 0, -1,
   "Successful renaming"},
  {{
      "1 login foo bar\r\n",
      "2 subscribe inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 NO subscribe Locking Error:  Too Many Retries\r\n", 1, 3, 1000, 987,
   "Subscribing failure"},
  {{
      "1 login foo bar\r\n",
      "2 subscribe inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 OK subscribe Completed\r\n", 1, 3, 0, -1,
   "Successful subscribing"},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 NO select Locking Error:  Too Many Retries\r\n", 1, 3, 1000, 987,
   "Selecting failure"},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 OK [READ-WRITE] select Completed\r\n", 1, 4, 0, -1,
   "Successful selecting"},
  {{
      "1 login foo bar\r\n",
      "2 examine inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 NO examine Locking Error:  Too Many Retries\r\n", 1, 3, 1000, 987,
   "Examining failure"},
  {{
      "1 login foo bar\r\n",
      "2 examine inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 OK [READ-ONLY] examine Completed\r\n", 1, 4, 0, -1,
   "Successful examining"},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 close\r\n",
      "4 close\r\n",
      "5 logout\r\n"
    }, 5, "3 NO close Locking Error:  Too Many Retries\r\n", 2, 5, 1000, 987,
   "Closing failure"},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 close\r\n",
      "4 logout\r\n"
    }, 4, "3 OK close Completed\r\n", 2, 5, 0, -1,
   "Successful closing"},
};

int main() {
  LockTestMaster master("localhost", 60, 1800, 900, 5, 12, 5);
  TestServer server(&master);
  TestSocket *s = new TestSocket();
  int pass_count = 0;
  int fail_count = 0;

  for (int i=0; i<(sizeof(descriptors)/sizeof(test_descriptor)); ++i) {
    bool result = test(server, s, descriptors[i].commands, descriptors[i].command_count, descriptors[i].match, descriptors[i].test_step, descriptors[i].response_number, descriptors[i].initial_retry_count, descriptors[i].final_retry_count);
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
