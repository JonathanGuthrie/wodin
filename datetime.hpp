#if !defined(_DATETIME_HPP_INCLUDED_)
#define _DATETIME_HPP_INCLUDED_

#include <string>

#include <stdint.h>
#include <time.h>

class DateTimeInvalidDateTime
{
};

class DateTime {
public:
    DateTime();
    DateTime(const uint8_t *s, size_t dataLen, size_t &parsingAt) throw(DateTimeInvalidDateTime);
    ~DateTime();
    bool IsValid(void) { return valid; }
    const std::string str(void) const throw(DateTimeInvalidDateTime);

private:
    bool valid;
    struct tm tm;
    int zone;
};

#endif // _DATETIME_HPP_INCLUDED_
