/******************************************************************************
 * $Id$
 *
 * Name:     test_ogrfielddomains.java
 * Project:  GDAL SWIG Interface
 * Purpose:  Test field domains API
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault
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

import org.gdal.ogr.*;
import java.util.*;

public class test_ogrfielddomains
{
    public static void testOGRCodedValueTypemap()
    {
        HashMap<String, String> enumeration = new java.util.HashMap<String, String>();
        enumeration.put("one", "1");
        enumeration.put("two", "2");
        enumeration.put("three", null);
        FieldDomain domain = ogr.CreateCodedFieldDomain("name", "description", ogr.OFTString, ogr.OFSTNone, enumeration);

        HashMap<String, String> enumerationGot = domain.GetEnumeration();
        if( enumerationGot.size() != 3 )
            throw new RuntimeException("enumerationGot.size() != 3");
        if( !enumerationGot.get("one").equals("1") )
            throw new RuntimeException("!enumerationGot.get(\"one\").equals(\"1\")");
        if( !enumerationGot.get("two").equals("2") )
            throw new RuntimeException("!enumerationGot.get(\"two\").equals(\"2\")");
        if( enumerationGot.get("three") != null )
            throw new RuntimeException("enumerationGot.get(\"three\") != null");
    }

    public static void main(String[] args)
    {
        testOGRCodedValueTypemap();
    }
}
