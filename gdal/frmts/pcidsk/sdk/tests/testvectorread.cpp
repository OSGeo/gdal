/******************************************************************************
 *
 * Purpose:  CPPUnit test for reading vector segments.
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
#include "pcidsk_vectorsegment.h"
#include <math.h>
#include <cppunit/extensions/HelperMacros.h>

using namespace PCIDSK;

class VectorReadTest : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE( VectorReadTest );
 
    CPPUNIT_TEST( testGeometry );
    CPPUNIT_TEST( testSchema );
    CPPUNIT_TEST( testRecords );
    CPPUNIT_TEST( testRandomRead );

    CPPUNIT_TEST_SUITE_END();

public:
    void setUp();
    void tearDown();
    void testGeometry();
    void testSchema();
    void testRecords();
    void testRandomRead();
};

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION( VectorReadTest );

void VectorReadTest::setUp()
{
}

void VectorReadTest::tearDown()
{
}

void VectorReadTest::testGeometry()
{
    PCIDSKFile *file;
    
    file = PCIDSK::Open( "irvine.pix", "r", NULL );

    CPPUNIT_ASSERT( file != NULL );

    PCIDSKSegment *seg = file->GetSegment( 26 );

    CPPUNIT_ASSERT( seg != NULL );
    CPPUNIT_ASSERT( seg->GetSegmentType() == SEG_VEC );

    PCIDSKVectorSegment *vecseg = dynamic_cast<PCIDSKVectorSegment*>( seg );
    
    CPPUNIT_ASSERT( vecseg != NULL );

    ShapeIterator it = vecseg->begin();
    std::vector<ShapeVertex> vertex_list;
    double vertex_sum = 0.0;
    unsigned int i;

    while( it != vecseg->end() )
    {
        vecseg->GetVertices( *it, vertex_list );

        for( i = 0; i < vertex_list.size(); i++ )
        {
            vertex_sum += vertex_list[i].x;
            vertex_sum += vertex_list[i].y;
            vertex_sum += vertex_list[i].z;
        }
        
        it++;
    }

    CPPUNIT_ASSERT( fabs(vertex_sum - 6903155159.15) < 1.0 );

    delete file;
}

void VectorReadTest::testSchema()
{
    PCIDSKFile *file;
    
    file = PCIDSK::Open( "polygon.pix", "r", NULL );

    CPPUNIT_ASSERT( file != NULL );

    PCIDSKSegment *seg = file->GetSegment( 2 );
    PCIDSKVectorSegment *vecseg = dynamic_cast<PCIDSKVectorSegment*>( seg );
    
    CPPUNIT_ASSERT( vecseg != NULL );

    CPPUNIT_ASSERT( vecseg->GetFieldCount() == 30 );

    CPPUNIT_ASSERT( vecseg->GetFieldName(28) == "AA" );
    CPPUNIT_ASSERT( vecseg->GetFieldType(28) == FieldTypeInteger );
    CPPUNIT_ASSERT( vecseg->GetFieldDescription(28) == "" );
    CPPUNIT_ASSERT( vecseg->GetFieldFormat(28) == "%8d" );
    CPPUNIT_ASSERT( vecseg->GetFieldDefault(28).GetValueInteger() == 0 );

    CPPUNIT_ASSERT( vecseg->GetFieldName(4) == "ATLAS_P" );
    CPPUNIT_ASSERT( vecseg->GetFieldType(4) == FieldTypeString );
    CPPUNIT_ASSERT( vecseg->GetFieldDescription(4) == "" );
    CPPUNIT_ASSERT( vecseg->GetFieldFormat(4) == "%16s" );
    CPPUNIT_ASSERT( vecseg->GetFieldDefault(4).GetValueString() == "" );

    CPPUNIT_ASSERT( vecseg->GetFieldName(29) == "RingStart" );
    CPPUNIT_ASSERT( vecseg->GetFieldType(29) == FieldTypeCountedInt );
    CPPUNIT_ASSERT( vecseg->GetFieldDescription(29) == "Ring Start" );
    CPPUNIT_ASSERT( vecseg->GetFieldFormat(29) == "%d" );
    CPPUNIT_ASSERT( vecseg->GetFieldDefault(29).GetValueCountedInt().size() == 0 );

    delete file;
}

void VectorReadTest::testRecords()
{
    PCIDSKFile *file;
    
    file = PCIDSK::Open( "polygon.pix", "r", NULL );

    CPPUNIT_ASSERT( file != NULL );

    PCIDSKSegment *seg = file->GetSegment( 2 );
    PCIDSKVectorSegment *vecseg = dynamic_cast<PCIDSKVectorSegment*>( seg );
    
    CPPUNIT_ASSERT( vecseg != NULL );

    ShapeIterator it = vecseg->begin();
    std::vector<ShapeField> field_list;
    int32 eas_id_sum = 0;
    double area_sum = 0.0;

    while( it != vecseg->end() )
    {
        vecseg->GetFields( *it, field_list );

        if( (int) *it == 17 )
        {
            CPPUNIT_ASSERT( fabs(field_list[0].GetValueDouble()-1214184.375) < 0.001 );
            CPPUNIT_ASSERT( field_list[2].GetValueInteger() == 19 );
            CPPUNIT_ASSERT( field_list[4].GetValueString() == "35045414" );
            CPPUNIT_ASSERT( field_list[28].GetValueInteger() == 35045414 );
            CPPUNIT_ASSERT( field_list[29].GetValueCountedInt().size() == 0 );
        }

        eas_id_sum += field_list[3].GetValueInteger();
        area_sum += field_list[0].GetValueDouble();
        
        it++;
    }

    CPPUNIT_ASSERT( fabs(area_sum - 165984002.771) < 1.0 );
    CPPUNIT_ASSERT( eas_id_sum == 110397 );

    delete file;
}

void VectorReadTest::testRandomRead()
{
    PCIDSKFile *file;
    
    file = PCIDSK::Open( "canada.pix", "r", NULL );

    CPPUNIT_ASSERT( file != NULL );

    PCIDSKSegment *seg = file->GetSegment( 11 );
    PCIDSKVectorSegment *vecseg = dynamic_cast<PCIDSKVectorSegment*>( seg );
    
    CPPUNIT_ASSERT( vecseg != NULL );

    std::vector<ShapeField> field_list;
    std::vector<ShapeVertex> vertex_list;

    vecseg->GetFields( 1544, field_list );

    CPPUNIT_ASSERT( field_list[0].GetValueInteger() == 1011 );
    CPPUNIT_ASSERT( field_list[6].GetValueInteger() == 1545 );
    CPPUNIT_ASSERT( field_list[9].GetValueString() == "route Transcanadienne" );

    vecseg->GetVertices( 1544, vertex_list );

    CPPUNIT_ASSERT( vertex_list.size() == 11 );
    CPPUNIT_ASSERT( fabs(vertex_list[10].y-68010.6171875) < 0.0000001 );

    vecseg->GetFields( 1, field_list );

    CPPUNIT_ASSERT( field_list[0].GetValueInteger() == 77 );
    CPPUNIT_ASSERT( field_list[6].GetValueInteger() == 2 );
    CPPUNIT_ASSERT( field_list[9].GetValueString() == "" );

    vecseg->GetVertices( 1, vertex_list );

    CPPUNIT_ASSERT( vertex_list.size() == 4 );
    CPPUNIT_ASSERT( fabs(vertex_list[3].y-1234782.125) < 0.0000001 );

    delete file;
}

