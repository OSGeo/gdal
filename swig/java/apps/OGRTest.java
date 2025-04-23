/******************************************************************************
 * Name:     OGRTest.java
 * Project:  OGR Java Interface
 * Purpose:  Test OGR module
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

import java.util.Arrays;
import org.gdal.ogr.*;

public class OGRTest
{
    public static void main(String[] args) throws Exception
    {
        ogr.RegisterAll();
        DataSource poDS = ogr.Open("tmp_test/iso_8859_1.csv", false);
        Layer lyr = poDS.GetLayer(0);
        FieldDefn fld_defn = lyr.GetLayerDefn().GetFieldDefn(1);
        String got = fld_defn.GetName();
        if (!got.equals("field_"))
            throw new Exception("Got '" + got + "'");
        byte[] byteArray = fld_defn.GetNameAsByteArray();
        if(!Arrays.equals(byteArray, new byte[]{'f', 'i', 'e', 'l', 'd', '_', 0xE9 - 256, 'x'}))
            throw new Exception("GetNameAsByteArray() returned unexpected content of size " + byteArray.length);
    }
}
