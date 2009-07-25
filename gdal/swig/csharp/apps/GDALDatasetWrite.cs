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

class GDALWrite {
	
	public static void usage() 

	{ 
		Console.WriteLine("usage: gdaldatasetwrite {dataset name}");
		System.Environment.Exit(-1);
	}
 
    public static void Main(string[] args) 
    {

        if (args.Length < 1) usage();

        int bXSize, bYSize;
        int w, h;

        w = 100;
        h = 100;

        if (args.Length > 1)
            w = int.Parse(args[1]);

        if (args.Length > 2)
            h = int.Parse(args[2]);

        bXSize = w;
        bYSize = 1;

        //try 
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
            Dataset ds = drv.Create(args[0], w, h, 3, DataType.GDT_Byte, null);
		
            if (ds == null) 
            {
                Console.WriteLine("Can't create " + args[0]);
                System.Environment.Exit(-1);
            }

            /* -------------------------------------------------------------------- */
            /*      Preparing the data in a byte buffer.                            */
            /* -------------------------------------------------------------------- */

            byte [] buffers = new byte [w * h * 3];

            for (int i = 0; i < w; i++)
            {
                for (int j = 0; j < h; j++)
                {
                    buffers[(i * w + j)] = (byte)(256 - i * 256 / w);
                    buffers[w * h + (i * w + j)] = 0;
                    buffers[2 * w * h + (i * w + j)] = (byte)(i * 256 / w);
                }
            }

            int[] iBandMap = { 1, 2, 3 };
            ds.WriteRaster(0, 0, w, h, buffers, w, h, 3, iBandMap, 0, 0, 0);

            ds.FlushCache();

        }
        /*catch (Exception e) 
        {
            Console.WriteLine("Application error: " + e.Message);
        }*/
    }
}