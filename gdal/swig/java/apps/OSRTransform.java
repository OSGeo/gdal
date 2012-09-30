/******************************************************************************
 * $Id$
 *
 * Name:     OSRTransform.java
 * Project:  GDAL Java Interface
 * Purpose:  A sample app to make coordinate transformations.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 * Port from OSRTransform.cs by Tamas Szekeres
 *
 ******************************************************************************
 * Copyright (c) 2009, Even Rouault
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

import org.gdal.osr.SpatialReference;
import org.gdal.osr.CoordinateTransformation;

/**
 * <p>Title: GDAL Java OSRTransform example.</p>
 * <p>Description: A sample app to make coordinate transformations.</p>
 * @author Tamas Szekeres (szekerest@gmail.com)
 * @version 1.0
 */

/// <summary>
/// A Java based sample to make simple transformations.
/// </summary> 
public class OSRTransform {
	public static void main(String[] args) {
		try
		{
			/* -------------------------------------------------------------------- */
			/*      Initialize srs                                                  */
			/* -------------------------------------------------------------------- */
			SpatialReference src = new SpatialReference("");
			src.ImportFromProj4("+proj=latlong +datum=WGS84 +no_defs");
			System.out.println( "SOURCE IsGeographic:" + src.IsGeographic() + " IsProjected:" + src.IsProjected() );
			SpatialReference dst = new SpatialReference("");
			dst.ImportFromProj4("+proj=somerc +lat_0=47.14439372222222 +lon_0=19.04857177777778 +x_0=650000 +y_0=200000 +ellps=GRS67 +units=m +no_defs");
			System.out.println( "DEST IsGeographic:" + dst.IsGeographic() + " IsProjected:" + dst.IsProjected() );
			/* -------------------------------------------------------------------- */
			/*      making the transform                                            */
			/* -------------------------------------------------------------------- */
            /* New in GDAL 2.0. Before was "new CoordinateTransformation(srs,dst)". */
			CoordinateTransformation ct = CoordinateTransformation.CreateCoordinateTransformation(src, dst);
			double[] p = new double[3];
			p[0] = 19; p[1] = 47; p[2] = 0;
			ct.TransformPoint(p);
			System.out.println("x:" + p[0] + " y:" + p[1] + " z:" + p[2]);
			ct.TransformPoint(p, 19.2, 47.5, 0);
			System.out.println("x:" + p[0] + " y:" + p[1] + " z:" + p[2]);
		}
		catch (Exception e)
		{
			System.out.println("Error occurred: " + e.getMessage());
			System.exit(-1);
		}
	}
}