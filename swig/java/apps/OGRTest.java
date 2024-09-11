/******************************************************************************
 * Name:     OGRTest.java
 * Project:  OGR Java Interface
 * Purpose:  Test OGR module
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault
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
