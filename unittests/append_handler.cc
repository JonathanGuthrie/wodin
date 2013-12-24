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

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mock_mailstore.h"
#include "mock_session.h"

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
    EXPECT_CALL((*test_store), addMessageToMailbox("inbox",::testing::_,::testing::_,::testing::_,0,::testing::_)).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), appendDataToMessage("inbox",::testing::_,::testing::_,::testing::_)).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), doneAppendingDataToMessage("inbox",::testing::_)).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
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
    EXPECT_CALL((*test_store), addMessageToMailbox("inbox",::testing::_,::testing::_,::testing::_,0,::testing::_)).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), appendDataToMessage("inbox",::testing::_,::testing::_,::testing::_)).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), doneAppendingDataToMessage("inbox",::testing::_)).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
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
    EXPECT_CALL((*test_store), addMessageToMailbox("inbox",::testing::_,::testing::_,::testing::_,0,::testing::_)).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), appendDataToMessage("inbox",::testing::_,::testing::_,::testing::_)).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), doneAppendingDataToMessage("inbox",::testing::_)).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
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
    EXPECT_CALL((*test_store), addMessageToMailbox("inbox",::testing::_,::testing::_,::testing::_,MailStore::IMAP_MESSAGE_DELETED|MailStore::IMAP_MESSAGE_DRAFT,::testing::_)).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), appendDataToMessage("inbox",::testing::_,::testing::_,::testing::_)).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), doneAppendingDataToMessage("inbox",::testing::_)).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
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
    EXPECT_CALL((*test_store), addMessageToMailbox("inbox",::testing::_,::testing::_,::testing::_,MailStore::IMAP_MESSAGE_DELETED|MailStore::IMAP_MESSAGE_FLAGGED,::testing::_)).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), appendDataToMessage("inbox",::testing::_,::testing::_,::testing::_)).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), doneAppendingDataToMessage("inbox",::testing::_)).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    ImapHandler *handler = appendHandler(&test_session, input);
    ASSERT_TRUE(NULL != handler);
    input.data = (uint8_t *)"\"inbox\" (\\DELETED \\FLAGGED) {1}";
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
    EXPECT_CALL((*test_store), addMessageToMailbox("inbox",::testing::_,::testing::_,::testing::_,MailStore::IMAP_MESSAGE_SEEN|MailStore::IMAP_MESSAGE_ANSWERED,::testing::_)).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), appendDataToMessage("inbox",::testing::_,::testing::_,::testing::_)).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), doneAppendingDataToMessage("inbox",::testing::_)).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    ImapHandler *handler = appendHandler(&test_session, input);
    ASSERT_TRUE(NULL != handler);
    input.data = (uint8_t *)"{5}";
    input.dataLen = strlen((const char *)input.data);
    input.parsingAt = 0;
    ASSERT_EQ(IMAP_NOTDONE, handler->receiveData(input));
    // This needs to have the CRLF appended to it because it won't be stripped by 
    // ImapSession::handleOneLine as it would be if it were not in a literal string
    strcpy((char *)buffer, "inbox (\\SEEN \\ANSWERED) {1}\r\n");
    input.data = buffer;
    input.dataLen = strlen((const char *)buffer);
    input.parsingAt = 0;
    ASSERT_EQ(IMAP_NOTDONE, handler->receiveData(input));
    input.data = (uint8_t *)" ";
    input.dataLen = strlen((const char *)input.data);
    input.parsingAt = 0;
    ASSERT_EQ(IMAP_OK, handler->receiveData(input));
}


TEST(AppendHandler, AtomDateNoFlags) {
    MockMailstore *test_store = new MockMailstore;
    MockSession test_session("append", test_store);
    INPUT_DATA_STRUCT input;

    ParseBuffer::buildSymbolTable();  // Or the flag processing isn't going to work.
    EXPECT_CALL((*test_store), lock("inbox")).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL(test_session, responseText("Ready for the Message Data")).Times(1);
    EXPECT_CALL((*test_store), addMessageToMailbox("inbox",::testing::_,::testing::_,::testing::_,0,::testing::_)).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), appendDataToMessage("inbox",::testing::_,::testing::_,::testing::_)).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), doneAppendingDataToMessage("inbox",::testing::_)).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    ImapHandler *handler = appendHandler(&test_session, input);
    ASSERT_TRUE(NULL != handler);
    input.data = (uint8_t *)"inbox \"06-jun-1983 12:23:58 -0600\" {1}";
    input.dataLen = strlen((const char *)input.data);
    input.parsingAt = 0;
    ASSERT_EQ(IMAP_NOTDONE, handler->receiveData(input));
    input.data = (uint8_t *)"         ";
    input.dataLen = strlen((const char *)input.data);
    input.parsingAt = 0;
    ASSERT_EQ(IMAP_OK, handler->receiveData(input));
}

TEST(AppendHandler, QstringDateNoFlags) {
    MockMailstore *test_store = new MockMailstore;
    MockSession test_session("append", test_store);
    INPUT_DATA_STRUCT input;

    ParseBuffer::buildSymbolTable();  // Or the flag processing isn't going to work.
    EXPECT_CALL((*test_store), lock("inbox")).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL(test_session, responseText("Ready for the Message Data")).Times(1);
    EXPECT_CALL((*test_store), addMessageToMailbox("inbox",::testing::_,::testing::_,::testing::_,0,::testing::_)).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), appendDataToMessage("inbox",::testing::_,::testing::_,::testing::_)).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), doneAppendingDataToMessage("inbox",::testing::_)).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    ImapHandler *handler = appendHandler(&test_session, input);
    ASSERT_TRUE(NULL != handler);
    input.data = (uint8_t *)"\"inbox\" \"12-JUN-1987 08:44:06 -0600\" {1}";
    input.dataLen = strlen((const char *)input.data);
    input.parsingAt = 0;
    ASSERT_EQ(IMAP_NOTDONE, handler->receiveData(input));
    input.data = (uint8_t *)"         ";
    input.dataLen = strlen((const char *)input.data);
    input.parsingAt = 0;
    ASSERT_EQ(IMAP_OK, handler->receiveData(input));
}

TEST(AppendHandler, LiteralDateNoFlags) {
    MockMailstore *test_store = new MockMailstore;
    MockSession test_session("append", test_store);
    INPUT_DATA_STRUCT input;
    uint8_t buffer[100];

    ParseBuffer::buildSymbolTable();  // Or the flag processing isn't going to work.
    EXPECT_CALL((*test_store), lock("inbox")).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL(test_session, responseText("Ready for the Message Data")).Times(1);
    EXPECT_CALL((*test_store), addMessageToMailbox("inbox",::testing::_,::testing::_,/* DateTime object here */::testing::_,0,::testing::_)).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), appendDataToMessage("inbox",::testing::_,::testing::_,::testing::_)).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    EXPECT_CALL((*test_store), doneAppendingDataToMessage("inbox",::testing::_)).Times(1).WillRepeatedly(::testing::Return(MailStore::SUCCESS));
    ImapHandler *handler = appendHandler(&test_session, input);
    ASSERT_TRUE(NULL != handler);
    input.data = (uint8_t *)"{5}";
    input.dataLen = strlen((const char *)input.data);
    input.parsingAt = 0;
    ASSERT_EQ(IMAP_NOTDONE, handler->receiveData(input));
    // This needs to have the CRLF appended to it because it won't be stripped by 
    // ImapSession::handleOneLine as it would be if it were not in a literal string
    strcpy((char *)buffer, "inbox \"18-Feb-1991 07:58:12 -0600\" {1}\r\n");
    input.data = buffer;
    input.dataLen = strlen((const char *)buffer);
    input.parsingAt = 0;
    ASSERT_EQ(IMAP_NOTDONE, handler->receiveData(input));
    input.data = (uint8_t *)" ";
    input.dataLen = strlen((const char *)input.data);
    input.parsingAt = 0;
    ASSERT_EQ(IMAP_OK, handler->receiveData(input));
}

