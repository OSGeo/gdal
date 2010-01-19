/******************************************************************************
 *
 * Purpose:  CPPUnit test for generic segment access and specialized access
 *           to some segment types.
 * 
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 50 West Wilmot Street, Richmond Hill, Ont, Canada
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

#include "pcidsk.h"
#include "pcidsk_georef.h"
#include "segment/cpcidskgeoref.h"
#include <cppunit/extensions/HelperMacros.h>

using namespace PCIDSK;

class ProjectionsTest : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE( ProjectionsTest );

    CPPUNIT_TEST( testTMRead );
    CPPUNIT_TEST( testTMWrite );
    CPPUNIT_TEST( testSPIFRead );
    CPPUNIT_TEST( testSPIFWrite );
    CPPUNIT_TEST( testUTME );

    CPPUNIT_TEST_SUITE_END();

public:
    void setUp();
    void tearDown();
    void testTMRead();
    void testTMWrite();
    void testSPIFRead();
    void testSPIFWrite();
    void testUTME();
};

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION( ProjectionsTest );

void ProjectionsTest::setUp()
{
}

void ProjectionsTest::tearDown()
{
}

void ProjectionsTest::testTMRead()
{
    // Test an existing complex projected image. 

    PCIDSKFile *file = PCIDSK::Open( "tm.pix", "r", NULL );

    PCIDSKGeoref *georef = dynamic_cast<PCIDSKGeoref*>(file->GetSegment(1));

    CPPUNIT_ASSERT( georef->GetGeosys() == "TM          D000" );

    std::vector<double> projparms;
    projparms = georef->GetParameters();
 
    CPPUNIT_ASSERT( projparms[2] == -117.0 );
    CPPUNIT_ASSERT( projparms[3] == 33.0 );
    CPPUNIT_ASSERT( projparms[8] == 0.998 );
    CPPUNIT_ASSERT( projparms[6] == 200000.0 );
    CPPUNIT_ASSERT( projparms[7] == 100000.0 );

    projparms = dynamic_cast<CPCIDSKGeoref*>(file->GetSegment(1))
        ->GetUSGSParameters();

    CPPUNIT_ASSERT( projparms[0] == 9.0 );   // ProjectionMethod = TM
    CPPUNIT_ASSERT( projparms[1] == 0.0 );   // Zone
    CPPUNIT_ASSERT( projparms[17] == 2 );    // UnitsCode = Meter?
    CPPUNIT_ASSERT( projparms[18] == 12 );   // Spheroid  = GRS80? 

    CPPUNIT_ASSERT( projparms[6] == -117000000.0 );
    CPPUNIT_ASSERT( projparms[7] ==   33000000.0 );
    CPPUNIT_ASSERT( projparms[4] == 0.998 );
    CPPUNIT_ASSERT( projparms[8] == 200000.0 );
    CPPUNIT_ASSERT( projparms[9] == 100000.0 );

    delete file;
}

void ProjectionsTest::testTMWrite()
{
    int i;
    eChanType channel_types[1] = {CHN_8U};

    PCIDSKFile *file = 
        PCIDSK::Create( "projfile_tm.pix", 50, 40, 1, channel_types, "BAND", NULL);

    PCIDSKGeoref *georef = dynamic_cast<PCIDSKGeoref*>(file->GetSegment(1));

    georef->WriteSimple( "TM E0", 0, 2.0, 0.0, 0, 0, -2.0 );

    std::vector<double> projparms;

    projparms.resize( 18 );
    for( i = 0; i < 18; i++ )
        projparms[i] = 0.0;

    projparms[2] = -117.0;
    projparms[3] = 33.0;
    projparms[8] = 0.998;
    projparms[6] = 200000;
    projparms[7] = 100000;
    georef->WriteParameters( projparms );

    CPPUNIT_ASSERT( georef->GetGeosys() == "TM          E000" );

    projparms.clear();
    projparms = georef->GetParameters();
 
    CPPUNIT_ASSERT( projparms[2] == -117.0 );
    CPPUNIT_ASSERT( projparms[3] == 33.0 );
    CPPUNIT_ASSERT( projparms[8] == 0.998 );
    CPPUNIT_ASSERT( projparms[6] == 200000.0 );
    CPPUNIT_ASSERT( projparms[7] == 100000.0 );

    projparms = dynamic_cast<CPCIDSKGeoref*>(file->GetSegment(1))
        ->GetUSGSParameters();

    CPPUNIT_ASSERT( projparms[0] == 9.0 );   // ProjectionMethod = TM
    CPPUNIT_ASSERT( projparms[17] == 2 );    // UnitsCode = Meter?
    CPPUNIT_ASSERT( projparms[18] == 0 );   // Spheroid  = GRS80? 

    CPPUNIT_ASSERT( projparms[6] == -117000000.0 );
    CPPUNIT_ASSERT( projparms[7] ==   33000000.0 );
    CPPUNIT_ASSERT( projparms[4] == 0.998 );
    CPPUNIT_ASSERT( projparms[8] == 200000.0 );
    CPPUNIT_ASSERT( projparms[9] == 100000.0 );

    delete file;

    unlink( "projfile_tm.pix" );
}

void ProjectionsTest::testSPIFRead()
{
    // Test an existing complex projected image. 

    PCIDSKFile *file = PCIDSK::Open( "spif.pix", "r", NULL );

    PCIDSKGeoref *georef = dynamic_cast<PCIDSKGeoref*>(file->GetSegment(1));

    CPPUNIT_ASSERT( georef->GetGeosys() == "SPIF 0102   D-02" );

    std::vector<double> projparms;

    projparms = georef->GetParameters();

    CPPUNIT_ASSERT( projparms[0] == 0 );
    CPPUNIT_ASSERT( projparms[17] == -1.0 );

    projparms = dynamic_cast<CPCIDSKGeoref*>(file->GetSegment(1))
        ->GetUSGSParameters();

    CPPUNIT_ASSERT( projparms[0] == 0.0 );    // ProjectionMethod 
    CPPUNIT_ASSERT( projparms[1] == 0.0 );  // Zone
    CPPUNIT_ASSERT( projparms[17] == 0 );     // UnitsCode = intl feet
    CPPUNIT_ASSERT( projparms[18] == 0 );     // Spheroid  = 

    delete file;
}

void ProjectionsTest::testSPIFWrite()
{
    eChanType channel_types[1] = {CHN_8U};

    PCIDSKFile *file = 
        PCIDSK::Create( "projfile.pix", 50, 40, 1, channel_types, "BAND", NULL);

    PCIDSKGeoref *georef = dynamic_cast<PCIDSKGeoref*>(file->GetSegment(1));

    georef->WriteSimple( "SPIF 102 D-2", 0, 2.0, 0.0, 0, 0, -2.0 );

    std::vector<double> projparms;

    CPPUNIT_ASSERT( georef->GetGeosys() == "SPIF  102   D-02" );

    projparms.clear();
    projparms = georef->GetParameters();

    CPPUNIT_ASSERT( projparms[0] == 0 );
    CPPUNIT_ASSERT( projparms[17] == 5.0 );   // INTL FEET

    projparms = dynamic_cast<CPCIDSKGeoref*>(file->GetSegment(1))
        ->GetUSGSParameters();

    CPPUNIT_ASSERT( projparms[0] == 2.0 );    // ProjectionMethod = stateplane
    CPPUNIT_ASSERT( projparms[1] == 102.0 );  // Zone
    CPPUNIT_ASSERT( projparms[17] == 5 );     // UnitsCode = intl feet
    CPPUNIT_ASSERT( projparms[18] == -1 );     // Spheroid  = 

    delete file;

    unlink( "projfile.pix" );
}

void ProjectionsTest::testUTME()
{
    eChanType channel_types[1] = {CHN_8U};

    PCIDSKFile *file = 
        PCIDSK::Create( "sdkc_utme.pix", 10, 10, 1, channel_types, "BAND", NULL);

    PCIDSKGeoref *georef = dynamic_cast<PCIDSKGeoref*>(file->GetSegment(1));

    georef->WriteSimple( "UTM 11 E225", 100000, 1000, 0, 200000, 0, -1000 );

    std::vector<double> projparms;

    CPPUNIT_ASSERT( georef->GetGeosys() == "UTM    11   E225" );

    projparms.clear();
    projparms = georef->GetParameters();

    CPPUNIT_ASSERT( projparms[0] == 0 );      // not set for simple projections
    CPPUNIT_ASSERT( projparms[17] == 2.0 );   // METERS

    projparms = dynamic_cast<CPCIDSKGeoref*>(file->GetSegment(1))
        ->GetUSGSParameters();

    CPPUNIT_ASSERT( projparms[0] == 9 );    // ProjectionMethod = UTM (TM?)
    CPPUNIT_ASSERT( projparms[1] == 11.0 );  // Zone
    CPPUNIT_ASSERT( projparms[6] == -117000000 );  //central meridian
    CPPUNIT_ASSERT( projparms[17] == 2 );     // UnitsCode = intl feet
    CPPUNIT_ASSERT( projparms[18] == -1 );     // Spheroid  = 

    delete file;

    unlink( "sdkc_utme.pix" );
}

