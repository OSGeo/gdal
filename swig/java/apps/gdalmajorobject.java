/******************************************************************************
 *
 * Name:     gdalmajorobject.java
 * Project:  GDAL Java Interface
 * Purpose:  A sample app for demonstrating the methods of MajorObject
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2009, Even Rouault
 *
 * SPDX-License-Identifier: MIT
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