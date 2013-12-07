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

#include "appendhandler.hpp"
#include "noophandler.hpp"
#include "unimplementedhandler.hpp"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

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

class MockSession : public ImapSession {
public:
    MockSession(const std::string &command, MockMailstore *mock_store) : ImapSession(NULL, NULL, NULL) {
	store(mock_store);
	INPUT_DATA_STRUCT t;

	m_parseBuffer = new ParseBuffer(100);
	// Fake the command parsing up to that point. It includes a tag and the name of
	// the command

	// First do the tag
	t.data = (uint8_t *)"1";
	t.dataLen = 1;
	t.parsingAt = 0;
	m_parseBuffer->addToParseBuffer(t, 1);

	// now, do the command
	t.data = (uint8_t *)command.c_str();
	t.dataLen = command.length();
	t.parsingAt = 0;
	m_parseBuffer->commandHere();
	m_parseBuffer->atom(t);
	m_parseBuffer->argumentsHere();
    }
    MOCK_METHOD1(responseText, void(const std::string &));
};

TEST(NoopHandler, AllTests) {
    MockSession test_session("noop", NULL);
    INPUT_DATA_STRUCT input;

    EXPECT_CALL(test_session, responseText(::testing::_)).Times(0);
    ImapHandler *handler = noopHandler(&test_session, input);
    ASSERT_TRUE(NULL != handler);
    ASSERT_EQ(IMAP_OK, handler->receiveData(input));
}

TEST(UnimplementedHandler, AllTests) {
    MockSession test_session("unimplemented", NULL);
    INPUT_DATA_STRUCT input;

    EXPECT_CALL(test_session, responseText("Unrecognized Command")).Times(1);
    ImapHandler *handler = unimplementedHandler(&test_session, input);
    ASSERT_TRUE(NULL != handler);
    ASSERT_EQ(IMAP_BAD, handler->receiveData(input));
}

// What does an append command look like?  Well, 
// "APPEND" SP mailbox [SP flag-list] [SP date-time] SP literal
// the mailbox is an atom, a quoted string, or a literal string.
// the flag-list is a list in parenthesis of space-separated flags
// where each flag begins with a backslash and is one of 
// Answered Flagged Deleted Seen Draft, but not Recent
// and could have others, specified as atoms, if user-defined flags are allowed.
// date-time is DQUOTE DD-MM-YYYY SP HH:MM:SS SP ('+'/'-')ZZZZ DQUOTE

// I need to test the parsing, for each of atom, quoted string, or string literal
// followed by each flags, date/time, flags and date/time, and nothing, and all of
// them followed by as little message data as I can get away with.  So, 12 different
// cases.

// No, more than that because this has to test the locking logic

// There should also be a response text test, which will require mocking
TEST(AppendHandler, AtomNoOtherArgs) {
    MockMailstore *test_store = new MockMailstore;
    MockSession test_session("append", test_store);
    INPUT_DATA_STRUCT input;

    EXPECT_CALL((*test_store), lock("inbox")).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL(test_session, responseText("Ready for the Message Data")).Times(1);
    EXPECT_CALL((*test_store), addMessageToMailbox("inbox",::testing::_,::testing::_,::testing::_,::testing::_,::testing::_)).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), appendDataToMessage("inbox",::testing::_,::testing::_,::testing::_)).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), doneAppendingDataToMessage("inbox",::testing::_)).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    ImapHandler *handler = appendHandler(&test_session, input);
    ASSERT_TRUE(NULL != handler);
    input.data = (uint8_t *)"inbox {1}";
    input.dataLen = strlen((const char *)input.data);
    input.parsingAt = 0;
    ASSERT_EQ(IMAP_NOTDONE, handler->receiveData(input));
    input.data = (uint8_t *)"         ";
    input.dataLen = strlen((const char *)input.data);
    input.parsingAt = 0;
    ASSERT_EQ(IMAP_OK, handler->receiveData(input));
}

TEST(AppendHandler, QstringNoOtherArgs) {
    MockMailstore *test_store = new MockMailstore;
    MockSession test_session("append", test_store);
    INPUT_DATA_STRUCT input;

    EXPECT_CALL((*test_store), lock("inbox")).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL(test_session, responseText("Ready for the Message Data")).Times(1);
    EXPECT_CALL((*test_store), addMessageToMailbox("inbox",::testing::_,::testing::_,::testing::_,::testing::_,::testing::_)).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), appendDataToMessage("inbox",::testing::_,::testing::_,::testing::_)).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), doneAppendingDataToMessage("inbox",::testing::_)).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    ImapHandler *handler = appendHandler(&test_session, input);
    ASSERT_TRUE(NULL != handler);
    input.data = (uint8_t *)"\"inbox\" {1}";
    input.dataLen = strlen((const char *)input.data);
    input.parsingAt = 0;
    ASSERT_EQ(IMAP_NOTDONE, handler->receiveData(input));
    input.data = (uint8_t *)"         ";
    input.dataLen = strlen((const char *)input.data);
    input.parsingAt = 0;
    ASSERT_EQ(IMAP_OK, handler->receiveData(input));
}

TEST(AppendHandler, LiteralNoOtherArgs) {
    MockMailstore *test_store = new MockMailstore;
    MockSession test_session("append", test_store);
    INPUT_DATA_STRUCT input;
    uint8_t buffer[100];

    EXPECT_CALL((*test_store), lock("inbox")).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL(test_session, responseText("Ready for the Message Data")).Times(1);
    EXPECT_CALL((*test_store), addMessageToMailbox("inbox",::testing::_,::testing::_,::testing::_,::testing::_,::testing::_)).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), appendDataToMessage("inbox",::testing::_,::testing::_,::testing::_)).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), doneAppendingDataToMessage("inbox",::testing::_)).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    ImapHandler *handler = appendHandler(&test_session, input);
    ASSERT_TRUE(NULL != handler);
    input.data = (uint8_t *)"{5}";
    input.dataLen = strlen((const char *)input.data);
    input.parsingAt = 0;
    ASSERT_EQ(IMAP_NOTDONE, handler->receiveData(input));
    // This needs to have the CRLF appended to it because it won't be stripped by 
    // ImapSession::handleOneLine as it would be if it were not in a literal string
    strcpy((char *)buffer, "inbox {1}\r\n");
    input.data = buffer;
    input.dataLen = strlen((const char *)buffer);
    input.parsingAt = 0;
    ASSERT_EQ(IMAP_NOTDONE, handler->receiveData(input));
    input.data = (uint8_t *)" ";
    input.dataLen = strlen((const char *)input.data);
    input.parsingAt = 0;
    ASSERT_EQ(IMAP_OK, handler->receiveData(input));
}

TEST(AppendHandler, AtomFlagsNoDate) {
    MockMailstore *test_store = new MockMailstore;
    MockSession test_session("append", test_store);
    INPUT_DATA_STRUCT input;

    ParseBuffer::buildSymbolTable();  // Or the flag processing isn't going to work.
    EXPECT_CALL((*test_store), lock("inbox")).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL(test_session, responseText("Ready for the Message Data")).Times(1);
    EXPECT_CALL((*test_store), addMessageToMailbox("inbox",::testing::_,::testing::_,::testing::_,::testing::_,::testing::_)).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), appendDataToMessage("inbox",::testing::_,::testing::_,::testing::_)).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), doneAppendingDataToMessage("inbox",::testing::_)).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    ImapHandler *handler = appendHandler(&test_session, input);
    ASSERT_TRUE(NULL != handler);
    input.data = (uint8_t *)"inbox (\\DELETED \\DRAFT) {1}";
    input.dataLen = strlen((const char *)input.data);
    input.parsingAt = 0;
    ASSERT_EQ(IMAP_NOTDONE, handler->receiveData(input));
    input.data = (uint8_t *)"         ";
    input.dataLen = strlen((const char *)input.data);
    input.parsingAt = 0;
    ASSERT_EQ(IMAP_OK, handler->receiveData(input));
}

TEST(AppendHandler, QstringFlagsNoDate) {
    MockMailstore *test_store = new MockMailstore;
    MockSession test_session("append", test_store);
    INPUT_DATA_STRUCT input;

    ParseBuffer::buildSymbolTable();  // Or the flag processing isn't going to work.
    EXPECT_CALL((*test_store), lock("inbox")).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL(test_session, responseText("Ready for the Message Data")).Times(1);
    EXPECT_CALL((*test_store), addMessageToMailbox("inbox",::testing::_,::testing::_,::testing::_,::testing::_,::testing::_)).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), appendDataToMessage("inbox",::testing::_,::testing::_,::testing::_)).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), doneAppendingDataToMessage("inbox",::testing::_)).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    ImapHandler *handler = appendHandler(&test_session, input);
    ASSERT_TRUE(NULL != handler);
    input.data = (uint8_t *)"\"inbox\" (\\DELETED \\DRAFT) {1}";
    input.dataLen = strlen((const char *)input.data);
    input.parsingAt = 0;
    ASSERT_EQ(IMAP_NOTDONE, handler->receiveData(input));
    input.data = (uint8_t *)"         ";
    input.dataLen = strlen((const char *)input.data);
    input.parsingAt = 0;
    ASSERT_EQ(IMAP_OK, handler->receiveData(input));
}

TEST(AppendHandler, LiteralFlagsNoDate) {
    MockMailstore *test_store = new MockMailstore;
    MockSession test_session("append", test_store);
    INPUT_DATA_STRUCT input;
    uint8_t buffer[100];

    ParseBuffer::buildSymbolTable();  // Or the flag processing isn't going to work.
    EXPECT_CALL((*test_store), lock("inbox")).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL(test_session, responseText("Ready for the Message Data")).Times(1);
    EXPECT_CALL((*test_store), addMessageToMailbox("inbox",::testing::_,::testing::_,::testing::_,::testing::_,::testing::_)).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), appendDataToMessage("inbox",::testing::_,::testing::_,::testing::_)).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), doneAppendingDataToMessage("inbox",::testing::_)).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    ImapHandler *handler = appendHandler(&test_session, input);
    ASSERT_TRUE(NULL != handler);
    input.data = (uint8_t *)"{5}";
    input.dataLen = strlen((const char *)input.data);
    input.parsingAt = 0;
    ASSERT_EQ(IMAP_NOTDONE, handler->receiveData(input));
    // This needs to have the CRLF appended to it because it won't be stripped by 
    // ImapSession::handleOneLine as it would be if it were not in a literal string
    strcpy((char *)buffer, "inbox (\\DELETED \\DRAFT) {1}\r\n");
    input.data = buffer;
    input.dataLen = strlen((const char *)buffer);
    input.parsingAt = 0;
    ASSERT_EQ(IMAP_NOTDONE, handler->receiveData(input));
    input.data = (uint8_t *)" ";
    input.dataLen = strlen((const char *)input.data);
    input.parsingAt = 0;
    ASSERT_EQ(IMAP_OK, handler->receiveData(input));
}

