/******************************************************************************
 * $Id$
 *
 * Name:     GDALAdjustContrast.cs
 * Project:  GDAL CSharp Interface
 * Purpose:  A sample app to demonstrate how to read the dataset into the
 *           memory, adjust the contrast of the image and write back to the 
 *           dataset persistently.
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
using System.Drawing.Drawing2D;
using OSGeo.GDAL;

/**
 * <p>Title: GDAL C# GDALAdjustContrast example.</p>
 * <p>Description: A sample app to demonstrate how to read the dataset into the
 * memory, adjust the contrast of the image and write back to the dataset persistently.</p>
 * @author Tamas Szekeres (szekerest@gmail.com)
 * @version 1.0
 */



/// <summary>
/// A sample app to demonstrate how to read the dataset into the
/// memory, adjust the contrast of the image and write back to the dataset persistently.
/// </summary> 

class GDALAdjustContrast {
	
	public static void usage() 

	{
        Console.WriteLine("usage: GDALAdjustContrast {dataset name} {contrast ratio}");
		System.Environment.Exit(-1);
	}


    // bitmap parameters
    static int[] bandMap;
    static ColorTable ct;
    static int channelCount;
    static bool hasAlpha;
    static bool isPremultiplied;
    static bool isIndexed;
    static int channelSize;
    static PixelFormat pixelFormat;
    static DataType dataType;
    static int pixelSpace;

    public static void Main(string[] args) 
    {

        if (args.Length != 2) usage();

        // Using early initialization of System.Console
        Console.WriteLine("Adjusting the image: " + args[0]);

        try 
        {
            float contrastRatio = float.Parse(args[1]);
            /* -------------------------------------------------------------------- */
            /*      Register driver(s).                                             */
            /* -------------------------------------------------------------------- */
            Gdal.AllRegister();

            /* -------------------------------------------------------------------- */
            /*      Open dataset.                                                   */
            /* -------------------------------------------------------------------- */
            Dataset ds = Gdal.Open(args[0], Access.GA_Update);
		
            if (ds == null) 
            {
                Console.WriteLine("Can't open " + args[0]);
                System.Environment.Exit(-1);
            }

            Bitmap bmp = CreateCompatibleBitmap(ds, ds.RasterXSize, ds.RasterYSize);
            LoadBitmapDirect(ds, bmp, 0, 0, ds.RasterXSize, ds.RasterYSize, ds.RasterXSize, ds.RasterYSize, 0);
            Bitmap newBitmap = (Bitmap)bmp.Clone();

            //create the ColorMatrix
            float[][] colormatrix = new float[][]
              {
                 new float[] {contrastRatio, 0, 0, 0, 0},
                 new float[] {0, contrastRatio, 0, 0, 0},
                 new float[] {0, 0, contrastRatio, 0, 0},
                 new float[] {0, 0, 0, 1, 0},
                 new float[] {0, 0, 0, 0, 1}
              };
            ColorMatrix colorMatrix = new ColorMatrix(colormatrix);

            //create the image attributes
            ImageAttributes attributes = new ImageAttributes();

            //set the color matrix attribute
            attributes.SetColorMatrix(colorMatrix);

            //get a graphics object from the new image
            Graphics g = Graphics.FromImage(newBitmap);
            //draw the original image on the new image
            g.DrawImage(bmp,
               new Rectangle(0, 0, bmp.Width, bmp.Height),
               0, 0, bmp.Width, bmp.Height,
               GraphicsUnit.Pixel, attributes);

            SaveBitmapDirect(ds, newBitmap, 0, 0, ds.RasterXSize, ds.RasterYSize, ds.RasterXSize, ds.RasterYSize);

            ds.FlushCache();

        }
        catch (Exception e) 
        {
            Console.WriteLine("Application error: " + e.Message);
        }
    }

    private static Bitmap CreateCompatibleBitmap(Dataset ds, int imageWidth, int imageHeight)
    {
        if (ds.RasterCount == 0)
            return null;

        bandMap = new int[4] { 1, 1, 1, 1 };
        channelCount = 1;
        hasAlpha = false;
        isPremultiplied = false;
        isIndexed = false;
        channelSize = 8;
        // Evaluate the bands and find out a proper image transfer format
        for (int i = 0; i < ds.RasterCount; i++)
        {
            Band band = ds.GetRasterBand(i + 1);
            if (Gdal.GetDataTypeSize(band.DataType) > 8)
                channelSize = 16;

            // retrieving the premultiplied alpha flag
            string[] metadata = band.GetMetadata("");
            for (int iMeta = 0; iMeta < metadata.Length; iMeta++)
            {
                if (metadata[iMeta].StartsWith("PREMULTIPLIED_ALPHA"))
                    isPremultiplied = true;
            }

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
                        if (isPremultiplied)
                            pixelFormat = PixelFormat.Format64bppArgb;
                        else
                            pixelFormat = PixelFormat.Format64bppPArgb;
                        dataType = DataType.GDT_UInt16;
                        pixelSpace = 8;
                    }
                    else
                    {
                        if (isPremultiplied)
                            pixelFormat = PixelFormat.Format32bppPArgb;
                        else
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
        return new Bitmap(imageWidth, imageHeight, pixelFormat);
    }

    private static void LoadBitmapDirect(Dataset ds, Bitmap bitmap, int xOff, int yOff, int width, int height, int imageWidth, int imageHeight, int iOverview)
    {
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

            ds.ReadRaster(xOff, yOff, width, height, buf, imageWidth, imageHeight, dataType,
                channelCount, bandMap, pixelSpace, stride, 1);
        }
        finally
        {
            bitmap.UnlockBits(bitmapData);
        }
    }

    private static void SaveBitmapDirect(Dataset ds, Bitmap bitmap, int xOff, int yOff, int width, int height, int imageWidth, int imageHeight)
    {
        if (bitmap != null)
        {
            if (isIndexed)
            {
                // TODO: setting up the color table
            }

            // Use GDAL writing method to write the image data directly from the Bitmap
            BitmapData bitmapData = bitmap.LockBits(new Rectangle(0, 0, imageWidth, imageHeight), ImageLockMode.ReadWrite, pixelFormat);

            try
            {
                int stride = bitmapData.Stride;
                IntPtr buf = bitmapData.Scan0;

                ds.WriteRaster(xOff, yOff, width, height, buf, imageWidth, imageHeight, dataType,
                    channelCount, bandMap, pixelSpace, stride, 1);
            }
            finally
            {
                bitmap.UnlockBits(bitmapData);
            }
        }
    }
}