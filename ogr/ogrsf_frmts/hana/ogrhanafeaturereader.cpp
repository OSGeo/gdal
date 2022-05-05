/******************************************************************************
 *
 * Project:  SAP HANA Spatial Driver
 * Purpose:  OGRHanaFeatureReader class implementation
 * Author:   Maxim Rylov
 *
 ******************************************************************************
 * Copyright (c) 2020, SAP SE
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "ogrhanafeaturereader.h"
#include "ogrhanautils.h"

#include "cpl_time.h"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <sstream>

#include "odbc/Types.h"

CPL_CVSID("$Id$")

namespace OGRHANA {
namespace {

template<typename T>
odbc::String CreateStringFromValues(
    const T* elements, int numElements, std::string (*toString)(T e))
{
    if (numElements == 0)
        return odbc::String();

    std::ostringstream os;
    for (int i = 0; i < numElements; ++i)
    {
        if (i > 0)
            os << ARRAY_VALUES_DELIMITER;
        os << toString(elements[i]);
    }
    return odbc::String(os.str());
}

template<typename T>
T castInt(int value)
{
    if (value < std::numeric_limits<T>::min() ||
        value > std::numeric_limits<T>::max())
        throw std::overflow_error("Integer value lies outside of the range");
    return static_cast<T>(value);
}

// Specialization to make Coverity Scan happy
template<> int castInt(int value)
{
    return value;
}

template<typename T>
T strToInt(const char* value)
{
    return castInt<T>(std::stoi(value));
}

} // anonymous namespace

OGRHanaFeatureReader::OGRHanaFeatureReader(OGRFeature& feature)
    : feature_(feature)
{
}

odbc::Boolean OGRHanaFeatureReader::GetFieldAsBoolean(int fieldIndex) const
{
    if (IsFieldSet(fieldIndex))
        return feature_.GetFieldAsInteger(fieldIndex) == 1;

    const char* defaultValue = GetDefaultValue(fieldIndex);
    if (defaultValue == nullptr)
        return odbc::Boolean();

    return (EQUAL(defaultValue, "1") || EQUAL(defaultValue, "'t'"));
}

odbc::Byte OGRHanaFeatureReader::GetFieldAsByte(int fieldIndex) const
{
    if (IsFieldSet(fieldIndex))
        return odbc::Byte(
            castInt<std::int8_t>(feature_.GetFieldAsInteger(fieldIndex)));

    const char* defaultValue = GetDefaultValue(fieldIndex);
    if (defaultValue == nullptr)
        return odbc::Byte();
    return odbc::Byte(strToInt<std::int8_t>(defaultValue));
}

odbc::Short OGRHanaFeatureReader::GetFieldAsShort(int fieldIndex) const
{
    if (IsFieldSet(fieldIndex))
        return odbc::Short(
            castInt<std::int16_t>(feature_.GetFieldAsInteger(fieldIndex)));

    const char* defaultValue = GetDefaultValue(fieldIndex);
    if (defaultValue == nullptr)
        return odbc::Short();
    return odbc::Short(strToInt<std::int16_t>(defaultValue));
}

odbc::Int OGRHanaFeatureReader::GetFieldAsInt(int fieldIndex) const
{
    if (IsFieldSet(fieldIndex))
        return odbc::Int(feature_.GetFieldAsInteger(fieldIndex));

    const char* defaultValue = GetDefaultValue(fieldIndex);
    if (defaultValue == nullptr)
        return odbc::Int();
    return odbc::Int(strToInt<int>(defaultValue));
}

odbc::Long OGRHanaFeatureReader::GetFieldAsLong(int fieldIndex) const
{
    if (IsFieldSet(fieldIndex))
        return odbc::Long(feature_.GetFieldAsInteger64(fieldIndex));

    const char* defaultValue = GetDefaultValue(fieldIndex);
    if (defaultValue == nullptr)
        return odbc::Long();
    return odbc::Long(std::stol(defaultValue));
}

odbc::Float OGRHanaFeatureReader::GetFieldAsFloat(int fieldIndex) const
{
    if (IsFieldSet(fieldIndex))
    {
        double dValue = feature_.GetFieldAsDouble(fieldIndex);
        return odbc::Float(static_cast<float>(dValue));
    }

    const char* defaultValue = GetDefaultValue(fieldIndex);
    if (defaultValue == nullptr)
        return odbc::Float();
    return odbc::Float(std::stof(defaultValue));
}

odbc::Double OGRHanaFeatureReader::GetFieldAsDouble(int fieldIndex) const
{
    if (IsFieldSet(fieldIndex))
        return odbc::Double(feature_.GetFieldAsDouble(fieldIndex));
    const char* defaultValue = GetDefaultValue(fieldIndex);
    if (defaultValue == nullptr)
        return odbc::Double();
    return odbc::Double(std::stod(defaultValue));
}

odbc::String OGRHanaFeatureReader::GetFieldAsString(
    int fieldIndex, int maxCharLength) const
{
    auto getString = [&](const char* str) {
        if (str == nullptr)
            return odbc::String();

        if (maxCharLength > 0
            && std::strlen(str) > static_cast<std::size_t>(maxCharLength))
            return odbc::String(
                std::string(str, static_cast<std::size_t>(maxCharLength)));
        return odbc::String(str);
    };

    if (IsFieldSet(fieldIndex))
        return getString(feature_.GetFieldAsString(fieldIndex));

    const char* defaultValue = GetDefaultValue(fieldIndex);
    if (defaultValue == nullptr)
        return odbc::String();

    if (defaultValue[0] == '\''
        && defaultValue[strlen(defaultValue) - 1] == '\'')
    {
        CPLString str(defaultValue + 1);
        str.resize(str.size() - 1);
        char* tmp = CPLUnescapeString(str, nullptr, CPLES_SQL);
        odbc::String ret = getString(tmp);
        CPLFree(tmp);
        return ret;
    }

    return odbc::String(defaultValue);
}

odbc::String OGRHanaFeatureReader::GetFieldAsNString(
    int fieldIndex, int maxCharLength) const
{
    auto getString = [&](const char* str) {
        if (str == nullptr)
            return odbc::String();

        if (maxCharLength <= 0)
            return odbc::String(std::string(str));

        int nSrcLen = static_cast<int>(std::strlen(str));
        int nSrcLenUTF = CPLStrlenUTF8(str);

        if (maxCharLength > 0 && nSrcLenUTF > maxCharLength)
        {
            CPLDebug("HANA",
                     "Truncated field value '%s' at index %d to %d characters.",
                     str, fieldIndex, maxCharLength);

            int iUTF8Char = 0;
            for (int iChar = 0; iChar < nSrcLen; ++iChar)
            {
                if ((str[iChar] & 0xc0) != 0x80)
                {
                    if (iUTF8Char == maxCharLength)
                    {
                        nSrcLen = iChar;
                        break;
                    }
                    ++iUTF8Char;
                }
            }
        }

        return odbc::String(std::string(str, static_cast<std::size_t>(nSrcLen)));
    };

    if (IsFieldSet(fieldIndex))
        return getString(feature_.GetFieldAsString(fieldIndex));

    const char* defaultValue = GetDefaultValue(fieldIndex);
    if (defaultValue == nullptr)
        return odbc::String();

    if (defaultValue[0] == '\''
        && defaultValue[strlen(defaultValue) - 1] == '\'')
    {
        CPLString str(defaultValue + 1);
        str.resize(str.size() - 1);
        char* tmp = CPLUnescapeString(str, nullptr, CPLES_SQL);
        odbc::String ret = getString(tmp);
        CPLFree(tmp);
        return ret;
    }

    return odbc::String(defaultValue);
}

odbc::Date OGRHanaFeatureReader::GetFieldAsDate(int fieldIndex) const
{
    if (IsFieldSet(fieldIndex))
    {
        int year = 0;
        int month = 0;
        int day = 0;
        int hour = 0;
        int minute = 0;
        int timeZoneFlag = 0;
        float second = 0.0f;
        feature_.GetFieldAsDateTime(
            fieldIndex, &year, &month, &day, &hour, &minute, &second,
            &timeZoneFlag);

        return odbc::makeNullable<odbc::date>(year, month, day);
    }

    const char* defaultValue = GetDefaultValue(fieldIndex);
    if (defaultValue == nullptr)
        return odbc::Date();

    if (EQUAL(defaultValue, "CURRENT_DATE"))
    {
        std::time_t t = std::time(nullptr);
        tm* now = std::localtime(&t);
        if (now == nullptr)
            return odbc::Date();
        return odbc::makeNullable<odbc::date>(
            now->tm_year + 1900, now->tm_mon + 1, now->tm_mday);
    }

    int year, month, day;
    sscanf(defaultValue, "'%04d/%02d/%02d'", &year, &month, &day);

    return odbc::makeNullable<odbc::date>(year, month, day);
}

odbc::Time OGRHanaFeatureReader::GetFieldAsTime(int fieldIndex) const
{
    if (IsFieldSet(fieldIndex))
    {
        int year = 0;
        int month = 0;
        int day = 0;
        int hour = 0;
        int minute = 0;
        int timeZoneFlag = 0;
        float second = 0.0f;
        feature_.GetFieldAsDateTime(
            fieldIndex, &year, &month, &day, &hour, &minute, &second,
            &timeZoneFlag);
        return odbc::makeNullable<odbc::time>(
            hour, minute, static_cast<int>(round(second)));
    }

    const char* defaultValue = GetDefaultValue(fieldIndex);
    if (defaultValue == nullptr)
        return odbc::Time();

    if (EQUAL(defaultValue, "CURRENT_TIME"))
    {
        std::time_t t = std::time(nullptr);
        tm* now = std::localtime(&t);
        if (now == nullptr)
            return odbc::Time();
        return odbc::makeNullable<odbc::time>(
            now->tm_hour, now->tm_min, now->tm_sec);
    }

    int hour = 0;
    int minute = 0;
    int second = 0;
    sscanf(defaultValue, "'%02d:%02d:%02d'", &hour, &minute, &second);
    return odbc::makeNullable<odbc::time>(hour, minute, second);
}

odbc::Timestamp OGRHanaFeatureReader::GetFieldAsTimestamp(int fieldIndex) const
{
    if (IsFieldSet(fieldIndex))
    {
        int year = 0;
        int month = 0;
        int day = 0;
        int hour = 0;
        int minute = 0;
        float secondWithMillisecond = 0.0f;
        int timeZoneFlag = 0;
        feature_.GetFieldAsDateTime(
            fieldIndex, &year, &month, &day, &hour, &minute,
            &secondWithMillisecond, &timeZoneFlag);
        double seconds = 0.0;
        double milliseconds = std::modf(static_cast<double>(secondWithMillisecond), &seconds);
        int second = static_cast<int>(std::floor(seconds));
        int millisecond = static_cast<int>(std::floor(milliseconds * 1000));

        if (!(timeZoneFlag == 0 || timeZoneFlag == 100 || timeZoneFlag == 1))
        {
            struct tm time;
            time.tm_year = year - 1900;
            time.tm_mon = month - 1;
            time.tm_mday = day;
            time.tm_hour = hour;
            time.tm_min = minute;
            time.tm_sec = second;
            GIntBig dt = CPLYMDHMSToUnixTime(&time);
            const int tzoffset = std::abs(timeZoneFlag - 100) * 15;
            dt -= tzoffset * 60;
            CPLUnixTimeToYMDHMS(dt, &time);
            year = time.tm_year + 1900;
            month = time.tm_mon + 1;
            day = time.tm_mday;
            hour = time.tm_hour;
            minute = time.tm_min;
            second = time.tm_sec;
        }

        return odbc::makeNullable<odbc::timestamp>(
            year, month, day, hour, minute, second, millisecond);
    }

    const char* defaultValue = GetDefaultValue(fieldIndex);
    if (defaultValue == nullptr)
        return odbc::Timestamp();

    if (EQUAL(defaultValue, "CURRENT_TIMESTAMP"))
    {
        time_t t = std::time(nullptr);
        tm* now = std::localtime(&t);
        if (now == nullptr)
            return odbc::Timestamp();
        return odbc::makeNullable<odbc::timestamp>(
            now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, now->tm_hour,
            now->tm_min, now->tm_sec, 0);
    }

    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    int millisecond = 0;

    if (strchr(defaultValue, '.') == nullptr)
        sscanf(
            defaultValue, "'%04d/%02d/%02d %02d:%02d:%02d'", &year, &month,
            &day, &hour, &minute, &second);
    else
        sscanf(
            defaultValue, "'%04d/%02d/%02d %02d:%02d:%02d.%03d'", &year, &month,
            &day, &hour, &minute, &second, &millisecond);

    return odbc::makeNullable<odbc::timestamp>(
        year, month, day, hour, minute, second, millisecond);
}

Binary OGRHanaFeatureReader::GetFieldAsBinary(int fieldIndex) const
{
    if (IsFieldSet(fieldIndex))
    {
        int size = 0;
        GByte* data = feature_.GetFieldAsBinary(fieldIndex, &size);
        return {data, static_cast<std::size_t>(size)};
    }

    const char* defaultValue = GetDefaultValue(fieldIndex);
    if (defaultValue == nullptr)
        return {nullptr, 0U};

    return {const_cast<GByte*>(reinterpret_cast<const GByte*>(defaultValue)),
            std::strlen(defaultValue)};
}

odbc::String OGRHanaFeatureReader::GetFieldAsIntArray(int fieldIndex) const
{
    if (!IsFieldSet(fieldIndex))
        return odbc::String();

    int numElements;
    const int* values =
        feature_.GetFieldAsIntegerList(fieldIndex, &numElements);
    return CreateStringFromValues<int>(values, numElements, &std::to_string);
}

odbc::String OGRHanaFeatureReader::GetFieldAsBigIntArray(int fieldIndex) const
{
    if (!IsFieldSet(fieldIndex))
        return odbc::String();

    int numElements;
    const GIntBig* values =
        feature_.GetFieldAsInteger64List(fieldIndex, &numElements);
    return CreateStringFromValues<GIntBig>(
        values, numElements, &std::to_string);
}

odbc::String OGRHanaFeatureReader::GetFieldAsRealArray(int fieldIndex) const
{
    if (!IsFieldSet(fieldIndex))
        return odbc::String();

    int numElements;
    const double* values =
        feature_.GetFieldAsDoubleList(fieldIndex, &numElements);
    return CreateStringFromValues<double>(
        values, numElements, [](double value) {
            return std::isnan(value)
                       ? "NULL"
                       : std::to_string(static_cast<float>(value));
        });
}

odbc::String OGRHanaFeatureReader::GetFieldAsDoubleArray(int fieldIndex) const
{
    if (!IsFieldSet(fieldIndex))
        return odbc::String();

    int numElements;
    const double* values =
        feature_.GetFieldAsDoubleList(fieldIndex, &numElements);
    return CreateStringFromValues<double>(
        values, numElements, [](double value) {
            return std::isnan(value) ? "NULL" : std::to_string(value);
        });
}

odbc::String OGRHanaFeatureReader::GetFieldAsStringArray(int fieldIndex) const
{
    if (!IsFieldSet(fieldIndex))
        return odbc::String();

    char** items = feature_.GetFieldAsStringList(fieldIndex);
    if (items == nullptr)
        return odbc::String();

    std::ostringstream os;
    bool firstItem = true;
    while (items && *items)
    {
        if (!firstItem)
            os << ARRAY_VALUES_DELIMITER;

        char* itemValue = *items;
        if (*itemValue != '\0')
        {
            os << '\'';
            while (*itemValue)
            {
                if (*itemValue == '\'')
                    os << "'";
                os << *itemValue;
                ++itemValue;
            }
            os << '\'';
        }

        ++items;
        firstItem = false;
    }

    return odbc::String(os.str());
}

const char* OGRHanaFeatureReader::GetDefaultValue(int fieldIndex) const
{
    const OGRFieldDefn* fieldDef = feature_.GetFieldDefnRef(fieldIndex);
    return fieldDef->GetDefault();
}

bool OGRHanaFeatureReader::IsFieldSet(int fieldIndex) const
{
    return feature_.IsFieldSet(fieldIndex) && !feature_.IsFieldNull(fieldIndex);
}

} /* end of OGRHANA namespace */
