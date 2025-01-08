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

class VSIMem {

	public static void usage()

	{
		Console.WriteLine("usage example: vsimem [image file]");
		System.Environment.Exit(-1);
	}

	public static void Main(string[] args) {

		if (args.Length != 1) usage();

		byte[] imageBuffer;

        using (FileStream fs = new FileStream(args[0], FileMode.Open, FileAccess.Read))
        {
            using (BinaryReader br = new BinaryReader(fs))
            {
                long numBytes = new FileInfo(args[0]).Length;
                imageBuffer = br.ReadBytes((int)numBytes);
                br.Close();
                fs.Close();
            }
        }

        Gdal.AllRegister();

        string memFilename = "/vsimem/inmemfile";
        try
        {
            Gdal.FileFromMemBuffer(memFilename, imageBuffer);
            Dataset ds = Gdal.Open(memFilename, Access.GA_ReadOnly);

            Console.WriteLine("Raster dataset parameters:");
            Console.WriteLine("  RasterCount: " + ds.RasterCount);
            Console.WriteLine("  RasterSize (" + ds.RasterXSize + "," + ds.RasterYSize + ")");

            Driver drv = Gdal.GetDriverByName("GTiff");

            if (drv == null)
            {
                Console.WriteLine("Can't get driver.");
                System.Environment.Exit(-1);
            }

            drv.CreateCopy("sample.tif", ds, 0, null, null, null);
        }
        catch (Exception ex)
        {
            Console.WriteLine(ex.Message);
        }
        finally
        {
            Gdal.Unlink(memFilename);
        }
	}
}