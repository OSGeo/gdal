/******************************************************************************
 * $Id$
 *
 * Name:     OGRTestGC.java
 * Project:  OGR Java Interface
 * Purpose:  A sample app for demonstrating the caveats with JNI and garbage collecting...
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
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
