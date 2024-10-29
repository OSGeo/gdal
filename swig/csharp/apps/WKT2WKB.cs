/******************************************************************************
 * $Id$
 *
 * Name:     WKT2WKB.cs
 * Project:  GDAL CSharp Interface
 * Purpose:  A sample app for demonstrating the usage of ExportToWkb.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Tamas Szekeres
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

using System;

using OSGeo.OGR;


/**

 * <p>Title: GDAL C# wkt2wkb example.</p>
 * <p>Description: A sample app for demonstrating the usage of ExportToWkb.</p>
 * @author Tamas Szekeres (szekerest@gmail.com)
 * @version 1.0
 */



/// <summary>
/// A C# based sample for demonstrating the usage of ExportToWkb.
/// </summary>

class WKT2WKB {

	public static void usage()

	{
		Console.WriteLine("usage example: wkt2wkb \"POINT(47.0 19.2)\"");
		System.Environment.Exit(-1);
	}

	public static void Main(string[] args) {

		if (args.Length != 1) usage();

		/* -------------------------------------------------------------------- */
		/*      Register format(s).                                             */
		/* -------------------------------------------------------------------- */
        Ogr.RegisterAll();

        Geometry geom = Geometry.CreateFromWkt(args[0]);

		long wkbSize = geom.WkbSize();
        if (wkbSize > 0)
        {
            byte[] wkb = new byte[wkbSize];
            geom.ExportToWkb( wkb );
            Console.WriteLine( "wkt-->wkb: " + BitConverter.ToString(wkb) );

			// wkb --> wkt (reverse test)
			Geometry geom2 = Geometry.CreateFromWkb(wkb);
			string geom_wkt;
			geom2.ExportToWkt(out geom_wkt);
			Console.WriteLine( "wkb->wkt: " + geom_wkt );
        }

		// wkt -- gml transformation
       string gml = geom.ExportToGML();
       Console.WriteLine( "wkt->gml: " + gml );

       Geometry geom3 = Geometry.CreateFromGML(gml);
	   string geom_wkt2;
	   geom3.ExportToWkt(out geom_wkt2);
	   Console.WriteLine( "gml->wkt: " + geom_wkt2 );
	}
}
