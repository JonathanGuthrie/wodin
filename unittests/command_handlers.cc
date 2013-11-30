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

#include "unimplementedhandler.hpp"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

class MockSession : public ImapSession {
public:
  MockSession() : ImapSession(NULL, NULL, NULL) {}
  MOCK_METHOD1(responseText, void(const std::string &));
};
  

// There should also be a response text test, which will require mocking
TEST(UnimplementedHandler, AllTests) {
  MockSession test_session;
  MockSession *temp_session = &test_session;
  INPUT_DATA_STRUCT input;

  EXPECT_CALL(test_session, responseText("Unrecognized Command")).Times(1);
  ImapHandler *handler = unimplementedHandler(temp_session, input);
  ASSERT_TRUE(NULL != handler);
  ASSERT_EQ(IMAP_BAD, handler->receiveData(input));
}
