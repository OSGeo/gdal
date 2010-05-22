/******************************************************************************
 * $Id$
 *
 * Name:     createdata.cs
 * Project:  GDAL CSharp Interface
 * Purpose:  A sample app to create a spatial data source and a layer.
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

 * <p>Title: GDAL C# createdata example.</p>
 * <p>Description: A sample app to create a spatial data source and a layer.</p>
 * @author Tamas Szekeres (szekerest@gmail.com)
 * @version 1.0
 */



/// <summary>
/// A C# based sample to create a layer.
/// </summary> 

class CreateData {
	
	public static void usage() 

	{ 
		Console.WriteLine("usage: createdata {data source name} {layername}");
		System.Environment.Exit(-1);
	}
 
	public static void Main(string[] args) {

		if (args.Length != 2) usage();

        // Using early initialization of System.Console
        Console.WriteLine("");

		/* -------------------------------------------------------------------- */
		/*      Register format(s).                                             */
		/* -------------------------------------------------------------------- */
        Ogr.RegisterAll();

		/* -------------------------------------------------------------------- */
		/*      Get driver                                                      */
		/* -------------------------------------------------------------------- */	
        Driver drv = Ogr.GetDriverByName("ESRI Shapefile");

		if (drv == null) 
		{
			Console.WriteLine("Can't get driver.");
            System.Environment.Exit(-1);
		}

        // TODO: drv.name is still unsafe with lazy initialization (Bug 1339)
        //string DriverName = drv.name;
        //Console.WriteLine("Using driver " + DriverName);

		/* -------------------------------------------------------------------- */
		/*      Creating the datasource                                         */
		/* -------------------------------------------------------------------- */	

        DataSource ds = drv.CreateDataSource( args[0], new string[] {} );
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
            if( layer != null && layer.GetLayerDefn().GetName() == args[1])
            {
                Console.WriteLine("Layer already existed. Recreating it.\n");
                ds.DeleteLayer(i);
                break;
            }
        }

        layer = ds.CreateLayer( args[1], null, wkbGeometryType.wkbPoint, new string[] {} );
        if( layer == null )
        {
            Console.WriteLine("Layer creation failed.");
            System.Environment.Exit(-1);
        }

        /* -------------------------------------------------------------------- */
        /*      Adding attribute fields                                         */
        /* -------------------------------------------------------------------- */

        FieldDefn fdefn = new FieldDefn( "Name", FieldType.OFTString );

        fdefn.SetWidth(32);

        if( layer.CreateField( fdefn, 1 ) != 0 )
        {
            Console.WriteLine("Creating Name field failed.");
            System.Environment.Exit(-1);
        }

		fdefn = new FieldDefn( "IntField", FieldType.OFTInteger );
		if( layer.CreateField( fdefn, 1 ) != 0 )
		{
			Console.WriteLine("Creating IntField field failed.");
			System.Environment.Exit(-1);
		}

		fdefn = new FieldDefn( "DbleField", FieldType.OFTReal );
		if( layer.CreateField( fdefn, 1 ) != 0 )
		{
			Console.WriteLine("Creating DbleField field failed.");
			System.Environment.Exit(-1);
		}

		fdefn = new FieldDefn( "DateField", FieldType.OFTDate );
		if( layer.CreateField( fdefn, 1 ) != 0 )
		{
			Console.WriteLine("Creating DateField field failed.");
			System.Environment.Exit(-1);
		}

        /* -------------------------------------------------------------------- */
        /*      Adding features                                                 */
        /* -------------------------------------------------------------------- */

        Feature feature = new Feature( layer.GetLayerDefn() );
        feature.SetField( "Name", "value" );
		feature.SetField( "IntField", (int)123 );
		feature.SetField( "DbleField", (double)12.345 );
		feature.SetField( "DateField", 2007, 3, 15, 18, 24, 30, 0 );

        Geometry geom = Geometry.CreateFromWkt("POINT(47.0 19.2)");
        
        if( feature.SetGeometry( geom ) != 0 )
        {
            Console.WriteLine( "Failed add geometry to the feature" );
            System.Environment.Exit(-1);
        }

        if( layer.CreateFeature( feature ) != 0 )
        {
            Console.WriteLine( "Failed to create feature in shapefile" );
            System.Environment.Exit(-1);
        }
        
		ReportLayer(layer);
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
				Console.WriteLine( feat.GetFieldAsString( iField ) );
			else
				Console.WriteLine( "(null)" );
            
		}

		if( feat.GetStyleString() != null )
			Console.WriteLine( "  Style = " + feat.GetStyleString() );
    
		Geometry geom = feat.GetGeometryRef();
		if( geom != null )
			Console.WriteLine( "  " + geom.GetGeometryName() + 
				"(" + geom.GetGeometryType() + ")" );

		Envelope env = new Envelope();
		geom.GetEnvelope(env);
		Console.WriteLine( "   ENVELOPE: " + env.MinX + "," + env.MaxX + "," +
			env.MinY + "," + env.MaxY);

		string geom_wkt;
		geom.ExportToWkt(out geom_wkt);
		Console.WriteLine( "  " + geom_wkt );

		Console.WriteLine( "" );
	}
}