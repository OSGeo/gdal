/******************************************************************************
 *
 * Project:  SAP HANA Spatial Driver
 * Purpose:  OGRHanaFeatureReader class declaration
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

#ifndef OGRHANAFEATUREREADER_H_INCLUDED
#define OGRHANAFEATUREREADER_H_INCLUDED

#include "ogr_hana.h"
#include "gdal_priv.h"
#include "odbc/Types.h"

namespace OGRHANA {

class OGRHanaFeatureReader
{
public:
    explicit OGRHanaFeatureReader(OGRFeature& feature);

    odbc::Boolean GetFieldAsBoolean(int fieldIndex) const;
    odbc::Byte GetFieldAsByte(int fieldIndex) const;
    odbc::Short GetFieldAsShort(int fieldIndex) const;
    odbc::Int GetFieldAsInt(int fieldIndex) const;
    odbc::Long GetFieldAsLong(int fieldIndex) const;
    odbc::Float GetFieldAsFloat(int fieldIndex) const;
    odbc::Double GetFieldAsDouble(int fieldIndex) const;
    odbc::String GetFieldAsString(int fieldIndex, int maxCharLength) const;
    odbc::String GetFieldAsNString(int fieldIndex, int maxCharLength) const;
    odbc::Date GetFieldAsDate(int fieldIndex) const;
    odbc::Time GetFieldAsTime(int fieldIndex) const;
    odbc::Timestamp GetFieldAsTimestamp(int fieldIndex) const;
    Binary GetFieldAsBinary(int fieldIndex) const;

    odbc::String GetFieldAsIntArray(int fieldIndex) const;
    odbc::String GetFieldAsBigIntArray(int fieldIndex) const;
    odbc::String GetFieldAsRealArray(int fieldIndex) const;
    odbc::String GetFieldAsDoubleArray(int fieldIndex) const;
    odbc::String GetFieldAsStringArray(int fieldIndex) const;

private:
    const char* GetDefaultValue(int fieldIndex) const;
    bool IsFieldSet(int fieldIndex) const;

private:
    const OGRFeature& feature_;
};

} /* end of OGRHANA namespace */

#endif // OGRHANAFEATUREREADER_H_INCLUDED
