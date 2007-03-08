using System;
using System.Drawing;
using System.Drawing.Imaging;

using OSGeo.GDAL;


/**

 * <p>Title: GDAL C# GDALRead example.</p>
 * <p>Description: A sample app to read GDAL raster data.</p>
 * @author Tamas Szekeres (szekerest@gmail.com)
 * @version 1.0
 */



/// <summary>
/// A C# based sample to read GDAL raster data.
/// </summary> 

class GDALRead {
	
	public static void usage() 

	{ 
		Console.WriteLine("usage: gdalread {GDAL dataset name} {output file name}");
		System.Environment.Exit(-1);
	}
 
    public static void Main(string[] args) 
    {

        if (args.Length != 2) usage();

        // Using early initialization of System.Console
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
            /*      Get raster band                                                 */
            /* -------------------------------------------------------------------- */
            for (int iBand = 1; iBand <= ds.RasterCount; iBand++) 
            {
                Band band = ds.GetRasterBand(iBand);
                Console.WriteLine("Band " + iBand + " :");
                Console.WriteLine("   DataType: " + band.DataType);
                Console.WriteLine("   Size (" + band.XSize + "," + band.YSize + ")");
            }

            /* -------------------------------------------------------------------- */
            /*      Processing the raster                                           */
            /* -------------------------------------------------------------------- */
            if (ds.RasterCount < 3) 
            {
                Console.WriteLine("The number of the raster bands is not enough to run this sample");
                System.Environment.Exit(-1);
            }

            SaveBitmapBuffered(ds, args[1]);
            
        }
        catch (Exception e) 
        {
            Console.WriteLine("Application error: " + e.Message);
        }
    }

    private static void SaveBitmapBuffered(Dataset ds, string filename) 
    {
        // Get the GDAL Band objects from the Dataset
        Band redBand = ds.GetRasterBand(1);
        Band greenBand = ds.GetRasterBand(2);
        Band blueBand = ds.GetRasterBand(3);

        // Get the width and height of the Dataset
        int width = ds.RasterXSize;
        int height = ds.RasterYSize;

        // Create a Bitmap to store the GDAL image in
        Bitmap bitmap = new Bitmap(width, height, PixelFormat.Format32bppRgb);

        DateTime start = DateTime.Now;
        
        byte[] r = new byte[width * height];
        byte[] g = new byte[width * height];
        byte[] b = new byte[width * height];

        redBand.ReadRaster(0, 0, width, height, r, width, height, 0, 0);
        greenBand.ReadRaster(0, 0, width, height, g, width, height, 0, 0);
        blueBand.ReadRaster(0, 0, width, height, b, width, height, 0, 0);
        TimeSpan renderTime = DateTime.Now - start;
        Console.WriteLine("SaveBitmapBuffered fetch time: " + renderTime.TotalMilliseconds + " ms");

        int i, j;
        for (i = 0; i< width; i++) 
        {
            for (j=0; j<height; j++)
            {
                Color newColor = Color.FromArgb(Convert.ToInt32(r[i+j*width]),Convert.ToInt32(g[i+j*width]), Convert.ToInt32(b[i+j*width]));
                bitmap.SetPixel(i, j, newColor);
            }
        }

        bitmap.Save(filename);
    }
}