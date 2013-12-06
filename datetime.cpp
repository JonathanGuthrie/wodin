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

#include <iomanip>
#include <sstream>

#include <string.h>
#include <stdlib.h>

#include "datetime.hpp"

DateTime::DateTime(STRING_FORMAT format) {
    m_format = format;
    time_t t = time(NULL);
    localtime_r(&t, &m_tm);
    m_zone = timezone;
    m_valid = true;
}


DateTime::DateTime(const uint8_t *data, size_t dataLen, size_t &parsingAt, STRING_FORMAT format) throw(DateTimeInvalidDateTime) {
    // The first character should be a double-quote
    if (!parse(data, dataLen, parsingAt, format)) {
	throw DateTimeInvalidDateTime();
    }
}


DateTime::~DateTime() {
}

const std::string DateTime::str(void) const throw(DateTimeInvalidDateTime) {
    if (!m_valid) {
	throw DateTimeInvalidDateTime();
    }
    std::stringstream ss;
    const char *wdayTable[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const char *monthTable[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    switch(m_format) {
    case IMAP:
	ss << std::setw(2) << m_tm.tm_mday << "-" <<
	    monthTable[m_tm.tm_mon] << "-" <<
	    (m_tm.tm_year + 1900) << " " <<
	    std::setw(2) << std::setfill('0') << m_tm.tm_hour << ":" <<
	    std::setw(2) << std::setfill('0') << m_tm.tm_min << ":" <<
	    std::setw(2) << std::setfill('0') << m_tm.tm_sec << " ";
	if (0 < m_zone) {
	    ss << '-';
	}
	else {
	    ss << '+';
	}
	{
	    unsigned z = abs(m_zone);
	    ss << std::setw(2) << std::setfill('0') << z / 3600;
	    ss << std::setw(2) << std::setfill('0') << ((z / 60) % 60);
	}
	break;

    case FROM_LINE:
	ss << wdayTable[m_tm.tm_wday] << " " << monthTable[m_tm.tm_mon] << " " <<
	    std::setw(2) << m_tm.tm_mday << " " <<
	    std::setw(2) << std::setfill('0') << m_tm.tm_hour << ":" <<
	    std::setw(2) << std::setfill('0') << m_tm.tm_min << ":" <<
	    std::setw(2) << std::setfill('0') << m_tm.tm_sec << " " <<
	    (m_tm.tm_year + 1900) << " ";
	ss << std::setw(2) << m_tm.tm_mday << "-" <<
	    monthTable[m_tm.tm_mon] << "-" <<
	    (m_tm.tm_year + 1900) << " " <<
	    std::setw(2) << std::setfill('0') << m_tm.tm_hour << ":" <<
	    std::setw(2) << std::setfill('0') << m_tm.tm_min << ":" <<
	    std::setw(2) << std::setfill('0') << m_tm.tm_sec << " ";
	if (0 < m_zone) {
	    ss << '-';
	}
	else {
	    ss << '+';
	}
	{
	    unsigned z = abs(m_zone);
	    ss << std::setw(2) << std::setfill('0') << z / 3600;
	    ss << std::setw(2) << std::setfill('0') << ((z / 60) % 60);
	}
	break;

    case CTIME:
    default:
	ss << wdayTable[m_tm.tm_wday] << " " << monthTable[m_tm.tm_mon] << " " <<
	    std::setw(2) << m_tm.tm_mday << " " <<
	    std::setw(2) << std::setfill('0') << m_tm.tm_hour << ":" <<
	    std::setw(2) << std::setfill('0') << m_tm.tm_min << ":" <<
	    std::setw(2) << std::setfill('0') << m_tm.tm_sec << " " <<
	    (m_tm.tm_year + 1900) << " ";
	break;
    }
    return ss.str();
}


bool DateTime::operator< (DateTime right) {
    time_t tr = mktime(&right.m_tm);
    time_t tl = mktime(&m_tm);
    return tl < tr;
}


bool DateTime::operator<= (DateTime right) {
    time_t tr = mktime(&right.m_tm);
    time_t tl = mktime(&m_tm);
    return tl <= tr;
}


bool DateTime::operator> (DateTime right) {
    time_t tr = mktime(&right.m_tm);
    time_t tl = mktime(&m_tm);
    return tl > tr;
}


bool DateTime::operator>= (DateTime right) {
    time_t tr = mktime(&right.m_tm);
    time_t tl = mktime(&m_tm);
    return tl >= tr;
}


void DateTime::addDays(int days) {
    time_t t = mktime(&m_tm);
    t += (86400 * days);
    localtime_r(&t, &m_tm);
}

// ctime returns
// DOW MMM DD hh:mm:ss YYYY
// Note, no time zone.
bool DateTime::parseCtime(const uint8_t *data, size_t dataLen, size_t &parsingAt) {
    (void) dataLen;

    m_valid = false;
    // Day of week
    // tuesday and thursday
    if ('T' == toupper(data[parsingAt])) {
	if (('U' == toupper(data[parsingAt+1])) && ('E' == toupper(data[parsingAt+2]))) {
	    m_tm.tm_wday = 2;
	}
	else if (('H' == toupper(data[parsingAt+1])) && ('U' == toupper(data[parsingAt+2]))) {
	    m_tm.tm_wday = 4;
	}
	else {
	    return false;
	}
    }
    else if ('S' == toupper(data[parsingAt])) {
	if (('A' == toupper(data[parsingAt+1])) && ('T' == toupper(data[parsingAt+2]))) {
	    m_tm.tm_wday = 6;
	}
	else if (('U' == toupper(data[parsingAt+1])) && ('N' == toupper(data[parsingAt+2]))) {
	    m_tm.tm_wday = 0;
	}
	else {
	    return false;
	}
    }
    else if (('M' == toupper(data[parsingAt])) && ('O' == toupper(data[parsingAt+1])) && ('N' == toupper(data[parsingAt+2]))) {
	m_tm.tm_wday = 1;
    }
    else if (('W' == toupper(data[parsingAt])) && ('E' == toupper(data[parsingAt+1])) && ('D' == toupper(data[parsingAt+2]))) {
	m_tm.tm_wday = 3;
    }
    else if (('F' == toupper(data[parsingAt])) && ('R' == toupper(data[parsingAt+1])) && ('I' == toupper(data[parsingAt+2]))) {
	m_tm.tm_wday = 5;
    }
    else {
	return false;
    }

    if (' ' != data[parsingAt]) {
	return false;
    }
    ++parsingAt;

    // Check Jan, Jun, and Jul first
    if ('J' == toupper(data[parsingAt])) {
	// Jun or Jul
	if ('U' == toupper(data[parsingAt+1])) {
	    if ('N' == toupper(data[parsingAt+2])) {
		m_tm.tm_mon = 5;
	    }
	    else if ('L' == toupper(data[parsingAt+2])) {
		m_tm.tm_mon = 6;
	    }
	    else {
		return false;
	    }
	}
	else if (('A' == toupper(data[parsingAt+1])) && ('N' == toupper(data[parsingAt+2]))) {
	    m_tm.tm_mon = 0;
	}
	else {
	    return false;
	}
    }
    // Next, Mar and May
    else if (('M' == toupper(data[parsingAt])) && ('A' == toupper(data[parsingAt+1]))) {
	if ('R' == toupper(data[parsingAt+2])) {
	    m_tm.tm_mon = 2;
	}
	else if ('Y' == toupper(data[parsingAt+2])) {
	    m_tm.tm_mon = 4;
	}
	else {
	    return false;
	}
    }
    // Next Apr and Aug
    else if ('A' == toupper(data[parsingAt])) {
	if (('P' == toupper(data[parsingAt+1])) && ('R' == toupper(data[parsingAt+2]))) {
	    m_tm.tm_mon = 3;
	}
	else {
	    if (('U' == toupper(data[parsingAt+1])) && ('G' == toupper(data[parsingAt+2]))) {
		m_tm.tm_mon = 7;
	    }
	    else {
		return false;
	    }
	}
    }
    // Feb
    else if (('F' == toupper(data[parsingAt])) && ('E' == toupper(data[parsingAt+1])) && ('B' == toupper(data[parsingAt+2]))) {
	m_tm.tm_mon = 1;
    }
    // Sep
    else if (('S' == toupper(data[parsingAt])) && ('E' == toupper(data[parsingAt+1])) && ('P' == toupper(data[parsingAt+2]))) {
	m_tm.tm_mon = 8;
    }
    // Oct
    else if (('O' == toupper(data[parsingAt])) && ('C' == toupper(data[parsingAt+1])) && ('T' == toupper(data[parsingAt+2]))) {
	m_tm.tm_mon = 9;
    }
    // Nov
    else if (('N' == toupper(data[parsingAt])) && ('O' == toupper(data[parsingAt+1])) && ('V' == toupper(data[parsingAt+2]))) {
	m_tm.tm_mon = 10;
    }
    // Dec
    else if (('D' == toupper(data[parsingAt])) && ('E' == toupper(data[parsingAt+1])) && ('C' == toupper(data[parsingAt+2]))) {
	m_tm.tm_mon = 11;
    }
    else {
	return false;
    }
    parsingAt += 3;

    if (' ' != data[parsingAt]) {
	return false;
    }
    ++parsingAt;

    m_tm.tm_mday = 0;
    if (' ' == data[parsingAt]) {
	++parsingAt;
	if (isdigit(data[parsingAt])) {
	    m_tm.tm_mday = data[parsingAt] - '0';
	    ++parsingAt;
	}
	else {
	    return false;
	}
    }
    else {
	if (isdigit(data[parsingAt])) {
	    m_tm.tm_mday = data[parsingAt] - '0';
	    ++parsingAt;
	    if (isdigit(data[parsingAt])) {
		m_tm.tm_mday = m_tm.tm_mday * 10 + data[parsingAt] - '0';
		++parsingAt;
	    }
	}
	else {
	    return false;
	}
    }
    if (' ' != data[parsingAt]) {
	return false;
    }
    ++parsingAt;

    m_tm.tm_hour = 0;
    if (' ' == data[parsingAt]) {
	++parsingAt;
	if (isdigit(data[parsingAt])) {
	    m_tm.tm_hour = data[parsingAt] - '0';
	    ++parsingAt;
	}
	else {
	    return false;
	}
    }
    else {
	if (isdigit(data[parsingAt])) {
	    m_tm.tm_hour = data[parsingAt] - '0';
	    ++parsingAt;
	    if (isdigit(data[parsingAt])) {
		m_tm.tm_hour = m_tm.tm_hour * 10 + data[parsingAt] - '0';
		++parsingAt;
	    }
	}
	else {
	    return false;
	}
    }
    if (':' != data[parsingAt]) {
	return false;
    }

    m_tm.tm_min = 0;
    if (isdigit(data[parsingAt])) {
	m_tm.tm_min = data[parsingAt] - '0';
	++parsingAt;
	if (isdigit(data[parsingAt])) {
	    m_tm.tm_min = m_tm.tm_min * 10 + data[parsingAt] - '0';
	    ++parsingAt;
	}
    }
    else {
	return false;
    }

    m_tm.tm_sec = 0;
    if (':' == data[parsingAt]) {
	if (isdigit(data[parsingAt])) {
	    m_tm.tm_sec = data[parsingAt] - '0';
	    ++parsingAt;
	    if (isdigit(data[parsingAt])) {
		m_tm.tm_sec = m_tm.tm_sec * 10 + data[parsingAt] - '0';
		++parsingAt;
	    }
	}
	else {
	    return false;
	}
    }
    if (' ' != data[parsingAt]) {
	return false;
    }
    ++parsingAt;

    m_tm.tm_year = (int) strtol((char *)&data[parsingAt], NULL, 10);


    // 30 days hath september, april, june, and november
    // all the rest have 31 except february
    if (1 == m_tm.tm_mon) {
	if ((29 < m_tm.tm_mday) ||
	    ((29 == m_tm.tm_mday) &&
	     ((0 != (m_tm.tm_year % 4)) || ((0 == m_tm.tm_year % 100) && (0 != m_tm.tm_year % 400))))) {
	    return false;
	}
    }
    else {
	if ((8 == m_tm.tm_mon) || (3 == m_tm.tm_mon) || (5 == m_tm.tm_mon) || (10 == m_tm.tm_mon)) {
	    if (30 < m_tm.tm_mday) {
		return false;
	    }
	}
	else {
	    if (31 < m_tm.tm_mday) {
		return false;
	    }
	}
    }

    // One last correction, tm_year is defined to hold years since 1900, but what's in it at this point
    // is years since zero, so I have to apply a correction.  I can't correct it before now because if I
    // do that, then Zeller's congruence may not work, so I correct it here.
    m_tm.tm_year -= 1900;

    tzset();
    m_zone = timezone;
    m_valid = true;
    return true;
}

// I start with the pointer at a double quote, and I should end after the other double-quote
// A date-time string is "DD-MMM-YYYY HH:MM:SS +ZZZZ" (quotes not included)
// That looks to be 11+2+8+5 = 26 characters
bool DateTime::parseImap(const uint8_t *data, size_t dataLen, size_t &parsingAt) {
    if (26 > (dataLen - parsingAt)) {
	return false;
    }

    if (((' ' != data[parsingAt]) && !isdigit(data[parsingAt])) ||
	!isdigit(data[parsingAt+1])) {
	return false;
    }
    m_tm.tm_mday = strtol((const char *)&data[parsingAt], NULL, 10);
    parsingAt += 2;

    if ((m_tm.tm_mday < 1) || (m_tm.tm_mday > 31) || ('-' != data[parsingAt])) {
	return false;
    }
    ++parsingAt;

    // Check Jan, Jun, and Jul first
    if ('J' == toupper(data[parsingAt])) {
	// Jun or Jul
	if ('U' == toupper(data[parsingAt+1])) {
	    if ('N' == toupper(data[parsingAt+2])) {
		if (m_tm.tm_mday > 30) {
		    return false;
		}
		m_tm.tm_mon = 5;
	    }
	    else if ('L' == toupper(data[parsingAt+2])) {
		m_tm.tm_mon = 6;
	    }
	    else {
		return false;
	    }
	}
	else if (('A' == toupper(data[parsingAt+1])) && ('N' == toupper(data[parsingAt+2]))) {
	    m_tm.tm_mon = 0;
	}
	else {
	    return false;
	}
    }
    // Next, Mar and May
    else if (('M' == toupper(data[parsingAt])) && ('A' == toupper(data[parsingAt+1]))) {
	if ('R' == toupper(data[parsingAt+2])) {
	    m_tm.tm_mon = 2;
	}
	else if ('Y' == toupper(data[parsingAt+2])) {
	    m_tm.tm_mon = 4;
	}
	else {
	    return false;
	}
    }
    // Next Apr and Aug
    else if ('A' == toupper(data[parsingAt])) {
	if (('P' == toupper(data[parsingAt+1])) && ('R' == toupper(data[parsingAt+2]))) {
	    if (m_tm.tm_mday > 30) {
		return false;
	    }
	    m_tm.tm_mon = 3;
	}
	else {
	    if (('U' == toupper(data[parsingAt+1])) && ('G' == toupper(data[parsingAt+2]))) {
		m_tm.tm_mon = 7;
	    }
	    else {
		return false;
	    }
	}
    }
    // Feb
    else if (('F' == toupper(data[parsingAt])) && ('E' == toupper(data[parsingAt+1])) && ('B' == toupper(data[parsingAt+2]))) {
	m_tm.tm_mon = 1;
	if (m_tm.tm_mday > 29) {
	    return false;
	}
    }
    // Sep
    else if (('S' == toupper(data[parsingAt])) && ('E' == toupper(data[parsingAt+1])) && ('P' == toupper(data[parsingAt+2]))) {
	if (m_tm.tm_mday > 30) {
	    return false;
	}
	m_tm.tm_mon = 8;
    }
    // Oct
    else if (('O' == toupper(data[parsingAt])) && ('C' == toupper(data[parsingAt+1])) && ('T' == toupper(data[parsingAt+2]))) {
	m_tm.tm_mon = 9;
    }
    // Nov
    else if (('N' == toupper(data[parsingAt])) && ('O' == toupper(data[parsingAt+1])) && ('V' == toupper(data[parsingAt+2]))) {
	if (m_tm.tm_mday > 30) {
	    return false;
	}
	m_tm.tm_mon = 10;
    }
    // Dec
    else if (('D' == toupper(data[parsingAt])) && ('E' == toupper(data[parsingAt+1])) && ('C' == toupper(data[parsingAt+2]))) {
	m_tm.tm_mon = 11;
    }
    else {
	return false;
    }
    parsingAt += 3;

    if ('-' != data[parsingAt]) {
	return false;
    }
    ++parsingAt;

    if (!isdigit(data[parsingAt]) || !isdigit(data[parsingAt+1]) || !isdigit(data[parsingAt+2]) || !isdigit(data[parsingAt+3])) {
	return false;
    }
    m_tm.tm_year = strtol((const char *)&data[parsingAt], NULL, 10);
    parsingAt += 4;

    // If I'm looking at Feb 29, then it needs to be a leap year
    if ((1 == m_tm.tm_mon) && (29 == m_tm.tm_mday) &&
	((0 != (m_tm.tm_year % 4)) || ((0 == m_tm.tm_year % 100) && (0 != m_tm.tm_year % 400)))) {
	return false;
    }

    if (' ' != data[parsingAt]) {
	return false;
    }
    ++parsingAt;

    if (!isdigit(data[parsingAt]) || !isdigit(data[parsingAt+1])) {
	return false;
    }
    m_tm.tm_hour = strtol((const char *)&data[parsingAt], NULL, 10);
    parsingAt += 2;

    if (':' != data[parsingAt]) {
	return false;
    }
    ++parsingAt;

    if (!isdigit(data[parsingAt]) || !isdigit(data[parsingAt+1])) {
	return false;
    }
    m_tm.tm_min = strtol((const char *)&data[parsingAt], NULL, 10);
    parsingAt += 2;

    if (':' != data[parsingAt]) {
	return false;
    }
    ++parsingAt;

    if (!isdigit(data[parsingAt]) || !isdigit(data[parsingAt+1])) {
	return false;
    }
    m_tm.tm_sec = strtol((const char *)&data[parsingAt], NULL, 10);
    parsingAt += 2;

    if (' ' != data[parsingAt]) {
	return false;
    }
    ++parsingAt;

    if ((('-' == data[parsingAt]) || ('+' == data[parsingAt])) &&
	isdigit(data[parsingAt+1]) && isdigit(data[parsingAt+2]) && isdigit(data[parsingAt+3]) && isdigit(data[parsingAt+4])) {
	// m_zone is seconds west of UTC but the string is +HHMM east of UTC
	m_zone = 3600 * (10 * (data[parsingAt+1] - '0') + data[parsingAt+2] - '0') + 60 * (10 * (data[parsingAt+3] - '0') + data[parsingAt+4] - '0');
	if ('+' == data[parsingAt]) {
	    m_zone *= -1;
	}
    }
    else {
	return false;
    }
    parsingAt += 5;

    // Use Zeller's Congruence to calculate the day of the week
    // See, for example http://en.wikipedia.org/wiki/Zeller's_congruence
    int zYear;
    int table[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    zYear = (m_tm.tm_mon < 2) ? m_tm.tm_year - 1 : m_tm.tm_year;
    m_tm.tm_wday = (zYear + zYear / 4 - zYear / 100 + zYear / 400 + table[m_tm.tm_mon] + m_tm.tm_mday) % 7;

    // One last correction, tm_year is defined to hold years since 1900, but what's in it at this point
    // is years since zero, so I have to apply a correction.  I can't correct it before now because if I
    // do that, then Zeller's congruence may not work, so I correct it here.
    m_tm.tm_year -= 1900;

    m_valid = true;
    return true;
}

/*
 * date-time = [ day-of-week "," ] date FWS time [CFWS]
 *
 * day-of-week = ([FWS] day-name) / obs-day-of-week
 *
 * day-name = "Mon" / "Tue" / "Wed" / "Thu" / "Fri" / "Sat" / "Sun"
 *
 * date = day month year
 *
 * year = 4*DIGIT / obs-year
 *
 * month = (FWS month-name FWS) / obs-month
 *
 * month-name = "Jan" / "Feb" / "Mar" / "Apr" /
 *              "May" / "Jun" / "Jul" / "Aug" /
 *              "Sep" / "Oct" / "Nov" / "Dec"
 *
 * day = [FWS] 1*2DIGIT / obs-day
 *
 * time = time-of-day FWS zone
 *
 * time-of-day = hour ":" minute [ ":" second ]
 *
 * hour = 2DIGIT / obs-hour
 *
 * minute = 2DIGIT / obs-minute
 *
 * second = 2DIGIT / obs-second
 *
 * zone = (( "+" / "-") 4DIGIT) / obs-zone
 *
 * obs-day-of-week = [CFWS] day-name [CFWS]
 *
 * obs-year = [CFWS] 2*DIGIT [CFWS]
 *
 * obs-month = CFWS month-name CFWS
 *
 * obs-day = [CFWS] 1*2DIGIT [CFWS]
 *
 * obs-hour = [CFWS] 2DIGIT [CFWS]
 *
 * obs-minute = [CFWS] 2DIGIT [CFWS]
 *
 * obs-second = [CFWS] 2DIGIT [CFWS]
 *
 * obs-zone = "UT" / "GMT" / "EST" / "EDT" / "CST" / "CDT" /
 *            "MST" / "MDT" / "PST" / "PDT" / %d65-73 / %d75-90
 *	      %d97-105 / %d107-122
 *
 * If a two digit year is encoutered, it is interpreted as being the year less 2000 if
 * the digits represent a number less than 50, otherwise it is interpreted as being the
 * year less 1900.  A three digit year is interpreted as being the year less 1900.
 * the single-letter time zones, UT, and GMT are the same as +0000, the EDT is -0400,
 * EST and CDT are -0500, CST and MDT are -0600, MST and PDT are -0700, and PST is -0800
 *
 * Okay, except for the two, three, and four-digit years and the time zone wierdness (and
 * the time zone is important because it's essential for comparing dates) it appears as if
 * the obsolete fields are the same as the current fields.
 */
bool DateTime::parseRfc822(const uint8_t *data, size_t dataLen, size_t &parsingAt) {
    (void) dataLen;

    m_valid = false;
    // Day of week
    // tuesday and thursday
    if (NULL != strchr((const char *)&data[parsingAt], ',')) {
	if ('T' == toupper(data[parsingAt])) {
	    if (('U' == toupper(data[parsingAt+1])) && ('E' == toupper(data[parsingAt+2]))) {
		m_tm.tm_wday = 2;
	    }
	    else if (('H' == toupper(data[parsingAt+1])) && ('U' == toupper(data[parsingAt+2]))) {
		m_tm.tm_wday = 4;
	    }
	    else {
		return false;
	    }
	}
	else if ('S' == toupper(data[parsingAt])) {
	    if (('A' == toupper(data[parsingAt+1])) && ('T' == toupper(data[parsingAt+2]))) {
		m_tm.tm_wday = 6;
	    }
	    else if (('U' == toupper(data[parsingAt+1])) && ('N' == toupper(data[parsingAt+2]))) {
		m_tm.tm_wday = 0;
	    }
	    else {
		return false;
	    }
	}
	else if (('M' == toupper(data[parsingAt])) && ('O' == toupper(data[parsingAt+1])) && ('N' == toupper(data[parsingAt+2]))) {
	    m_tm.tm_wday = 1;
	}
	else if (('W' == toupper(data[parsingAt])) && ('E' == toupper(data[parsingAt+1])) && ('D' == toupper(data[parsingAt+2]))) {
	    m_tm.tm_wday = 3;
	}
	else if (('F' == toupper(data[parsingAt])) && ('R' == toupper(data[parsingAt+1])) && ('I' == toupper(data[parsingAt+2]))) {
	    m_tm.tm_wday = 5;
	}
	else {
	    return false;
	}
	if (',' != data[parsingAt]) {
	    return false;
	}
	++parsingAt;
    }

    if (' ' != data[parsingAt]) {
	return false;
    }
    while (' ' == data[parsingAt]) {
	++parsingAt;
    }

    m_tm.tm_mday = 0;
    if (!isdigit(data[parsingAt])) {
	return false;
    }
    else {
	m_tm.tm_mday = data[parsingAt] - '0';
	++parsingAt;
	if (isdigit(data[parsingAt])) {
	    m_tm.tm_mday = m_tm.tm_mday * 10 + data[parsingAt] - '0';
	    ++parsingAt;
	}
    }
    if (' ' != data[parsingAt]) {
	return false;
    }

    m_tm.tm_mday = 0;
    if (' ' == data[parsingAt]) {
	++parsingAt;
	if (isdigit(data[parsingAt])) {
	    m_tm.tm_mday = data[parsingAt] - '0';
	    ++parsingAt;
	}
	else {
	    return false;
	}
    }
    else {
	if (isdigit(data[parsingAt])) {
	    m_tm.tm_mday = data[parsingAt] - '0';
	    ++parsingAt;
	    if (isdigit(data[parsingAt])) {
		m_tm.tm_mday = m_tm.tm_mday * 10 + data[parsingAt] - '0';
		++parsingAt;
	    }
	}
	else {
	    return false;
	}
    }
    if (' ' != data[parsingAt]) {
	return false;
    }
    while (' ' == data[parsingAt]) {
	++parsingAt;
    }

    // Check Jan, Jun, and Jul first
    if ('J' == toupper(data[parsingAt])) {
	// Jun or Jul
	if ('U' == toupper(data[parsingAt+1])) {
	    if ('N' == toupper(data[parsingAt+2])) {
		m_tm.tm_mon = 5;
	    }
	    else if ('L' == toupper(data[parsingAt+2])) {
		m_tm.tm_mon = 6;
	    }
	    else {
		return false;
	    }
	}
	else if (('A' == toupper(data[parsingAt+1])) && ('N' == toupper(data[parsingAt+2]))) {
	    m_tm.tm_mon = 0;
	}
	else {
	    return false;
	}
    }
    // Next, Mar and May
    else if (('M' == toupper(data[parsingAt])) && ('A' == toupper(data[parsingAt+1]))) {
	if ('R' == toupper(data[parsingAt+2])) {
	    m_tm.tm_mon = 2;
	}
	else if ('Y' == toupper(data[parsingAt+2])) {
	    m_tm.tm_mon = 4;
	}
	else {
	    return false;
	}
    }
    // Next Apr and Aug
    else if ('A' == toupper(data[parsingAt])) {
	if (('P' == toupper(data[parsingAt+1])) && ('R' == toupper(data[parsingAt+2]))) {
	    m_tm.tm_mon = 3;
	}
	else {
	    if (('U' == toupper(data[parsingAt+1])) && ('G' == toupper(data[parsingAt+2]))) {
		m_tm.tm_mon = 7;
	    }
	    else {
		return false;
	    }
	}
    }
    // Feb
    else if (('F' == toupper(data[parsingAt])) && ('E' == toupper(data[parsingAt+1])) && ('B' == toupper(data[parsingAt+2]))) {
	m_tm.tm_mon = 1;
    }
    // Sep
    else if (('S' == toupper(data[parsingAt])) && ('E' == toupper(data[parsingAt+1])) && ('P' == toupper(data[parsingAt+2]))) {
	m_tm.tm_mon = 8;
    }
    // Oct
    else if (('O' == toupper(data[parsingAt])) && ('C' == toupper(data[parsingAt+1])) && ('T' == toupper(data[parsingAt+2]))) {
	m_tm.tm_mon = 9;
    }
    // Nov
    else if (('N' == toupper(data[parsingAt])) && ('O' == toupper(data[parsingAt+1])) && ('V' == toupper(data[parsingAt+2]))) {
	m_tm.tm_mon = 10;
    }
    // Dec
    else if (('D' == toupper(data[parsingAt])) && ('E' == toupper(data[parsingAt+1])) && ('C' == toupper(data[parsingAt+2]))) {
	m_tm.tm_mon = 11;
    }
    else {
	return false;
    }
    parsingAt += 3;

    if (' ' != data[parsingAt]) {
	return false;
    }
    while (' ' == data[parsingAt]) {
	++parsingAt;
    }

    m_tm.tm_year = (int) strtol((char *)&data[parsingAt], NULL, 10);
    while (isdigit(data[parsingAt])) {
	++parsingAt;
    }
    if (' ' != data[parsingAt]) {
	return false;
    }
    while (' ' == data[parsingAt]) {
	++parsingAt;
    }
    if (m_tm.tm_year < 1000) {
	m_tm.tm_year += 1900;
    }
    else if (m_tm.tm_year < 100) {
	if (m_tm.tm_year < 50) {
	    m_tm.tm_year += 2000;
	}
	else {
	    m_tm.tm_year += 1900;
	}
    }

    // 30 days hath september, april, june, and november
    // all the rest have 31 except february
    if (1 == m_tm.tm_mon) {
	if ((29 < m_tm.tm_mday) ||
	    ((29 == m_tm.tm_mday) &&
	     ((0 != (m_tm.tm_year % 4)) || ((0 == m_tm.tm_year % 100) && (0 != m_tm.tm_year % 400))))) {
	    return false;
	}
    }
    else {
	if ((8 == m_tm.tm_mon) || (3 == m_tm.tm_mon) || (5 == m_tm.tm_mon) || (10 == m_tm.tm_mon)) {
	    if (30 < m_tm.tm_mday) {
		return false;
	    }
	}
	else {
	    if (31 < m_tm.tm_mday) {
		return false;
	    }
	}
    }

    m_tm.tm_hour = 0;
    if (' ' == data[parsingAt]) {
	++parsingAt;
	if (isdigit(data[parsingAt])) {
	    m_tm.tm_hour = data[parsingAt] - '0';
	    ++parsingAt;
	}
	else {
	    return false;
	}
    }
    else {
	if (isdigit(data[parsingAt])) {
	    m_tm.tm_hour = data[parsingAt] - '0';
	    ++parsingAt;
	    if (isdigit(data[parsingAt])) {
		m_tm.tm_hour = m_tm.tm_hour * 10 + data[parsingAt] - '0';
		++parsingAt;
	    }
	}
	else {
	    return false;
	}
    }
    if (':' != data[parsingAt]) {
	return false;
    }

    m_tm.tm_min = 0;
    if (isdigit(data[parsingAt])) {
	m_tm.tm_min = data[parsingAt] - '0';
	++parsingAt;
	if (isdigit(data[parsingAt])) {
	    m_tm.tm_min = m_tm.tm_min * 10 + data[parsingAt] - '0';
	    ++parsingAt;
	}
    }
    else {
	return false;
    }

    m_tm.tm_sec = 0;
    if (':' == data[parsingAt]) {
	if (isdigit(data[parsingAt])) {
	    m_tm.tm_sec = data[parsingAt] - '0';
	    ++parsingAt;
	    if (isdigit(data[parsingAt])) {
		m_tm.tm_sec = m_tm.tm_sec * 10 + data[parsingAt] - '0';
		++parsingAt;
	    }
	}
	else {
	    return false;
	}
    }
    if (' ' != data[parsingAt]) {
	return false;
    }
    while (' ' == data[parsingAt]) {
	++parsingAt;
    }

    m_zone = 99999;
    if ('E' == toupper(data[parsingAt])) {
	if (('D' == toupper(data[parsingAt+1])) && ('T' == toupper(data[parsingAt+2]))) {
	    m_zone = 14400;
	}
	else if (('S' == toupper(data[parsingAt+1])) && ('T' == toupper(data[parsingAt+2]))) {
	    m_zone = 18000;
	}
	else {
	    return false;
	}
	if (' ' != data[parsingAt+3]) {
	    return false;
	}
	parsingAt += 4;
    }
    else if ('C' == toupper(data[parsingAt])) {
	if (('D' == toupper(data[parsingAt+1])) && ('T' == toupper(data[parsingAt+2]))) {
	    m_zone = 18000;
	}
	else if (('S' == toupper(data[parsingAt+1])) && ('T' == toupper(data[parsingAt+2]))) {
	    m_zone = 21600;
	}
	else {
	    return false;
	}
	if (' ' != data[parsingAt+3]) {
	    return false;
	}
	parsingAt += 4;
    }
    else if ('M' == toupper(data[parsingAt])) {
	if (('D' == toupper(data[parsingAt+1])) && ('T' == toupper(data[parsingAt+2]))) {
	    m_zone = 21600;
	}
	else if (('S' == toupper(data[parsingAt+1])) && ('T' == toupper(data[parsingAt+2]))) {
	    m_zone = 25200;
	}
	else {
	    return false;
	}
	if (' ' != data[parsingAt+3]) {
	    return false;
	}
	parsingAt += 4;
    }
    else if ('P' == toupper(data[parsingAt])) {
	if (('D' == toupper(data[parsingAt+1])) && ('T' == toupper(data[parsingAt+2]))) {
	    m_zone = 25200;
	}
	else if (('S' == toupper(data[parsingAt+1])) && ('T' == toupper(data[parsingAt+2]))) {
	    m_zone = 28800;
	}
	else {
	    return false;
	}
	if (' ' != data[parsingAt+3]) {
	    return false;
	}
	parsingAt += 4;
    }
    else if (('-' == data[parsingAt]) || ('+' == data[parsingAt])) {
	if (isdigit(data[parsingAt+1]) && isdigit(data[parsingAt+2]) && isdigit(data[parsingAt+3]) && isdigit(data[parsingAt+4])) {
	    // m_zone is seconds west of UTC but the string is +HHMM east of UTC
	    m_zone = 3600 * (10 * (data[parsingAt+1] - '0') + data[parsingAt+2] - '0') + 60 * (10 * (data[parsingAt+3] - '0') + data[parsingAt+4] - '0');
	    if ('+' == data[parsingAt]) {
		m_zone *= -1;
	    }
	}
	if (' ' != data[parsingAt+5]) {
	    return false;
	}
	parsingAt += 6;
    }
    if (99999 == m_zone) {
	return false;
    }

    // One last correction, tm_year is defined to hold years since 1900, but what's in it at this point
    // is years since zero, so I have to apply a correction.  I can't correct it before now because if I
    // do that, then Zeller's congruence may not work, so I correct it here.
    m_tm.tm_year -= 1900;

    m_valid = true;
    return true;
}

/*
 * From section 6.12 of the IMAP Toolkit Frequently Asked Questions:
 *
 * You just answered your own question. If any line that starts with "From "
 * is treated as the start of a message, then every message text line which
 * starts with "From " has to be quoted (typically by prefixing a ">"
 * character). People complain about this -- "why did a > get stuck in my
 * message?"
 *
 * So, good mail reading software only considers a line to be a "From " line
 * if it follows the actual specification for a "From " line. This means,
 * among other things, that the day of week is fixed-format: "May 14", but
 * "May  7" (note the extra space) as opposed to "May 7". ctime() format for
 * the date is the most common, although POSIX also allows a numeric timezone
 * after the year. For compatibility with ancient software, the seconds are
 * optional, the timezone may appear before the year, the old 3-letter
 * timezones are also permitted, and "remote from xxx" may appear after the
 * whole thing.
 *
 * Unfortunately, some software written by novices use other formats. The
 * most common error is to have a variable-width day of month, perhaps in
 * the erroneous belief that RFC 2822 (or RFC 822) defines the format of
 * the date/time in the "From " line (it doesn't; no RFC describes internal
 * formats). I've seen a few other goofs, such as a single-digit second,
 * but these are less common.
 *
 * If you are writing your own software that writes mailbox files, and you
 * really aren't all that savvy with all the ins and outs and ancient
 * history, you should seriously consider using the c-client library (e.g.
 * routine mail_append()) instead of doing the file writes yourself. If
 * you must do it yourself, use ctime(), as in:
 *
 * fprintf (mbx,"From %s@%h %s",user,host,ctime (time (0)));
 *
 * rather than try to figure out a good format yourself. ctime() is the
 * most traditional format and nobody will flame you for using it.
 */

bool DateTime::parseFromLine(const uint8_t *data, size_t dataLen, size_t &parsingAt) {
    (void) dataLen;

    m_valid = false;

    // It appears as if PostFix puts an extra space before the date
    if (' ' == data[parsingAt]) {
	++parsingAt;
    }

    // Day of week
    // tuesday and thursday
    if ('T' == toupper(data[parsingAt])) {
	if (('U' == toupper(data[parsingAt+1])) && ('E' == toupper(data[parsingAt+2]))) {
	    m_tm.tm_wday = 2;
	}
	else if (('H' == toupper(data[parsingAt+1])) && ('U' == toupper(data[parsingAt+2]))) {
	    m_tm.tm_wday = 4;
	}
	else {
	    return false;
	}
    }
    else if ('S' == toupper(data[parsingAt])) {
	if (('A' == toupper(data[parsingAt+1])) && ('T' == toupper(data[parsingAt+2]))) {
	    m_tm.tm_wday = 6;
	}
	else if (('U' == toupper(data[parsingAt+1])) && ('N' == toupper(data[parsingAt+2]))) {
	    m_tm.tm_wday = 0;
	}
	else {
	    return false;
	}
    }
    else if (('M' == toupper(data[parsingAt])) && ('O' == toupper(data[parsingAt+1])) && ('N' == toupper(data[parsingAt+2]))) {
	m_tm.tm_wday = 1;
    }
    else if (('W' == toupper(data[parsingAt])) && ('E' == toupper(data[parsingAt+1])) && ('D' == toupper(data[parsingAt+2]))) {
	m_tm.tm_wday = 3;
    }
    else if (('F' == toupper(data[parsingAt])) && ('R' == toupper(data[parsingAt+1])) && ('I' == toupper(data[parsingAt+2]))) {
	m_tm.tm_wday = 5;
    }
    else {
	return false;
    }
    parsingAt += 3;

    if (' ' != data[parsingAt]) {
	return false;
    }
    ++parsingAt;

    // Check Jan, Jun, and Jul first
    if ('J' == toupper(data[parsingAt])) {
	// Jun or Jul
	if ('U' == toupper(data[parsingAt+1])) {
	    if ('N' == toupper(data[parsingAt+2])) {
		m_tm.tm_mon = 5;
	    }
	    else if ('L' == toupper(data[parsingAt+2])) {
		m_tm.tm_mon = 6;
	    }
	    else {
		return false;
	    }
	}
	else if (('A' == toupper(data[parsingAt+1])) && ('N' == toupper(data[parsingAt+2]))) {
	    m_tm.tm_mon = 0;
	}
	else {
	    return false;
	}
    }
    // Next, Mar and May
    else if (('M' == toupper(data[parsingAt])) && ('A' == toupper(data[parsingAt+1]))) {
	if ('R' == toupper(data[parsingAt+2])) {
	    m_tm.tm_mon = 2;
	}
	else if ('Y' == toupper(data[parsingAt+2])) {
	    m_tm.tm_mon = 4;
	}
	else {
	    return false;
	}
    }
    // Next Apr and Aug
    else if ('A' == toupper(data[parsingAt])) {
	if (('P' == toupper(data[parsingAt+1])) && ('R' == toupper(data[parsingAt+2]))) {
	    m_tm.tm_mon = 3;
	}
	else {
	    if (('U' == toupper(data[parsingAt+1])) && ('G' == toupper(data[parsingAt+2]))) {
		m_tm.tm_mon = 7;
	    }
	    else {
		return false;
	    }
	}
    }
    // Feb
    else if (('F' == toupper(data[parsingAt])) && ('E' == toupper(data[parsingAt+1])) && ('B' == toupper(data[parsingAt+2]))) {
	m_tm.tm_mon = 1;
    }
    // Sep
    else if (('S' == toupper(data[parsingAt])) && ('E' == toupper(data[parsingAt+1])) && ('P' == toupper(data[parsingAt+2]))) {
	m_tm.tm_mon = 8;
    }
    // Oct
    else if (('O' == toupper(data[parsingAt])) && ('C' == toupper(data[parsingAt+1])) && ('T' == toupper(data[parsingAt+2]))) {
	m_tm.tm_mon = 9;
    }
    // Nov
    else if (('N' == toupper(data[parsingAt])) && ('O' == toupper(data[parsingAt+1])) && ('V' == toupper(data[parsingAt+2]))) {
	m_tm.tm_mon = 10;
    }
    // Dec
    else if (('D' == toupper(data[parsingAt])) && ('E' == toupper(data[parsingAt+1])) && ('C' == toupper(data[parsingAt+2]))) {
	m_tm.tm_mon = 11;
    }
    else {
	return false;
    }
    parsingAt += 3;

    if (' ' != data[parsingAt]) {
	return false;
    }
    ++parsingAt;

    m_tm.tm_mday = 0;
    if (' ' == data[parsingAt]) {
	++parsingAt;
	if (isdigit(data[parsingAt])) {
	    m_tm.tm_mday = data[parsingAt] - '0';
	    ++parsingAt;
	}
	else {
	    return false;
	}
    }
    else {
	if (isdigit(data[parsingAt])) {
	    m_tm.tm_mday = data[parsingAt] - '0';
	    ++parsingAt;
	    if (isdigit(data[parsingAt])) {
		m_tm.tm_mday = m_tm.tm_mday * 10 + data[parsingAt] - '0';
		++parsingAt;
	    }
	}
	else {
	    return false;
	}
    }
    if (' ' != data[parsingAt]) {
	return false;
    }
    ++parsingAt;

    m_tm.tm_hour = 0;
    if (' ' == data[parsingAt]) {
	++parsingAt;
	if (isdigit(data[parsingAt])) {
	    m_tm.tm_hour = data[parsingAt] - '0';
	    ++parsingAt;
	}
	else {
	    return false;
	}
    }
    else {
	if (isdigit(data[parsingAt])) {
	    m_tm.tm_hour = data[parsingAt] - '0';
	    ++parsingAt;
	    if (isdigit(data[parsingAt])) {
		m_tm.tm_hour = m_tm.tm_hour * 10 + data[parsingAt] - '0';
		++parsingAt;
	    }
	}
	else {
	    return false;
	}
    }
    if (':' != data[parsingAt]) {
	return false;
    }
    ++parsingAt;

    m_tm.tm_min = 0;
    if (isdigit(data[parsingAt])) {
	m_tm.tm_min = data[parsingAt] - '0';
	++parsingAt;
	if (isdigit(data[parsingAt])) {
	    m_tm.tm_min = m_tm.tm_min * 10 + data[parsingAt] - '0';
	    ++parsingAt;
	}
    }
    else {
	return false;
    }

    m_tm.tm_sec = 0;
    if (':' == data[parsingAt]) {
	++parsingAt;
	if (isdigit(data[parsingAt])) {
	    m_tm.tm_sec = data[parsingAt] - '0';
	    ++parsingAt;
	    if (isdigit(data[parsingAt])) {
		m_tm.tm_sec = m_tm.tm_sec * 10 + data[parsingAt] - '0';
		++parsingAt;
	    }
	}
	else {
	    return false;
	}	
    }
    if (' ' != data[parsingAt]) {
	tzset();
	m_zone = timezone;
	m_valid = true;
	return true;
    }
    ++parsingAt;

    m_zone = 99999;
    if ('E' == toupper(data[parsingAt])) {
	if (('D' == toupper(data[parsingAt+1])) && ('T' == toupper(data[parsingAt+2]))) {
	    m_zone = 14400;
	}
	else if (('S' == toupper(data[parsingAt+1])) && ('T' == toupper(data[parsingAt+2]))) {
	    m_zone = 18000;
	}
	else {
	    return false;
	}
	if (' ' != data[parsingAt+3]) {
	    return false;
	}
	parsingAt += 4;
    }
    else if ('C' == toupper(data[parsingAt])) {
	if (('D' == toupper(data[parsingAt+1])) && ('T' == toupper(data[parsingAt+2]))) {
	    m_zone = 18000;
	}
	else if (('S' == toupper(data[parsingAt+1])) && ('T' == toupper(data[parsingAt+2]))) {
	    m_zone = 21600;
	}
	else {
	    return false;
	}
	if (' ' != data[parsingAt+3]) {
	    return false;
	}
	parsingAt += 4;
    }
    else if ('M' == toupper(data[parsingAt])) {
	if (('D' == toupper(data[parsingAt+1])) && ('T' == toupper(data[parsingAt+2]))) {
	    m_zone = 21600;
	}
	else if (('S' == toupper(data[parsingAt+1])) && ('T' == toupper(data[parsingAt+2]))) {
	    m_zone = 25200;
	}
	else {
	    return false;
	}
	if (' ' != data[parsingAt+3]) {
	    return false;
	}
	parsingAt += 4;
    }
    else if ('P' == toupper(data[parsingAt])) {
	if (('D' == toupper(data[parsingAt+1])) && ('T' == toupper(data[parsingAt+2]))) {
	    m_zone = 25200;
	}
	else if (('S' == toupper(data[parsingAt+1])) && ('T' == toupper(data[parsingAt+2]))) {
	    m_zone = 28800;
	}
	else {
	    return false;
	}
	if (' ' != data[parsingAt+3]) {
	    return false;
	}
	parsingAt += 4;
    }
    else if (('-' == data[parsingAt]) || ('+' == data[parsingAt])) {
	if (isdigit(data[parsingAt+1]) && isdigit(data[parsingAt+2]) && isdigit(data[parsingAt+3]) && isdigit(data[parsingAt+4])) {
	    // m_zone is seconds west of UTC but the string is +HHMM east of UTC
	    m_zone = 3600 * (10 * (data[parsingAt+1] - '0') + data[parsingAt+2] - '0') + 60 * (10 * (data[parsingAt+3] - '0') + data[parsingAt+4] - '0');
	    if ('+' == data[parsingAt]) {
		m_zone *= -1;
	    }
	}
	if (' ' != data[parsingAt+5]) {
	    return false;
	}
	parsingAt += 6;
    }
    if (isdigit(data[parsingAt])) {
	char *end;
	
	m_tm.tm_year = strtol((char *)&data[parsingAt], &end, 10);
	parsingAt = ((uint8_t *)end) - data;
	if (99999 == m_zone) {
	    tzset();
	    m_zone = timezone;
	}
    }
    else {
	return false;
    }

    // 30 days hath september, april, june, and november
    // all the rest have 31 except february
    if (1 == m_tm.tm_mon) {
	if ((29 < m_tm.tm_mday) ||
	    ((29 == m_tm.tm_mday) &&
	     ((0 != (m_tm.tm_year % 4)) || ((0 == m_tm.tm_year % 100) && (0 != m_tm.tm_year % 400))))) {
	    return false;
	}
    }
    else {
	if ((8 == m_tm.tm_mon) || (3 == m_tm.tm_mon) || (5 == m_tm.tm_mon) || (10 == m_tm.tm_mon)) {
	    if (30 < m_tm.tm_mday) {
		return false;
	    }
	}
	else {
	    if (31 < m_tm.tm_mday) {
		return false;
	    }
	}
    }

    // One last correction, tm_year is defined to hold years since 1900, but what's in it at this point
    // is years since zero, so I have to apply a correction.  I can't correct it before now because if I
    // do that, then Zeller's congruence may not work, so I correct it here.
    m_tm.tm_year -= 1900;

    m_valid = true;
    return true;
}

bool DateTime::parse(const uint8_t *data, size_t dataLen, size_t &parsingAt) {
    bool result;
    m_valid = false;

    switch(m_format) {
    case CTIME:
	result = parseCtime(data, dataLen, parsingAt);
	break;

    case IMAP:
	result = parseImap(data, dataLen, parsingAt);
	break;

    case RFC822:
	result = parseRfc822(data, dataLen, parsingAt);
	break;

    case FROM_LINE:
	result = parseFromLine(data, dataLen, parsingAt);
	break;

    default:
	result = false;
	break;
    }
    return result;
}

bool DateTime::parse(const uint8_t *data, size_t dataLen, size_t &parsingAt, STRING_FORMAT format) {
    m_format = format;
    return parse(data, dataLen, parsingAt);
}

bool DateTime::parse(const std::string &timeString) {
    size_t at = 0;

    return parse((uint8_t *)timeString.c_str(), timeString.size(), at);
}

bool DateTime::parse(const std::string &timeString, STRING_FORMAT format) {
    m_format = format;
    return parse(timeString);
}

bool DateTime::parse(const insensitiveString &timeString) {
    size_t at = 0;

    return parse((uint8_t *)timeString.c_str(), timeString.size(), at);
}

bool DateTime::parse(const insensitiveString &timeString, STRING_FORMAT format) {
    m_format = format;
    return parse(timeString);
}
