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

namespace OGRHANA {
namespace {

enum DataLengthIndicator
{
    MAX_ONE_BYTE = 245,
    TWO_BYTE = 246,
    FOUR_BYTE = 247,
    DEFAULT_VALUE = 254,
    NULL_VALUE = 255
};

} // anonymous namespace

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

void OGRHanaFeatureWriter::SetFieldValue(int fieldIndex, const char* value)
{
    if (value == nullptr)
        feature_.SetFieldNull(fieldIndex);
    else
        feature_.SetField(fieldIndex, value);
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

    if (value == nullptr)
        feature_.SetFieldNull(fieldIndex);
    else
        feature_.SetField(fieldIndex, static_cast<int>(size), value);
}

void OGRHanaFeatureWriter::SetFieldValueAsStringArray(
    int fieldIndex, const odbc::Binary& value)
{
    if ( value.isNull() || value->size() == 0 )
    {
        feature_.SetFieldNull(fieldIndex);
        return;
    }

    const char* ptr = value->data();
    const uint32_t numElements = *reinterpret_cast<const uint32_t*>(ptr);
    ptr += sizeof(uint32_t);

    char** values = nullptr;

    for (uint32_t i = 0; i < numElements; ++i)
    {
        uint8_t indicator = *ptr;
        ++ptr;

        int32_t len = 0;
        if (indicator <= DataLengthIndicator::MAX_ONE_BYTE)
        {
            len = indicator;
        }
        else if (indicator == DataLengthIndicator::TWO_BYTE)
        {
            len = *reinterpret_cast<const int16_t*>(ptr);
            ptr += sizeof(int16_t);
        }
        else
        {
            len = *reinterpret_cast<const int32_t*>(ptr);
            ptr += sizeof(int32_t);
        }

        if (len == 0)
        {
            values = CSLAddString(values, "");
        }
        else
        {
            if (ptr[0] == '\0')
            {
                values = CSLAddString(values, ptr);
            }
            else
            {
                char* val = static_cast<char*>(CPLMalloc(len + 1));
                memcpy(val, ptr, len);
                val[len] = '\0';
                values = CSLAddString(values, val);
                CPLFree(val);
            }
        }

        ptr += len;
    }

    feature_.SetField(fieldIndex, values);
    CSLDestroy(values);
}

} /* end of OGRHANA namespace */
