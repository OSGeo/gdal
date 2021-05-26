/******************************************************************************
 * $Id$
 *
 * Name:     GetCRSInfo.cs
 * Project:  GDAL CSharp Interface
 * Purpose:  A sample app for demonstrating of reading the CRSInfo database.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2020, Tamas Szekeres
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

using OSGeo.OSR;
/**

 * <p>Title: GDAL C# readxml example.</p>
 * <p>Description: A sample app for demonstrating of reading the CRSInfo database.</p>
 * @author Tamas Szekeres (szekerest@gmail.com)
 * @version 1.0
 */



/// <summary>
/// A C# based sample for demonstrating of reading the CRSInfo database.
/// </summary>

class GetCRSInfo
{

	public static void usage()

	{
		Console.WriteLine("usage example: getcrsinfo {auth name} {max count}");
		System.Environment.Exit(-1);
	}

	public static void Main(string[] args) {

		if (args.Length < 1) usage();

        int count = 0; ;
        CRSInfoList list = Osr.GetCRSInfoListFromDatabase(args[0], out count);

        if (args.Length > 1)
        {
            int maxcount = int.Parse(args[1]);
            if (count > maxcount)
                count = maxcount;
        }


        for (int i = 0; i < count; i++)
        {
            PrintCRSInfo(i, list[i]);
        }
	}

    public static void PrintCRSInfo(int recnum, CRSInfo info)
    {
        Console.WriteLine(string.Format("{0}. auth_name: {1}, code: {2}, name: {3}, type: {4}, deprecated: {5}, bbox_valid: {6}, west_lon_degree: {7}, south_lat_degree: {8}, east_lon_degree: {9}, north_lat_degree: {10}, area_name: {11}, projection_method: {12}", recnum, info.auth_name, info.code, info.name, info.type, info.deprecated, info.bbox_valid, info.west_lon_degree, info.south_lat_degree, info.east_lon_degree, info.north_lat_degree, info.area_name, info.projection_method));
    }
}
