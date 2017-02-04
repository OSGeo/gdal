/******************************************************************************
 * $Id$
 *
 * Name:     VSIMem.cs
 * Project:  GDAL CSharp Interface
 * Purpose:  A sample app for demonstrating the in-memory virtual file support.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Tamas Szekeres
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