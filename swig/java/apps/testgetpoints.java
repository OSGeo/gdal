/******************************************************************************
 *
 * Name:     testgetpoints.java
 * Project:  OGR Java Interface
 * Purpose:  A sample app to test Geometry.GetPoints()
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault
 *
 * SPDX-License-Identifier: MIT
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