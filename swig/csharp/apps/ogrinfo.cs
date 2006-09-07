using System;
using OGR;

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
		/* -------------------------------------------------------------------- */
		/*      Register format(s).                                             */
		/* -------------------------------------------------------------------- */
		ogr.RegisterAll();
		/* -------------------------------------------------------------------- */
		/*      Open data source.                                               */
		/* -------------------------------------------------------------------- */
		DataSource ds = ogr.Open( args[0], 0 );
		
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
		for( int iLayer = 0; iLayer < ds.GetLayerCount(); iLayer++ )
		{
			Layer layer = ds.GetLayerByIndex(iLayer);

			if( layer == null )
			{
				Console.WriteLine( "FAILURE: Couldn't fetch advertised layer " + iLayer );
				System.Environment.Exit(-1);
			}
			Console.WriteLine(layer.GetName());
			ReportLayer(layer);
		}
	}
	public static void ReportLayer(Layer layer)
	{
		FeatureDefn def = layer.GetLayerDefn();
		Console.WriteLine( "Layer name: " + def.GetName() );
		//poDefn->GetGeomType()
		Console.WriteLine( "Feature Count: " + layer.GetFeatureCount(1) );
		Console.WriteLine( "Extent: " + layer.GetExtent(1) );

		SpatialReference sr = layer.GetSpatialRef();
		string srs_wkt = "(unknown)";
		if ( sr != null )
			//sr.ExportToPrettyWkt( 1 );

        Console.WriteLine( "Layer SRS WKT: " + srs_wkt );

		Console.WriteLine("Field definition:");
		for( int iAttr = 0; iAttr < def.GetFieldCount(); iAttr++ )
		{
			FieldDefn fdef = def.GetFieldDefn( iAttr );
            
			Console.WriteLine( fdef.GetNameRef() + ": " + 
				fdef.GetFieldTypeName( fdef.GetFieldType() ) + " (" +
				fdef.GetWidth() + "." +
				fdef.GetPrecision() + ")");
		}

	
	}
}