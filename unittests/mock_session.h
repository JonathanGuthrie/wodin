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

#if defined(MOCK_SESSION_INCLUDED)
#define MOCK_SESSION_INCLUDED

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

#endif // defined(MOCK_SESSION_INCLUDED)
