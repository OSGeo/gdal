/******************************************************************************
 * $Id: GDALOverviews.cs 13678 2008-02-02 23:29:37Z tamas $
 *
 * Name:     GDALOverviews.cs
 * Project:  GDAL CSharp Interface
 * Purpose:  A sample app to get the band histograms.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Tamas Szekeres
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

using System;

using OSGeo.GDAL;


/**

 * <p>Title: GDAL C# GDALGetHistogram example.</p>
 * <p>Description: A sample app to get the band histograms.</p>
 * @author Tamas Szekeres (szekerest@gmail.com)
 * @version 1.0
 */



/// <summary>
/// A C# based sample to get the band histograms.
/// </summary> 

class GDALGetHistogram {
	
	public static void usage() 

	{ 
		Console.WriteLine("usage: gdalgethistogram {GDAL dataset name}");
		System.Environment.Exit(-1);
	}
 
    public static void Main(string[] args) 
    {
        if (args.Length < 1) usage();
        
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
            Dataset ds = Gdal.Open( args[0], Access.GA_ReadOnly );
		
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
            /*      Get the Histogram arrays                                        */
            /* -------------------------------------------------------------------- */
            for (int iBand = 1; iBand <= ds.RasterCount; iBand++) 
            {
                Console.WriteLine("Band " + iBand + ":");
                int[] histData = new int[256]; 
                Band band = ds.GetRasterBand(iBand);
                double pdfMin, pdfMax, pdfMean, pdfStdDev;
                band.GetStatistics(0, 1, out pdfMin, out pdfMax, out pdfMean, out pdfStdDev);
                Console.WriteLine("Min=" + pdfMin);
                Console.WriteLine("Max=" + pdfMax);
                Console.WriteLine("Mean=" + pdfMean);
                Console.WriteLine("StdDev=" + pdfStdDev);
                band.GetHistogram(-0.5, 255.5, 255, histData, 0, 1,
                           new Gdal.GDALProgressFuncDelegate(ProgressFunc), "");
                for (int i = 0; i < 255; i++)
                    Console.Write(String.Format(" {0:x}", histData[i]));
                Console.WriteLine("");
            }
            Console.WriteLine("Completed.");
        }
        catch (Exception e) 
        {
            Console.WriteLine("Application error: " + e.Message);
        }
    }

	public static int ProgressFunc(double Complete, IntPtr Message, IntPtr Data)
	{
		Console.Write("Processing ... " + Complete * 100 + "% Completed.");
		if (Message != IntPtr.Zero)
			Console.Write(" Message:" + System.Runtime.InteropServices.Marshal.PtrToStringAnsi(Message));
		if (Data != IntPtr.Zero)
			Console.Write(" Data:" + System.Runtime.InteropServices.Marshal.PtrToStringAnsi(Data));
	
		Console.WriteLine("");
		return 1;
	}
}