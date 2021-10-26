/******************************************************************************
 * $Id: GDALReadDirect.cs 13437 2007-12-21 21:02:38Z tamas $
 *
 * Name:     GDALDatasetRasterIO.cs
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
using System.Runtime.InteropServices;

using OSGeo.GDAL;


/**

 * <p>Title: GDAL C# GDALDatasetRasterIO example.</p>
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
        Console.WriteLine("usage: GDALDatasetRasterIOEx {GDAL dataset name} {output file name} {scale = 0.5} {version = 1} {resample alg = GRIORA_NearestNeighbour}");
		System.Environment.Exit(-1);
	}

    public static int ProgressFunc(double Complete, IntPtr Message, IntPtr Data)
    {
        Console.Write("Processing ... " + Complete * 100 + "% Completed.");
        if (Message != IntPtr.Zero)
            Console.Write(" Message:" + System.Runtime.InteropServices.Marshal.PtrToStringAnsi(Message));
        if (Data != IntPtr.Zero)
            Console.Write(" Data:" + System.Runtime.InteropServices.Marshal.PtrToStringUni(Data));

        Console.WriteLine("");
        return 1;
    }

    private static RIOResampleAlg resampleAlg = RIOResampleAlg.GRIORA_NearestNeighbour;
    private static int argVersion = 1;
    private static double scale = 0.5;
 
    public static void Main(string[] args) 
    {
        if (args.Length < 2) usage();

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

            if (args.Length > 2)
                scale = double.Parse(args[2]);

            if (args.Length > 3)
                argVersion = int.Parse(args[3]);

            if (args.Length > 4)
                resampleAlg = (RIOResampleAlg)Enum.Parse(typeof(RIOResampleAlg), args[4]);

            /* -------------------------------------------------------------------- */
            /*      Processing the raster                                           */
            /* -------------------------------------------------------------------- */
            SaveBitmapDirect(args[1], ds, 0, 0, ds.RasterXSize, ds.RasterYSize, Convert.ToInt32(scale * ds.RasterXSize), Convert.ToInt32(scale * ds.RasterYSize));
            
        }
        catch (Exception e)
        {
            Console.WriteLine("Application error: " + e.Message);
        }
    }

    private static void SaveBitmapDirect(string filename, Dataset ds, int xOff, int yOff, int width, int height, int imageWidth, int imageHeight)
    {
        if (ds.RasterCount == 0)
            return;

        int[] bandMap = new int[4] { 1, 1, 1, 1 };
        int channelCount = 1;
        bool hasAlpha = false;
        bool isIndexed = false;
        int channelSize = 8;
        ColorTable ct = null;
        // Evaluate the bands and find out a proper image transfer format
        for (int i = 0; i < ds.RasterCount; i++)
        {
            Band band = ds.GetRasterBand(i + 1);
            if (Gdal.GetDataTypeSize(band.DataType) > 8)
                channelSize = 16;
            switch (band.GetRasterColorInterpretation())
            {
                case ColorInterp.GCI_AlphaBand:
                    channelCount = 4;
                    hasAlpha = true;
                    bandMap[3] = i + 1;
                    break;
                case ColorInterp.GCI_BlueBand:
                    if (channelCount < 3)
                        channelCount = 3;
                    bandMap[0] = i + 1;
                    break;
                case ColorInterp.GCI_RedBand:
                    if (channelCount < 3)
                        channelCount = 3;
                    bandMap[2] = i + 1;
                    break;
                case ColorInterp.GCI_GreenBand:
                    if (channelCount < 3)
                        channelCount = 3;
                    bandMap[1] = i + 1;
                    break;
                case ColorInterp.GCI_PaletteIndex:
                    ct = band.GetRasterColorTable();
                    isIndexed = true;
                    bandMap[0] = i + 1;
                    break;
                case ColorInterp.GCI_GrayIndex:
                    isIndexed = true;
                    bandMap[0] = i + 1;
                    break;
                default:
                    // we create the bandmap using the dataset ordering by default
                    if (i < 4 && bandMap[i] == 0)
                    {
                        if (channelCount < i)
                            channelCount = i;
                        bandMap[i] = i + 1;
                    }
                    break;
            }
        }

        // find out the pixel format based on the gathered information
        PixelFormat pixelFormat;
        DataType dataType;
        int pixelSpace;

        if (isIndexed)
        {
            pixelFormat = PixelFormat.Format8bppIndexed;
            dataType = DataType.GDT_Byte;
            pixelSpace = 1;
        }
        else
        {
            if (channelCount == 1)
            {
                if (channelSize > 8)
                {
                    pixelFormat = PixelFormat.Format16bppGrayScale;
                    dataType = DataType.GDT_Int16;
                    pixelSpace = 2;
                }
                else
                {
                    pixelFormat = PixelFormat.Format24bppRgb;
                    channelCount = 3;
                    dataType = DataType.GDT_Byte;
                    pixelSpace = 3;
                }
            }
            else
            {
                if (hasAlpha)
                {
                    if (channelSize > 8)
                    {
                        pixelFormat = PixelFormat.Format64bppArgb;
                        dataType = DataType.GDT_UInt16;
                        pixelSpace = 8;
                    }
                    else
                    {
                        pixelFormat = PixelFormat.Format32bppArgb;
                        dataType = DataType.GDT_Byte;
                        pixelSpace = 4;
                    }
                    channelCount = 4;
                }
                else
                {
                    if (channelSize > 8)
                    {
                        pixelFormat = PixelFormat.Format48bppRgb;
                        dataType = DataType.GDT_UInt16;
                        pixelSpace = 6;
                    }
                    else
                    {
                        pixelFormat = PixelFormat.Format24bppRgb;
                        dataType = DataType.GDT_Byte;
                        pixelSpace = 3;
                    }
                    channelCount = 3;
                }
            }
        }


        // Create a Bitmap to store the GDAL image in
        Bitmap bitmap = new Bitmap(imageWidth, imageHeight, pixelFormat);

        if (isIndexed)
        {
            // setting up the color table
            if (ct != null)
            {
                int iCol = ct.GetCount();
                ColorPalette pal = bitmap.Palette;
                for (int i = 0; i < iCol; i++)
                {
                    ColorEntry ce = ct.GetColorEntry(i);
                    pal.Entries[i] = Color.FromArgb(ce.c4, ce.c1, ce.c2, ce.c3);
                }
                bitmap.Palette = pal;
            }
            else
            {
                // grayscale
                ColorPalette pal = bitmap.Palette;
                for (int i = 0; i < 256; i++)
                    pal.Entries[i] = Color.FromArgb(255, i, i, i);
                bitmap.Palette = pal;
            }
        }

        // Use GDAL raster reading methods to read the image data directly into the Bitmap
        BitmapData bitmapData = bitmap.LockBits(new Rectangle(0, 0, imageWidth, imageHeight), ImageLockMode.ReadWrite, pixelFormat);

        try
        {
            int stride = bitmapData.Stride;
            IntPtr buf = bitmapData.Scan0;

            using (RasterIOExtraArg arg = new RasterIOExtraArg())
            {
                GCHandle handle = GCHandle.Alloc("Test data", GCHandleType.Pinned);
                try {               
                    arg.nVersion = argVersion;
                    arg.eResampleAlg = resampleAlg;
                    arg.pfnProgress = new Gdal.GDALProgressFuncDelegate(ProgressFunc);
                    arg.pProgressData = handle.AddrOfPinnedObject();  // or IntPtr.Zero if not data to be added;
                    arg.bFloatingPointWindowValidity = 0;
                    ds.ReadRaster(xOff, yOff, width, height, buf, imageWidth, imageHeight, dataType,
                        channelCount, bandMap, pixelSpace, stride, 1, arg);
                }
                finally
                {
                    handle.Free();
                }
            }
        }
        finally
        {
            bitmap.UnlockBits(bitmapData);
        }

        bitmap.Save(filename);
    }
}