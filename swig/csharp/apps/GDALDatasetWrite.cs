/******************************************************************************
 * $Id: GDALWrite.cs 14912 2008-07-14 21:36:55Z tamas $
 *
 * Name:     GDALWrite.cs
 * Project:  GDAL CSharp Interface
 * Purpose:   sample app to write a GDAL raster.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Tamas Szekeres
 * Copyright (c) 2026, Paul Harwood
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

using System;
using System.Linq;
using System.Globalization;

using OSGeo.GDAL;


/**
 * <p>Title: GDAL C# GDALWrite example.</p>
 * <p>Description: A sample app to write a GDAL raster dataset.</p>
 * @author Tamas Szekeres (szekerest@gmail.com)
 * @version 1.0
 */



/// <summary>
/// A C# based sample to write a GDAL raster dataset.
/// </summary> 

class GDALWrite
{

    public static void usage()

    {
        Console.WriteLine("usage: gdaldatasetwrite {dataset name}");
        System.Environment.Exit(-1);
    }

    public static void Main(string[] args)
    {

        if (args.Length < 1) usage();

        int w, h;

        w = 100;
        h = 100;

        if (args.Length > 1)
            w = int.Parse(args[1], CultureInfo.InvariantCulture);

        if (args.Length > 2)
            h = int.Parse(args[2], CultureInfo.InvariantCulture);

        {
            /* -------------------------------------------------------------------- */
            /*      Register driver(s).                                             */
            /* -------------------------------------------------------------------- */
            Gdal.AllRegister();

            /* -------------------------------------------------------------------- */
            /*      Get driver                                                      */
            /* -------------------------------------------------------------------- */
            Driver drv = Gdal.GetDriverByName("GTiff");

            if (drv == null)
            {
                Console.WriteLine("Can't get driver.");
                System.Environment.Exit(-1);
            }

            Console.WriteLine("Using driver " + drv.LongName);

            /* -------------------------------------------------------------------- */
            /*      Open dataset.                                                   */
            /* -------------------------------------------------------------------- */
            using (Dataset ds = drv.Create(args[0], w, h, 3, DataType.GDT_Byte, null))
            {
                if (ds == null)
                {
                    Console.WriteLine("Can't create " + args[0]);
                    System.Environment.Exit(-1);
                }

                /* -------------------------------------------------------------------- */
                /*      Preparing the data in a byte buffer.                            */
                /* -------------------------------------------------------------------- */

                byte[] buffers = new byte[w * h * 3];

                for (int y = 0; y < h; y++)
                {
                    for (int x = 0; x < w; x++)
                    {
                        //red
                        buffers[(y * w + x)] = ToByte(256 * x * (h - y) / (w * h));
                        //green
                        buffers[w * h + (y * w + x)] = ToByte(256 - 256 * (w - x) * y / (w * h));
                        //blue
                        buffers[2 * w * h + (y * w + x)] = ToByte(Random.Shared.Next(0, 256));
                    }
                }

                ds.WriteRaster(0, 0, w, h, buffers, w, h, 3);
                ds.FlushCache();

                for (int i = 1; i <= ds.RasterCount; i++)
                {
                    using var band = ds.GetRasterBand(i);
                    Console.WriteLine($"Band {i} Histogram (0   ->  255)");
                    Console.WriteLine($"================================");
                    DrawHistogram(band);
                    Console.WriteLine();
                }
            }
        }
    }

    private static byte ToByte(double value, int min = byte.MinValue, int max = byte.MaxValue)
        => value < min ? (byte)min : value > max ? (byte)max : (byte)value;

    private static void DrawHistogram(Band band)
    {
        const int height = 22;
        const int width = 32;
        const char block = '█';
        int[] histogram = new int[width];
        band.GetHistogram(histogram: histogram, approx_ok: false);

        double value = histogram.Max();
        double vStep = value / height;
        for (int y = 0; y < height; y++, value -= vStep)
        {
            for (int x = 0; x < histogram.Length; x++)
            {
                Console.Write(histogram[x] >= value ? block : ' ');
            }
            Console.WriteLine();
        }
    }
}
