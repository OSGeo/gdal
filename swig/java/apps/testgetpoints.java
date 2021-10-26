/******************************************************************************
 * $Id$
 *
 * Name:     testgetpoints.java
 * Project:  OGR Java Interface
 * Purpose:  A sample app to test Geometry.GetPoints()
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault
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

import org.gdal.ogr.Geometry;

public class testgetpoints
{
    public static void showCoords(Geometry geom)
    {
        double[][] coords = geom.GetPoints();
        for(int i = 0; i < coords.length; i++)
        {
            if (i > 0)
                System.out.print(", ");
            if (coords[i].length == 2)
                System.out.print(coords[i][0] + " " + coords[i][1]);
            else
                System.out.print(coords[i][0] + " " + coords[i][1] + " " + coords[i][2]);
        }
        System.out.print("\n");
    }

    public static void showCoords(Geometry geom, int nCoordDimension)
    {
        double[][] coords = geom.GetPoints(nCoordDimension);
        for(int i = 0; i < coords.length; i++)
        {
            if (i > 0)
                System.out.print(", ");
            if (coords[i].length == 2)
                System.out.print(coords[i][0] + " " + coords[i][1]);
            else
                System.out.print(coords[i][0] + " " + coords[i][1] + " " + coords[i][2]);
        }
        System.out.print("\n");
    }

    public static void main(String[] args)
    {
        showCoords(Geometry.CreateFromWkt("LINESTRING(0 1,2 3)"));
        showCoords(Geometry.CreateFromWkt("LINESTRING(0 1 2,3 4 5)"));
        showCoords(Geometry.CreateFromWkt("LINESTRING(0 1,2 3)"), 3);
        showCoords(Geometry.CreateFromWkt("LINESTRING(0 1 2,3 4 5)"), 2);
    }
}