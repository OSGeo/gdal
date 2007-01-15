using System;

using GDAL;


/**
 * <p>Title: GDAL C# GDALWrite example.</p>
 * <p>Description: A sample app to write a GDAL raster.</p>
 * @author Tamas Szekeres (szekerest@gmail.com)
 * @version 1.0
 */



/// <summary>
/// A C# based sample to write a GDAL raster.
/// </summary> 

class GDALWrite {
	
	public static void usage() 

	{ 
		Console.WriteLine("usage: gdalwrite {dataset name}");
		System.Environment.Exit(-1);
	}
 
    public static void Main(string[] args) 
    {

        if (args.Length != 1) usage();

        // Using early initialization of System.Console
        Console.WriteLine("");

        int bXSize, bYSize;
        int w, h;

        w = 100;
        h = 100;

        bXSize = w;
        bYSize = 1;

        try 
        {
            /* -------------------------------------------------------------------- */
            /*      Register driver(s).                                             */
            /* -------------------------------------------------------------------- */
            gdal.AllRegister();
            
            /* -------------------------------------------------------------------- */
            /*      Get driver                                                      */
            /* -------------------------------------------------------------------- */	
            Driver drv = gdal.GetDriverByName("GTiff");

            if (drv == null) 
            {
                Console.WriteLine("Can't get driver.");
                System.Environment.Exit(-1);
            }
            
            Console.WriteLine("Using driver " + drv.LongName);

            /* -------------------------------------------------------------------- */
            /*      Open dataset.                                                   */
            /* -------------------------------------------------------------------- */
            string[] options = new string [] {"BLOCKXSIZE=" + bXSize, "BLOCKYSIZE=" + bYSize};
            Dataset ds = drv.Create(args[0], w, h, 1, gdalconst.GDT_Byte, options);
		
            if (ds == null) 
            {
                Console.WriteLine("Can't open " + args[0]);
                System.Environment.Exit(-1);
            }

            Band ba = ds.GetRasterBand(1);

            byte [] buffer = new byte [w * h];

            for (int i = 0; i < buffer.Length; i++)
                buffer[i] = (byte)(i*256/buffer.Length);

            ba.WriteRaster(0, 0, w, h, buffer, w, h, 0, 0);

            ba.FlushCache();
            ds.FlushCache();

        }
        catch (Exception e) 
        {
            Console.WriteLine("Application error: " + e.Message);
        }
    }
}