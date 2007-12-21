/******************************************************************************
 * $Id$
 *
 * Name:     GDALReadDirect.cs
 * Project:  GDAL CSharp Interface
 * Purpose:  A sample app to read GDAL raster data directly to a C# bitmap.
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
		Console.WriteLine("usage: gdalread {GDAL dataset name} {output file name} {overview}");
		System.Environment.Exit(-1);
	}
 
    public static void Main(string[] args) 
    {

        int iOverview = -1;
        if (args.Length < 2) usage();
        if (args.Length == 3) iOverview = int.Parse(args[2]);

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

            /* -------------------------------------------------------------------- */
            /*      Processing the raster                                           */
            /* -------------------------------------------------------------------- */
            SaveBitmapDirect(ds, args[1], iOverview);
            
        }
        catch (Exception e)
        {
            Console.WriteLine("Application error: " + e.Message);
        }
    }

    private static void SaveBitmapDirect(Dataset ds, string filename, int iOverview) 
    {
        // Get the GDAL Band objects from the Dataset
        Band redBand = ds.GetRasterBand(1);

        if (redBand.GetRasterColorInterpretation() == ColorInterp.GCI_PaletteIndex)
        {
            SaveBitmapPaletteDirect(ds, filename, iOverview);
            return;
        }

        if (redBand.GetRasterColorInterpretation() == ColorInterp.GCI_GrayIndex)
        {
            SaveBitmapGrayDirect(ds, filename, iOverview);
            return;
        }

        if (ds.RasterCount < 3) 
        {
            Console.WriteLine("The number of the raster bands is not enough to run this sample");
            System.Environment.Exit(-1);
        }

        if (iOverview >= 0 && redBand.GetOverviewCount() > iOverview) 
            redBand = redBand.GetOverview(iOverview);

        Band greenBand = ds.GetRasterBand(2);

        if (iOverview >= 0 && greenBand.GetOverviewCount() > iOverview) 
            greenBand = greenBand.GetOverview(iOverview);

        Band blueBand = ds.GetRasterBand(3);

        if (iOverview >= 0 && blueBand.GetOverviewCount() > iOverview) 
            blueBand = blueBand.GetOverview(iOverview);

        // Get the width and height of the Dataset
        int width = redBand.XSize;
        int height = redBand.YSize;

        // Create a Bitmap to store the GDAL image in
        Bitmap bitmap = new Bitmap(width, height, PixelFormat.Format32bppRgb);

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

    private static void SaveBitmapPaletteDirect(Dataset ds, string filename, int iOverview) 
    {
        // Get the GDAL Band objects from the Dataset
        Band band = ds.GetRasterBand(1);
        if (iOverview >= 0 && band.GetOverviewCount() > iOverview) 
            band = band.GetOverview(iOverview);

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
        int width = band.XSize;
        int height = band.YSize;

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

    private static void SaveBitmapGrayDirect(Dataset ds, string filename, int iOverview) 
    {
        // Get the GDAL Band objects from the Dataset
        Band band = ds.GetRasterBand(1);
        if (iOverview >= 0 && band.GetOverviewCount() > iOverview) 
            band = band.GetOverview(iOverview);

        // Get the width and height of the Dataset
        int width = band.XSize;
        int height = band.YSize;

        // Create a Bitmap to store the GDAL image in
        Bitmap bitmap = new Bitmap(width, height, PixelFormat.Format8bppIndexed);

        DateTime start = DateTime.Now;
        
        byte[] r = new byte[width * height];
		
        band.ReadRaster(0, 0, width, height, r, width, height, 0, 0);
        // Use GDAL raster reading methods to read the image data directly into the Bitmap
        BitmapData bitmapData = bitmap.LockBits(new Rectangle(0, 0, width, height), ImageLockMode.ReadWrite, PixelFormat.Format8bppIndexed);

        try 
        {
            ColorPalette pal = bitmap.Palette; 
            for(int i = 0; i < 256; i++) 
                pal.Entries[i] = Color.FromArgb( 255, i, i, i ); 
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