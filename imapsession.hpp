/*
 * Copyright 2013 Jonathan R. Guthrie
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if !defined(_IMAPSESSION_HPP_INCLUDED_)
#define _IMAPSESSION_HPP_INCLUDED_

#include <map>
#include <string>

#include <stdint.h>

#include <clotho/insensitive.hpp>
#include <clotho/internetsession.hpp>
#include <clotho/server.hpp>
#include <clotho/sessiondriver.hpp>

#include "imapuser.hpp"
#include "namespace.hpp"
#include "sasl.hpp"
#include "mailmessage.hpp"

class ImapMaster;
class SessionDriver;
class MailMessage;

enum ImapState {
  ImapNotAuthenticated = 0,
  ImapAuthenticated,
  ImapSelected,
  ImapLogoff
};

enum ImapStringState {
  ImapStringGood,
  ImapStringBad,
  ImapStringPending
};

typedef struct {
  uint8_t* data;
  size_t dataLen;
  size_t parsingAt;
} INPUT_DATA_STRUCT;

/*--------------------------------------------------------------------------------------*/
/* Imap											*/
/*--------------------------------------------------------------------------------------*/

class ImapSession;
class ImapHandler;

class ParseBuffer {
private:
  uint8_t  *m_parseBuffer;
  uint32_t m_parseBuffLen;
  uint32_t m_parsePointer; 			// This points past the end o
  uint32_t m_literalLength;
  uint32_t m_argumentPtr;
  uint32_t m_argument2Ptr;
  uint32_t m_argument3Ptr;
  uint32_t m_commandPtr;
  uint32_t m_executePtr;

public:
  ParseBuffer(size_t initial_size);
  ~ParseBuffer();

  uint32_t literalLength() const { return m_literalLength; }
  void literalLength(uint32_t length) { m_literalLength = length; }
  const char *arguments() const;
  void argumentsHere();
  const char *argument2() const;
  void argument2Here();
  const char *argument3() const;
  void argument3Here();
  const char *tag() const { return (char *) m_parseBuffer; }
  const char *command() const { return (char *) &m_parseBuffer[m_commandPtr]; }
  void commandHere() { m_commandPtr = m_parsePointer; }
  const char *parsingAt() const { return (char *)&m_parseBuffer[m_parsePointer]; }
  char parseChar(size_t tokenPointer = 0) const { return (char)m_parseBuffer[tokenPointer]; }
  char *parseStr(size_t tokenPointer = 0) const { return (char *)&m_parseBuffer[tokenPointer]; }
  size_t parsePointer() const { return m_parsePointer; }
  // This appends data to the Parse Buffer, extending the parse buffer as necessary.
  void addToParseBuffer(INPUT_DATA_STRUCT &input, size_t length, bool bNulTerminate = true);
  uint32_t addLiteralToParseBuffer(INPUT_DATA_STRUCT &input);
  void atom(INPUT_DATA_STRUCT &input); 
  enum ImapStringState astring(INPUT_DATA_STRUCT &input, bool makeUppercase, const char *additionalChars);
  size_t readEmailFlags(INPUT_DATA_STRUCT &input, bool &okay);
  static void buildSymbolTable(void);
};

typedef enum {
  IMAP_OK,
  IMAP_NO,
  IMAP_BAD,
  IMAP_NOTDONE,
  IMAP_NO_WITH_PAUSE,
  IMAP_MBOX_ERROR,
  IMAP_IN_LITERAL,
  IMAP_TRY_AGAIN
} IMAP_RESULTS;

#define MAX_RESPONSE_STRING_LENGTH	80
#define IMAP_BUFFER_LEN			8192

typedef struct {
  bool levels[ImapLogoff+1];
  bool sendUpdatedStatus;
  ImapHandler  *(*handler)(ImapSession *, INPUT_DATA_STRUCT &input);
} symbol;

typedef std::map<insensitiveString, symbol> IMAPSYMBOLS;

class ImapSession : public InternetSession {
public:
  ImapSession(ImapMaster *master, SessionDriver *driver, Server *server);
  virtual ~ImapSession();
  static void buildSymbolTables(void);
  virtual void receiveData(uint8_t* pData, size_t dwDataLen );

  void asynchronousEvent(void);
  ImapMaster *master(void) const { return m_master; }
  Server *server(void) const { return m_server; }
  SessionDriver *driver(void) const { return m_driver; };
  time_t lastCommandTime() const { return m_lastCommandTime; }
  ImapState state(void) const { return m_state; }
  const ImapUser *user(void) const { return m_userData; }
  void user(ImapUser *userData) { delete m_userData; m_userData = userData; }

  // ReceiveData processes data as it comes it.  It's primary job is to chop the incoming data into
  // lines and to pass each line to HandleOneLine, which is the core command processor for the system
  void handleOneLine(INPUT_DATA_STRUCT &input);
  void doRetry(void);
  void idleTimeout(void);
  // This creates a properly formatted capability string based on the current state and configuration
  // of the IMAP server 
  std::string capabilityString(void);
  // Close the mailbox and set the state to the passed value
  void state(ImapState newState) { m_state = newState; }
  void closeMailbox(ImapState newState);
  // Set the response text to the passed string
  virtual void responseText(const std::string &msg);
  virtual void responseText(void);
  // NOT a const char because it's used to set the response code
  char *responseCode(void) { return m_responseCode; }
  void responseCode(const std::string &msg);
  // Get/set the namespace
  Namespace *store(void) const { return m_store; }
  void store(Namespace *mailstore) { m_store = mailstore; }
  ParseBuffer *parseBuffer(void) const { return m_parseBuffer; }
  bool loginDisabled(void) const { return m_loginDisabled; }
  MailStore::MAIL_STORE_RESULT mboxErrorCode(MailStore::MAIL_STORE_RESULT code) { m_mboxErrorCode = code; return code; }
  MailStore::MAIL_STORE_RESULT mboxErrorCode(void) const { return m_mboxErrorCode; }
  const uint8_t *lineBuffer(void) const { return m_lineBuffer; }
  void purgedMessages(NUMBER_SET purgedMessages) { m_purgedMessages.insert(purgedMessages.begin(), purgedMessages.end()); }

  // This sends the untagged responses associated with selecting a mailbox
  void selectData(const std::string &mailbox, bool isReadWrite);
  bool msnSequenceSet(SEARCH_RESULT &r_srVector, size_t &tokenPointer);
  bool uidSequenceSet(SEARCH_RESULT &r_srVector, size_t &tokenPointer);

  // These are used in two different sets of handlers, and I don't want one to depend on
  // the other, so I put them here.
  void fetchResponseUid(unsigned long uid);
  void fetchResponseFlags(uint32_t flags);

private:
  // These are configuration items
  bool m_loginDisabled;
  static bool m_anonymousEnabled;
  unsigned m_failedLoginPause;
  SessionDriver *m_driver;
  MailStore::MAIL_STORE_RESULT m_mboxErrorCode;
  size_t m_savedParsingAt;

  // These constitute the session's state
  enum ImapState m_state;
  ImapHandler *m_currentHandler;

  uint32_t m_mailFlags;
  uint8_t *m_lineBuffer;			// This buffers the incoming data into lines to be processed
  uint32_t m_lineBuffPtr, m_lineBuffLen;
  // As lines come in from the outside world, they are processed into m_bBuffer, which holds
  // them until the entire command has been received at which point it can be processed, often
  // by the *Execute() methods.
  ParseBuffer *m_parseBuffer;

  Namespace *m_store;
  char m_responseCode[MAX_RESPONSE_STRING_LENGTH+1];
  char m_responseText[MAX_RESPONSE_STRING_LENGTH+1];
  uint32_t m_commandString, m_arguments;
  uint32_t m_parseStage;

  // These are used to send live updates in case the mailbox is accessed by multiple client simultaneously
  uint32_t m_currentNextUid, m_currentMessageCount;
  uint32_t m_currentRecentCount, m_currentUnseen, m_currentUidValidity;

  static IMAPSYMBOLS m_symbols;

  std::string formatTaggedResponse(IMAP_RESULTS status, bool sendUpdatedStatus);

  ImapUser *m_userData;
  ImapMaster *m_master;
  Server *m_server;
  time_t m_lastCommandTime;
  NUMBER_SET m_purgedMessages;
  bool m_sendUpdatedStatus;
  unsigned m_retries;
  unsigned m_maxRetries;
  unsigned m_retryDelay;
};

#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif

typedef std::map<insensitiveString, MailStore::MAIL_STORE_FLAGS> FLAG_SYMBOL_T;
extern FLAG_SYMBOL_T flagSymbolTable;

#endif //_IMAPSESSION_HPP_INCLUDED_
