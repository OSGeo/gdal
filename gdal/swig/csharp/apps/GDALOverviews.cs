using System;
using System.Drawing;
using System.Drawing.Imaging;

using OSGeo.GDAL;


/**

 * <p>Title: GDAL C# GDALOverviews example.</p>
 * <p>Description: A sample app to create GDAL raster overviews.</p>
 * @author Tamas Szekeres (szekerest@gmail.com)
 * @version 1.0
 */



/// <summary>
/// A C# based sample to create GDAL raster overviews.
/// </summary> 

class GDALOverviews {
	
	public static void usage() 

	{ 
		Console.WriteLine("usage: gdaloverviews {GDAL dataset name} {resamplealg} {level1} {level2} ....");
		System.Environment.Exit(-1);
	}
 
    public static void Main(string[] args) 
    {
        if (args.Length <= 2) usage();
        
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
            Dataset ds = Gdal.Open( args[0], Access.GA_Update );
		
            if (ds == null) 
            {
                Console.WriteLine("Can't open " + args[0]);
                System.Environment.Exit(-1);
            }

            Console.WriteLine("Raster dataset parameters:");
            Console.WriteLine("  Projection: " + ds.GetProjectionRef());
            Console.WriteLine("  RasterCount: " + ds.RasterCount);
            Console.WriteLine("  RasterSize (" + ds.RasterXSize + "," + ds.RasterYSize + ")");
            
            int[] levels = new int[args.Length -2];

            Console.WriteLine(levels.Length);
           
            for (int i = 2; i < args.Length; i++)
            {
                levels[i-2] = int.Parse(args[i]);
            }
            if (ds.BuildOverviews(args[1], levels) != (int)CPLErr.CE_None)
            {
                Console.WriteLine("The BuildOverviews operation doesn't work");
                System.Environment.Exit(-1);
            }
 
            /* -------------------------------------------------------------------- */
            /*      Displaying the raster parameters                                */
            /* -------------------------------------------------------------------- */
            for (int iBand = 1; iBand <= ds.RasterCount; iBand++) 
            {
                Band band = ds.GetRasterBand(iBand);
                Console.WriteLine("Band " + iBand + " :");
                Console.WriteLine("   DataType: " + band.DataType);
                Console.WriteLine("   Size (" + band.XSize + "," + band.YSize + ")");
                Console.WriteLine("   PaletteInterp: " + band.GetRasterColorInterpretation().ToString());

                for (int iOver = 0; iOver < band.GetOverviewCount(); iOver++)
                {
                    Band over = band.GetOverview(iOver);
                    Console.WriteLine("      OverView " + iOver + " :");
                    Console.WriteLine("         DataType: " + over.DataType);
                    Console.WriteLine("         Size (" + over.XSize + "," + over.YSize + ")");
                    Console.WriteLine("         PaletteInterp: " + over.GetRasterColorInterpretation().ToString());
                }
            }
            Console.WriteLine("Completed.");
            Console.WriteLine("Use:  gdalread " + args[0] + " outfile.png [overview] to extract a particular overview!" );
        }
        catch (Exception e) 
        {
            Console.WriteLine("Application error: " + e.Message);
        }
    }
}