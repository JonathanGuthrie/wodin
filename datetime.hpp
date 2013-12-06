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

#if !defined(_DATETIME_HPP_INCLUDED_)
#define _DATETIME_HPP_INCLUDED_

#include <string>

#include <stdint.h>
#include <time.h>

#include <clotho/insensitive.hpp>

class DateTimeInvalidDateTime {
};

class DateTime {
public:
    typedef enum {
	CTIME,
	IMAP,
	RFC822,
	FROM_LINE
    } STRING_FORMAT;
    DateTime(STRING_FORMAT format = CTIME);
    DateTime(const uint8_t *s, size_t dataLen, size_t &parsingAt, STRING_FORMAT format = IMAP) throw(DateTimeInvalidDateTime);
    ~DateTime();
    bool isValid(void) { return m_valid; }
    // I need to have multiple different output formats
    // the original is "WWW MMM DD hh:mm:ss YYYY +ZZZZ" (CTIME format)
    // the IMAP format is "DD-MMM-YYYY hh:mm:ss +ZZZZ" (IMAP format)
    const std::string str(void) const throw(DateTimeInvalidDateTime);
    bool operator< (DateTime right);
    bool operator<= (DateTime right);
    bool operator> (DateTime right);
    bool operator>= (DateTime right);
    void addDays(int days);
    struct tm tm(void) const { return m_tm; }
    void format(STRING_FORMAT format) { m_format = format; }
    bool parse(const std::string &timeString);
    bool parse(const std::string &timeString, STRING_FORMAT format);
    bool parse(const insensitiveString &timeString);
    bool parse(const insensitiveString &timeString, STRING_FORMAT format);
    bool parse(const uint8_t *data, size_t dataLen, size_t &parsingAt);
    bool parse(const uint8_t *data, size_t dataLen, size_t &parsingAt, STRING_FORMAT format);

private:
    bool parseCtime(const uint8_t *data, size_t dataLen, size_t &parsingAt);
    bool parseImap(const uint8_t *data, size_t dataLen, size_t &parsingAt);
    bool parseRfc822(const uint8_t *data, size_t dataLen, size_t &parsingAt);
    bool parseFromLine(const uint8_t *data, size_t dataLen, size_t &parsingAt);
    bool m_valid;
    struct tm m_tm;
    // This is seconds west of Greenwich
    int m_zone;
    STRING_FORMAT m_format;
};

#endif // _DATETIME_HPP_INCLUDED_
