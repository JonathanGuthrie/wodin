#if !defined(_DATETIME_HPP_INCLUDED_)
#define _DATETIME_HPP_INCLUDED_

#include <string>

#include <stdint.h>
#include <time.h>

#include "insensitive.hpp"

class DateTimeInvalidDateTime
{
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
    bool IsValid(void) { return m_valid; }
    // I need to have multiple different output formats
    // the original is "WWW MMM DD hh:mm:ss YYYY +ZZZZ" (CTIME format)
    // the IMAP format is "DD-MMM-YYYY hh:mm:ss +ZZZZ" (IMAP format)
    const std::string str(void) const throw(DateTimeInvalidDateTime);
    bool operator< (DateTime right);
    bool operator<= (DateTime right);
    bool operator> (DateTime right);
    bool operator>= (DateTime right);
    void AddDays(int days);
    struct tm GetTm(void) const { return m_tm; }
    void SetFormat(STRING_FORMAT format) { m_format = format; }
    bool Parse(const std::string &timeString);
    bool Parse(const std::string &timeString, STRING_FORMAT format);
    bool Parse(const insensitiveString &timeString);
    bool Parse(const insensitiveString &timeString, STRING_FORMAT format);
    bool Parse(const uint8_t *data, size_t dataLen, size_t &parsingAt);
    bool Parse(const uint8_t *data, size_t dataLen, size_t &parsingAt, STRING_FORMAT format);

private:
    bool ParseCtime(const uint8_t *data, size_t dataLen, size_t &parsingAt);
    bool ParseImap(const uint8_t *data, size_t dataLen, size_t &parsingAt);
    bool ParseRfc822(const uint8_t *data, size_t dataLen, size_t &parsingAt);
    bool ParseFromLine(const uint8_t *data, size_t dataLen, size_t &parsingAt);
    bool m_valid;
    struct tm m_tm;
    // This is seconds west of Greenwich
    int m_zone;
    STRING_FORMAT m_format;
};

#endif // _DATETIME_HPP_INCLUDED_
