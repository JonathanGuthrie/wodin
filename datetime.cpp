#include "datetime.hpp"

// SYZYGY -- this whole thing
DateTime::DateTime() {
    time_t t = time(NULL);
    gmtime_r(&t, &tm);
    zone = 0;
    valid = true;
}

// I start with the pointer at a double quote, and I should end after the other double-quote
// A date-time string is "DD-MMM-YYYY HH:MM:SS +ZZZZ" (quotes included)
// That looks to be 2+11+2+8+5 = 28 characters

DateTime::DateTime(const uint8_t *data, size_t dataLen, size_t &parsingAt) throw(DateTimeInvalidDateTimeString) {
    valid = false;
    // The first character should be a double-quote
    if (28 <= (dataLen - parsingAt)) {
	if ('"' != data[parsingAt]) {
	    throw DateTimeInvalidDateTimeString();
	}
	++parsingAt;

	if (((' ' != data[parsingAt]) && !isdigit(data[parsingAt])) ||
	    !isdigit(data[parsingAt+1])) {
	    throw DateTimeInvalidDateTimeString();
	}
	tm.tm_mday = strtol((const char *)&data[parsingAt], NULL, 10);
	parsingAt += 2;

	if ((tm.tm_mday < 1) || (tm.tm_mday > 31) || ('-' != data[parsingAt])) {
	    throw DateTimeInvalidDateTimeString();
	}
	++parsingAt;

	// Check Jan, Jun, and Jul first
	if ('J' == data[parsingAt]) {
	    // Jun or Jul
	    if ('u' == data[parsingAt+1]) {
		if ('n' == data[parsingAt+2]) {
		    if (tm.tm_mday > 30) {
			throw DateTimeInvalidDateTimeString();
		    }
		    tm.tm_mon = 5;
		}
		else if ('l' == data[parsingAt+2]) {
		    tm.tm_mon = 6;
		}
		else {
		    throw DateTimeInvalidDateTimeString();
		}
	    }
	    else if (('a' == data[parsingAt+1]) && ('n' == data[parsingAt+2])) {
		tm.tm_mon = 0;
	    }
	    else {
		throw DateTimeInvalidDateTimeString();
	    }
	}
	// Next, Mar and May
	else if (('M' == data[parsingAt]) && ('a' == data[parsingAt+1])) {
	    if ('r' == data[parsingAt+2]) {
		tm.tm_mon = 2;
	    }
	    else if ('y' == data[parsingAt+2]) {
		tm.tm_mon = 4;
	    }
	    else {
		throw DateTimeInvalidDateTimeString();
	    }
	}
	// Next Apr and Aug
	else if ('A' == data[parsingAt]) {
	    if (('p' == data[parsingAt+1]) && ('r' == data[parsingAt+2])) {
		if (tm.tm_mday > 30) {
		    throw DateTimeInvalidDateTimeString();
		}
		tm.tm_mon = 3;
	    }
	    else {
		if (('u' == data[parsingAt+1]) && ('g' == data[parsingAt+2])) {
		    tm.tm_mon = 7;
		}
		else {
		    throw DateTimeInvalidDateTimeString();
		}
	    }
	}
	// Feb
	else if (('F' == data[parsingAt]) && ('e' == data[parsingAt+1]) && ('b' == data[parsingAt+2])) {
	    tm.tm_mon = 1;
	    if (tm.tm_mday > 29) {
		throw DateTimeInvalidDateTimeString();
	    }
	}
	// Sep
	else if (('S' == data[parsingAt]) && ('e' == data[parsingAt+1]) && ('p' == data[parsingAt+2])) {
	    if (tm.tm_mday > 30) {
		throw DateTimeInvalidDateTimeString();
	    }
	    tm.tm_mon = 8;
	}
	// Oct
	else if (('O' == data[parsingAt]) && ('c' == data[parsingAt+1]) && ('t' == data[parsingAt+2])) {
	    tm.tm_mon = 9;
	}
	// Nov
	else if (('N' == data[parsingAt]) && ('o' == data[parsingAt+1]) && ('v' == data[parsingAt+2])) {
	    if (tm.tm_mday > 30) {
		throw DateTimeInvalidDateTimeString();
	    }
	    tm.tm_mon = 10;
	}
	// Dec
	else if (('D' == data[parsingAt]) && ('e' == data[parsingAt+1]) && ('c' == data[parsingAt+2])) {
	    tm.tm_mon = 11;
	}
	else {
	    throw DateTimeInvalidDateTimeString();
	}
	parsingAt += 3;

	if ('-' != data[parsingAt]) {
	    throw DateTimeInvalidDateTimeString();
	}
	++parsingAt;

	if (!isdigit(data[parsingAt]) || !isdigit(data[parsingAt+1]) || !isdigit(data[parsingAt+2]) || !isdigit(data[parsingAt+3])) {
	    throw DateTimeInvalidDateTimeString();
	}
	tm.tm_year = strtol((const char *)&data[parsingAt], NULL, 10);
	parsingAt += 4;

	// If I'm looking at Feb 29, then it needs to be a leap year
	if ((1 == tm.tm_mon) && (29 == tm.tm_mday) &&
	    (0 != (tm.tm_year % 4)) || ((0 == tm.tm_year % 100) && (0 != tm.tm_year % 400))) {
	    throw DateTimeInvalidDateTimeString();
	}

	if (' ' != data[parsingAt]) {
	    throw DateTimeInvalidDateTimeString();
	}
	++parsingAt;

	if (!isdigit(data[parsingAt]) || !isdigit(data[parsingAt+1])) {
	    throw DateTimeInvalidDateTimeString();
	}
	tm.tm_hour = strtol((const char *)&data[parsingAt], NULL, 10);
	parsingAt += 2;

	if (':' != data[parsingAt]) {
	    throw DateTimeInvalidDateTimeString();
	}
	++parsingAt;

	if (!isdigit(data[parsingAt]) || !isdigit(data[parsingAt+1])) {
	    throw DateTimeInvalidDateTimeString();
	}
	tm.tm_min = strtol((const char *)&data[parsingAt], NULL, 10);
	parsingAt += 2;

	if (':' != data[parsingAt]) {
	    throw DateTimeInvalidDateTimeString();
	}
	++parsingAt;

	if (!isdigit(data[parsingAt]) || !isdigit(data[parsingAt+1])) {
	    throw DateTimeInvalidDateTimeString();
	}
	tm.tm_sec = strtol((const char *)&data[parsingAt], NULL, 10);
	parsingAt += 2;

	if (' ' != data[parsingAt]) {
	    throw DateTimeInvalidDateTimeString();
	}
	++parsingAt;

	if ((('-' != data[parsingAt]) && ('+' != data[parsingAt])) ||
	    !isdigit(data[parsingAt+1]) || !isdigit(data[parsingAt+2]) || !isdigit(data[parsingAt+3]) || !isdigit(data[parsingAt+4])) {
	    throw DateTimeInvalidDateTimeString();
	}
	zone = strtol((const char *)&data[parsingAt], NULL, 10);
	parsingAt += 5;

	if ((2459 < abs(zone)) || (59 < (abs(zone)%100))) {
	    throw DateTimeInvalidDateTimeString();
	}

	if ('"' != data[parsingAt]) {
	    throw DateTimeInvalidDateTimeString();
	}
	++parsingAt;
    }
    valid = true;
}


DateTime::~DateTime() {
}
