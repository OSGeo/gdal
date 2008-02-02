/******************************************************************************
 * $Id$
 *
 * Name:     GDALColorTable.cs
 * Project:  GDAL CSharp Interface
 * Purpose:  A sample app for demonstrating the usage of the ColorTable object.
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
 * <p>Title: GDAL C# GDALColorTable example.</p>
 * <p>Description: A sample app for demonstrating the usage of the ColorTable object.</p>
 * @author Tamas Szekeres (szekerest@gmail.com)
 * @version 1.0
 */



/// <summary>
/// A C# based sample for demonstrating the usage of the ColorTable object.
/// </summary> 

class GDALColorTable {
	
	public static void usage() 

	{ 
		Console.WriteLine("usage: gdalcolortable {source dataset} {destination file}");
		System.Environment.Exit(-1);
	}
 
    public static void Main(string[] args) 
    {

        if (args.Length != 2) usage();

        string file = args[0];
        string file_out = args[1];
        try {

            /* -------------------------------------------------------------------- */
            /*      Register driver(s).                                             */
            /* -------------------------------------------------------------------- */
            Gdal.AllRegister();
            
            Driver dv = null;
            Dataset ds = null, ds_out = null;
            Band ba = null, ba_out = null;
            ColorTable ct = null, ct_out = null;
            byte [] buffer;

            /* -------------------------------------------------------------------- */
            /*      Open dataset.                                                   */
            /* -------------------------------------------------------------------- */
            ds = Gdal.Open(file, Access.GA_ReadOnly);
            ba = ds.GetRasterBand(1);
            ct = ba.GetRasterColorTable();

            if( ct != null )
                Console.WriteLine( "Band has a color table with " + ct.GetCount() + " entries.");
            else 
            {
                Console.WriteLine( "Data source has no color table");
                return;
            }

            buffer = new byte [ds.RasterXSize * ds.RasterYSize];
            ba.ReadRaster(0, 0, ds.RasterXSize, ds.RasterYSize, buffer,
                ds.RasterXSize, ds.RasterYSize, 0, 0);

            /* -------------------------------------------------------------------- */
            /*      Get driver                                                      */
            /* -------------------------------------------------------------------- */	
            dv = Gdal.GetDriverByName("GTiff");

            ds_out = dv.Create(file_out, ds.RasterXSize, ds.RasterYSize,
                ds.RasterCount, ba.DataType, new string [] {});
            ba_out = ds_out.GetRasterBand(1);
            ct_out = new ColorTable(PaletteInterp.GPI_RGB);

            ba_out.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, buffer,
                ds.RasterXSize, ds.RasterYSize, 0, 0);

            /* -------------------------------------------------------------------- */
            /*      Copying the colortable                                          */
            /* -------------------------------------------------------------------- */
            for (int i = 0; i < ct.GetCount(); i++)
            {
                ColorEntry ce = null, ce_out = null;

                ce = ct.GetColorEntry(i);
                ce_out = new ColorEntry();

                ce_out.c1 = ce.c1;
                ce_out.c2 = ce.c2;
                ce_out.c3 = ce.c3;
                ce_out.c4 = ce.c4;

                ct_out.SetColorEntry(i, ce_out);

                ce.Dispose();
                ce_out.Dispose();
            }

            ba_out.SetRasterColorTable(ct_out);
        }
        catch (Exception e) 
        {
            Console.WriteLine("Application error: " + e.Message);
        }
    }
}