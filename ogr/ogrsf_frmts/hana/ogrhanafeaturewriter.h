/******************************************************************************
 *
 * Project:  SAP HANA Spatial Driver
 * Purpose:  OGRHanaFeatureWriter class declaration
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

#ifndef OGRHANAFEATUREWRITER_H_INCLUDED
#define OGRHANAFEATUREWRITER_H_INCLUDED

#include "gdal_priv.h"
#include "odbc/Types.h"

namespace OGRHANA {

class OGRHanaFeatureWriter
{
public:
    explicit OGRHanaFeatureWriter(OGRFeature& feature);

    template<typename T>
    void SetFieldValue(int fieldIndex, const odbc::Nullable<T>& value);

    void SetFieldValue(int fieldIndex, const odbc::Long& value);
    void SetFieldValue(int fieldIndex, const odbc::Float& value);
    void SetFieldValue(int fieldIndex, const odbc::Decimal& value);
    void SetFieldValue(int fieldIndex, const odbc::String& value);
    void SetFieldValue(int fieldIndex, const odbc::Date& value);
    void SetFieldValue(int fieldIndex, const odbc::Time& value);
    void SetFieldValue(int fieldIndex, const odbc::Timestamp& value);
    void SetFieldValue(int fieldIndex, const odbc::Binary& value);
    void SetFieldValue(int fieldIndex, const char* value);
    void SetFieldValue(int fieldIndex, const void* value, std::size_t size);

    template<typename InputT, typename ResultT>
    void SetFieldValueAsArray(int fieldIndex, const odbc::Binary& value);
    void SetFieldValueAsStringArray(int fieldIndex, const odbc::Binary& value);

private:
    OGRFeature& feature_;
};

template<typename T>
void OGRHanaFeatureWriter::SetFieldValue(
    int fieldIndex, const odbc::Nullable<T>& value)
{
    if (value.isNull())
        feature_.SetFieldNull(fieldIndex);
    else
        feature_.SetField(fieldIndex, *value);
}

template<typename InputT, typename ResultT>
void OGRHanaFeatureWriter::SetFieldValueAsArray(
    int fieldIndex, const odbc::Binary& value)
{
    if ( value.isNull() || value->size() == 0 )
    {
        feature_.SetFieldNull(fieldIndex);
        return;
    }

    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(value->data());
    const uint32_t numElements = *reinterpret_cast<const uint32_t*>(ptr);
    ptr += sizeof(uint32_t);

    std::vector<ResultT> values;
    values.reserve(numElements);

    bool elemHasLength = numElements * sizeof(InputT) != (value->size() - sizeof(uint32_t));

    for (uint32_t i = 0; i < numElements; ++i)
    {
        if (elemHasLength)
        {
            uint8_t len = *ptr;
            ++ptr;

            if (len > 0)
                values.push_back(*reinterpret_cast<const InputT*>(ptr));
            else
                values.push_back(ResultT());
        }
        else
            values.push_back(*reinterpret_cast<const InputT*>(ptr));

        ptr += sizeof(InputT);
    }

    feature_.SetField(
        fieldIndex, static_cast<int32_t>(values.size()), values.data());
}

} /* end of OGRHANA namespace */

#endif // OGRHANAFEATUREWRITER_H_INCLUDED
