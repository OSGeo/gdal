/******************************************************************************
 *
 * Name:     GDALWarp.cs
 * Project:  GDAL CSharp Interface
 * Purpose:  A sample app for demonstrating the GDAL warp capabilities.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2020, Tamas Szekeres
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

using System;
using System.IO;

using System.Runtime.InteropServices;
using OSGeo.GDAL;


/**

 * <p>Title: GDAL C# GDAL warp example.</p>
 * <p>Description: A sample app for demonstrating the GDAL warp capabilities.</p>
 * @author Tamas Szekeres (szekerest@gmail.com)
 * @version 1.0
 */



/// <summary>
/// A C# based sample for demonstrating the GDAL warp capabilities.
/// </summary>

class GDALWarp {

	public static void usage()

	{
		Console.WriteLine("usage example: GDALWarp \"destfile\" \"options\" \"input datasets\"");
		System.Environment.Exit(-1);
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

    public static void Main(string[] args) {

		if (args.Length != 3) usage();

        Gdal.AllRegister();

        GDALWarpAppOptions options = new GDALWarpAppOptions(args[1].Split(new char[] { ' ' }, StringSplitOptions.RemoveEmptyEntries));

        string[] dstNames = args[2].Split(new char[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);

        Dataset[] ds = new Dataset[dstNames.Length];

        for (int i = 0; i < dstNames.Length; i++)
        {
            ds[i] = Gdal.Open(dstNames[i], Access.GA_ReadOnly);
        }

        Dataset dso = Gdal.Warp(args[0], ds, options, new Gdal.GDALProgressFuncDelegate(ProgressFunc), "Sample Data");

        if (dso == null)
        {
            Console.WriteLine("Can't create dest dataset " + args[1]);
            System.Environment.Exit(-1);
        }
	}
}
