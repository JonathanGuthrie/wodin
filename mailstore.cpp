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

#include "mailstore.hpp"

#include <algorithm>

#include <string.h>

MailStore::MailStore(ImapSession *session) : m_session(session) {
  m_errnoFromLibrary = 0;
}

MailStore::~MailStore() {
}

std::string MailStore::turnErrorCodeIntoString(MAIL_STORE_RESULT code) {
  const char *response[] = {
    "Success",
    "General Failure",
    "You cannot create the INBOX",
    "You cannot delete the INBOX",
    "The mailbox already exists",
    "The mailbox does not exist",
    "There is a problem with the path",
    "The mailbox has already been subscribed",
    "The mailbox is not subscribed",
    "The mailbox is in use",
    "The mailbox has subfolders",
    "The mailbox is not selectable",
    "The mailbox was opened read only",
    "No mailbox is open",
    "Message file open failed",
    "Message file write failed",
    "Message not found",
    "The Namespaces Don't Match"
  };
  if (code < (sizeof(response) / sizeof(const char*))) {
    std::string result = response[code];
    if (0 != m_errnoFromLibrary) {
      result += ": ";
      result += strerror(m_errnoFromLibrary);
    }
    return result;
  }
  else {
    return "Invalid mailbox error code";
  }
}

NUMBER_LIST MailStore::mailboxMsnToUid(const NUMBER_LIST &msns) {
  NUMBER_LIST result;
  for (NUMBER_LIST::const_iterator i=msns.begin(); i!=msns.end(); ++i) {
    result.push_back(mailboxMsnToUid(*i));
  }
  return result;
}

unsigned long MailStore::mailboxMsnToUid(const unsigned long msn) {
  unsigned long result = 0;
  if (msn <= m_uidGivenMsn.size()) {
    result = m_uidGivenMsn[msn-1];
  }
  return result;
}

NUMBER_LIST MailStore::mailboxUidToMsn(const NUMBER_LIST &uids) {
  NUMBER_LIST result;
  for (NUMBER_LIST::const_iterator i=uids.begin(); i!=uids.end(); ++i) {
    result.push_back(mailboxUidToMsn(*i));
  }
}

unsigned long MailStore::mailboxUidToMsn(unsigned long uid) {
  unsigned long result = 0;

  MSN_TO_UID::const_iterator i = find(m_uidGivenMsn.begin(), m_uidGivenMsn.end(), uid);
  if (i != m_uidGivenMsn.end()) {
    result = (i - m_uidGivenMsn.begin()) + 1;
  }
  return result;
}


MailStore::MAIL_STORE_RESULT MailStore::messageList(SEARCH_RESULT &msns) const {
  MailStore::MAIL_STORE_RESULT result = MailStore::SUCCESS;
  msns.clear(); 
  if (NULL != m_openMailboxName) {
    for (int i=0; i<m_uidGivenMsn.size(); ++i) {
      msns.push_back(m_uidGivenMsn[i]);
    }
  }
  else {
    result = MAILBOX_NOT_OPEN;
  }
  return result;
}
