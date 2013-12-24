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

#if defined(MOCK_MAILSTORE_INCLUDED)
#define MOCK_MAILSTORE_INCLUDED

#include "mock_session.h"

class MockMailstore : public Namespace {
public:
    MockMailstore(void) : Namespace(NULL) {}
    MOCK_METHOD1(lock, MailStore::MAIL_STORE_RESULT(const std::string &));
    MOCK_METHOD1(unlock, MailStore::MAIL_STORE_RESULT(const std::string &));
    MOCK_METHOD6(addMessageToMailbox,
		 MailStore::MAIL_STORE_RESULT(const std::string &MailboxName,
					      const uint8_t *data,
					      size_t length,
					      DateTime &createTime,
					      uint32_t messageFlags,
					      size_t *newUid));
    MOCK_METHOD4(appendDataToMessage,
		 MailStore::MAIL_STORE_RESULT(const std::string &MailboxName,
					      size_t uid,
					      const uint8_t *data,
					      size_t length));
    MOCK_METHOD2(doneAppendingDataToMessage,
		 MailStore::MAIL_STORE_RESULT(const std::string &MailboxName, size_t uid));
};

#endif // defined(MOCK_MAILSTORE_INCLUDED)
