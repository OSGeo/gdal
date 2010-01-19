/******************************************************************************
 *
 * Purpose:  CPPUnit test for PCIDSK exception handling.
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

using PCIDSK::PCIDSKException;

class PCIDSKExceptionTest : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE( PCIDSKExceptionTest );
 
    CPPUNIT_TEST( testFormatting );

    CPPUNIT_TEST_SUITE_END();

public:
    void setUp();
    void tearDown();

    void testFormatting();
};

// Registers the fixture into the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION( PCIDSKExceptionTest );

void PCIDSKExceptionTest::setUp()
{
}


void PCIDSKExceptionTest::tearDown()
{
}

void PCIDSKExceptionTest::testFormatting()
{
    PCIDSKException ex( "Illegal Value:%s", "value" );

    CPPUNIT_ASSERT( strcmp(ex.what(),"Illegal Value:value") == 0 );
}
