/******************************************************************************
 *
 * Name:     GDALWrite.cs
 * Project:  GDAL CSharp Interface
 * Purpose:   sample app to write a GDAL raster.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Tamas Szekeres
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

using System;

using OSGeo.GDAL;


/**
 * <p>Title: GDAL C# GDALCreateCopy example.</p>
 * <p>Description: A sample app to write a GDAL raster using CreateCopy.</p>
 * @author Tamas Szekeres (szekerest@gmail.com)
 * @version 1.0
 */



/// <summary>
/// A C# based sample to write a GDAL raster using CreateCopy.
/// </summary>

class GDALWrite {

	public static void usage()

	{
		Console.WriteLine("usage: gdalcreatecopy {dataset name} {out file name}");
		System.Environment.Exit(-1);
	}

    public static void Main(string[] args)
    {

        if (args.Length != 2) usage();

        Console.WriteLine("");

        try
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
            Dataset ds = Gdal.Open( args[0], Access.GA_ReadOnly );

            if (ds == null)
            {
                Console.WriteLine("Can't open source dataset " + args[0]);
                System.Environment.Exit(-1);
            }

            string[] options = new string [] {"TILED=YES"};
            Dataset dso = drv.CreateCopy(args[1], ds, 0, options, new Gdal.GDALProgressFuncDelegate(ProgressFunc), "Sample Data");

            if (dso == null)
            {
                Console.WriteLine("Can't create dest dataset " + args[1]);
                System.Environment.Exit(-1);
            }
        }
        catch (Exception e)
        {
            Console.WriteLine("Application error: " + e.Message);
        }
    }

    public static int ProgressFunc(double Complete, IntPtr Message, IntPtr Data)
    {
        Console.Write("Processing ... " + Complete * 100 + "% Completed.");
        if (Message != IntPtr.Zero)
            Console.Write(" Message:" + System.Runtime.InteropServices.Marshal.PtrToStringAnsi(Message));
        if (Data != IntPtr.Zero)
            Console.Write(" Data:" + System.Runtime.InteropServices.Marshal.PtrToStringAnsi(Data));

        Console.WriteLine("");
        return 1;
    }
}
