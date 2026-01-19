/******************************************************************************
 *
 * Name:     GDALTestVSI.java
 * Project:  GDAL Java Interface
 * Purpose:  Smoke test for VSI functions
 * Author:   David Iglesias, <iglesiasd at predictia dot es>
 *
 ******************************************************************************
 * Copyright (c) 2009, Even Rouault
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

import org.gdal.gdal.gdal;

public class GDALTestVSI
{

	/**
	 * @param args
	 */
	public static void main(String[] args) {
        gdal.AllRegister();
        gdal.VSICurlClearCache();
        gdal.VSICurlPartialClearCache("/cache/key");
    }
}
