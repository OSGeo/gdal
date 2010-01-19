/******************************************************************************
 *
 * Purpose:  CPPUnit test for updating raster data.
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

class PCIDSKUpdateTest : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE( PCIDSKUpdateTest );
 
    CPPUNIT_TEST( updateBandInterleaved );
    CPPUNIT_TEST( updatePixelInterleaved );
    CPPUNIT_TEST( testReadonly );
    CPPUNIT_TEST( testSync );

    CPPUNIT_TEST_SUITE_END();

public:
    void setUp();
    void tearDown();
    void updateBandInterleaved();
    void updatePixelInterleaved();
    void testReadonly();
    void testSync();
};

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION( PCIDSKUpdateTest );

void PCIDSKUpdateTest::setUp()
{
}

void PCIDSKUpdateTest::tearDown()
{
}

/************************************************************************/
/*                       updateBandInterleaved()                        */
/************************************************************************/

void PCIDSKUpdateTest::updateBandInterleaved()
{
/* -------------------------------------------------------------------- */
/*      Create a simple pcidsk file.                                    */
/* -------------------------------------------------------------------- */
    PCIDSKFile *band_file;
    eChanType channel_types[4] = {CHN_8U, CHN_8U, CHN_8U, CHN_32R};

    band_file = PCIDSK::Create( "band_update.pix", 300, 200, 4, channel_types, 
                                 "BAND", NULL );

    CPPUNIT_ASSERT( band_file != NULL );
    
/* -------------------------------------------------------------------- */
/*      Update channel 2.                                               */
/* -------------------------------------------------------------------- */
    PCIDSKChannel *chan;

    uint8 data_line_8[300];
    int i;

    for( i = 0; i < 300; i++ )
        data_line_8[i] = i % 256;

    chan = band_file->GetChannel(2);
    chan->WriteBlock( 3, data_line_8 );

/* -------------------------------------------------------------------- */
/*      Update channel 4.                                               */
/* -------------------------------------------------------------------- */

    float data_line_32[300];

    for( i = 0; i < 300; i++ )
        data_line_32[i] = i * 1.5;

    chan = band_file->GetChannel(4);
    chan->WriteBlock( 3, data_line_32 );

/* -------------------------------------------------------------------- */
/*      Close and reopen file.                                          */
/* -------------------------------------------------------------------- */
    delete band_file;
    band_file = PCIDSK::Open( "band_update.pix", "r", NULL );
    
/* -------------------------------------------------------------------- */
/*      Read and check channel 2.                                       */
/* -------------------------------------------------------------------- */
    chan = band_file->GetChannel(2);
    chan->ReadBlock( 3, data_line_8 );

    for( i = 0; i < 300; i++ )
        CPPUNIT_ASSERT( data_line_8[i] == i % 256 );
    
/* -------------------------------------------------------------------- */
/*      Read and check channel 4.                                       */
/* -------------------------------------------------------------------- */
    chan = band_file->GetChannel(4);
    chan->ReadBlock( 3, data_line_32 );

    for( i = 0; i < 300; i++ )
        CPPUNIT_ASSERT( data_line_32[i] == i * 1.5 );

    delete band_file;

    unlink( "band_update.pix" );
}

/************************************************************************/
/*                       updatePixelInterleaved()                       */
/************************************************************************/

void PCIDSKUpdateTest::updatePixelInterleaved()
{
/* -------------------------------------------------------------------- */
/*      Create a simple pcidsk file.                                    */
/* -------------------------------------------------------------------- */
    PCIDSKFile *pixel_file;
    eChanType channel_types[4] = {CHN_8U, CHN_8U, CHN_8U, CHN_32R};

    pixel_file = PCIDSK::Create( "pixel_update.pix", 300, 200, 4, channel_types,
                                 "PIXEL", NULL );

    CPPUNIT_ASSERT( pixel_file != NULL );
    
/* -------------------------------------------------------------------- */
/*      Update channel 2.                                               */
/* -------------------------------------------------------------------- */
    PCIDSKChannel *chan;

    uint8 data_line_8[300];
    int i;

    for( i = 0; i < 300; i++ )
        data_line_8[i] = i % 256;

    chan = pixel_file->GetChannel(2);
    chan->WriteBlock( 3, data_line_8 );

/* -------------------------------------------------------------------- */
/*      Update channel 4.                                               */
/* -------------------------------------------------------------------- */

    float data_line_32[300];

    for( i = 0; i < 300; i++ )
        data_line_32[i] = i * 1.5;

    chan = pixel_file->GetChannel(4);
    chan->WriteBlock( 3, data_line_32 );

/* -------------------------------------------------------------------- */
/*      Close and reopen file.                                          */
/* -------------------------------------------------------------------- */
    delete pixel_file;
    pixel_file = PCIDSK::Open( "pixel_update.pix", "r", NULL );
    
/* -------------------------------------------------------------------- */
/*      Read and check channel 2.                                       */
/* -------------------------------------------------------------------- */
    chan = pixel_file->GetChannel(2);
    chan->ReadBlock( 3, data_line_8 );

    for( i = 0; i < 300; i++ )
        CPPUNIT_ASSERT( data_line_8[i] == i % 256 );
    
/* -------------------------------------------------------------------- */
/*      Read and check channel 4.                                       */
/* -------------------------------------------------------------------- */
    chan = pixel_file->GetChannel(4);
    chan->ReadBlock( 3, data_line_32 );

    for( i = 0; i < 300; i++ )
        CPPUNIT_ASSERT( data_line_32[i] == i * 1.5 );

    delete pixel_file;

    unlink( "pixel_update.pix" );
}

/************************************************************************/
/*                            testReadonly()                            */
/************************************************************************/

void PCIDSKUpdateTest::testReadonly()
{
    PCIDSKFile *file = PCIDSK::Open( "eltoro.pix", "r", NULL );

    CPPUNIT_ASSERT( !file->GetUpdatable() );

    try 
    {
        uint8 line_buffer[1024];

        file->GetChannel(1)->WriteBlock( 1, line_buffer );
        CPPUNIT_ASSERT( false );
    } 
    catch( PCIDSK::PCIDSKException ex )
    {
        CPPUNIT_ASSERT( strstr(ex.what(),"update") != NULL );
    }

    delete file;
}

/************************************************************************/
/*                              testSync()                              */
/*                                                                      */
/*      Test support for the Synchronize() method. In particular we     */
/*      create a tiled file, write some data, write some metadata,      */
/*      and then confirm that after a sync we are able to read this     */
/*      back on a second file handle without having closed the first    */
/*      yet.                                                            */
/************************************************************************/

void PCIDSKUpdateTest::testSync()
{
/* -------------------------------------------------------------------- */
/*      Create a simple pcidsk file.                                    */
/* -------------------------------------------------------------------- */
    PCIDSKFile *file;
    eChanType channel_types[1] = {CHN_8U};

    file = PCIDSK::Create( "sync_test.pix", 300, 200, 1, channel_types,
                           "TILED", NULL );

    CPPUNIT_ASSERT( file != NULL );
    
/* -------------------------------------------------------------------- */
/*      Update channel 1.                                               */
/* -------------------------------------------------------------------- */
    PCIDSKChannel *chan;

    uint8 data_tile_8[127*127];
    unsigned int i;
    
    for( i = 0; i < sizeof(data_tile_8); i++ )
        data_tile_8[i] = i % 256;

    chan = file->GetChannel(1);
    chan->WriteBlock( 1, data_tile_8 );

/* -------------------------------------------------------------------- */
/*      Write some metadata.                                            */
/* -------------------------------------------------------------------- */
    chan->SetMetadataValue( "ABC", "DEF" );

/* -------------------------------------------------------------------- */
/*      Synchronize the file to disk.                                   */
/* -------------------------------------------------------------------- */
    file->Synchronize();
    
/* -------------------------------------------------------------------- */
/*      Open the file for read access via another handle.               */
/* -------------------------------------------------------------------- */
    PCIDSKFile *file2 = PCIDSK::Open( "sync_test.pix", "r", NULL );

/* -------------------------------------------------------------------- */
/*      Read and check pixel data.                                      */
/* -------------------------------------------------------------------- */
    uint8 data_tile_8_r[127*127];

    chan = file2->GetChannel(1);
    chan->ReadBlock( 1, data_tile_8_r );

    for( i = 0; i < sizeof(data_tile_8_r); i++ )
    {
        CPPUNIT_ASSERT( data_tile_8[i] == data_tile_8_r[i] );
    }
    
/* -------------------------------------------------------------------- */
/*      Check metadata.                                                 */
/* -------------------------------------------------------------------- */
    CPPUNIT_ASSERT( chan->GetMetadataValue( "ABC" ) == "DEF" );
    
/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    delete file2;
    delete file;

    unlink( "sync_test.pix" );
}

