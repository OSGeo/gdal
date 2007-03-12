using System;
using System.Drawing;
using System.Drawing.Imaging;

using OSGeo.GDAL;


/**

 * <p>Title: GDAL C# GDALRead example.</p>
 * <p>Description: A sample app to read GDAL raster data directly to a C# bitmap.</p>
 * @author Tamas Szekeres (szekerest@gmail.com)
 * @version 1.0
 */



/// <summary>
/// A C# based sample to read GDAL raster data directly to a C# bitmap.
/// </summary> 

class GDALReadDirect {
	
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

        //try 
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
            SaveBitmapDirect(ds, args[1]);
            
        }
        /*catch (Exception e)
        {
            Console.WriteLine("Application error: " + e.Message);
        }/**/
    }

    private static void SaveBitmapDirect(Dataset ds, string filename) 
    {
        // Get the GDAL Band objects from the Dataset
        Band redBand = ds.GetRasterBand(1);

        if (redBand.GetRasterColorInterpretation() == ColorInterp.GCI_PaletteIndex)
        {
            SaveBitmapPaletteDirect(ds, filename);
            return;
        }

        if (ds.RasterCount < 3) 
        {
            Console.WriteLine("The number of the raster bands is not enough to run this sample");
            System.Environment.Exit(-1);
        }

        Band greenBand = ds.GetRasterBand(2);
        Band blueBand = ds.GetRasterBand(3);

        // Get the width and height of the Dataset
        int width = ds.RasterXSize;
        int height = ds.RasterYSize;

        // Create a Bitmap to store the GDAL image in
        Bitmap bitmap = new Bitmap(ds.RasterXSize, ds.RasterYSize, PixelFormat.Format32bppRgb);

        DateTime start = DateTime.Now;
        
        // Use GDAL raster reading methods to read the image data directly into the Bitmap
        BitmapData bitmapData = bitmap.LockBits(new Rectangle(0, 0, width, height), ImageLockMode.ReadWrite, PixelFormat.Format32bppRgb);

        try 
        {
            int stride = bitmapData.Stride;
            IntPtr buf = bitmapData.Scan0;

            blueBand.ReadRaster(0, 0, width, height, buf, width, height, DataType.GDT_Byte, 4, stride);
            greenBand.ReadRaster(0, 0, width, height, new IntPtr(buf.ToInt32()+1), width, height, DataType.GDT_Byte, 4, stride);
            redBand.ReadRaster(0, 0, width, height, new IntPtr(buf.ToInt32()+2), width, height, DataType.GDT_Byte, 4, stride);
            TimeSpan renderTime = DateTime.Now - start;
            Console.WriteLine("SaveBitmapDirect fetch time: " + renderTime.TotalMilliseconds + " ms");
        }
        finally 
        {
            bitmap.UnlockBits(bitmapData);
        }

        bitmap.Save(filename);
    }

    private static void SaveBitmapPaletteDirect(Dataset ds, string filename) 
    {
        // Get the GDAL Band objects from the Dataset
        Band band = ds.GetRasterBand(1);

        ColorTable ct = band.GetRasterColorTable();
        if (ct == null)
        {
            Console.WriteLine("   Band has no color table!");
            return;
        }

        if (ct.GetPaletteInterpretation() != PaletteInterp.GPI_RGB)
        {
            Console.WriteLine("   Only RGB palette interp is supported by this sample!");
            return;
        }

        // Get the width and height of the Dataset
        int width = ds.RasterXSize;
        int height = ds.RasterYSize;

        // Create a Bitmap to store the GDAL image in
        Bitmap bitmap = new Bitmap(width, height, PixelFormat.Format8bppIndexed);

        DateTime start = DateTime.Now;
        
        byte[] r = new byte[width * height];
		
        band.ReadRaster(0, 0, width, height, r, width, height, 0, 0);
        // Use GDAL raster reading methods to read the image data directly into the Bitmap
        BitmapData bitmapData = bitmap.LockBits(new Rectangle(0, 0, width, height), ImageLockMode.ReadWrite, PixelFormat.Format8bppIndexed);

        try 
        {
            int iCol = ct.GetCount();
            ColorPalette pal = bitmap.Palette;
            for (int i = 0; i < iCol; i++)
            {
                ColorEntry ce = ct.GetColorEntry(i);
                pal.Entries[i] = Color.FromArgb(ce.c4, ce.c1, ce.c2, ce.c3);
            }
            bitmap.Palette = pal;
            
            int stride = bitmapData.Stride;
            IntPtr buf = bitmapData.Scan0;

            band.ReadRaster(0, 0, width, height, buf, width, height, DataType.GDT_Byte, 1, stride);
            TimeSpan renderTime = DateTime.Now - start;
            Console.WriteLine("SaveBitmapDirect fetch time: " + renderTime.TotalMilliseconds + " ms");
        }
        finally 
        {
            bitmap.UnlockBits(bitmapData);
        }

        bitmap.Save(filename);
    }
}