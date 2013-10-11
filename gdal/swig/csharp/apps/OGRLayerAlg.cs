/******************************************************************************
 * $Id$
 *
 * Name:     OGRLayerAlg.cs
 * Project:  GDAL CSharp Interface
 * Purpose:  A sample app to demonstrate the usage of the RFC-39 functions.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Tamas Szekeres
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

 * <p>Title: OGR C# OGRLayerAlg example.</p>
 * <p>Description: A sample app to demonstrate the usage of the RFC-39 functions.</p>
 * @author Tamas Szekeres (szekerest@gmail.com)
 * @version 1.0
 */



/// <summary>
/// A sample app to demonstrate the usage of the RFC-39 functions.
/// </summary> 

class OGRLayerAlg {
	
	public static void usage() 

	{
        Console.WriteLine("usage: ogrlayeralg {function} {Data Source 1} {layer1} {Data Source 2} {layer2} {Result Data Source Name} {Result Layer Name}");
        Console.WriteLine("example: ogrlayeralg union data1.shp layer1 data.shp layer2 result.shp resultlayer");
		System.Environment.Exit(-1);
	}
 
    public static void Main(string[] args) 
    {
        if (args.Length <= 6) usage();
        
        Console.WriteLine("");

        try 
        {
            /* -------------------------------------------------------------------- */
            /*      Register driver(s).                                             */
            /* -------------------------------------------------------------------- */
            Ogr.RegisterAll();

            /* -------------------------------------------------------------------- */
            /*      Open data source.                                              */
            /* -------------------------------------------------------------------- */
            DataSource ds1 = Ogr.Open(args[1], 0);

            if (ds1 == null)
            {
                Console.WriteLine("Can't open " + args[1]);
                System.Environment.Exit(-1);
            }

            Layer layer1 = ds1.GetLayerByName(args[2]);

            if (layer1 == null)
            {
                Console.WriteLine("FAILURE: Couldn't fetch layer " + args[2]);
                System.Environment.Exit(-1);
            }

            DataSource ds2 = Ogr.Open(args[3], 0);

            if (ds2 == null)
            {
                Console.WriteLine("Can't open " + args[3]);
                System.Environment.Exit(-1);
            }

            Layer layer2 = ds1.GetLayerByName(args[4]);

            if (layer1 == null)
            {
                Console.WriteLine("FAILURE: Couldn't fetch layer " + args[4]);
                System.Environment.Exit(-1);
            }

            if (layer1.GetLayerDefn().GetGeomType() != layer2.GetLayerDefn().GetGeomType())
            {
                Console.WriteLine("FAILURE: Geometry type doesn't match");
                System.Environment.Exit(-1);
            }

            /* -------------------------------------------------------------------- */
		    /*      Get driver for creating the result ds                          */
		    /* -------------------------------------------------------------------- */	
            Driver drv = Ogr.GetDriverByName("ESRI Shapefile");

		    if (drv == null) 
		    {
			    Console.WriteLine("Can't get driver.");
                System.Environment.Exit(-1);
		    }

            /* -------------------------------------------------------------------- */
		    /*      Creating the datasource                                         */
		    /* -------------------------------------------------------------------- */	

            DataSource ds = drv.CreateDataSource( args[5], new string[] {} );
            if (drv == null) 
            {
                Console.WriteLine("Can't create the datasource.");
                System.Environment.Exit(-1);
            }

            /* -------------------------------------------------------------------- */
            /*      Creating the layer                                              */
            /* -------------------------------------------------------------------- */

            Layer layer;
        
            int i;
            for(i=0;i<ds.GetLayerCount();i++)
            {
                layer = ds.GetLayerByIndex( i );
                if( layer != null && layer.GetLayerDefn().GetName() == args[6])
                {
                    Console.WriteLine("Layer already existed. Recreating it.\n");
                    ds.DeleteLayer(i);
                    break;
                }
            }

            layer = ds.CreateLayer(args[6], null, layer1.GetLayerDefn().GetGeomType(), new string[] { });
            if( layer == null )
            {
                Console.WriteLine("Layer creation failed.");
                System.Environment.Exit(-1);
            }

            switch(args[0])
            {
                case "Intersection":
                    layer1.Intersection(layer2, layer, null, new Ogr.GDALProgressFuncDelegate(ProgressFunc), "Intersection");
                    break;
                case "Union":
                    layer1.Union(layer2, layer, null, new Ogr.GDALProgressFuncDelegate(ProgressFunc), "Union");
                    break;
                case "SymDifference":
                    layer1.SymDifference(layer2, layer, null, new Ogr.GDALProgressFuncDelegate(ProgressFunc), "SymDifference");
                    break;
                case "Identity":
                    layer1.Identity(layer2, layer, null, new Ogr.GDALProgressFuncDelegate(ProgressFunc), "Identity");
                    break;
                case "Update":
                    layer1.Update(layer2, layer, null, new Ogr.GDALProgressFuncDelegate(ProgressFunc), "Update");
                    break;
                case "Clip":
                    layer1.Clip(layer2, layer, null, new Ogr.GDALProgressFuncDelegate(ProgressFunc), "Clip");
                    break;
                case "Erase":
                    layer1.Erase(layer2, layer, null, new Ogr.GDALProgressFuncDelegate(ProgressFunc), "Erase");
                    break;
            }
        }
        catch (Exception e) 
        {
            Console.WriteLine("Application error: " + e.Message);
        }
    }

	public static int ProgressFunc(double Complete, IntPtr Message, IntPtr Data)
	{
		Console.Write("Processing ... " + Complete * 100 + "% Completed.");
		if (Message != IntPtr.Zero)
			Console.Write(" Message:" + System.Runtime.InteropServices.Marshal.PtrToStringAnsi(Message));
		if (Data != IntPtr.Zero)
			Console.Write(" Data:" + System.Runtime.InteropServices.Marshal.PtrToStringAnsi(Data));
	
		Console.WriteLine("");
		return 1;
	}
}