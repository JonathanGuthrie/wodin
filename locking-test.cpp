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
  uint32_t bitmap;
} commands;

typedef std::list<commands> StringList;

class TestSocket : public Socket {
public:
  TestSocket(void);
  virtual ~TestSocket();
  virtual ssize_t receive(uint8_t *buffer, size_t size);
  virtual ssize_t send(const uint8_t *data, size_t length);
  void in(const std::string &s, int count, uint32_t bitmap);
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
    g_lockState.testControl((*m_pIn).bitmap);
    g_lockState.retryCount((*m_pIn).retry_count);
    // std::cout << (*m_pIn).s << std::endl;
    m_pIn++;
  }
  else {
    g_lockState.testControl(~LockState::Fail);
    g_lockState.retryCount(1000);
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

void TestSocket::in(const std::string &s, int count, uint32_t bitmap) {
  if (NULL == m_inputs) {
    m_inputs = new StringList;
  }
  commands command = { s, count, bitmap };
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
  uint32_t bitmap;
  int orphanCount;
} test_descriptor;

bool test(TestServer &server, Namespace *utilityNamespace, const test_descriptor *descriptor) {
  bool result = true;
  const std::string *o;
  TestSocket *s = new TestSocket();

  s->reset();
  for (int i=0; i<descriptor->command_count; ++i) {
    if (i == descriptor->test_step) {
      s->in(descriptor->commands[i], descriptor->initial_retry_count, descriptor->bitmap);
    }
    else {
      s->in(descriptor->commands[i], 0, 0);
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
  if (0 != (LockState::TestUpdateStats & descriptor->bitmap)) {
    NUMBER_SET nowGone;
    g_lockState.testControl(0);
    utilityNamespace->mailboxUpdateStats(&nowGone);
  }
  if (descriptor->final_retry_count != retries) {
    std:: cout << "The retry count is " << retries << " and I was expecting " << descriptor->final_retry_count << std::endl;
    result = false;
  }
  if (descriptor->orphanCount != utilityNamespace->orphanCount()) {
    std::cout << "The namespace has " << utilityNamespace->orphanCount() << " orphans in it but is expecting " << descriptor->orphanCount << std::endl;
    result = false;
  }
  NUMBER_SET nowGone;
  g_lockState.testControl(0);
  utilityNamespace->mailboxUpdateStats(&nowGone);
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
   LockState::TestOpen | LockState::TestClose, 0},
  {{
    "1 login foo bar\r\n",
    "2 create inbox\r\n",
    "3 logout\r\n"
    }, 3, "2 OK create Completed\r\n", 1, 3, 0, -1,
   "Successful creation",
   LockState::TestOpen | LockState::TestClose, 0},
  {{
      "1 login foo bar\r\n",
      "2 delete inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 NO delete Locking Error:  Too Many Retries\r\n", 1, 3, 1000, 987,
   "Deletion failure",
   LockState::TestOpen | LockState::TestClose, 0},
  {{
      "1 login foo bar\r\n",
      "2 delete inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 OK delete Completed\r\n", 1, 3, 0, -1,
   "Successful deletion",
   LockState::TestOpen | LockState::TestClose, 0},
  {{
      "1 login foo bar\r\n",
      "2 rename inbox outbox\r\n",
      "3 logout\r\n"
    }, 3, "2 NO rename Locking Error:  Too Many Retries\r\n", 1, 3, 1000, 987,
   "Renaming failure",
   LockState::TestOpen | LockState::TestClose, 0},
  {{
      "1 login foo bar\r\n",
      "2 rename inbox outbox\r\n",
      "3 logout\r\n"
    }, 3, "2 OK rename Completed\r\n", 1, 3, 0, -1,
   "Successful renaming",
   LockState::TestOpen | LockState::TestClose, 0},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 OK [READ-WRITE] select Completed\r\n", 1, 4, 0, -1,
   "Successful selecting",
   LockState::TestOpen | LockState::TestClose, 0},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 NO select Locking Error:  Too Many Retries\r\n", 1, 3, 1000, 987,
   "Selecting failure (open)",
   LockState::TestOpen, 0},
  {{
      "1 login foo bar\r\n",
      "2 select foo\r\n",
      "3 select bar\r\n",
      "4 logout\r\n"
    }, 4, "3 OK [READ-WRITE] select Completed\r\n", 2, 6, 1000, 999,
   "Selecting failure (close)",
   LockState::TestClose, 1},
  {{
      "1 login foo bar\r\n",
      "2 select foo\r\n",
      "3 select foo\r\n",
      "4 logout\r\n"
    }, 4, "3 OK [READ-WRITE] select Completed\r\n", 2, 6, 1000, 999,
   "Selecting failure (reopen)",
   LockState::TestClose, 0},
  {{
      "1 login foo bar\r\n",
      "2 examine inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 OK [READ-ONLY] examine Completed\r\n", 1, 4, 0, -1,
   "Successful examining",
   LockState::TestOpen | LockState::TestClose, 0},
  {{
      "1 login foo bar\r\n",
      "2 examine inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 NO examine Locking Error:  Too Many Retries\r\n", 1, 3, 1000, 987,
   "Examining failure (open)",
   LockState::TestOpen, 0},
  {{
      "1 login foo bar\r\n",
      "2 examine foo\r\n",
      "3 examine bar\r\n",
      "4 logout\r\n"
    }, 4, "3 OK [READ-ONLY] examine Completed\r\n", 2, 6, 1000, 999,
   "Examining failure (close)",
   LockState::TestClose, 1},
  {{
      "1 login foo bar\r\n",
      "2 examine foo\r\n",
      "3 examine foo\r\n",
      "4 logout\r\n"
    }, 4, "3 OK [READ-ONLY] examine Completed\r\n", 2, 6, 1000, 999,
   "Examining failure (reopen)",
   LockState::TestClose, 0},
  // close doesn't fail any more.  Instead, it generates orphans
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 close\r\n",
      "4 close\r\n",
      "5 logout\r\n"
    }, 5, "3 NO close Locking Error:  Too Many Retries\r\n", 2, 5, 1000, 987,
   "Closing failure",
   LockState::TestOpen | LockState::TestClose, 0},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 close\r\n",
      "4 logout\r\n"
    }, 4, "3 OK close Completed\r\n", 2, 5, 0, -1,
   "Successful closing",
   LockState::TestOpen | LockState::TestClose, 0},
  {{
      "1 login foo bar\r\n",
      "2 subscribe inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 NO subscribe Locking Error:  Too Many Retries\r\n", 1, 3, 1000, 987,
   "Subscribing failure",
   LockState::TestOpen | LockState::TestClose, 0},
  {{
      "1 login foo bar\r\n",
      "2 subscribe inbox\r\n",
      "3 logout\r\n"
    }, 3, "2 OK subscribe Completed\r\n", 1, 3, 0, -1,
   "Successful subscribing",
   LockState::TestOpen | LockState::TestClose, 0},
  {{
      "1 login foo bar\r\n",
      "2 append inbox {3}\r\n",
      "foo\r\n\r\n",
      "3 logout\r\n"
    }, 5, "2 NO append Locking Error:  Too Many Retries\r\n", 1, 3, 1000, 987,
   "Appending failure",
   LockState::TestOpen | LockState::TestClose, 0},
  {{
      "1 login foo bar\r\n",
      "2 append inbox {3}\r\n",
      "foo\r\n",
      "10 logout\r\n"
    }, 4, "+ Ready for the Message Data\r\n", 1, 3, 0, -1,
   "Successful appending",
   LockState::TestOpen | LockState::TestClose, 0},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 copy 1 foo\r\n",
      "4 logout\r\n"
    }, 4, "3 NO copy Locking Error:  Too Many Retries\r\n", 2, 5, 1000, 987,
   "Copying failure",
   LockState::TestOpen | LockState::TestClose, 0},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 copy 1 foo\r\n",
      "4 logout\r\n"
    }, 4, "3 OK copy Completed\r\n", 2, 5, 0, -2,
   "Successful copying",
   LockState::TestOpen | LockState::TestClose, 0},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 close\r\n",
      "4 logout\r\n"
    }, 4, "3 NO close Locking Error:  Too Many Retries\r\n", 2, 5, 1000, 987,
   "Closing failure",
   0, 0},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 close\r\n",
      "4 logout\r\n"
    }, 4, "3 OK close Completed\r\n", 2, 5, 0, -1,
   "Successful close",
   0, 0},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 logout\r\n",
    }, 3, "* BYE IMAP4rev1 server closing\r\n", 2, 5, 1000, 999,
   "Logout leaves an orphan",
   LockState::TestOpen | LockState::TestClose, 1},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 logout\r\n",
    }, 3, "* BYE IMAP4rev1 server closing\r\n", 2, 5, 1000, 999,
   "Update Stats cleans up orphans",
   LockState::TestOpen | LockState::TestClose | LockState::TestUpdateStats, 0},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 expunge\r\n",
      "4 logout\r\n",
    }, 4, "3 OK expunge Completed\r\n", 2, 5, 0, -1,
   "Expunge success",
   LockState::TestOpen | LockState::TestClose | LockState::TestUpdateStats, 0},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 expunge\r\n",
      "4 logout\r\n",
    }, 4, "3 NO expunge Locking Error:  Too Many Retries\r\n", 2, 5, 1000, 987,
   "Expunge failure",
   LockState::TestOpen | LockState::TestClose | LockState::TestUpdateStats, 0},
  {{
      "1 login foo bar\r\n",
      "2 status bar (uidnext messages)\r\n",
      "3 logout\r\n",
    }, 3, "* STATUS bar (MESSAGES 1 UIDNEXT 2)\r\n", 1, 3, 0, -1,
   "status command success",
   LockState::TestOpen | LockState::TestClose | LockState::TestUpdateStats, 0},
  {{
      "1 login foo bar\r\n",
      "2 status bar (uidnext messages)\r\n",
      "3 logout\r\n",
    }, 3, "2 NO status Locking Error:  Too Many Retries\r\n", 1, 3, 1000, 987,
   "status command failure",
   LockState::TestOpen | LockState::TestClose | LockState::TestUpdateStats, 0},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 noop\r\n",
    }, 3, "2 OK [READ-WRITE] select Completed\r\n", 2, 4, 1000, 0,
   "select and sudden disconnect",
   0, 1},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 search ALL\r\n",
      "4 logout\r\n",
    }, 3, "3 OK search Completed\r\n", 2, 6, 0, -1,
   "search success",
   0, 1},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 search ALL\r\n",
      "4 logout\r\n",
    }, 3, "3 NO search Locking Error:  Too Many Retries\r\n", 2, 5, 1000, 987,
   "search fail",
   0, 1},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 fetch 1 (all)\r\n",
      "4 logout\r\n",
    }, 3, "3 OK fetch Completed\r\n", 2, 14, 0, -1,
   "fetch success",
   0, 1},
  {{
      "1 login foo bar\r\n",
      "2 select inbox\r\n",
      "3 fetch 1 (all)\r\n",
      "4 logout\r\n",
    }, 3, "3 NO fetch Locking Error:  Too Many Retries\r\n", 2, 5, 1000, 987,
   "fetch fail",
   0, 1},
#if 0
#endif // 0
};

int main() {
  LockTestMaster master("localhost", 60, 1800, 900, 5, 12, 5);
  Namespace *utilityNamespace = master.mailStore(NULL);
  TestServer server(&master);
  int pass_count = 0;
  int fail_count = 0;

  for (int i=0; i<(sizeof(descriptors)/sizeof(test_descriptor)); ++i) {
    bool result = test(server, utilityNamespace, &descriptors[i]);
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

  delete utilityNamespace;

  return 0;
}
