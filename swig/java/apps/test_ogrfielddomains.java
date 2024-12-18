/******************************************************************************
 *
 * Name:     test_ogrfielddomains.java
 * Project:  GDAL SWIG Interface
 * Purpose:  Test field domains API
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault
 *
 * SPDX-License-Identifier: MIT
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
