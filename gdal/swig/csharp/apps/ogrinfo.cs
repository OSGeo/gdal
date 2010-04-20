/******************************************************************************
 * $Id$
 *
 * Name:     ogrinfo.cs
 * Project:  GDAL CSharp Interface
 * Purpose:  A sample app to dump information from a spatial data source.
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
using OSGeo.OSR;


/**

 * <p>Title: GDAL C# ogrinfo example.</p>
 * <p>Description: A sample app to dump information from a spatial data source.</p>
 * @author Tamas Szekeres (szekerest@gmail.com)
 * @version 1.0
 */



/// <summary>
/// A C# based sample to dump information from a data source.
/// </summary> 

class OGRInfo {
	
	public static void usage() 

	{ 
		Console.WriteLine("usage: ogrinfo {data source name}");
		System.Environment.Exit(-1);
	}
 
	public static void Main(string[] args) {

		if (args.Length != 1) usage();

        // Using early initialization of System.Console
        Console.WriteLine("");

		/* -------------------------------------------------------------------- */
		/*      Register format(s).                                             */
		/* -------------------------------------------------------------------- */
		Ogr.RegisterAll();

		/* -------------------------------------------------------------------- */
		/*      Open data source.                                               */
		/* -------------------------------------------------------------------- */
		DataSource ds = Ogr.Open( args[0], 0 );
		
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
        // TODO: drv.name is still unsafe with lazy initialization (Bug 1339)
        Console.WriteLine("Using driver " + drv.name);

		/* -------------------------------------------------------------------- */
		/*      Iterating through the layers                                    */
		/* -------------------------------------------------------------------- */	

		for( int iLayer = 0; iLayer < ds.GetLayerCount(); iLayer++ )
		{
			Layer layer = ds.GetLayerByIndex(iLayer);

			if( layer == null )
			{
				Console.WriteLine( "FAILURE: Couldn't fetch advertised layer " + iLayer );
				System.Environment.Exit(-1);
			}
			ReportLayer(layer);
		}
	}

	public static void ReportLayer(Layer layer)
	{
		FeatureDefn def = layer.GetLayerDefn();
		Console.WriteLine( "Layer name: " + def.GetName() );
		Console.WriteLine( "Feature Count: " + layer.GetFeatureCount(1) );
		Envelope ext = new Envelope();
		layer.GetExtent(ext, 1);
		Console.WriteLine( "Extent: " + ext.MinX + "," + ext.MaxX + "," +
			ext.MinY + "," + ext.MaxY);
		
		/* -------------------------------------------------------------------- */
		/*      Reading the spatial reference                                   */
		/* -------------------------------------------------------------------- */
        OSGeo.OSR.SpatialReference sr = layer.GetSpatialRef();
		string srs_wkt;
		if ( sr != null ) 
		{
			sr.ExportToPrettyWkt( out srs_wkt, 1 );
		}
		else
			srs_wkt = "(unknown)";


        Console.WriteLine( "Layer SRS WKT: " + srs_wkt );

		/* -------------------------------------------------------------------- */
		/*      Reading the fields                                              */
		/* -------------------------------------------------------------------- */
		Console.WriteLine("Field definition:");
		for( int iAttr = 0; iAttr < def.GetFieldCount(); iAttr++ )
		{
			FieldDefn fdef = def.GetFieldDefn( iAttr );
            
			Console.WriteLine( fdef.GetNameRef() + ": " + 
				fdef.GetFieldTypeName( fdef.GetFieldType() ) + " (" +
				fdef.GetWidth() + "." +
				fdef.GetPrecision() + ")");
		}

		/* -------------------------------------------------------------------- */
		/*      Reading the shapes                                              */
		/* -------------------------------------------------------------------- */
		Console.WriteLine( "" );
		Feature feat;
		while( (feat = layer.GetNextFeature()) != null )
		{
			ReportFeature(feat, def);
			feat.Dispose();
		}
	}

	public static void ReportFeature(Feature feat, FeatureDefn def)
	{
		Console.WriteLine( "Feature(" + def.GetName() + "): " + feat.GetFID() );
		for( int iField = 0; iField < feat.GetFieldCount(); iField++ )
		{
			FieldDefn fdef = def.GetFieldDefn( iField );
            
			Console.Write( fdef.GetNameRef() + " (" +
				fdef.GetFieldTypeName(fdef.GetFieldType()) + ") = ");

			if( feat.IsFieldSet( iField ) )
            {
                if (fdef.GetFieldType() == FieldType.OFTStringList)
                {
                    string[] sList = feat.GetFieldAsStringList(iField);
                    foreach (string s in sList)
                    {
                        Console.Write("\"" + s + "\" ");
                    }
                    Console.WriteLine();
                }
                else if (fdef.GetFieldType() == FieldType.OFTIntegerList)
                {
                    int count;
                    int[] iList = feat.GetFieldAsIntegerList(iField, out count);
                    for (int i = 0; i < count; i++)
                    {
                        Console.Write(iList[i] + " ");
                    }
                    Console.WriteLine();
                }
                else if (fdef.GetFieldType() == FieldType.OFTRealList)
                {
                    int count;
                    double[] iList = feat.GetFieldAsDoubleList(iField, out count);
                    for (int i = 0; i < count; i++)
                    {
                        Console.Write(iList[i].ToString() + " ");
                    }
                    Console.WriteLine();
                }
                else
                    Console.WriteLine(feat.GetFieldAsString(iField));
            }
			else
				Console.WriteLine( "(null)" );
            
		}

		if( feat.GetStyleString() != null )
			Console.WriteLine( "  Style = " + feat.GetStyleString() );
    
		Geometry geom = feat.GetGeometryRef();
		if( geom != null ) 
		{
			Console.WriteLine( "  " + geom.GetGeometryName() + 
				"(" + geom.GetGeometryType() + ")" );
			Geometry sub_geom;
			for (int i = 0; i < geom.GetGeometryCount(); i++)
			{
				sub_geom = geom.GetGeometryRef(i);
				if ( sub_geom != null )
				{
					Console.WriteLine( "  subgeom" + i + ": " + sub_geom.GetGeometryName() + 
						"(" + sub_geom.GetGeometryType() + ")" );
				}
			}
            Envelope env = new Envelope();
            geom.GetEnvelope(env);
            Console.WriteLine("   ENVELOPE: " + env.MinX + "," + env.MaxX + "," +
                env.MinY + "," + env.MaxY);

            string geom_wkt;
            geom.ExportToWkt(out geom_wkt);
            Console.WriteLine("  " + geom_wkt);
		}

		Console.WriteLine( "" );
	}
}