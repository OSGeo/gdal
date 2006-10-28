using System;
using OGR;

/**
 * <p>Title: GDAL C# ogrinfo example.</p>
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
		Console.WriteLine("usage: ogrinfo {data source name} {layername}");
		System.Environment.Exit(-1);
	}
 
	public static void Main(string[] args) {
		if (args.Length != 2) usage();
		/* -------------------------------------------------------------------- */
		/*      Register format(s).                                             */
		/* -------------------------------------------------------------------- */
		ogr.RegisterAll();

		/* -------------------------------------------------------------------- */
		/*      Get driver                                                      */
		/* -------------------------------------------------------------------- */	
		Driver drv = ogr.GetDriverByName("ESRI Shapefile");

		if (drv == null) 
		{
			Console.WriteLine("Can't get driver.");
			System.Environment.Exit(-1);
		}
        // TODO: drv.name is still unsafe
        //Console.WriteLine("Using driver " + drv.name);
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

        layer = ds.CreateLayer( args[1], null, ogr.wkbPoint, new string[] {} );
        if( layer == null )
        {
            Console.WriteLine("Layer creation failed.");
            System.Environment.Exit(-1);
        }

        /* -------------------------------------------------------------------- */
        /*      Adding attribute fields                                         */
        /* -------------------------------------------------------------------- */

        FieldDefn fdefn = new FieldDefn( "Name", ogr.OFTString );

        fdefn.SetWidth(32);

        if( layer.CreateField( fdefn, 1 ) != 0 )
        {
            Console.WriteLine("Creating Name field failed.");
            System.Environment.Exit(-1);
        }

        /* -------------------------------------------------------------------- */
        /*      Adding features                                                 */
        /* -------------------------------------------------------------------- */

        Feature feature = new Feature( layer.GetLayerDefn() );
        feature.SetField( "Name", "value" );

        Geometry geom = new Geometry(ogr.wkbUnknown, "POINT(47.0 19.2)", 0, null, null);
        
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