/******************************************************************************
 *
 * Purpose:  CPPUnit test for overviews.
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
#include <cppunit/extensions/HelperMacros.h>
#include <cstring>

using namespace PCIDSK;

class OverviewTest : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE( OverviewTest );
 
    CPPUNIT_TEST( readOverviews );
    CPPUNIT_TEST( createOverviews );

    CPPUNIT_TEST_SUITE_END();

public:
    void setUp();
    void tearDown();
    void readOverviews();
    void createOverviews();
};

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION( OverviewTest );

void OverviewTest::setUp()
{
}

void OverviewTest::tearDown()
{
}

/************************************************************************/
/*                           readOverviews()                            */
/************************************************************************/

void OverviewTest::readOverviews()
{
    PCIDSKFile *file;

    file = PCIDSK::Open( "eltoro.pix", "r", NULL );

    CPPUNIT_ASSERT( file != NULL );
    CPPUNIT_ASSERT( !file->GetUpdatable() );

    PCIDSKChannel *channel = file->GetChannel(1);
    
    CPPUNIT_ASSERT( channel->GetOverviewCount() == 1 );

    PCIDSKChannel *overview = channel->GetOverview(0);

    CPPUNIT_ASSERT( overview->GetWidth() == 512 );
    CPPUNIT_ASSERT( overview->GetBlockWidth() == 127 );
    CPPUNIT_ASSERT( overview->GetType() == CHN_8U );

    // test windowed access.    
    uint8 data[127*127];

    overview->ReadBlock( 0, data );
    CPPUNIT_ASSERT( data[100*127+100] == 31 );
    
    delete file;
}

/************************************************************************/
/*                          createOverviews()                           */
/************************************************************************/

void OverviewTest::createOverviews()
{
    PCIDSKFile *file;
    eChanType channel_types[4] = {CHN_16S, CHN_32R };
    int  target_channel = 2;
    
    file = PCIDSK::Create( "overview_file.pix", 300, 200, 2, channel_types,
                           "PIXEL", NULL );

    CPPUNIT_ASSERT( file != NULL );

    file->CreateOverviews( 0, NULL, 2, "NEAREST" );
    file->CreateOverviews( 1, &target_channel, 4, "NEAREST" );

    PCIDSKChannel *channel = file->GetChannel(1);
    
    CPPUNIT_ASSERT( channel->GetOverviewCount() == 1 );

    channel = file->GetChannel(2);
    
    CPPUNIT_ASSERT( channel->GetOverviewCount() == 2 );

    PCIDSKChannel *overview = channel->GetOverview(0);

    CPPUNIT_ASSERT( overview->GetWidth() == 150 );
    CPPUNIT_ASSERT( overview->GetBlockWidth() == 127 );
    CPPUNIT_ASSERT( overview->GetType() == CHN_32R );

    overview = channel->GetOverview(1);

    CPPUNIT_ASSERT( overview->GetWidth() == 75 );
    CPPUNIT_ASSERT( overview->GetHeight() == 50 );

    // confirm we can write and read imagery.
    float data[127*127], data2[127*127];

    memset( data, 0, 127*127*4 );
    data[30] = 150.5;
    
    overview->WriteBlock( 0, data );

    delete file;

    file = PCIDSK::Open( "overview_file.pix", "r", NULL );

    channel = file->GetChannel(2);
    
    CPPUNIT_ASSERT( channel->GetOverviewCount() == 2 );

    overview = channel->GetOverview(1);

    CPPUNIT_ASSERT( overview->GetWidth() == 75 );
    CPPUNIT_ASSERT( overview->GetHeight() == 50 );

    overview->ReadBlock( 0, data2 );

    CPPUNIT_ASSERT( data2[10] == 0.0 );

    CPPUNIT_ASSERT( data2[30] == 150.5 );

    delete file;

    unlink( "overview_file.pix" );
}

