/******************************************************************************
 * $Id$
 *
 * Name:     OGRGEOS.cs
 * Project:  GDAL CSharp Interface
 * Purpose:  A sample app for testing the OGR GEOS support.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Tamas Szekeres
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

using System;

using OSGeo.OGR;


/**

 * <p>Title: OGR C# GEOS example.</p>
 * <p>Description: A sample app for testing the OGR GEOS support.</p>
 * @author Tamas Szekeres (szekerest@gmail.com)
 * @version 1.0
 */



/// <summary>
/// A C# based sample for testing the OGR GEOS support.
/// </summary> 

class OGRGEOS {
	
	public static void Main(string[] args) {
        Console.WriteLine("");

        if (TestContains() != 0)
            System.Environment.Exit(-1);
        if (TestIntersect() != 0)
            System.Environment.Exit(-1);
	}

	private static int TestContains()
    {
        Geometry g1 = Geometry.CreateFromWkt( "POLYGON((0 0, 10 10, 10 0, 0 0))" );
        Geometry g2 = Geometry.CreateFromWkt("POLYGON((-90 -90, -90 90, 190 -90, -90 -90))");

        bool result = g2.Contains( g1 );

        if (result == false)
        {
            Console.WriteLine( "Contains: wrong result (got false)" );
            return -1;
        }

        result = g1.Contains( g2 );

        if (result != false)
        {
            Console.WriteLine( "Contains: wrong result (got false)" );
            return -1;
        }
        Console.WriteLine("Contains test OK");
        return 0;
    }

    private static int TestIntersect()
    {
        Geometry g1 = Geometry.CreateFromWkt("LINESTRING(0 0, 10 10)");
        Geometry g2 = Geometry.CreateFromWkt("LINESTRING(10 0, 0 10)");

        bool result = g1.Intersect(g2);

        if (result == false)
        {
            Console.WriteLine("Intersect: wrong result (got false)");
            return -1;
        }

        g1 = Geometry.CreateFromWkt("LINESTRING(0 0, 10 10)");
        g2 = Geometry.CreateFromWkt("POLYGON((20 20, 20 30, 30 20, 20 20))");

        result = g1.Intersect(g2);

        if (result == true)
        {
            Console.WriteLine("Intersect: wrong result (got true)");
            return -1;
        }
        
        Console.WriteLine("Intersect test OK");
        return 0;
    }
}
