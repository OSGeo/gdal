/******************************************************************************
 *
 * Project:  SAP HANA Spatial Driver
 * Purpose:  OGRHanaFeatureWriter class implementation
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

#include "ogrhanafeaturewriter.h"
#include <limits>

CPL_CVSID("$Id$")

OGRHanaFeatureWriter::OGRHanaFeatureWriter(OGRFeature& feature)
    : feature_(feature)
{
}

void OGRHanaFeatureWriter::SetFieldValue(
    int fieldIndex, const odbc::Long& value)
{
    if (value.isNull())
        feature_.SetFieldNull(fieldIndex);
    else
        feature_.SetField(fieldIndex, static_cast<GIntBig>(*value));
}

void OGRHanaFeatureWriter::SetFieldValue(
    int fieldIndex, const odbc::Float& value)
{
    if (value.isNull())
        feature_.SetFieldNull(fieldIndex);
    else
        feature_.SetField(fieldIndex, static_cast<double>(*value));
}

void OGRHanaFeatureWriter::SetFieldValue(
    int fieldIndex, const odbc::Decimal& value)
{
    if (value.isNull())
        feature_.SetFieldNull(fieldIndex);
    else
        feature_.SetField(fieldIndex, value->toString().c_str());
}

void OGRHanaFeatureWriter::SetFieldValue(
    int fieldIndex, const odbc::String& value)
{
    if (value.isNull())
        feature_.SetFieldNull(fieldIndex);
    else
        feature_.SetField(fieldIndex, value->c_str());
}

void OGRHanaFeatureWriter::SetFieldValue(
    int fieldIndex, const odbc::NString& value)
{
    if (value.isNull())
        feature_.SetFieldNull(fieldIndex);
    else
        SetFieldValue(fieldIndex, value->data());
}

void OGRHanaFeatureWriter::SetFieldValue(
    int fieldIndex, const odbc::Date& value)
{
    if (value.isNull())
        feature_.SetFieldNull(fieldIndex);
    else
        feature_.SetField(
            fieldIndex, value->year(), value->month(), value->day(), 0, 0, 0,
            0);
}

void OGRHanaFeatureWriter::SetFieldValue(
    int fieldIndex, const odbc::Time& value)
{
    if (value.isNull())
        feature_.SetFieldNull(fieldIndex);
    else
        feature_.SetField(
            fieldIndex, 0, 0, 0, value->hour(), value->minute(),
            static_cast<float>(value->second()), 0);
}

void OGRHanaFeatureWriter::SetFieldValue(
    int fieldIndex, const odbc::Timestamp& value)
{
    if (value.isNull())
        feature_.SetFieldNull(fieldIndex);
    else
        feature_.SetField(
            fieldIndex, value->year(), value->month(), value->day(),
            value->hour(), value->minute(),
            static_cast<float>(
                value->second() + value->milliseconds() / 1000.0),
            0);
}

void OGRHanaFeatureWriter::SetFieldValue(
    int fieldIndex, const odbc::Binary& value)
{
    if (value.isNull())
        feature_.SetFieldNull(fieldIndex);
    else
        SetFieldValue(fieldIndex, value->data(), value->size());
}

void OGRHanaFeatureWriter::SetFieldValue(int fieldIndex, const char16_t* value)
{
    char* utf8;
    if (sizeof(wchar_t) == sizeof(char16_t))
    {
        utf8 = CPLRecodeFromWChar(
            reinterpret_cast<const wchar_t*>(value), CPL_ENC_UTF16,
            CPL_ENC_UTF8);
    }
    else
    {
        std::size_t len = std::char_traits<char16_t>::length(value);
        std::wstring str(len, 0);
        for (std::size_t i = 0; i < len; ++i)
            str[i] = value[i];
        utf8 = CPLRecodeFromWChar(str.c_str(), CPL_ENC_UTF16, CPL_ENC_UTF8);
    }
    feature_.SetField(fieldIndex, utf8);
    CPLFree(utf8);
}

void OGRHanaFeatureWriter::SetFieldValue(
    int fieldIndex, const void* value, std::size_t size)
{
    if (size > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Data size is larger than maximum integer value");
        return;
    }

    feature_.SetField(fieldIndex, static_cast<int>(size), value);
}

void OGRHanaFeatureWriter::SetFieldValueAsStringArray(
    int fieldIndex, const odbc::Binary& value)
{
    const char* data = value->data();
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data);
    uint32_t numElements = *(reinterpret_cast<const uint32_t*>(ptr));
    data += sizeof(uint32_t);

    if (numElements == 0)
    {
        feature_.SetFieldNull(fieldIndex);
        return;
    }

    char** values = nullptr;

    for (uint32_t i = 0; i < numElements; ++i)
    {
        uint8_t len = *ptr;
        ++ptr;

        if (len == 0)
        {
            values = CSLAddString(values, "");
        }
        else
        {
            if (ptr[0] == '\0')
            {
                values = CSLAddString(values, data);
            }
            else
            {
                char* val = static_cast<char*>(CPLMalloc(len + 1));
                memcpy(val, data, len);
                val[len] = '\0';
                values = CSLAddString(values, val);
                CPLFree(val);
            }
        }
        data += len;
    }

    feature_.SetField(fieldIndex, values);
    CSLDestroy(values);
}
