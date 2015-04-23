/******************************************************************************
 * $Id$
 *
 * Name:     OGRFeatureEdit.cs
 * Project:  GDAL CSharp Interface
 * Purpose:  Sample application to update/delete feature.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2015, Tamas Szekeres
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
using OSGeo.OSR;


/// <summary>
/// Sample application to update/delete feature.
/// </summary> 

class OGRFeatureEdit
{
	public static void usage() 
	{
        Console.WriteLine("usage:");
        Console.WriteLine("OGRFeatureEdit {delete} {data source name} {layer name} {fid}");
        Console.WriteLine("OGRFeatureEdit {update} {data source name} {layer name} {fid} {fieldname} {new value}");
        Console.WriteLine("OGRFeatureEdit {copy} {data source name} {layer name} {src_fid} {dst_fid}");
		System.Environment.Exit(-1);
	}
 
	public static void Main(string[] args) {

		if (args.Length < 4) usage();

        // Using early initialization of System.Console
        Console.WriteLine("");

		/* -------------------------------------------------------------------- */
		/*      Register format(s).                                             */
		/* -------------------------------------------------------------------- */
		Ogr.RegisterAll();

		/* -------------------------------------------------------------------- */
		/*      Open data source.                                               */
		/* -------------------------------------------------------------------- */
		DataSource ds = Ogr.Open( args[1], 1 );
		
		if (ds == null) {
			Console.WriteLine("Can't open " + args[0]);
			System.Environment.Exit(-1);
		}

		/* -------------------------------------------------------------------- */
		/*      Get driver                                                      */
		/* -------------------------------------------------------------------- */	
		Driver drv = ds.GetDriver();

		if (drv == null) 
		{
			Console.WriteLine("Can't get driver.");
			System.Environment.Exit(-1);
		}
        
        Console.WriteLine("Using driver " + drv.name);

		/* -------------------------------------------------------------------- */
		/*      Iterating through the layers                                    */
		/* -------------------------------------------------------------------- */
        Layer layer = ds.GetLayerByName(args[2]);

        if (layer == null)
        {
            Console.WriteLine("FAILURE: Couldn't fetch advertised layer " + args[2]);
            System.Environment.Exit(-1);
        }

        if (args[0].Equals("update", StringComparison.InvariantCultureIgnoreCase))
        {
            Feature feature = layer.GetFeature(long.Parse(args[3]));
            if (feature != null)
            {
                feature.SetField(args[4], args[5]);
                if (layer.SetFeature(feature) == Ogr.OGRERR_NON_EXISTING_FEATURE)
                    Console.WriteLine("feature not found");
                else
                    Console.WriteLine("feature updated successfully");
            }
            Console.WriteLine("feature not found");
        }
        else if (args[0].Equals("copy", StringComparison.InvariantCultureIgnoreCase))
        {
            Feature feature = layer.GetFeature(long.Parse(args[3]));
            if (feature != null)
            {
                feature.SetFID(long.Parse(args[4]));
                if (layer.SetFeature(feature) == Ogr.OGRERR_NON_EXISTING_FEATURE)
                    Console.WriteLine("feature not found");
                else
                    Console.WriteLine("feature copied successfully");
            }
            else
                Console.WriteLine("feature not found");
        }
        else if (args[0].Equals("delete", StringComparison.InvariantCultureIgnoreCase))
        {
            if (layer.TestCapability("DeleteFeature"))
            {
                if (layer.DeleteFeature(long.Parse(args[3])) == Ogr.OGRERR_NON_EXISTING_FEATURE)
                    Console.WriteLine("feature not found");
                else
                    Console.WriteLine("feature removed successfully");
            }
            else
                Console.WriteLine("DeleteFeature not supported");
        }
        else
            Console.WriteLine("invalid command " + args[0]);
	}
}
