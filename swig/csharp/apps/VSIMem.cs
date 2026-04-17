/******************************************************************************
 *
 * Name:     VSIMem.cs
 * Project:  GDAL CSharp Interface
 * Purpose:  A sample app for demonstrating the in-memory virtual file support.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Tamas Szekeres
 
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

using System;
using System.IO;

using System.Runtime.InteropServices;

using OSGeo.GDAL;


/**

 * <p>Title: GDAL C# VSIMem example.</p>
 * <p>Description: A sample app for demonstrating the in-memory virtual file support.</p>
 * @author Tamas Szekeres (szekerest@gmail.com)
 * @version 1.0
 */



/// <summary>
/// A C# based sample for demonstrating the in-memory virtual file support.
/// </summary>

class VSIMem
{

    public static void usage()

    {
        Console.WriteLine("usage example: vsimem [image file]");
        System.Environment.Exit(-1);
    }

    public static void Main(string[] args)
    {

        if (args.Length != 1) usage();

        byte[] imageBuffer;

        try
        {
            IntPtr vfInput = Gdal.VSIFOpenL(args[0], "r");
            if (vfInput == IntPtr.Zero)
            {
                Console.WriteLine($"Failed to {nameof(Gdal.VSIFOpenL)} input file");
                System.Environment.Exit(-1);
            }
            if (!Gdal.VSIFSeekL(vfInput, 0, SeekOrigin.End))
            {
                Console.WriteLine($"{nameof(Gdal.VSIFSeekL)} failed to seek to end of input file");
                System.Environment.Exit(-1);
            }
            long vfInputLength = Gdal.VSIFTellL(vfInput);
            if (vfInputLength <= 0)
            {
                Console.WriteLine($"{nameof(Gdal.VSIFTellL)} reports incorrect location");
                System.Environment.Exit(-1);
            }
            if (!Gdal.VSIFSeekL(vfInput, 0, SeekOrigin.Begin) || Gdal.VSIFTellL(vfInput) != 0)
            {
                Console.WriteLine($"{nameof(Gdal.VSIFSeekL)} failed to seek to beginning of input file");
                System.Environment.Exit(-1);
            }

            imageBuffer = new byte[vfInputLength];
            if (Gdal.VSIFReadL(imageBuffer, 0, imageBuffer.Length, vfInput) != imageBuffer.Length)
            {
                Console.WriteLine($"Failed to {nameof(Gdal.VSIFReadL)} from input file");
                System.Environment.Exit(-1);
            }
            if (Gdal.VSIFCloseL(vfInput) != 0)
            {
                Console.WriteLine($"Failed to {nameof(Gdal.VSIFCloseL)} input file");
                System.Environment.Exit(-1);
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine(ex.Message);
            throw;
        }

        Gdal.AllRegister();

        string memFilename = "/vsimem/inmemfile";
        try
        {
            using (var file = Gdal.FileFromMemBufferNoCopy(memFilename, imageBuffer, 0, imageBuffer.Length))
            {
                if (file == null)
                {
                    Console.WriteLine("Failed to open in-memory file.");
                    System.Environment.Exit(-1);
                }
                using (Dataset ds = Gdal.Open(memFilename, Access.GA_ReadOnly))
                {
                    if (ds == null)
                    {
                        Console.WriteLine("Can't open in-memory file.");
                        System.Environment.Exit(-1);
                    }
                    Console.WriteLine("Raster dataset parameters:");
                    Console.WriteLine("  RasterCount: " + ds.RasterCount);
                    Console.WriteLine("  RasterSize (" + ds.RasterXSize + "," + ds.RasterYSize + ")");

                    Driver drv = Gdal.GetDriverByName("GTiff");

                    if (drv == null)
                    {
                        Console.WriteLine("Can't get driver.");
                        System.Environment.Exit(-1);
                    }

                    drv.CreateCopy("sample.tif", ds, 0, null, null, null).Dispose();
                }
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine(ex.Message);
            throw;
        }
    }
}
