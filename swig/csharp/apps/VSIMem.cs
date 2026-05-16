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
        => ErrorAndQuit("usage example: vsimem [image file]");

    private static void ErrorAndQuit(string errorMessage)
    {
        Console.Error.WriteLine(errorMessage);
        Environment.Exit(-1);
    }

    public static void Main(string[] args)
    {
        if (args.Length != 1) usage();

        Gdal.AllRegister();
        string memFilename = "/vsimem/inmemfile";

        try
        {
            byte[] imageBytes = ReadInputFile(args[0]);
            CopyToInMemoryFile(memFilename, imageBytes);
            using (Dataset ds = Gdal.Open(memFilename, Access.GA_ReadOnly))
            {
                if (ds == null)
                {
                    ErrorAndQuit("Can't open in-memory file.");
                }
                Console.WriteLine("Raster dataset parameters:");
                Console.WriteLine("  RasterCount: " + ds.RasterCount);
                Console.WriteLine("  RasterSize (" + ds.RasterXSize + "," + ds.RasterYSize + ")");

                Driver drv = Gdal.GetDriverByName("GTiff");

                if (drv == null)
                {
                    ErrorAndQuit("Can't get driver.");
                }

                drv.CreateCopy("sample.tif", ds, 0, null, null, null).Dispose();
            }
        }
        finally
        {
            Gdal.Unlink(memFilename);
        }
    }

    /// <summary> Test VSIFOpenL, VSIFSeekL, VSIFTellL, VSIFReadL, and VSIFCloseL. </summary>
    private static byte[] ReadInputFile(string inputFilename)
    {
        IntPtr vfInput = Gdal.VSIFOpenL(inputFilename, "r");
        if (vfInput == IntPtr.Zero)
        {
            ErrorAndQuit($"Failed to {nameof(Gdal.VSIFOpenL)} input file: '{inputFilename}'");
        }
        if (!Gdal.VSIFSeekL(vfInput, 0, SeekOrigin.End))
        {
            ErrorAndQuit($"{nameof(Gdal.VSIFSeekL)} failed to seek to end of input file");
        }

        long vfInputLength = Gdal.VSIFTellL(vfInput);
        if (vfInputLength <= 0)
        {
            ErrorAndQuit($"{nameof(Gdal.VSIFTellL)} reports incorrect location");
        }
        if (!Gdal.VSIFSeekL(vfInput, 0, SeekOrigin.Begin) || Gdal.VSIFTellL(vfInput) != 0)
        {
            ErrorAndQuit($"{nameof(Gdal.VSIFSeekL)} failed to seek to beginning of input file");
        }

        byte[] inputFileBuffer = new byte[vfInputLength];
        if (Gdal.VSIFReadL(inputFileBuffer, 0, inputFileBuffer.Length, vfInput) != inputFileBuffer.Length)
        {
            ErrorAndQuit($"Failed to {nameof(Gdal.VSIFReadL)} from input file");
        }
        if (Gdal.VSIFCloseL(vfInput) != 0)
        {
            ErrorAndQuit($"Failed to {nameof(Gdal.VSIFCloseL)} input file");
        }
        return inputFileBuffer;
    }

    /// <summary> Test in-memory file methods and VSIFWriteL </summary>
    private static void CopyToInMemoryFile(string inMemFilename, byte[] data)
    {
        byte[] imageCopy = new byte[data.Length];
        //Create an in-memory file from a managed buffer.
        //The managed buffer is used to read/write and is not copied to GDAL.
        //Must dispose of the file to unpin the managed buffer and unlink the filename.
        using (var inMemFile = Gdal.FileFromMemBuffer(inMemFilename, imageCopy, 0, imageCopy.Length, vsiTakeOwnership: false))
        {
            if (inMemFile == null)
            {
                ErrorAndQuit("Failed to create no-copy in-memory file.");
            }
            IntPtr fpCopy = Gdal.VSIFOpenL(inMemFilename, "w");
            if (fpCopy == IntPtr.Zero)
            {
                ErrorAndQuit("Failed to open in-memory file.");
            }
            if (Gdal.VSIFWriteL(data, 0, data.Length, fpCopy) != data.Length)
            {
                ErrorAndQuit("Failed to write to in-memory file.");
            }
            if (Gdal.VSIFCloseL(fpCopy) != 0)
            {
                ErrorAndQuit("Failed to close in-memory file.");
            }
        }

        //Create an in-memory file by copying the data into an unmanaged buffer owed by GDAL.
        //Must call Gdal.Unlink() on the file to free the unmanaged buffer.
        if (!Gdal.FileFromMemBuffer(inMemFilename, imageCopy))
        {
            ErrorAndQuit("Failed to create copied in-memory file.");
        }

        if (!Gdal.VSIStatExL(inMemFilename, out long size, out int mode, nFlags: 0))
        {
            ErrorAndQuit("Failed to stat in-memory file.");
        }

        if (size != imageCopy.Length)
        {
            ErrorAndQuit("In-memory file size differs from source buffer size");
        }

        if ((mode & 0x8000) != 0x8000)
        {
            ErrorAndQuit("In-memory is not 'Regular'");
        }
    }
}
