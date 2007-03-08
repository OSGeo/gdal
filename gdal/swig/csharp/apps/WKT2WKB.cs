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

        Geometry geom = new Geometry(wkbGeometryType.wkbUnknown, args[0], 0, null, null);
        
        int wkbSize = geom.WkbSize();
        if (wkbSize > 0) 
        {
            byte[] wkb = new byte[wkbSize];
            geom.ExportToWkb( wkb );
            Console.WriteLine( BitConverter.ToString(wkb) );
        }
	}
}