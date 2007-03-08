using System;

using OSGeo.OGR;


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
		SpatialReference sr = layer.GetSpatialRef();
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