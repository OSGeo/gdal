/******************************************************************************
 *
 * Project:  SAP HANA Spatial Driver
 * Purpose:  OGRHanaFeatureReader class declaration
 * Author:   Maxim Rylov
 *
 ******************************************************************************
 * Copyright (c) 2020, SAP SE
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRHANAFEATUREREADER_H_INCLUDED
#define OGRHANAFEATUREREADER_H_INCLUDED

#include "ogr_hana.h"
#include "gdal_priv.h"
#include "odbc/Types.h"

namespace OGRHANA
{

class OGRHanaFeatureReader
{
  public:
    explicit OGRHanaFeatureReader(const OGRFeature &feature);

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
    const char *GetDefaultValue(int fieldIndex) const;
    bool IsFieldSet(int fieldIndex) const;

  private:
    const OGRFeature &feature_;
};

}  // namespace OGRHANA

#endif  // OGRHANAFEATUREREADER_H_INCLUDED
