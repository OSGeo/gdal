/******************************************************************************
 * $Id$
 *
 * Project: GDAL
 * Purpose: Compute each pixel's proximity to a set of target pixels.
 * Author:  Ivan Lucena, ivan.lucena@pmldnet.com
 *          translated from "GDALProximity.cpp" by
 *          Frank Warmerdam, warmerdam@pobox.com
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam
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
 ****************************************************************************/
package proximity;

import java.io.File;

import org.gdal.gdal.Band;
import org.gdal.gdal.Dataset;
import org.gdal.gdal.Driver;
import org.gdal.gdal.gdal;
import org.gdal.gdalconst.gdalconstConstants;

/**
Compute the proximity of all pixels in the image to a set of pixels in the source image.

This function attempts to compute the proximity of all pixels in
the image to a set of pixels in the source image.  The following
options are used to define the behavior of the function.  By
default all non-zero pixels in hSrcBand will be considered the
"target", and all proximities will be computed in pixels.  Note
that target pixels are set to the value corresponding to a distance
of zero.

The progress function args may be NULL or a valid progress reporting function
such as GDALTermProgress/NULL.

Options:

VALUES=n[,n]*

A list of target pixel values to measure the distance from.  If this
option is not provided proximity will be computed from non-zero
pixel values.  Currently pixel values are internally processed as
integers.

DISTUNITS=[PIXEL]/GEO

Indicates whether distances will be computed in pixel units or
in georeferenced units.  The default is pixel units.  This also
determines the interpretation of MAXDIST.

MAXDIST=n

The maximum distance to search.  Proximity distances greater than
this value will not be computed.  Instead output pixels will be
set to a nodata value.

NODATA=n

The NODATA value to use on the output band for pixels that are
beyond MAXDIST.  If not provided, the hProximityBand will be
queried for a nodata value.  If one is not found, 65535 will be used.

FIXED_BUF_VAL=n

If this option is set, all pixels within the MAXDIST threadhold are
set to this fixed value instead of to a proximity distance.
 */
public class GDALProximity {

    public static void Usage() {
        System.out.println("Usage: Proximity srcfile dstfile [-srcband n] [-dstband n]");
        System.out.println("                 [-of format] [-co name=value]*");
        System.out.println("                 [-ot Byte/Int16/Int32/Float32/etc]");
        System.out.println("                 [-values n,n,n] [-distunits PIXEL/GEO]");
        System.out.println("                 [-maxdist n] [-nodata n] [-fixed-buf-val n]");
        System.exit(1);
    }

    public static void main(String[] args) {

        String SourceFilename = null;
        String OutputFilename = null;

        /*
         * Parse arguments
         */

        float MaxDistance = -1.0F;
        boolean GeoUnits = false;
        float Nodata = 0.0F;
        float BufferValue = 0.0F;
        boolean hasBufferValue = false;
        String OutputFormat = "GTiff";
        String OutputType = null;
        
        for (int i = 0; i < args.length; i++) {
            if (args[i].equals("-of")) {
                i++;
                OutputFormat = args[i];
            } else if (args[i].equals("-ot")) {
                i++;
                OutputType = args[i];
            } else if (args[i].equals("-maxdist")) {
                i++;
                MaxDistance = Float.parseFloat(args[i]);
            } else if (args[i].equals("-distunits")) {
                i++;
                if( args[i].equals("geo") ) {
                    GeoUnits = true;
                } else if ( args[i].equals("pixel") ) {
                    GeoUnits = false;
                } else {
                    Usage();
                }
            } else if (args[i].equals("-nodata")) {
                i++;
                Nodata = Float.parseFloat(args[i]);
            /*
             * TODO: parse "-values"
             */
            } else if (args[i].equals("-fixed-buf-val")) {
                i++;
                BufferValue = Float.parseFloat(args[i]);
                hasBufferValue = true;
            } else if (SourceFilename == null) {
                SourceFilename = args[i];
            } else if (OutputFilename == null) {
                OutputFilename = args[i];
            } else {
                Usage();
            }
        }

        if (SourceFilename == null) {
            Usage();
        }

        gdal.AllRegister();

        /*
         * Open Input
         */

        Dataset SourceDataset = null;
        Dataset WorkProximityDataset = null;
        Driver WorkProximityDriver = null;

        SourceDataset = gdal.Open(SourceFilename, gdalconstConstants.GA_ReadOnly);

        if (SourceDataset == null) {
            System.err.println("GDALOpen failed - " + gdal.GetLastErrorNo());
            System.err.println(gdal.GetLastErrorMsg());
            System.exit(1);
        }

        /*
         * Open Output
         */

        WorkProximityDriver = gdal.IdentifyDriver(OutputFilename);

        if (WorkProximityDriver != null) {
            WorkProximityDataset = gdal.Open(OutputFilename, gdalconstConstants.GA_Update);
        } else {
            /*
             * Create a new output dataset
             */
            WorkProximityDriver = gdal.GetDriverByName(OutputFormat);

            WorkProximityDataset = WorkProximityDriver.Create(OutputFilename, 
                    SourceDataset.getRasterXSize(), SourceDataset.getRasterYSize(),
                    SourceDataset.getRasterCount(), gdalconstConstants.GDT_Float32);

            WorkProximityDataset.SetGeoTransform(SourceDataset.GetGeoTransform());
            WorkProximityDataset.SetProjection(SourceDataset.GetProjectionRef());
        }

        if (WorkProximityDataset == null) {

            System.err.println("GDALOpen failed - " + gdal.GetLastErrorNo());
            System.err.println(gdal.GetLastErrorMsg());
            System.exit(1);
        }

        long startTime = System.currentTimeMillis();

        Run(SourceDataset.GetRasterBand(1),
                WorkProximityDataset.GetRasterBand(1), MaxDistance, GeoUnits, Nodata,
                hasBufferValue, BufferValue);

        long stopTime = System.currentTimeMillis();

        SourceDataset.delete();

        /* Not strictly required. delete() will also FlushCache() */
        WorkProximityDataset.FlushCache();

        WorkProximityDataset.delete();

        gdal.GDALDestroyDriverManager();

        System.out.println("Done in " + ((double) (stopTime - startTime) / 1000.0) + " seconds");
    }

    public static void Run(Band SrcBand, Band ProximityBand,
            float MaxDistance, boolean GeoUnits, float NoData,
            boolean HasBufferValue, float BufferValue) {

        /*
         * TODO: Are we using pixels or georeferenced coordinates for distances?
         */

        float DistMult = 1.0F;

        /*
         *
         */

        if (MaxDistance == -1.0F) {
            MaxDistance = SrcBand.GetXSize() + SrcBand.GetYSize();
        }

        /*
         * Create working band
         */

        Band WorkProximityBand = ProximityBand;
        Dataset WorkProximityDS = null;

        int ProxType = ProximityBand.getDataType();

        String tempFilename = null;

        if (ProxType == gdalconstConstants.GDT_Byte 
                || ProxType == gdalconstConstants.GDT_UInt16
                || ProxType == gdalconstConstants.GDT_UInt32) {
            tempFilename = "/vsimem/proximity_" + String.valueOf(System.currentTimeMillis()) + ".tif";
            WorkProximityDS = gdal.GetDriverByName("GTiff").Create(tempFilename,
                    ProximityBand.getXSize(), ProximityBand.getYSize(), 1,
                    gdalconstConstants.GDT_Float32);
            WorkProximityBand = WorkProximityDS.GetRasterBand(1);
        }

        int xSize = WorkProximityBand.getXSize();
        int ySize = WorkProximityBand.getYSize();

        /*
         * AllocateDirect buffers as Byte
         */

        short nearXBuffer[] = new short[xSize];
        short nearYBuffer[] = new short[xSize];
        int scanline[] = new int[xSize];
        float proximityBuffer[] = new float[xSize];

        /*
         * Loop from top to bottom of the image
         */

        for (int i = 0; i < xSize; i++) {
            nearXBuffer[i] = -1;
            nearYBuffer[i] = -1;
        }

        for (int iLine = 0; iLine < ySize; iLine++) {

            SrcBand.ReadRaster(0, iLine, xSize, 1, xSize, 1,
                    gdalconstConstants.GDT_Int32, scanline);

            for (int i = 0; i < xSize; i++) {
                proximityBuffer[i] = -1F;
            }

            /*
             * Left to Right
             */

            ProcessProximityLine(scanline, nearXBuffer, nearYBuffer,
                    true, iLine, xSize, MaxDistance, proximityBuffer);

            /*
             * Right to Left
             */

            ProcessProximityLine(scanline, nearXBuffer, nearYBuffer,
                    false, iLine, xSize, MaxDistance, proximityBuffer);

            /*
             * Write to Proximity Band
             */

            WorkProximityBand.WriteRaster(0, iLine, xSize, 1, xSize, 1,
                    gdalconstConstants.GDT_Float32, proximityBuffer);
        }

        /*
         * Loop from bottom to top of the image
         */

        for (int i = 0; i < xSize; i++) {
            nearXBuffer[i] = -1;
            nearYBuffer[i] = -1;
        }

        for (int iLine = ySize - 1; iLine >= 0; iLine--) {

            WorkProximityBand.ReadRaster(0, iLine, xSize, 1, xSize, 1,
                    gdalconstConstants.GDT_Float32, proximityBuffer);

            SrcBand.ReadRaster(0, iLine, xSize, 1, xSize, 1,
                    gdalconstConstants.GDT_Int32, scanline);

            /*
             * Right to Left
             */

            ProcessProximityLine(scanline, nearXBuffer, nearYBuffer,
                    false, iLine, xSize, MaxDistance, proximityBuffer);

            /*
             * Left to Right
             */

            ProcessProximityLine(scanline, nearXBuffer, nearYBuffer,
                    true, iLine, xSize, MaxDistance, proximityBuffer);

            /*
             * Final post processing of distances.
             */

            for (int i = 0; i < xSize; i++) {
                if (proximityBuffer[i] < 0.0F) {
                    proximityBuffer[i] = NoData;
                } else if (proximityBuffer[i] > 0.0F) {
                    if (HasBufferValue) {
                        proximityBuffer[i] = BufferValue;
                    } else {
                        proximityBuffer[i] = DistMult * proximityBuffer[i];
                    }
                }
            }

            /*
             * Write to Proximity Band
             */

            ProximityBand.WriteRaster(0, iLine, xSize, 1, xSize, 1,
                    gdalconstConstants.GDT_Float32, proximityBuffer);
        }

        ProximityBand.FlushCache();

        /*
         * Delete temporary file
         */

        if (WorkProximityDS != null) {
            WorkProximityDS.delete();
            /* /vsimem files are not standard, so they must be deleted */
            /* with GDAL API to free the associated memory */
            gdal.Unlink(tempFilename);
        }
    }

    public static void ProcessProximityLine(int[] scanlineArray,
            short[] nearXArray, short[] nearYArray, boolean Forward,
            int iLine, int XSize, float MaxDist, float[] proximityArray) {

        int iStart, iEnd, iStep, iPixel;

        if (Forward) {
            iStart = 0;
            iEnd = XSize;
            iStep = 1;
        } else {
            iStart = XSize - 1;
            iEnd = -1;
            iStep = -1;
        }

        for (iPixel = iStart; iPixel != iEnd; iPixel += iStep) {

            /*
             * TODO: Is the current pixel a target pixel?
             */

            boolean isATarger = scanlineArray[iPixel] != 0.0F;

            if (isATarger) {
                proximityArray[iPixel] = 0.0F;
                nearXArray[iPixel] = (short) iPixel;
                nearYArray[iPixel] = (short) iLine;
                continue;
            }

            float NearDistSq = (float) Math.max((double) MaxDist, (double) XSize);

            NearDistSq = NearDistSq * NearDistSq * 2.0F;

            float DistSq = 0.0F;

            /*
             * Are we near(er) to to the closest target to the above (below)
             * pixel?
             */

            if (nearXArray[iPixel] != -1) {
                DistSq = (nearXArray[iPixel] - iPixel) * (nearXArray[iPixel] - iPixel)
                       + (nearYArray[iPixel] - iLine) * (nearYArray[iPixel] - iLine);
                if (DistSq < NearDistSq) {
                    NearDistSq = DistSq;
                } else {
                    nearXArray[iPixel] = (short) -1;
                    nearYArray[iPixel] = (short) -1;
                }
            }

            /*
             * Are we near(er) to to the closest target to the left (right)
             * pixel?
             */

            int iLast = iPixel - iStep;

            if (iPixel != iStart && nearXArray[iLast] != -1) {
                DistSq = (nearXArray[iLast] - iPixel) * (nearXArray[iLast] - iPixel)
                       + (nearYArray[iLast] - iLine) * (nearYArray[iLast] - iLine);
                if (DistSq < NearDistSq) {
                    NearDistSq = DistSq;
                    nearXArray[iPixel] = nearXArray[iLast];
                    nearYArray[iPixel] = nearYArray[iLast];
                }
            }

            /*
             * Are we near(er) to the closest target to the top right (bottom
             * left) pixel?
             */

            int iTarget = iPixel + iStep;

            if (iTarget != iEnd && nearXArray[iTarget] != -1) {
                DistSq = (nearXArray[iTarget] - iPixel) * (nearXArray[iTarget] - iPixel)
                       + (nearYArray[iTarget] - iLine) * (nearYArray[iTarget] - iLine);
                if (DistSq < NearDistSq) {
                    NearDistSq = DistSq;
                    nearXArray[iPixel] = nearXArray[iTarget];
                    nearYArray[iPixel] = nearYArray[iTarget];
                }
            }

            /*
             * Update our proximity value.
             */

            if (nearXArray[iPixel] != -1
                    && NearDistSq <= (MaxDist * MaxDist)
                    && (proximityArray[iPixel] < 0
                        || NearDistSq < (proximityArray[iPixel] * proximityArray[iPixel]))) {
                proximityArray[iPixel] = (float) Math.sqrt((double) NearDistSq);
            }
        }
    }
}
