#if !defined(_DATETIME_HPP_INCLUDED_)
#define _DATETIME_HPP_INCLUDED_

#include <stdint.h>
#include <string>

class DateTime {
public:
    DateTime();
    DateTime(uint8_t *s, size_t dataLen, size_t &parsingAt);
    ~DateTime();
    bool IsValid(void) { return valid; }

private:
    bool valid;
};

#endif // _DATETIME_HPP_INCLUDED_
