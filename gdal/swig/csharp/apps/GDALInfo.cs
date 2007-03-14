using System;

using OSGeo.GDAL;


/**

 * <p>Title: GDAL C# GDALRead example.</p>
 * <p>Description: A sample app to read GDAL raster data information.</p>
 * @author Tamas Szekeres (szekerest@gmail.com)
 * @version 1.0
 */



/// <summary>
/// A C# based sample to read GDAL raster data information.
/// </summary> 

class GDALInfo {
	
	public static void usage() 

	{ 
		Console.WriteLine("usage: gdalinfo {GDAL dataset name}");
		System.Environment.Exit(-1);
	}
 
    public static void Main(string[] args) 
    {

        if (args.Length != 1) usage();

        Console.WriteLine("");

        try 
        {
            /* -------------------------------------------------------------------- */
            /*      Register driver(s).                                             */
            /* -------------------------------------------------------------------- */
            Gdal.AllRegister();

            /* -------------------------------------------------------------------- */
            /*      Open dataset.                                                   */
            /* -------------------------------------------------------------------- */
            Dataset ds = Gdal.Open( args[0], 0 );
		
            if (ds == null) 
            {
                Console.WriteLine("Can't open " + args[0]);
                System.Environment.Exit(-1);
            }

            Console.WriteLine("Raster dataset parameters:");
            Console.WriteLine("  Projection: " + ds.GetProjectionRef());
            Console.WriteLine("  RasterCount: " + ds.RasterCount);
            Console.WriteLine("  RasterSize (" + ds.RasterXSize + "," + ds.RasterYSize + ")");
            
            /* -------------------------------------------------------------------- */
            /*      Get driver                                                      */
            /* -------------------------------------------------------------------- */	
            Driver drv = ds.GetDriver();

            if (drv == null) 
            {
                Console.WriteLine("Can't get driver.");
                System.Environment.Exit(-1);
            }
            
            Console.WriteLine("Using driver " + drv.LongName);

            /* -------------------------------------------------------------------- */
            /*      Get metadata                                                    */
            /* -------------------------------------------------------------------- */
            string[] metadata = ds.GetMetadata("");
            if (metadata.Length > 0)
            {
                Console.WriteLine("  Metadata:");
                for (int iMeta = 0; iMeta < metadata.Length; iMeta++)
                {
                    Console.WriteLine("    " + iMeta + ":  " + metadata[iMeta]);
                }
                Console.WriteLine("");
            }

            /* -------------------------------------------------------------------- */
            /*      Report "IMAGE_STRUCTURE" metadata.                              */
            /* -------------------------------------------------------------------- */
            metadata = ds.GetMetadata("IMAGE_STRUCTURE");
            if (metadata.Length > 0)
            {
                Console.WriteLine("  Image Structure Metadata:");
                for (int iMeta = 0; iMeta < metadata.Length; iMeta++)
                {
                    Console.WriteLine("    " + iMeta + ":  " + metadata[iMeta]);
                }
                Console.WriteLine("");
            }

            /* -------------------------------------------------------------------- */
            /*      Report subdatasets.                                             */
            /* -------------------------------------------------------------------- */
            metadata = ds.GetMetadata("SUBDATASETS");
            if (metadata.Length > 0)
            {
                Console.WriteLine("  Subdatasets:");
                for (int iMeta = 0; iMeta < metadata.Length; iMeta++)
                {
                    Console.WriteLine("    " + iMeta + ":  " + metadata[iMeta]);
                }
                Console.WriteLine("");
            }

            /* -------------------------------------------------------------------- */
            /*      Report geolocation.                                             */
            /* -------------------------------------------------------------------- */
            metadata = ds.GetMetadata("GEOLOCATION");
            if (metadata.Length > 0)
            {
                Console.WriteLine("  Geolocation:");
                for (int iMeta = 0; iMeta < metadata.Length; iMeta++)
                {
                    Console.WriteLine("    " + iMeta + ":  " + metadata[iMeta]);
                }
                Console.WriteLine("");
            }

            /* -------------------------------------------------------------------- */
            /*      Report corners.                                                 */
            /* -------------------------------------------------------------------- */
            Console.WriteLine( "Corner Coordinates:" );
            Console.WriteLine("  Upper Left (" + GDALInfoGetPosition( ds, 0.0, 0.0) + ")");
            Console.WriteLine("  Lower Left (" + GDALInfoGetPosition( ds, 0.0, ds.RasterYSize) + ")");
            Console.WriteLine("  Upper Right (" + GDALInfoGetPosition( ds, ds.RasterXSize, 0.0) + ")");
            Console.WriteLine("  Lower Right (" + GDALInfoGetPosition( ds, ds.RasterXSize, ds.RasterYSize) + ")");
            Console.WriteLine("  Center (" + GDALInfoGetPosition( ds, ds.RasterXSize / 2, ds.RasterYSize / 2) + ")");
            Console.WriteLine("");

            /* -------------------------------------------------------------------- */
            /*      Get raster band                                                 */
            /* -------------------------------------------------------------------- */
            for (int iBand = 1; iBand <= ds.RasterCount; iBand++) 
            {
                Band band = ds.GetRasterBand(iBand);
                Console.WriteLine("Band " + iBand + " :");
                Console.WriteLine("   DataType: " + Gdal.GetDataTypeName(band.DataType));
                Console.WriteLine("   ColorInterpretation: " + Gdal.GetColorInterpretationName(band.GetRasterColorInterpretation()));
                ColorTable ct = band.GetRasterColorTable();
				if (ct != null)
					Console.WriteLine("   Band has a color table with " + ct.GetCount() + " entries.");
                
				Console.WriteLine("   Description: " + band.GetDescription());
                Console.WriteLine("   Size (" + band.XSize + "," + band.YSize + ")");
                int BlockXSize, BlockYSize;
                band.GetBlockSize(out BlockXSize, out BlockYSize);
                Console.WriteLine("   BlockSize (" + BlockXSize + "," + BlockYSize + ")");
                double val;
                int hasval;
                band.GetMinimum(out val, out hasval);
                if (hasval != 0) Console.WriteLine("   Minimum: " + val.ToString());
                band.GetMaximum(out val, out hasval);
                if (hasval != 0) Console.WriteLine("   Maximum: " + val.ToString());
                band.GetNoDataValue(out val, out hasval);
                if (hasval != 0) Console.WriteLine("   NoDataValue: " + val.ToString());
                band.GetOffset(out val, out hasval);
                if (hasval != 0) Console.WriteLine("   Offset: " + val.ToString());
                band.GetScale(out val, out hasval);
                if (hasval != 0) Console.WriteLine("   Scale: " + val.ToString());
            }
        }
        catch (Exception e) 
        {
            Console.WriteLine("Application error: " + e.Message);
        }
    }

    private static string GDALInfoGetPosition(Dataset ds, double x, double y)
    {
        double[] adfGeoTransform = new double[6];
        double	dfGeoX, dfGeoY;
        ds.GetGeoTransform(adfGeoTransform);

        dfGeoX = adfGeoTransform[0] + adfGeoTransform[1] * x + adfGeoTransform[2] * y;
        dfGeoY = adfGeoTransform[3] + adfGeoTransform[4] * x + adfGeoTransform[5] * y;

        return dfGeoX.ToString() + ", " + dfGeoY.ToString();
    }
}