#include <iomanip>
#include <sstream>

#include "datetime.hpp"

DateTime::DateTime() {
    time_t t = time(NULL);
    gmtime_r(&t, &tm);
    zone = 0;
    valid = true;
}

// I start with the pointer at a double quote, and I should end after the other double-quote
// A date-time string is "DD-MMM-YYYY HH:MM:SS +ZZZZ" (quotes included)
// That looks to be 2+11+2+8+5 = 28 characters

DateTime::DateTime(const uint8_t *data, size_t dataLen, size_t &parsingAt) throw(DateTimeInvalidDateTime) {
    valid = false;
    // The first character should be a double-quote
    if (28 > (dataLen - parsingAt)) {
	throw DateTimeInvalidDateTime();
    }
    if ('"' != data[parsingAt]) {
 	throw DateTimeInvalidDateTime();
    }
    ++parsingAt;

    if (((' ' != data[parsingAt]) && !isdigit(data[parsingAt])) ||
	!isdigit(data[parsingAt+1])) {
	throw DateTimeInvalidDateTime();
    }
    tm.tm_mday = strtol((const char *)&data[parsingAt], NULL, 10);
    parsingAt += 2;

    if ((tm.tm_mday < 1) || (tm.tm_mday > 31) || ('-' != data[parsingAt])) {
	throw DateTimeInvalidDateTime();
    }
    ++parsingAt;

    // Check Jan, Jun, and Jul first
    if ('J' == data[parsingAt]) {
	// Jun or Jul
	if ('u' == data[parsingAt+1]) {
	    if ('n' == data[parsingAt+2]) {
		if (tm.tm_mday > 30) {
		    throw DateTimeInvalidDateTime();
		}
		tm.tm_mon = 5;
	    }
	    else if ('l' == data[parsingAt+2]) {
		tm.tm_mon = 6;
	    }
	    else {
		throw DateTimeInvalidDateTime();
	    }
	}
	else if (('a' == data[parsingAt+1]) && ('n' == data[parsingAt+2])) {
	    tm.tm_mon = 0;
	}
	else {
	    throw DateTimeInvalidDateTime();
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
	    throw DateTimeInvalidDateTime();
	}
    }
    // Next Apr and Aug
    else if ('A' == data[parsingAt]) {
	if (('p' == data[parsingAt+1]) && ('r' == data[parsingAt+2])) {
	    if (tm.tm_mday > 30) {
		throw DateTimeInvalidDateTime();
	    }
	    tm.tm_mon = 3;
	}
	else {
	    if (('u' == data[parsingAt+1]) && ('g' == data[parsingAt+2])) {
		tm.tm_mon = 7;
	    }
	    else {
		throw DateTimeInvalidDateTime();
	    }
	}
    }
    // Feb
    else if (('F' == data[parsingAt]) && ('e' == data[parsingAt+1]) && ('b' == data[parsingAt+2])) {
	tm.tm_mon = 1;
	if (tm.tm_mday > 29) {
	    throw DateTimeInvalidDateTime();
	}
    }
    // Sep
    else if (('S' == data[parsingAt]) && ('e' == data[parsingAt+1]) && ('p' == data[parsingAt+2])) {
	if (tm.tm_mday > 30) {
	    throw DateTimeInvalidDateTime();
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
	    throw DateTimeInvalidDateTime();
	}
	tm.tm_mon = 10;
    }
    // Dec
    else if (('D' == data[parsingAt]) && ('e' == data[parsingAt+1]) && ('c' == data[parsingAt+2])) {
	tm.tm_mon = 11;
    }
    else {
	throw DateTimeInvalidDateTime();
    }
    parsingAt += 3;

    if ('-' != data[parsingAt]) {
	throw DateTimeInvalidDateTime();
    }
    ++parsingAt;

    if (!isdigit(data[parsingAt]) || !isdigit(data[parsingAt+1]) || !isdigit(data[parsingAt+2]) || !isdigit(data[parsingAt+3])) {
	throw DateTimeInvalidDateTime();
    }
    tm.tm_year = strtol((const char *)&data[parsingAt], NULL, 10);
    parsingAt += 4;

    // If I'm looking at Feb 29, then it needs to be a leap year
    if ((1 == tm.tm_mon) && (29 == tm.tm_mday) &&
	(0 != (tm.tm_year % 4)) || ((0 == tm.tm_year % 100) && (0 != tm.tm_year % 400))) {
	throw DateTimeInvalidDateTime();
    }

    if (' ' != data[parsingAt]) {
	throw DateTimeInvalidDateTime();
    }
    ++parsingAt;

    if (!isdigit(data[parsingAt]) || !isdigit(data[parsingAt+1])) {
	throw DateTimeInvalidDateTime();
    }
    tm.tm_hour = strtol((const char *)&data[parsingAt], NULL, 10);
    parsingAt += 2;

    if (':' != data[parsingAt]) {
	throw DateTimeInvalidDateTime();
    }
    ++parsingAt;

    if (!isdigit(data[parsingAt]) || !isdigit(data[parsingAt+1])) {
	throw DateTimeInvalidDateTime();
    }
    tm.tm_min = strtol((const char *)&data[parsingAt], NULL, 10);
    parsingAt += 2;

    if (':' != data[parsingAt]) {
	throw DateTimeInvalidDateTime();
    }
    ++parsingAt;

    if (!isdigit(data[parsingAt]) || !isdigit(data[parsingAt+1])) {
	throw DateTimeInvalidDateTime();
    }
    tm.tm_sec = strtol((const char *)&data[parsingAt], NULL, 10);
    parsingAt += 2;

    if (' ' != data[parsingAt]) {
	throw DateTimeInvalidDateTime();
    }
    ++parsingAt;

    if ((('-' != data[parsingAt]) && ('+' != data[parsingAt])) ||
	!isdigit(data[parsingAt+1]) || !isdigit(data[parsingAt+2]) || !isdigit(data[parsingAt+3]) || !isdigit(data[parsingAt+4])) {
	throw DateTimeInvalidDateTime();
    }
    zone = strtol((const char *)&data[parsingAt], NULL, 10);
    parsingAt += 5;

    if ((2459 < abs(zone)) || (59 < (abs(zone)%100))) {
	throw DateTimeInvalidDateTime();
    }

    if ('"' != data[parsingAt]) {
	throw DateTimeInvalidDateTime();
    }
    ++parsingAt;

    // Use Zeller's Congruence to calculate the day of the week
    // See, for example http://en.wikipedia.org/wiki/Zeller's_congruence
    int zMonth, zYear;
    int table[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    zYear = (tm.tm_mon < 2) ? tm.tm_year - 1 : tm.tm_year;
    tm.tm_wday = (zYear + zYear / 4 - zYear / 100 + zYear / 400 + table[tm.tm_mon] + tm.tm_mday) % 7;

    // One last correction, tm_year is defined to hold years since 1900, but what's in it at this point
    // is years since zero, so I have to apply a correction.  I can't correct it before now because if I
    // do that, then Zeller's congruence may not work, so I correct it here.
    tm.tm_year -= 1900;

    valid = true;
}


DateTime::~DateTime() {
}

const std::string DateTime::str(void) const throw(DateTimeInvalidDateTime) {
    if (!valid) {
	throw DateTimeInvalidDateTime();
    }
    std::stringstream ss;
    const char *wdayTable[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const char *monthTable[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    ss << wdayTable[tm.tm_wday] << " " << monthTable[tm.tm_mon] << " " <<
	std::setw(2) << tm.tm_mday << " " <<
	std::setw(2) << std::setfill('0') << tm.tm_hour << ":" <<
	std::setw(2) << std::setfill('0') << tm.tm_min << ":" <<
	std::setw(2) << std::setfill('0') << tm.tm_sec << " " <<
	(tm.tm_year + 1900) << " ";
    // I KNOW that zero is neither positive or negative, and so "00000" is mathematically correct,
    // but it time-zonally bogus, so I work around a program broken due to correctness
    if (0 == zone) {
	ss << "+0000";
    }
    else {
	ss << std::setw(5) << std::setfill('0') << std::internal << std::showpos << zone;
    }
    return ss.str();
}


bool DateTime::operator< (DateTime right) {
    time_t tr = mktime(&right.tm);
    time_t tl = mktime(&tm);
    return tl < tr;
}


bool DateTime::operator<= (DateTime right) {
    struct tm tm_right = right.GetTm();
    time_t tr = mktime(&tm_right);
    time_t tl = mktime(&tm);
    return tl <= tr;
}

void DateTime::AddDays(int days) {
    time_t t = mktime(&tm);
    t += (86400 * days);
    localtime_r(&t, &tm);
}
