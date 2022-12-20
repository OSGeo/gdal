///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Test general OGR features.
// Author:   Simon South
//
// Note the LGPL license of that file. See
// https://github.com/OSGeo/gdal/issues/5198
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019, Simon South
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the
// Free Software Foundation, Inc., 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.
///////////////////////////////////////////////////////////////////////////////

#include "ogr_p.h"
#include "ogrsf_frmts.h"

#include <string>

#include "gtest_include.h"

namespace
{

TEST(test_ogr_lgpl, OGRGetXMLDateTime)
{
    OGRField sField;
    char *pszDateTime;

    sField.Date.Year = 2001;
    sField.Date.Month = 2;
    sField.Date.Day = 3;
    sField.Date.Hour = 4;
    sField.Date.Minute = 5;

    // Unknown time zone (TZFlag = 0), no millisecond count
    sField.Date.TZFlag = 0;
    sField.Date.Second = 6.0f;
    pszDateTime = OGRGetXMLDateTime(&sField);
    ASSERT_TRUE(nullptr != pszDateTime);
    // OGRGetXMLDateTime formats date/time field with
    // unknown time zone, no millisecond count
    EXPECT_STREQ("2001-02-03T04:05:06", pszDateTime);
    CPLFree(pszDateTime);

    // Unknown time zone (TZFlag = 0), millisecond count
    sField.Date.TZFlag = 0;
    sField.Date.Second = 6.789f;
    pszDateTime = OGRGetXMLDateTime(&sField);
    ASSERT_TRUE(nullptr != pszDateTime);
    // OGRGetXMLDateTime formats date/time field with
    // unknown time zone, millisecond count
    EXPECT_STREQ("2001-02-03T04:05:06.789", pszDateTime);
    CPLFree(pszDateTime);

    // Local time zone (TZFlag = 1), no millisecond count
    sField.Date.TZFlag = 1;
    sField.Date.Second = 6.0f;
    pszDateTime = OGRGetXMLDateTime(&sField);
    ASSERT_TRUE(nullptr != pszDateTime);
    // OGRGetXMLDateTime formats date/time field with
    // local time zone, no millisecond count
    EXPECT_STREQ("2001-02-03T04:05:06", pszDateTime);
    CPLFree(pszDateTime);

    // Local time zone (TZFlag = 1), millisecond count
    sField.Date.TZFlag = 1;
    sField.Date.Second = 6.789f;
    pszDateTime = OGRGetXMLDateTime(&sField);
    ASSERT_TRUE(nullptr != pszDateTime);
    // OGRGetXMLDateTime formats date/time field with
    // local time zone, millisecond count
    EXPECT_STREQ("2001-02-03T04:05:06.789", pszDateTime);
    CPLFree(pszDateTime);

    // GMT time zone (TZFlag = 100), no millisecond count
    sField.Date.TZFlag = 100;
    sField.Date.Second = 6.0f;
    pszDateTime = OGRGetXMLDateTime(&sField);
    ASSERT_TRUE(nullptr != pszDateTime);
    // OGRGetXMLDateTime formats date/time field with
    // GMT time zone, no millisecond count
    EXPECT_STREQ("2001-02-03T04:05:06Z", pszDateTime);
    CPLFree(pszDateTime);

    // GMT time zone (TZFlag = 100), millisecond count
    sField.Date.TZFlag = 100;
    sField.Date.Second = 6.789f;
    pszDateTime = OGRGetXMLDateTime(&sField);
    ASSERT_TRUE(nullptr != pszDateTime);
    // OGRGetXMLDateTime formats date/time field with
    // GMT time zone, millisecond count
    EXPECT_STREQ("2001-02-03T04:05:06.789Z", pszDateTime);
    CPLFree(pszDateTime);

    // Positive time-zone offset, no millisecond count
    sField.Date.TZFlag = 111;
    sField.Date.Second = 6.0f;
    pszDateTime = OGRGetXMLDateTime(&sField);
    ASSERT_TRUE(nullptr != pszDateTime);
    // OGRGetXMLDateTime formats date/time field with
    // positive time-zone offset, no millisecond count
    EXPECT_STREQ("2001-02-03T04:05:06+02:45", pszDateTime);
    CPLFree(pszDateTime);

    // Positive time-zone offset, millisecond count
    sField.Date.TZFlag = 111;
    sField.Date.Second = 6.789f;
    pszDateTime = OGRGetXMLDateTime(&sField);
    ASSERT_TRUE(nullptr != pszDateTime);
    // OGRGetXMLDateTime formats date/time field with
    //  positive time-zone offset, millisecond count
    EXPECT_STREQ("2001-02-03T04:05:06.789+02:45", pszDateTime);
    CPLFree(pszDateTime);

    // Negative time-zone offset, no millisecond count
    sField.Date.TZFlag = 88;
    sField.Date.Second = 6.0f;
    pszDateTime = OGRGetXMLDateTime(&sField);
    ASSERT_TRUE(nullptr != pszDateTime);
    // OGRGetXMLDateTime formats date/time field with
    // negative time-zone offset, no millisecond count
    EXPECT_STREQ("2001-02-03T04:05:06-03:00", pszDateTime);
    CPLFree(pszDateTime);

    // Negative time-zone offset, millisecond count
    sField.Date.TZFlag = 88;
    sField.Date.Second = 6.789f;
    pszDateTime = OGRGetXMLDateTime(&sField);
    ASSERT_TRUE(nullptr != pszDateTime);
    // OGRGetXMLDateTime formats date/time field with
    // negative time-zone offset, millisecond count
    EXPECT_STREQ("2001-02-03T04:05:06.789-03:00", pszDateTime);
    CPLFree(pszDateTime);
}

}  // namespace
