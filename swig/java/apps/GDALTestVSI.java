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
        if( gdal.GetConfigOption("foo") != null ) {
             throw new RuntimeException("failed: gdal.GetConfigOption(\"foo\") != null");
        }
        gdal.SetConfigOption("foo", "bar");
        if( !gdal.GetConfigOption("foo").equals("bar") ) {
             throw new RuntimeException("failed: !gdal.GetConfigOption(\"foo\").equals(\"bar\")");
        }
        gdal.SetConfigOption("foo", null);
        if( gdal.GetConfigOption("foo") != null ) {
             throw new RuntimeException("failed: gdal.GetConfigOption(\"foo\") != null");
        }
        try {
            gdal.SetConfigOption(null, null);
            throw new RuntimeException("exception expected");
        }
        catch(java.lang.NullPointerException e) {
        }
        catch(java.lang.IllegalArgumentException e) {
        }
        try {
            gdal.VSICurlPartialClearCache(null);
            throw new RuntimeException("exception expected");
        }
        catch(java.lang.NullPointerException e) {
        }
        catch(java.lang.IllegalArgumentException e) {
        }
    }
}
