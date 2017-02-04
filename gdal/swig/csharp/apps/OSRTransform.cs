/******************************************************************************
 * $Id$
 *
 * Name:     OSRTransform.cs
 * Project:  GDAL CSharp Interface
 * Purpose:  A sample app to make coordinate transformations.
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
using OSGeo.OSR;

/**
 * <p>Title: GDAL C# OSRTransform example.</p>
 * <p>Description: A sample app to make coordinate transformations.</p>
 * @author Tamas Szekeres (szekerest@gmail.com)
 * @version 1.0
 */

/// <summary>
/// A C# based sample to make simple transformations.
/// </summary>
class OSRTransform {
	public static void Main(string[] args) {
		try
		{
			/* -------------------------------------------------------------------- */
			/*      Initialize srs                                                  */
			/* -------------------------------------------------------------------- */
			SpatialReference src = new SpatialReference("");
			src.ImportFromProj4("+proj=latlong +datum=WGS84 +no_defs");
			Console.WriteLine( "SOURCE IsGeographic:" + src.IsGeographic() + " IsProjected:" + src.IsProjected() );
			SpatialReference dst = new SpatialReference("");
			dst.ImportFromProj4("+proj=somerc +lat_0=47.14439372222222 +lon_0=19.04857177777778 +x_0=650000 +y_0=200000 +ellps=GRS67 +units=m +no_defs");
			Console.WriteLine( "DEST IsGeographic:" + dst.IsGeographic() + " IsProjected:" + dst.IsProjected() );
			/* -------------------------------------------------------------------- */
			/*      making the transform                                            */
			/* -------------------------------------------------------------------- */
			CoordinateTransformation ct = new CoordinateTransformation(src, dst);
			double[] p = new double[3];
			p[0] = 19; p[1] = 47; p[2] = 0;
			ct.TransformPoint(p);
			Console.WriteLine("x:" + p[0] + " y:" + p[1] + " z:" + p[2]);
			ct.TransformPoint(p, 19.2, 47.5, 0);
			Console.WriteLine("x:" + p[0] + " y:" + p[1] + " z:" + p[2]);
		}
		catch (Exception e)
		{
			Console.WriteLine("Error occurred: " + e.Message);
			System.Environment.Exit(-1);
		}
	}
}