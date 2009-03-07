/******************************************************************************
 * $Id$
 *
 * Name:     gdalmajorobject.java
 * Project:  GDAL Java Interface
 * Purpose:  A sample app for demonstrating the methods of MajorObject
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2009, Even Rouault
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

import org.gdal.gdal.gdal;
import org.gdal.gdal.Driver;
import org.gdal.gdal.Dataset;
import java.util.Vector;
import java.util.Hashtable;

public class gdalmajorobject
{
    public static void main(String args[])
    {
        gdal.AllRegister();
        Driver memDriver = gdal.GetDriverByName("MEM");
        Dataset ds = memDriver.Create("mem", 1, 1);

        ds.SetDescription("RasterAttributeTable");
        if (!ds.GetDescription().equals("RasterAttributeTable"))
            throw new RuntimeException();

        ds.SetMetadataItem("key", "value");
        if (!ds.GetMetadataItem("key").equals("value"))
            throw new RuntimeException();

        Vector v = ds.GetMetadata_List();
        if (!((String)v.elementAt(0)).equals("key=value"))
            throw new RuntimeException();

        Hashtable h = ds.GetMetadata_Dict();
        if (!h.get("key").equals("value"))
            throw new RuntimeException();

        ds.delete();
        ds = memDriver.Create("mem", 1, 1);

        ds.SetMetadata("key=value");
        if (!ds.GetMetadataItem("key").equals("value"))
            throw new RuntimeException();

        ds.delete();
        ds = memDriver.Create("mem", 1, 1);

        h = new Hashtable();
        h.put("key", "value");
        ds.SetMetadata(h);
        if (!ds.GetMetadataItem("key").equals("value"))
            throw new RuntimeException();
        else
            System.out.println("Success");
    }
}