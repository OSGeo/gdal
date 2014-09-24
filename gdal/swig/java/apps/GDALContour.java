/******************************************************************************
 * $Id: GDALCountuor.java $
 * 
 * Project: GDAL Java applications 
 * Purpose: Contour Generator mainline
 * Author:  Ivan Lucena, ivan.lucena@pmldnet.com, 
 *          translated from gdal_counter.cpp 
 *          originally written by Frank Warmerdam <warmerdam@pobox.com>
 ****************************************************************************** 
 * Copyright (c) 2010, Ivan Lucena
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ****************************************************************************/

import org.gdal.gdal.Band;
import org.gdal.gdal.Dataset;
import org.gdal.gdal.ProgressCallback;
import org.gdal.gdal.gdal;
import org.gdal.gdalconst.gdalconstConstants;
import org.gdal.ogr.DataSource;
import org.gdal.ogr.Driver;
import org.gdal.ogr.FeatureDefn;
import org.gdal.ogr.FieldDefn;
import org.gdal.ogr.Layer;
import org.gdal.ogr.ogr;
import org.gdal.osr.SpatialReference;

public class GDALContour {

    public static void Usage() {
        System.out
                .println(""
                        + "Usage: gdal_contour [-b <band>] [-a <attribute_name>] [-3d] [-inodata]\n"
                        + "                    [-snodata n] [-f <formatname>] [-i <interval>]\n"
                        + "                    [-off <offset>] [-fl <level>]*\n"
                        + "                    [-nln <outlayername>] [-q]\n"
                        + "                    <src_filename> <dst_filename>\n");
        System.exit(-1);
    }

    public static void main(String[] args) {

        String sourceFilename = null;
        String outputFilename = null;
        int sourceBand = 1;
        String attributName = null;
        boolean threeDimension = false;
        boolean ignoreNodata = false;
        boolean hasSourceNodata = false;
        double sourceNodata = 0.0;
        String outputFormat = "ESRI Shapefile";
        double contourInterval = 0.0;
        double offset = 0.0;
        String fixedLevels = null;
        String newLayerName  = "contour";
        boolean quiet = false;
        ProgressCallback progressCallback = null;

        /*
         * Register GDAL and OGR format(s)
         */

        gdal.AllRegister();
        ogr.RegisterAll();

        /*
         * Parse arguments
         */

        args = ogr.GeneralCmdLineProcessor(args);

        if (args.length < 2) {
            Usage();
        }

        for (int i = 0; i < args.length; i++) {

            if (args[i].equals("---utility_version")) {

                System.out
                        .println("Running against GDAL " + gdal.VersionInfo());
                return;

            } else if (args[i].equals("-a") && args.length > i) {

                attributName = args[++i];

            } else if (args[i].equals("-off") && args.length > i) {

                offset = Float.parseFloat(args[++i]);

            } else if (args[i].equals("-i") && args.length > i) {

                contourInterval = Float.parseFloat(args[++i]);

            } else if (args[i].equals("-fl") && args.length > i) {

                if (fixedLevels == null) {
                    fixedLevels = args[++i];
                } else {
                    fixedLevels += ':' + args[++i];
                }

            } else if (args[i].equals("-b") && args.length > i) {

                sourceBand = Integer.parseInt(args[++i]);

            } else if (args[i].equals("-f") && args.length > i) {

                outputFormat = args[++i];

            } else if (args[i].equals("-3d")) {

                threeDimension = true;

            } else if (args[i].equals("-snodata") && args.length > i) {

                hasSourceNodata = true;
                sourceNodata = Float.parseFloat(args[++i]);

            } else if (args[i].equals("-nln") && args.length > i) {

                newLayerName = args[++i];

            } else if (args[i].equals("-inodata")) {

                ignoreNodata = true;

            } else if (args[i].equals("-q") || args[i].equals("-quiet")) {

                quiet = true;

            } else if (sourceFilename == null) {

                sourceFilename = args[i];

            } else if (outputFilename == null) {

                outputFilename = args[i];

            } else {

                Usage();
            }
        }

        if (sourceFilename == null || outputFilename == null) {

            Usage();
        }

        double[] fixedLevelsDouble = null;

        if (fixedLevels != null) {

            String[] fixedLevelsArray = fixedLevels.split(":");
            fixedLevelsDouble = new double[fixedLevelsArray.length];

            for (int i = 0; i < fixedLevelsDouble.length; i++) {

                if (fixedLevelsDouble[i] == 0.0) {

                    Usage();
                }
            }
        }

        /*
         * Open source raster file.
         */

        Dataset dataset = gdal.Open(sourceFilename,
                gdalconstConstants.GA_ReadOnly);

        if (dataset == null) {
            System.err.println("GDALOpen failed - " + gdal.GetLastErrorNo());
            System.err.println(gdal.GetLastErrorMsg());
            System.exit(2);
        }

        Band band = dataset.GetRasterBand(sourceBand);

        if (band == null) {
            System.err.println("Band does not exist on dataset");
            System.err.println("GDALOpen failed - " + gdal.GetLastErrorNo());
            System.err.println(gdal.GetLastErrorMsg());
            System.exit(3);
        }

        if (!hasSourceNodata && !ignoreNodata) {

            Double val[] = new Double[1];

            band.GetNoDataValue(val);

            hasSourceNodata = true;

            if (val[0] != null) {
                sourceNodata = val[0];
            } else {
                hasSourceNodata = false;
            }
        }

        /*
         * Try to get a coordinate system from the raster.
         */

        SpatialReference srs = null;

        String wkt = dataset.GetProjection();

        if (wkt.length() > 0) {

            srs = new SpatialReference(wkt);
        }

        /*
         * Create the outputfile.
         */

        DataSource dataSource = null;
        Driver driver = ogr.GetDriverByName(outputFormat);
        FieldDefn field = null;
        Layer layer = null;

        if (driver == null) {

            System.err.println("Unable to find format driver named "
                    + outputFormat);
            System.exit(10);
        }

        dataSource = driver.CreateDataSource(outputFilename);

        if (dataSource == null) {
            System.exit(1);
        }

        if (threeDimension) {
            layer = dataSource.CreateLayer(newLayerName, srs,
                    ogr.wkbLineString25D);
        } else {
            layer = dataSource
                    .CreateLayer(newLayerName, srs, ogr.wkbLineString);
        }

        if (layer == null) {
            System.exit(1);
        }

        field = new FieldDefn("ID", ogr.OFTInteger);
        field.SetWidth(8);

        layer.CreateField(field, 0);
        field.delete();

        if (attributName != null) {

            field = new FieldDefn(attributName, ogr.OFTReal);
            field.SetWidth(12);
            field.SetPrecision(3);

            layer.CreateField(field, 0);
            layer.delete();
        }

        /*
         * Use terminal progress report
         */

        if (quiet == false) {
            progressCallback = new ProgressCallback();
        }

        /*
         * Invoke.
         */

        FeatureDefn feature = layer.GetLayerDefn();
        
        gdal.ContourGenerate(band, contourInterval, offset, fixedLevelsDouble,
                (ignoreNodata ? 1 : 0), sourceNodata, layer, feature.GetFieldIndex("ID"),
                (attributName != null ? feature.GetFieldIndex(attributName) : -1),
                progressCallback);
        
        dataSource.delete();
        dataset.delete();
    }
}
