/******************************************************************************
 * $Id$
 *
 * Name:     gdaltransformer.java
 * Project:  GDAL Java Interface
 * Purpose:  A sample app for demonstrating the methods of Transformer class
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
import org.gdal.gdal.Transformer;
import org.gdal.osr.SpatialReference;
import java.util.Vector;

public class gdaltransformer
{
    public static void main(String args[])
    {
        gdal.AllRegister();
        Driver memDriver = gdal.GetDriverByName("MEM");
        Dataset ds1 = memDriver.Create("mem", 100, 100);
        ds1.SetGeoTransform(new double[] { 2, 0.01, 0, 49, 0, -0.01 });
        SpatialReference sr1 = new SpatialReference();
        sr1.ImportFromEPSG(4326);

        Dataset ds2 = memDriver.Create("mem2", 100, 100);
        ds2.SetGeoTransform(new double[] { 400000, 1000, 0, 5500000, 0, -1000 });
        SpatialReference sr2 = new SpatialReference();
        sr2.ImportFromEPSG(32631);

        Vector options = new Vector();
        options.add("SRC_SRS=" + sr1.ExportToWkt());
        options.add("DST_SRS=" + sr2.ExportToWkt());
        Transformer t = new Transformer(ds1, ds2, options);
        double argout[] = new double[3];
        argout[0] = 0;
        argout[1] = 0;
        int ret = t.TransformPoint(0, argout);
        if (ret == 0)
            throw new RuntimeException();
        System.out.println("x=" + argout[0] + " y=" + argout[1]);

        ret = t.TransformPoint(1, argout);
        if (ret == 0)
            throw new RuntimeException();
        System.out.println("x=" + argout[0] + " y=" + argout[1]);

        ret = t.TransformPoint(argout, 0, 0, 0, 0);
        if (ret == 0)
            throw new RuntimeException();
        System.out.println("x=" + argout[0] + " y=" + argout[1]);

        ret = t.TransformPoint(argout, 0, 0, 0);
        if (ret == 0)
            throw new RuntimeException();
        System.out.println("x=" + argout[0] + " y=" + argout[1]);

        double arrayOfPoints[][] = new double[][] { new double[]{0, 0}, new double[]{100,100} };
        ret = t.TransformPoints(0, arrayOfPoints, null);
        if (ret == 0)
            throw new RuntimeException();
        System.out.println("x=" + arrayOfPoints[0][0] + " y=" + arrayOfPoints[0][1]);
        System.out.println("x=" + arrayOfPoints[1][0] + " y=" + arrayOfPoints[1][1]);

        ret = t.TransformPoints(1, arrayOfPoints, null);
        if (ret == 0)
            throw new RuntimeException();
        System.out.println("x=" + arrayOfPoints[0][0] + " y=" + arrayOfPoints[0][1]);
        System.out.println("x=" + arrayOfPoints[1][0] + " y=" + arrayOfPoints[1][1]);
    }
}
