#include "TimestampPattern.hpp"

// C++ standard libraries
#include <chrono>
#include <cstring>
#include <vector>

// Date library
#include "../submodules/date/include/date/date.h"

// spdlog
#include <spdlog/spdlog.h>

using std::string;
using std::to_string;
using std::vector;

// Static member default initialization
std::unique_ptr<TimestampPattern[]> TimestampPattern::m_known_ts_patterns = nullptr;
size_t TimestampPattern::m_known_ts_patterns_len = 0;

// File-scope constants
static constexpr int cNumDaysInWeek = 7;
static const char* cAbbrevDaysOfWeek[cNumDaysInWeek] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static constexpr int cNumMonths = 12;
static const char* cAbbrevMonthNames[cNumMonths] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
static const char* cMonthNames[cNumMonths] = {
    "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"
};

// File-scope functions
/**
* Converts a value to a padded string with the given length and appends it to the given string
* @param value
* @param padding_character
* @param length
* @param str
*/
static void append_padded_value (int value, char padding_character, size_t length, string& str);
/**
 * Converts a padded decimal integer string (from a larger string) to an integer
 * @param str String containing the numeric string
 * @param begin_ix Start position of the numeric string
 * @param end_ix End position of the numeric string
 * @param padding_character
 * @param value String as a number
 * @return true if conversion succeeds, false otherwise
 */
static bool convert_string_to_number (const string& str, size_t begin_ix, size_t end_ix, char padding_character, int& value);

static void append_padded_value (const int value, const char padding_character, const size_t length, string& str) {
    string value_str = to_string(value);
    str.append(length - value_str.length(), padding_character);
    str += value_str;
}

static bool convert_string_to_number (const string& str, const size_t begin_ix, const size_t end_ix, const char padding_character, int& value) {
    // Consume padding characters
    size_t ix = begin_ix;
    while (ix < end_ix && padding_character == str[ix]) {
        ++ix;
    }

    // Convert remaining characters to number
    int converted_value = 0;
    for (; ix < end_ix; ++ix) {
        char c = str[ix];
        if (c < '0' || c > '9') {
            return false;
        }

        converted_value *= 10;
        converted_value += c - '0';
    }

    value = converted_value;
    return true;
}

/*
 * To initialize m_known_ts_patterns, we first create a vector of patterns then copy it to a dynamic array. This eases
 * maintenance of the list and the cost doesn't matter since it is only done once when the program starts.
 */
void TimestampPattern::init () {
    // First create vector of observed patterns so that it's easy to maintain
    vector<TimestampPattern> patterns;
    // E.g. 2015-01-31T15:50:45.392
    patterns.emplace_back(0, "%Y-%m-%dT%H:%M:%S.%3");
    // E.g. 2015-01-31T15:50:45,392
    patterns.emplace_back(0, "%Y-%m-%dT%H:%M:%S,%3");
    // E.g. [2015-01-31T15:50:45
    patterns.emplace_back(0, "[%Y-%m-%dT%H:%M:%S");
    // E.g. [20170106-16:56:41]
    patterns.emplace_back(0, "[%Y%m%d-%H:%M:%S]");
    // E.g. 2015-01-31 15:50:45,392
    patterns.emplace_back(0, "%Y-%m-%d %H:%M:%S,%3");
    // E.g. 2015-01-31 15:50:45.392
    patterns.emplace_back(0, "%Y-%m-%d %H:%M:%S.%3");
    // E.g. [2015-01-31 15:50:45,085]
    patterns.emplace_back(0, "[%Y-%m-%d %H:%M:%S,%3]");
    // E.g. 2015-01-31 15:50:45
    patterns.emplace_back(0, "%Y-%m-%d %H:%M:%S");
    // E.g. Start-Date: 2015-01-31  15:50:45
    patterns.emplace_back(1, "%Y-%m-%d  %H:%M:%S");
    // E.g. 2015/01/31 15:50:45
    patterns.emplace_back(0, "%Y/%m/%d %H:%M:%S");
    // E.g. 15/01/31 15:50:45
    patterns.emplace_back(0, "%y/%m/%d %H:%M:%S");
    // E.g. 150131  9:50:45
    patterns.emplace_back(0, "%y%m%d %k:%M:%S");
    // E.g. 01 Jan 2016 15:50:17,085
    patterns.emplace_back(0, "%d %b %Y %H:%M:%S,%3");
    // E.g. Jan 01, 2016 3:50:17 PM
    patterns.emplace_back(0, "%b %d, %Y %l:%M:%S %p");
    // E.g. January 31, 2015 15:50
    patterns.emplace_back(0, "%B %d, %Y %H:%M");
    // E.g. E [31/Jan/2015:15:50:45
    patterns.emplace_back(1, "[%d/%b/%Y:%H:%M:%S");
    // E.g. localhost - - [01/Jan/2016:15:50:17
    // E.g. 192.168.4.5 - - [01/Jan/2016:15:50:17
    patterns.emplace_back(3, "[%d/%b/%Y:%H:%M:%S");
    // E.g. 192.168.4.5 - - [01/01/2016:15:50:17
    patterns.emplace_back(3, "[%d/%m/%Y:%H:%M:%S");
    // E.g. INFO [main] 2015-01-31 15:50:45,085
    patterns.emplace_back(2, "%Y-%m-%d %H:%M:%S,%3");
    // E.g. Started POST "/api/v3/internal/allowed" for 127.0.0.1 at 2017-06-18 00:20:44
    patterns.emplace_back(6, "%Y-%m-%d %H:%M:%S");
    // E.g. update-alternatives 2015-01-31 15:50:45
    patterns.emplace_back(1, "%Y-%m-%d %H:%M:%S");
    // E.g. ERROR: apport (pid 4557) Sun Jan  1 15:50:45 2015
    patterns.emplace_back(4, "%a %b %e %H:%M:%S %Y");
    // E.g. <<<2016-11-10 03:02:29:936
    patterns.emplace_back(0, "<<<%Y-%m-%d %H:%M:%S:%3");
    // E.g. Jan 21 11:56:42
    patterns.emplace_back(0, "%b %d %H:%M:%S");

    // Initialize m_known_ts_patterns with vector's contents
    m_known_ts_patterns_len = patterns.size();
    m_known_ts_patterns = std::make_unique<TimestampPattern[]>(m_known_ts_patterns_len);
    for (size_t i = 0; i < patterns.size(); ++i) {
        m_known_ts_patterns[i] = patterns[i];
    }
}

const TimestampPattern* TimestampPattern::search_known_ts_patterns (const string& line, epochtime_t& timestamp, size_t& timestamp_begin_pos,
                                                                    size_t& timestamp_end_pos)
{
    for (size_t i = 0; i < m_known_ts_patterns_len; ++i) {
        if (m_known_ts_patterns[i].parse_timestamp(line, timestamp, timestamp_begin_pos, timestamp_end_pos)) {
            return &m_known_ts_patterns[i];
        }
    }

    timestamp_begin_pos = string::npos;
    timestamp_end_pos = string::npos;
    return nullptr;
}

const string& TimestampPattern::get_format () const {
    return m_format;
}

uint8_t TimestampPattern::get_num_spaces_before_ts () const {
    return m_num_spaces_before_ts;
}

bool TimestampPattern::is_empty () const {
    return m_format.empty();
}

void TimestampPattern::clear () {
    m_num_spaces_before_ts = 0;
    m_format.clear();
}

bool TimestampPattern::parse_timestamp (const string& line, epochtime_t& timestamp, size_t& timestamp_begin_pos, size_t& timestamp_end_pos) const {
    size_t line_ix = 0;
    const size_t line_length = line.length();

    // Find beginning of timestamp
    int num_spaces_found;
    for (num_spaces_found = 0; num_spaces_found < m_num_spaces_before_ts && line_ix < line_length; ++line_ix) {
        if (' ' == line[line_ix]) {
            ++num_spaces_found;
        }
    }
    if (num_spaces_found < m_num_spaces_before_ts) {
        return false;
    }
    size_t ts_begin_ix = line_ix;

    int date = 1;
    int month = 1;
    int year = 1970;
    int hour = 0;
    bool uses_12_hour_clock = false;
    int minute = 0;
    int second = 0;
    int millisecond = 0;
    bool is_pm = false;

    const size_t format_length = m_format.length();
    size_t format_ix = 0;
    bool is_specifier = false;
    for (; format_ix < format_length && line_ix < line_length; ++format_ix) {
        if (!is_specifier) {
            if ('%' == m_format[format_ix]) {
                is_specifier = true;
            } else {
                if (m_format[format_ix] != line[line_ix]) {
                    // Doesn't match
                    return false;
                }
                ++line_ix;
            }
        } else {
            // Parse fields
            switch (m_format[format_ix]) {
                case '%':
                    if ('%' != line[line_ix]) {
                        return false;
                    }
                    ++line_ix;
                    break;

                case 'y': { // Zero-padded year in century
                    constexpr int cFieldLength = 2;
                    if (line_ix + cFieldLength > line_length) {
                        // Too short
                        return false;
                    }

                    int value;
                    if (!convert_string_to_number(line, line_ix, line_ix + cFieldLength, '0', value) || value < 0 || value > 99) {
                        return false;
                    }
                    year = value;
                    // Year >= 69 treated as 1900s, year below 69 treated as 2000s
                    if (year >= 69) {
                        year += 1900;
                    } else {
                        year += 2000;
                    }
                    line_ix += cFieldLength;

                    break;
                }

                case 'Y': { // Zero-padded year with century
                    constexpr int cFieldLength = 4;
                    if (line_ix + cFieldLength > line_length) {
                        // Too short
                        return false;
                    }

                    int value;
                    if (!convert_string_to_number(line, line_ix, line_ix + cFieldLength, '0', value) || value < 0 || value > 9999) {
                        return false;
                    }
                    year = value;
                    line_ix += cFieldLength;

                    break;
                }

                case 'B': { // Month name
                    bool match_found = false;
                    for (int month_ix = 0; !match_found && month_ix < cNumMonths; ++month_ix) {
                        const size_t length = strlen(cMonthNames[month_ix]);
                        if (0 == line.compare(line_ix, length, cMonthNames[month_ix])) {
                            month = month_ix + 1;
                            match_found = true;
                            line_ix += length;
                        }
                    }
                    if (!match_found) {
                        return false;
                    }

                    break;
                }

                case 'b': { // Abbreviated month name
                    bool match_found = false;
                    for (int month_ix = 0; !match_found && month_ix < cNumMonths; ++month_ix) {
                        const size_t length = strlen(cAbbrevMonthNames[month_ix]);
                        if (0 == line.compare(line_ix, length, cAbbrevMonthNames[month_ix])) {
                            month = month_ix + 1;
                            match_found = true;
                            line_ix += length;
                        }
                    }
                    if (!match_found) {
                        return false;
                    }

                    break;
                }

                case 'm': { // Zero-padded month
                    constexpr int cFieldLength = 2;
                    if (line_ix + cFieldLength > line_length) {
                        // Too short
                        return false;
                    }

                    int value;
                    if (!convert_string_to_number(line, line_ix, line_ix + cFieldLength, '0', value) || value < 1 || value > 12) {
                        return false;
                    }
                    month = value;
                    line_ix += cFieldLength;

                    break;
                }

                case 'd': { // Zero-padded day in month
                    constexpr int cFieldLength = 2;
                    if (line_ix + cFieldLength > line_length) {
                        // Too short
                        return false;
                    }

                    int value;
                    if (!convert_string_to_number(line, line_ix, line_ix + cFieldLength, '0', value) || value < 1 || value > 31) {
                        return false;
                    }
                    date = value;
                    line_ix += cFieldLength;

                    break;
                }

                case 'e': { // Space-padded day in month
                    constexpr int cFieldLength = 2;
                    if (line_ix + cFieldLength > line_length) {
                        // Too short
                        return false;
                    }

                    int value;
                    if (!convert_string_to_number(line, line_ix, line_ix + cFieldLength, ' ', value) || value < 1 || value > 31) {
                        return false;
                    }
                    date = value;
                    line_ix += cFieldLength;

                    break;
                }

                case 'a': { // Abbreviated day of week
                    bool match_found = false;
                    for (int day_ix = 0; !match_found && day_ix < cNumDaysInWeek; ++day_ix) {
                        const size_t abbrev_length = strlen(cAbbrevDaysOfWeek[day_ix]);
                        if (0 == line.compare(line_ix, abbrev_length, cAbbrevDaysOfWeek[day_ix])) {
                            match_found = true;
                            line_ix += abbrev_length;
                        }
                    }
                    if (!match_found) {
                        return false;
                    }
                    // Weekday is not useful in determining absolute timestamp, so we don't do anything with it

                    break;
                }

                case 'p': { // Part of day
                    if (0 == line.compare(line_ix, 2, "AM")) {
                        is_pm = false;
                    } else if (0 == line.compare(line_ix, 2, "PM")) {
                        is_pm = true;
                    } else {
                        return false;
                    }
                    line_ix += 2;

                    break;
                }

                case 'H': { // Zero-padded hour on 24-hour clock
                    constexpr int cFieldLength = 2;
                    if (line_ix + cFieldLength > line_length) {
                        // Too short
                        return false;
                    }

                    int value;
                    if (!convert_string_to_number(line, line_ix, line_ix + cFieldLength, '0', value) || value < 0 || value > 23) {
                        return false;
                    }
                    hour = value;
                    line_ix += cFieldLength;

                    break;
                }

                case 'k': { // Space-padded hour on 24-hour clock
                    constexpr int cFieldLength = 2;
                    if (line_ix + cFieldLength > line_length) {
                        // Too short
                        return false;
                    }

                    int value;
                    if (!convert_string_to_number(line, line_ix, line_ix + cFieldLength, ' ', value) || value < 0 || value > 23) {
                        return false;
                    }
                    hour = value;
                    line_ix += cFieldLength;

                    break;
                }

                case 'I': { // Zero-padded hour on 12-hour clock
                    constexpr int cFieldLength = 2;
                    if (line_ix + cFieldLength > line_length) {
                        // Too short
                        return false;
                    }

                    int value;
                    if (!convert_string_to_number(line, line_ix, line_ix + cFieldLength, '0', value) || value < 1 || value > 12) {
                        return false;
                    }
                    hour = value;
                    uses_12_hour_clock = true;
                    line_ix += cFieldLength;

                    break;
                }

                case 'l': { // Space-padded hour on 12-hour clock
                    constexpr int cFieldLength = 2;
                    if (line_ix + cFieldLength > line_length) {
                        // Too short
                        return false;
                    }

                    int value;
                    if (!convert_string_to_number(line, line_ix, line_ix + cFieldLength, ' ', value) || value < 1 || value > 12) {
                        return false;
                    }
                    hour = value;
                    uses_12_hour_clock = true;
                    line_ix += cFieldLength;

                    break;
                }

                case 'M': { // Zero-padded minute
                    constexpr int cFieldLength = 2;
                    if (line_ix + cFieldLength > line_length) {
                        // Too short
                        return false;
                    }

                    int value;
                    if (!convert_string_to_number(line, line_ix, line_ix + cFieldLength, '0', value) || value < 0 || value > 59) {
                        return false;
                    }
                    minute = value;
                    line_ix += cFieldLength;

                    break;
                }

                case 'S': { // Zero-padded second
                    constexpr int cFieldLength = 2;
                    if (line_ix + cFieldLength > line_length) {
                        // Too short
                        return false;
                    }

                    int value;
                    if (!convert_string_to_number(line, line_ix, line_ix + cFieldLength, '0', value) || value < 0 || value > 60) {
                        return false;
                    }
                    second = value;
                    line_ix += cFieldLength;

                    break;
                }

                case '3': { // Zero-padded millisecond
                    constexpr int cFieldLength = 3;
                    if (line_ix + cFieldLength > line_length) {
                        // Too short
                        return false;
                    }

                    int value;
                    if (!convert_string_to_number(line, line_ix, line_ix + cFieldLength, '0', value) || value < 0 || value > 999) {
                        return false;
                    }
                    millisecond = value;
                    line_ix += cFieldLength;

                    break;
                }

                default:
                    return false;
            }
            is_specifier = false;
        }
    }
    if (format_ix < format_length) {
        // Complete format string not present in line
        return false;
    }

    // Process parsed fields
    if (uses_12_hour_clock) {
        if (12 == hour) {
            // 12s require special handling
            if (!is_pm) {
                // hour == 12AM which is 0 on 24-hour clock
                hour = 0;
            }
        } else {
            if (is_pm) {
                // All PMs except 12 should be +12, e.g. 1PM becomes (1 + 12)PM
                hour += 12;
            }
        }
    }

    // Create complete date
    auto year_month_date = date::year(year)/month/date;
    if (!year_month_date.ok()) {
        return false;
    }
    // Convert complete timestamp into a time point with millisecond resolution
    auto timestamp_point = date::sys_days(year_month_date) + std::chrono::hours(hour) + std::chrono::minutes(minute) + std::chrono::seconds(second) +
                           std::chrono::milliseconds(millisecond);
    // Get time point since epoch
    auto unix_epoch_point = date::sys_days(date::year(1970)/1/1);
    // Get timestamp since epoch
    auto duration_since_epoch = timestamp_point - unix_epoch_point;
    // Convert to raw milliseconds
    timestamp = duration_since_epoch.count();

    timestamp_begin_pos = ts_begin_ix;
    timestamp_end_pos = line_ix;

    return true;
}

void TimestampPattern::insert_formatted_timestamp (const epochtime_t timestamp, string& msg) const {
    size_t msg_length = msg.length();

    string new_msg;
    // We add 50 as an estimate of the timestamp's length
    new_msg.reserve(msg_length + 50);

    // Find where timestamp should go
    size_t ts_begin_ix = 0;
    int num_spaces_found;
    for (num_spaces_found = 0; num_spaces_found < m_num_spaces_before_ts && ts_begin_ix < msg_length; ++ts_begin_ix) {
        if (' ' == msg[ts_begin_ix]) {
            ++num_spaces_found;
        }
    }
    if (num_spaces_found < m_num_spaces_before_ts) {
        SPDLOG_ERROR("{} has {} spaces, but pattern has {}", msg.c_str(), num_spaces_found, m_num_spaces_before_ts);
        throw OperationFailed(ErrorCode_Failure, __FILENAME__, __LINE__);
    }

    // Copy text before timestamp
    new_msg.assign(msg, 0, ts_begin_ix);

    // Separate parts of timestamp
    auto timestamp_point = date::sys_days(date::year(1970)/1/1) + std::chrono::milliseconds(timestamp);
    auto timestamp_date = date::floor<date::days>(timestamp_point);
    int day_of_week_ix = (date::year_month_weekday(timestamp_date).weekday_indexed().weekday() - date::Sunday).count();
    auto year_month_date = date::year_month_day(timestamp_date);
    unsigned date = (unsigned)year_month_date.day();
    unsigned month = (unsigned)year_month_date.month();
    int year = (int)year_month_date.year();

    auto time_of_day_duration = timestamp_point - timestamp_date;
    auto time_of_day = date::make_time(time_of_day_duration);
    int hour = time_of_day.hours().count();
    int minute = time_of_day.minutes().count();
    int second = time_of_day.seconds().count();
    int millisecond = time_of_day.subseconds().count();

    const size_t format_length = m_format.length();
    bool is_specifier = false;
    for (size_t format_ix = 0; format_ix < format_length; ++format_ix) {
        if (!is_specifier) {
            if ('%' == m_format[format_ix]) {
                is_specifier = true;
            } else {
                new_msg += m_format[format_ix];
            }
        } else {
            // Parse fields
            switch (m_format[format_ix]) {
                case '%':
                    new_msg += m_format[format_ix];
                    break;

                case 'y': { // Zero-padded year in century
                    int value = year;
                    if (year >= 2000) {
                        // year must be in range [2000,2068]
                        value -= 2000;
                    } else {
                        // year must be in range [1969,1999]
                        value -= 1900;
                    }
                    append_padded_value(value, '0', 2, new_msg);
                    break;
                }

                case 'Y': // Zero-padded year with century
                    append_padded_value(year, '0', 4, new_msg);
                    break;

                case 'B': // Month name
                    new_msg += cMonthNames[month - 1];
                    break;

                case 'b': // Abbreviated month name
                    new_msg += cAbbrevMonthNames[month - 1];
                    break;

                case 'm': // Zero-padded month
                    append_padded_value(month, '0', 2, new_msg);
                    break;

                case 'd': // Zero-padded day in month
                    append_padded_value(date, '0', 2, new_msg);
                    break;

                case 'e': // Space-padded day in month
                    append_padded_value(date, ' ', 2, new_msg);
                    break;

                case 'a': // Abbreviated day of week
                    new_msg += cAbbrevDaysOfWeek[day_of_week_ix];
                    break;

                case 'p': { // Part of day
                    if (hour > 11) {
                        new_msg += "PM";
                    } else {
                        new_msg += "AM";
                    }
                    break;
                }

                case 'H': // Zero-padded hour on 24-hour clock
                    append_padded_value(hour, '0', 2, new_msg);
                    break;

                case 'k': // Space-padded hour on 24-hour clock
                    append_padded_value(hour, ' ', 2, new_msg);
                    break;

                case 'I': { // Zero-padded hour on 12-hour clock
                    int value = hour;
                    if (0 == value) {
                        value = 12;
                    } else if (value > 13) {
                        value -= 12;
                    }
                    append_padded_value(value, '0', 2, new_msg);
                    break;
                }

                case 'l': { // Space-padded hour on 12-hour clock
                    int value = hour;
                    if (0 == value) {
                        value = 12;
                    } else if (value > 13) {
                        value -= 12;
                    }
                    append_padded_value(value, ' ', 2, new_msg);
                    break;
                }

                case 'M': // Zero-padded minute
                    append_padded_value(minute, '0', 2, new_msg);
                    break;

                case 'S': // Zero-padded second
                    append_padded_value(second, '0', 2, new_msg);
                    break;

                case '3': // Zero-padded millisecond
                    append_padded_value(millisecond, '0', 3, new_msg);
                    break;

                default: {
                    throw OperationFailed(ErrorCode_Unsupported, __FILENAME__, __LINE__);
                }
            }
            is_specifier = false;
        }
    }

    // Copy text after timestamp
    new_msg.append(msg, ts_begin_ix, string::npos);

    msg = new_msg;
}

bool operator== (const TimestampPattern& lhs, const TimestampPattern& rhs) {
    return (lhs.m_num_spaces_before_ts == rhs.m_num_spaces_before_ts && lhs.m_format == rhs.m_format);
}

bool operator!= (const TimestampPattern& lhs, const TimestampPattern& rhs) {
    return !(lhs == rhs);
}
