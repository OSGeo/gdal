/******************************************************************************
 *
 * Name:     OGRTestGC.java
 * Project:  OGR Java Interface
 * Purpose:  A sample app for demonstrating the caveats with JNI and garbage collecting...
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2009, Even Rouault
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

import org.gdal.ogr.ogr;
import org.gdal.ogr.Feature;
import org.gdal.ogr.FeatureDefn;
import org.gdal.ogr.Geometry;

/* This test should run fine as we use a smarter GC for */
/* Geometry and Feature objects */

public class OGRTestGC
{
    public static void main(String[] args)
    {
        FeatureDefn featureDefn = new FeatureDefn();

        Geometry point1 = new Geometry(ogr.wkbPoint);
        Geometry multipoint1 = new Geometry(ogr.wkbMultiPoint);
        multipoint1.AddGeometryDirectly(point1);
        try
        {
            /* Just to show that we are smart ! */
            multipoint1.AddGeometryDirectly(point1);
            System.err.println("should not reach that point");
        }
        catch(RuntimeException re)
        {
        }

        Geometry multipoint2 = new Geometry(ogr.wkbMultiPoint);
        multipoint2.AddGeometry(multipoint1.GetGeometryRef(0));
        try
        {
            /* Just to show that we are smart ! */
            multipoint2.AddGeometryDirectly(multipoint1.GetGeometryRef(0));
            System.err.println("should not reach that point");
        }
        catch(RuntimeException re)
        {
        }

        Geometry point3 = new Geometry(ogr.wkbPoint);
        new Feature(featureDefn).SetGeometryDirectly(point3);

        multipoint1 = null;

        for (int i = 0; i < 500000; i++)
        {
            if ((i % 100000) == 0) System.out.println(i);
            Feature feat = new Feature(featureDefn);
            feat.SetGeometryDirectly(new Geometry(ogr.wkbMultiPoint));
            feat.SetGeometry(null);
            feat.GetGeometryRef();
        }

        // Add features
        for (int i = 0; i < 1000000; i++)
        {
            if ((i % 100000) == 0) System.out.println(i);
            Feature feat = new Feature(featureDefn);
            feat.SetGeometry(new Geometry(ogr.wkbPoint));

            Geometry point = new Geometry(ogr.wkbPoint);
            Geometry multipoint = new Geometry(ogr.wkbMultiPoint);
            multipoint.AddGeometryDirectly(point);
        }

        /* Check that the objects are still alive despite their */
        /* Java containers and associated native objects */
        /* would have been finalized without a trick */
        System.out.println(point1.ExportToWkt());
        System.out.println(point3.ExportToWkt());

    }
}
