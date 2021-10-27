/******************************************************************************
 * $Id$
 *
 * Name:     OSRTransform.java
 * Project:  GDAL Java Interface
 * Purpose:  A sample app to make coordinate transformations.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
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

import java.lang.Math;
import java.lang.RuntimeException;
import org.gdal.osr.osr;
import org.gdal.osr.SpatialReference;
import org.gdal.osr.CoordinateTransformation;
import org.gdal.osr.CoordinateTransformationOptions;

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
            /* New in GDAL 1.10. Before was "new CoordinateTransformation(srs,dst)". */
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
		
		testTransformPointWithErrorCode();
		testTransformPointsWithErrorCodes();
		testSetDesiredAccuracy();
		testSetBallparkAllowed();
	}

        public static void check(boolean b)
        {
            class CheckException extends RuntimeException
            {
                public CheckException(String s)
                {
                    super(s);
                }
            }

            if( !b )
                throw new CheckException("failed test");
        }

        public static void testTransformPointWithErrorCode()
        {
            if( osr.GetPROJVersionMajor() < 8 )
            {
                System.out.println("Skip testTransformPointWithErrorCode() due to PROJ < 8");
                return;
            }
            SpatialReference s = new SpatialReference("");
            s.SetFromUserInput("+proj=longlat +ellps=GRS80");
            SpatialReference t = new SpatialReference("");
            t.SetFromUserInput("+proj=tmerc +ellps=GRS80");
            CoordinateTransformation ct = CoordinateTransformation.CreateCoordinateTransformation(s, t);

            {
                double[] out = new double[4];
                int errorCode = ct.TransformPointWithErrorCode(out, 1, 2, 0, 0);
                check(Math.abs(out[0] - 111257.80439304397) < 1e-5);
                check(Math.abs(out[1] - 221183.3401672801) < 1e-5);
                check(errorCode == 0);
            }

            {
                double[] out = new double[4];
                int errorCode = ct.TransformPointWithErrorCode(out, 90, 0, 0, 0);
                check(out[0] == Double.POSITIVE_INFINITY);
                check(errorCode == osr.PROJ_ERR_COORD_TRANSFM_OUTSIDE_PROJECTION_DOMAIN);
            }

        }

        public static void testTransformPointsWithErrorCodes()
        {
            if( osr.GetPROJVersionMajor() < 8 )
            {
                System.out.println("Skip testTransformPointsWithErrorCodes() due to PROJ < 8");
                return;
            }
            SpatialReference s = new SpatialReference("");
            s.SetFromUserInput("+proj=longlat +ellps=GRS80");
            SpatialReference t = new SpatialReference("");
            t.SetFromUserInput("+proj=tmerc +ellps=GRS80");
            CoordinateTransformation ct = CoordinateTransformation.CreateCoordinateTransformation(s, t);
            double[][] coords = new double[][] {
                new double[] {1, 2},
                new double[] {1, 2, 3, 4},
                new double[] {90, 0}
            };
            int[] errorCodes = ct.TransformPointsWithErrorCodes(coords);

            check(Math.abs(coords[0][0] - 111257.80439304397) < 1e-5);
            check(Math.abs(coords[0][1] - 221183.3401672801) < 1e-5);
            check(errorCodes[0] == 0);

            check(Math.abs(coords[1][0] - 111257.80439304397) < 1e-5);
            check(Math.abs(coords[1][1] - 221183.3401672801) < 1e-5);
            check(Math.abs(coords[1][2] - 3) < 1e-5);
            check(Math.abs(coords[1][3] - 4) < 1e-5);
            check(errorCodes[1] == 0);

            check(coords[2][0] == Double.POSITIVE_INFINITY);
            check(errorCodes[2] == osr.PROJ_ERR_COORD_TRANSFM_OUTSIDE_PROJECTION_DOMAIN);
	}

        public static void testSetDesiredAccuracy()
        {
            SpatialReference s = new SpatialReference("");
            s.SetFromUserInput("EPSG:4326"); // WGS84
            SpatialReference t = new SpatialReference("");
            t.SetFromUserInput("EPSG:4258"); // ETRS89

            CoordinateTransformationOptions options = new CoordinateTransformationOptions();
            options.SetDesiredAccuracy(0.05);
            try
            {
                CoordinateTransformation ct = CoordinateTransformation.CreateCoordinateTransformation(s, t, options);
                check( false );
            }
            catch( RuntimeException e)
            {
            }
        }

        public static void testSetBallparkAllowed()
        {
            SpatialReference s = new SpatialReference("");
            s.SetFromUserInput("EPSG:4267"); // NAD 27
            SpatialReference t = new SpatialReference("");
            t.SetFromUserInput("EPSG:4258"); // ETRS89

            CoordinateTransformationOptions options = new CoordinateTransformationOptions();
            options.SetBallparkAllowed(false);
            try
            {
                CoordinateTransformation ct = CoordinateTransformation.CreateCoordinateTransformation(s, t, options);
                check( false );
            }
            catch( RuntimeException e)
            {
            }
        }
}
