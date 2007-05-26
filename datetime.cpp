#include "datetime.hpp"

// SYZYGY -- this whole thing
DateTime::DateTime() {
    valid = true;
}

DateTime::DateTime(uint8_t *s, size_t dataLen, size_t &parsingAt) {
    valid = false;
}


DateTime::~DateTime() {
}
