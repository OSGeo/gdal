/******************************************************************************
 * $Id$
 *
 * Name:     WKT2WKB.java
 * Project:  GDAL Java Interface
 * Purpose:  A sample app for demonstrating the usage of ExportToWkb.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 * Port from WKT2WKB.cs by Tamas Szekeres
 *
 ******************************************************************************
 * Copyright (c) 2009, Even Rouault
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

import org.gdal.ogr.Geometry;

/**

 * <p>Title: GDAL Java wkt2wkb example.</p>
 * <p>Description: A sample app for demonstrating the usage of ExportToWkb.</p>
 * @author Tamas Szekeres (szekerest@gmail.com)
 * @version 1.0
 */



/// <summary>
/// A Java based sample for demonstrating the usage of ExportToWkb.
/// </summary> 

public class WKT2WKB {
        
        public static void usage() 
        { 
                System.out.println("usage example: wkt2wkb \"POINT(47.0 19.2)\"");
                System.exit(-1);
        }

        public static void main(String[] args)
        {

            if (args.length != 1) usage();

            Geometry geom = Geometry.CreateFromWkt(args[0]);

            int wkbSize = geom.WkbSize();
            byte[] wkb = geom.ExportToWkb();
            if (wkb.length != wkbSize)
            {
                System.exit(-1);
            }
            if (wkbSize > 0) 
            {
                System.out.print( "wkt-->wkb: ");
                for(int i=0;i<wkbSize;i++)
                {
                    if (i>0)
                        System.out.print("-");
                    int val = wkb[i];
                    if (val < 0)
                        val = 256 + val;
                    String hexVal = Integer.toHexString(val);
                    if (hexVal.length() == 1)
                        System.out.print("0");
                    System.out.print(hexVal);
                }
                System.out.print("\n");

                // wkb --> wkt (reverse test)
                Geometry geom2 = Geometry.CreateFromWkb(wkb);
                String geom_wkt = geom2.ExportToWkt();
                System.out.println( "wkb->wkt: " + geom_wkt );
            }

            // wkt -- gml transformation
            String gml = geom.ExportToGML();
            System.out.println( "wkt->gml: " + gml );

            Geometry geom3 = Geometry.CreateFromGML(gml);
            String geom_wkt2 = geom3.ExportToWkt();
            System.out.println( "gml->wkt: " + geom_wkt2 );
        }
}
