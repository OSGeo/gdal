/******************************************************************************
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
using System.Globalization;

using OSGeo.GDAL;


/**
 * <p>Title: GDAL C# GDALWrite example.</p>
 * <p>Description: A sample app to write a GDAL raster.</p>
 * @author Tamas Szekeres (szekerest@gmail.com)
 * @version 1.0
 */



/// <summary>
/// A C# based sample to write a GDAL raster.
/// </summary>

class GDALWrite
{

    public static void usage()

    {
        Console.WriteLine("usage: gdalwrite {dataset name} {width} {height}");
        System.Environment.Exit(-1);
    }

    public static void Main(string[] args)
    {

        if (args.Length < 1) usage();

        // Using early initialization of System.Console
        Console.WriteLine("Writing sample: " + args[0]);

        int bXSize, bYSize;
        int w, h;

        w = 100;
        h = 100;

        if (args.Length > 1)
            w = int.Parse(args[1], CultureInfo.InvariantCulture);

        if (args.Length > 2)
            h = int.Parse(args[2], CultureInfo.InvariantCulture);

        bXSize = w;
        bYSize = 1;

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
        string[] options = new string[] {
                "BLOCKXSIZE=" + bXSize,
                "BLOCKYSIZE=" + bYSize
            };

        using (Dataset ds = drv.Create(args[0], w, h, 1, DataType.GDT_Byte, options))
        {
            if (ds == null)
            {
                Console.WriteLine("Can't open " + args[0]);
                System.Environment.Exit(-1);
            }

            /* -------------------------------------------------------------------- */
            /*      Setting corner GCPs.                                            */
            /* -------------------------------------------------------------------- */
            GCP[] GCPs = new GCP[] {
                new GCP(44.5, 27.5, 0, 0, 0, "info0", "id0"),
                new GCP(45.5, 27.5, 0, 100, 0, "info1", "id1"),
                new GCP(44.5, 26.5, 0, 0, 100, "info2", "id2"),
                new GCP(45.5, 26.5, 0, 100, 100, "info3", "id3")
            };
            ds.SetGCPs(GCPs);
            CompareGCPs(GCPs, ds.GetGCPs());
            ds.SetGCPs2(GCPs);
            CompareGCPs(GCPs, ds.GetGCPs());

            using (Band band = ds.GetRasterBand(1))
            {
                byte[] buffer = new byte[w * h];

                for (int i = 0; i < w; i++)
                {
                    for (int j = 0; j < h; j++)
                    {
                        buffer[i * w + j] = (byte)(i * 256 / w);
                    }
                }

                band.WriteRaster(0, 0, w, h, buffer, w, h, 0, 0);

                band.FlushCache();
            }
            ds.FlushCache();
        }
    }

    private static void CompareGCPs(GCP[] set1, GCP[] set2)
    {
        if (set1.Length != set2.Length) ErrorAndClose("GCP count mismatch");
        for (int i = 0; i < set1.Length; i++)
        {
            if (set1[i].Id != set2[i].Id) ErrorAndClose("GCP id mismatch");
            if (set1[i].Info != set2[i].Info) ErrorAndClose("GCP info mismatch");
            if (set1[i].GCPPixel != set2[i].GCPPixel) ErrorAndClose("GCP pixel mismatch");
            if (set1[i].GCPLine != set2[i].GCPLine) ErrorAndClose("GCP line mismatch");
            if (set1[i].GCPX != set2[i].GCPX) ErrorAndClose("GCP X mismatch");
            if (set1[i].GCPY != set2[i].GCPY) ErrorAndClose("GCP Y mismatch");
            if (set1[i].GCPZ != set2[i].GCPZ) ErrorAndClose("GCP Z mismatch");
        }
    }

    private static void ErrorAndClose(string message)
    {
        Console.WriteLine("Error: " + message);
        System.Environment.Exit(-1);
    }
}
