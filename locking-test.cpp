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
  bool test_open;
  bool test_close;
} commands;

typedef std::list<commands> StringList;

class TestSocket : public Socket {
public:
  TestSocket(void);
  virtual ~TestSocket();
  virtual ssize_t receive(uint8_t *buffer, size_t size);
  virtual ssize_t send(const uint8_t *data, size_t length);
  void in(const std::string &s, int count, bool open, bool close);
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
    g_lockState.testOpen((*m_pIn).test_open);
    g_lockState.testClose((*m_pIn).test_close);
    g_lockState.retryCount((*m_pIn).retry_count);
    // std::cout << (*m_pIn).s << std::endl;
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

void TestSocket::in(const std::string &s, int count, bool test_open, bool test_close) {
  if (NULL == m_inputs) {
    m_inputs = new StringList;
  }
  commands command = { s, count, test_open, test_close };
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

typedef struct {
  const std::string commands[100];
  int command_count;
  const std::string match;
  int test_step;
  int response_number;
  int initial_retry_count;
  int final_retry_count;
  const std::string test_name;
  bool test_open;
  bool test_close;
} test_descriptor;

bool test(TestServer &server, TestSocket *s, const test_descriptor *descriptor) {
  bool result = true;
  const std::string *o;

  s->reset();
  for (int i=0; i<descriptor->command_count; ++i) {
    if (i == descriptor->test_step) {
      s->in(descriptor->commands[i], descriptor->initial_retry_count, descriptor->test_open, descriptor->test_close);
    }
    else {
      s->in(descriptor->commands[i], 0, false, false);
    }
  }
  server.test(s);
  int retries;
  for (int i=0; i<descriptor->response_number; ++i) {
    retries = s->retry_out();
    o = s->out();
  }
  if ((NULL == o) || (*o != descriptor->match)) {
    std::cout << "Response doesn't match. \"";
    if (NULL != o) {
      std::cout << *o;
    }
    else {
      std::cout << "NULL";
    }
    std::cout << "\" expecting \"" << descriptor->match << "\"" << std::endl;
    result = false;
  }
  if (descriptor->final_retry_count != retries) {
    std:: cout << "The retry count is " << retries << " and I was expecting " << descriptor->final_retry_count << std::endl;
    result = false;
  }
  return result;
}

test_descriptor descriptors[] = {
#if 0
#endif // 0
  {{
    "1 login foo bar\r\n",
    "2 create inbox\r\n",
    "3 logout\r\n"
    }, 3, "2 NO create Locking Error:  Too Many Retries\r\n", 1, 3, 1000, 987,
   "Creation failure",
   true, true},
  {{
    "1 login foo bar\r\n",
    "2 create inbox\r\n",
    "3 logout\r\n"
    }, 3, "2 OK create Completed\r\n", 1, 3, 0, -1,
   "Successful creation",
   true, true},
  {{
      "1 login foo bar\r\n",
      "2 delete inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 NO delete Locking Error:  Too Many Retries\r\n", 1, 3, 1000, 987,
   "Deletion failure",
   true, true},
  {{
      "1 login foo bar\r\n",
      "2 delete inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 OK delete Completed\r\n", 1, 3, 0, -1,
   "Successful deletion",
   true, true},
  {{
      "1 login foo bar\r\n",
      "2 rename inbox outbox\r\n",
      "3 logout\r\n"
    }, 3, "2 NO rename Locking Error:  Too Many Retries\r\n", 1, 3, 1000, 987,
   "Renaming failure",
   true, true},
  {{
      "1 login foo bar\r\n",
      "2 rename inbox outbox\r\n",
      "3 logout\r\n"
    }, 3, "2 OK rename Completed\r\n", 1, 3, 0, -1,
   "Successful renaming",
   true, true},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 OK [READ-WRITE] select Completed\r\n", 1, 4, 0, -1,
   "Successful selecting",
   true, true},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 NO select Locking Error:  Too Many Retries\r\n", 1, 3, 1000, 987,
   "Selecting failure (open)",
   true, false},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 select inbox\r\n",
      "4 logout\r\n"
    }, 4, "2 OK [READ-WRITE] select Completed\r\n", 1, 4, 0, 0,
   "Selecting failure (close)",
   false, true},
  {{
      "1 login foo bar\r\n",
      "2 examine inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 OK [READ-ONLY] examine Completed\r\n", 1, 4, 0, -1,
   "Successful examining",
   true, true},
  {{
      "1 login foo bar\r\n",
      "2 examine inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 NO examine Locking Error:  Too Many Retries\r\n", 1, 3, 1000, 987,
   "Examining failure (open)",
   true, false},
  {{
      "1 login foo bar\r\n",
      "2 examine inbox\r\n",
      "3 examine inbox\r\n",
      "4 logout\r\n"
    }, 4, "2 OK [READ-ONLY] examine Completed\r\n", 1, 4, 0, 0,
   "Examining failure (close)",
   false, true},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 close\r\n",
      "4 close\r\n",
      "5 logout\r\n"
    }, 5, "3 NO close Locking Error:  Too Many Retries\r\n", 2, 5, 1000, 987,
   "Closing failure",
   true, true},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 close\r\n",
      "4 logout\r\n"
    }, 4, "3 OK close Completed\r\n", 2, 5, 0, -1,
   "Successful closing",
   true, true},
  {{
      "1 login foo bar\r\n",
      "2 subscribe inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 NO subscribe Locking Error:  Too Many Retries\r\n", 1, 3, 1000, 987,
   "Subscribing failure",
   true, true},
  {{
      "1 login foo bar\r\n",
      "2 subscribe inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 OK subscribe Completed\r\n", 1, 3, 0, -1,
   "Successful subscribing",
   true, true},
  {{
      "1 login foo bar\r\n",
      "2 append inbox {3}\r\n",
      "foo\r\n\r\n",
      "3 logout\r\n"
    }, 5, "2 NO append Locking Error:  Too Many Retries\r\n", 1, 3, 1000, 987,
   "Appending failure",
   true, true},
  {{
      "1 login foo bar\r\n",
      "2 append inbox {3}\r\n",
      "foo\r\n",
      "10 logout\r\n"
    }, 4, "2 OK append Completed\r\n", 1, 4, 0, -2,  // -2 not -1 because it does two actions that could conceivably lock
   "Successful appending",
   true, true},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 copy 1 foo\r\n",
      "4 logout\r\n"
    }, 4, "3 NO copy Locking Error:  Too Many Retries\r\n", 2, 5, 1000, 987,
   "Copying failure",
   true, true},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 copy 1 foo\r\n",
      "4 logout\r\n"
    }, 4, "3 OK copy Completed\r\n", 2, 5, 0, -3,
   "Successful copying",
   true, true},
#if 0
#endif // 0
};

int main() {
  LockTestMaster master("localhost", 60, 1800, 900, 5, 12, 5);
  TestServer server(&master);
  TestSocket *s = new TestSocket();
  int pass_count = 0;
  int fail_count = 0;

  for (int i=0; i<(sizeof(descriptors)/sizeof(test_descriptor)); ++i) {
    bool result = test(server, s, &descriptors[i]);
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
