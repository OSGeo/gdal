/******************************************************************************
 * $Id: GDALGrid.java $
 * 
 * Project: GDAL Java applications 
 * Purpose: GDAL scattered data gridding (interpolation) tool 
 * Author:  Ivan Lucena, ivan.lucena@pmldnet.com, 
 *          translated from gdal_grid.cpp 
 *          originally written by Andrey Kiselev, dron@ak4719.spb.edu
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

import java.util.ArrayList;
import java.util.List;
import java.nio.ByteBuffer;

import org.gdal.gdal.Band;
import org.gdal.gdal.Dataset;
import org.gdal.gdal.Driver;
import org.gdal.gdal.ProgressCallback;
import org.gdal.gdal.TermProgressCallback;
import org.gdal.gdal.gdal;
import org.gdal.gdalconst.gdalconstConstants;
import org.gdal.ogr.DataSource;
import org.gdal.ogr.Feature;
import org.gdal.ogr.Geometry;
import org.gdal.ogr.Layer;
import org.gdal.ogr.ogr;
import org.gdal.ogr.ogrConstants;
import org.gdal.osr.SpatialReference;

public class GDALGrid {

    public static void Usage() {
        System.out
                .println("Usage: gridcreate [--help-general] [--formats]\n"
                        + "    [-ot {Byte/Int16/UInt16/UInt32/Int32/Float32/Float64/\n"
                        + "            CInt16/CInt32/CFloat32/CFloat64}]\n"
                        + "    [-of format] [-co \"NAME=VALUE\"]\n"
                        + "    [-zfield field_name]\n"
                        + "    [-a_srs srs_def] [-spat xmin ymin xmax ymax]\n"
                        + "    [-clipsrc <xmin ymin xmax ymax>|WKT|datasource|spat_extent]\n"
                        + "    [-clipsrcsql sql_statement] [-clipsrclayer layer]\n"
                        + "    [-clipsrcwhere expression]\n"
                        + "    [-l layername]* [-where expression] [-sql select_statement]\n"
                        + "    [-txe xmin xmax] [-tye ymin ymax] [-outsize xsize ysize]\n"
                        + "    [-a algorithm[:parameter1=value1]*]\n"
                        + "    [-q]\n"
                        + "    <src_datasource> <dst_filename>\n"
                        + "\n"
                        + "Available algorithms and parameters with their's defaults:\n"
                        + "    Inverse distance to a power (default)\n"
                        + "        invdist:power=2.0:smoothing=0.0:radius1=0.0:radius2=0.0:"
                        + "angle=0.0:max_points=0:min_points=0:nodata=0.0\n"
                        + "    Moving average\n"
                        + "        average:radius1=0.0:radius2=0.0:angle=0.0:min_points=0:nodata=0.0\n"
                        + "    Nearest neighbor\n"
                        + "        nearest:radius1=0.0:radius2=0.0:angle=0.0:nodata=0.0\n"
                        + "    Various data metrics\n"
                        + "        <metric name>:radius1=0.0:radius2=0.0:angle=0.0:min_points=0:nodata=0.0\n"
                        + "        possible metrics are:\n"
                        + "            minimum\n"
                                                + "            maximum\n"
                        + "            range\n"
                                                + "            count\n"
                        + "            average_distance\n"
                        + "            average_distance_pts\n");
        System.exit(-1);
    }

    /*
     * Print algorithm and options.
     */
    public static void PrintAlgorithmAndOptions(String algorithmAndOptions) {

        if (algorithmAndOptions == null) {

            System.out
                    .println("Algorithm name: not selected, using default Inverse Distance");
        } else {

            int firstColon = algorithmAndOptions.indexOf(':');
            System.out.println("Algorithm name: "
                    + algorithmAndOptions.substring(0, firstColon));
            System.out.println("Options are \""
                    + algorithmAndOptions.substring(firstColon + 1) + "\"");
        }
    }

    /*
     * ProcessGeometry
     * 
     * Extract point coordinates from the geometry reference and set the Z value
     * as requested. Test whether we are in the clipped region before
     * processing.
     */
    public static void ProcessGeometry(Geometry point, Geometry clipSrc,
            int burnField, double burnValue, List<Double> X, List<Double> Y,
            List<Double> Z) {

        if (clipSrc != null && point.Within(clipSrc) == false)
            return;

        X.add(point.GetX());
        Y.add(point.GetY());

        if (burnField < 0) {

            Z.add(point.GetZ());
        } else {

            Z.add(burnValue);
        }
    }

    /*
     * Process all the features in a layer selection, collecting geometries and
     * burn values.
     */
    public static void ProcessLayer(Layer srcLayer, Dataset dstDS,
            Geometry clipSrc, int sizeX, int sizeY, int bandIndex,
            boolean[] isXExtentSet, boolean[] isYExtentSet, double[] minX,
            double[] maxX, double[] minY, double[] maxY, String burnAttribute,
            int type, String algorithmAndOptions, boolean quiet,
            ProgressCallback progressCallback) {

        /*
         * Get field index, and check.
         */

        int burnFieldIndex = -1;

        if (burnAttribute != null) {

            burnFieldIndex = srcLayer.GetLayerDefn().GetFieldIndex(
                    burnAttribute);

            if (burnFieldIndex == -1) {

                System.out.println("Failed to find field " + burnAttribute
                        + " on layer " + srcLayer.GetLayerDefn().GetName());
                return;
            }
        }

        /*
         * Collect the geometries from this layer, and build list of values to
         * be interpolated.
         */

        Feature feature;

        List<Double> X = new ArrayList<Double>();
        List<Double> Y = new ArrayList<Double>();
        List<Double> Z = new ArrayList<Double>();

        srcLayer.ResetReading();

        while ((feature = srcLayer.GetNextFeature()) != null) {

            Geometry geometry = feature.GetGeometryRef();

            if (geometry != null) {

                int geomtype = geometry.GetGeometryType()
                        & (~ogrConstants.wkb25DBit);

                double burnValue = 0.0;

                if (burnFieldIndex >= 0) {

                    burnValue = feature.GetFieldAsDouble(burnFieldIndex);
                }

                if (geomtype == ogr.wkbMultiPoint) {

                    int geomIndex = 0;
                    int geomCount = geometry.GetGeometryCount();

                    for (geomIndex = 0; geomIndex < geomCount; geomIndex++) {

                        ProcessGeometry(geometry.GetGeometryRef(geomIndex),
                                clipSrc, burnFieldIndex, burnValue, X, Y, Z);
                    }

                } else {

                    ProcessGeometry(geometry, clipSrc, burnFieldIndex,
                            burnValue, X, Y, Z);
                }
            }

            feature.delete();
        }

        if (X.size() == 0) {

            System.out.println("No point geometry found on layer "
                    + srcLayer.GetLayerDefn().GetName() + ", skipping.");
            return;
        }

        /*
         * Compute grid geometry.
         */

        if (isXExtentSet[0] == false) {

            minX[0] = X.get(0);
            maxX[0] = X.get(0);

            for (int i = 1; i < X.size(); i++) {

                if (minX[0] > X.get(i))
                    minX[0] = X.get(i);

                if (maxX[0] < X.get(i))
                    maxX[0] = X.get(i);
            }

            isXExtentSet[0] = true;
        }

        if (isYExtentSet[0] == false) {

            minY[0] = Y.get(0);
            maxY[0] = Y.get(0);

            for (int i = 1; i < Y.size(); i++) {

                if (minY[0] > Y.get(i))
                    minY[0] = Y.get(i);

                if (maxY[0] < Y.get(i))
                    maxY[0] = Y.get(i);
            }

            isYExtentSet[0] = true;
        }

        /*
         * Perform gridding.
         */

        Double deltaX = (maxX[0] - minX[0]) / sizeX;
        Double deltaY = (maxY[0] - minY[0]) / sizeY;

        if (!quiet) {

            System.out.println("Grid data type is "
                    + gdal.GetDataTypeName(type));
            System.out.println("Grid size = (" + sizeX + " " + sizeY + ").");
            System.out.println("Corner coordinates = (" + (minX[0] - deltaX / 2)
                    + " " + (maxY[0] + deltaY / 2) + ") - (" + (maxX[0] + deltaX / 2)
                    + " " + (minY[0] - deltaY / 2) + ")");
            System.out.println("Grid cell size = (" + deltaX + " " + deltaY
                    + ").");
            System.out.println("Source point count = " + X.size() + " .");
            PrintAlgorithmAndOptions(algorithmAndOptions);
        }

        Band band = dstDS.GetRasterBand(bandIndex);

        if (X.size() == 0) {

            Double val[] = new Double[1];

            band.GetNoDataValue(val);

            if (val[0] != null) {
                band.Fill(val[0]);
            } else {
                band.Fill(0.0);
            }

            return;
        }

        int offsetX = 0;
        int offsetY = 0;
        int[] blockXSize = new int[1];
        int[] blockYSize = new int[1];

        band.GetBlockSize(blockXSize, blockYSize);

        int bufferSize = blockXSize[0] * blockYSize[0]
                * gdal.GetDataTypeSize(type) / 8;

        ByteBuffer data = ByteBuffer.allocateDirect(bufferSize);

        int blockIndex = 0;
        int blockCount = ((sizeX + blockXSize[0] - 1) / blockXSize[0])
                * ((sizeY + blockYSize[0] - 1) / blockYSize[0]);

        GDALGridScaledProgress griddingProgress = null;

        for (offsetY = 0; offsetY < sizeY; offsetY += blockYSize[0]) {

            for (offsetX = 0; offsetX < sizeX; offsetX += blockXSize[0]) {

                int requestX = blockXSize[0];

                if (offsetX + requestX > sizeX) {

                    requestX = sizeX - offsetX;
                }

                int requestY = blockYSize[0];

                if (offsetY + requestY > sizeY) {

                    requestY = sizeY - offsetY;
                }

                
                /*
                 * Reformat arguments
                 */

                double[][] points = new double[X.size()][3];
                for (int i = 0; i < X.size(); i++)
                    points[i][0] = X.get(i);
                for (int i = 0; i < Y.size(); i++)
                    points[i][1] = Y.get(i);
                for (int i = 0; i < Z.size(); i++)
                    points[i][2] = Z.get(i);

                /*
                 * Create Scaled progress report
                 */

                if (quiet == false) {
                    griddingProgress = new GDALGridScaledProgress(
                            blockIndex * 1.0 / blockCount, 
                            (blockIndex + 1) * 1.0 / blockCount, 
                            progressCallback);
                }

                /*
                 * Create Grid
                 */
                                
                gdal.GridCreate(algorithmAndOptions, points, minX[0] + deltaX
                        * offsetX, minX[0] + deltaX * (offsetX + requestX), minY[0]
                        + deltaY * offsetY, minY[0] + deltaY
                        * (offsetY + requestY), requestX, requestY, type, data,
                        griddingProgress);

                /*
                 * Write grid to raster output
                 */

                band.WriteRaster_Direct(offsetX, offsetY, requestX, requestY,
                        requestX, requestY, type, data);

                if (quiet == false) {
                    griddingProgress.delete();
                }

                blockIndex++;
            }
        }
    }

    /*
     * LoadGeometry
     * 
     * Read geometries from the given dataset using specified filters and
     * returns a collection of read geometries.
     */
    static Geometry LoadGeometry(String srcDS, String srcSQL, String srcLyr,
            String srcWhere) {

        DataSource DS;
        Layer lyr;
        Feature feat;
        Geometry geom = null;

        DS = ogr.Open(srcDS, false);

        if (DS == null) {

            return null;
        }

        if (srcSQL != null) {

            lyr = DS.ExecuteSQL(srcSQL, null, null);
        } else if (srcLyr != null) {

            lyr = DS.GetLayerByName(srcLyr);
        } else {

            lyr = DS.GetLayer(0);
        }

        if (lyr == null) {

            System.err
                    .println("Failed to identify source layer from datasource.");
            DS.delete();
            return null;
        }

        if (srcWhere != null) {

            lyr.SetAttributeFilter(srcWhere);
        }

        while ((feat = lyr.GetNextFeature()) != null) {

            Geometry srcGeom = feat.GetGeometryRef();

            if (srcGeom != null) {

                int srcType = srcGeom.GetGeometryType()
                        & (~ogrConstants.wkb25DBit);

                if (geom == null) {

                    geom = new Geometry(ogr.wkbMultiPolygon);
                }

                if (srcType == ogr.wkbPolygon) {

                    geom.AddGeometry(srcGeom);
                } else if (srcType == ogr.wkbMultiPolygon) {

                    int geomIndex = 0;
                    int geomCount = srcGeom.GetGeometryCount();

                    for (geomIndex = 0; geomIndex < geomCount; geomIndex++) {

                        geom.AddGeometry(srcGeom.GetGeometryRef(geomIndex));
                    }

                } else {

                    System.err
                            .println("FAILURE: Geometry not of polygon type.");

                    if (srcSQL != null) {

                        DS.ReleaseResultSet(lyr);
                    }

                    DS.delete();

                    return null;
                }
            }
        }

        if (srcSQL != null) {

            DS.ReleaseResultSet(lyr);
        }

        DS.delete();

        return geom;
    }

    public static void main(String[] args) {

        String sourceFilename = null;
        String outputFilename = null;
        String outputFormat = "GTiff";
        String layers = null;
        String burnAttribute = null;
        String where = null;
        int outputType = gdalconstConstants.GDT_Float64;
        String createOptions = null;
        boolean quiet = false;
        double[] minX = new double[1];
        double[] maxX = new double[1];
        double[] minY = new double[1];
        double[] maxY = new double[1];
        int sizeX = 0;
        int sizeY = 0;
        String SQL = null;
        boolean hasClipSrc = false;
        String clipSQL = null;
        String clipLayer = null;
        String clipWhere = null;
        String outputSRS = null;
        String algorithmAndOptions = null;
        Geometry clipSrc = null;
        Geometry spatialFilter = null;
        ProgressCallback progressCallback = null;
        String clipSrcDS = null;
        boolean[] isXExtentSet = new boolean[1];
        boolean[] isYExtentSet = new boolean[1];

        minX[0] = 0.0;
        maxX[0] = 0.0;
        minY[0] = 0.0;
        maxY[0] = 0.0;

        isXExtentSet[0] = false;
        isYExtentSet[0] = false;

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

                System.out.println("Running against GDAL " + gdal.VersionInfo());
                return;

            } else if (args[i].equals("-of") && args.length > i) {

                outputFormat = args[++i];

            } else if (args[i].equals("-q") || args[i].equals("-quiet")) {

                quiet = true;

            } else if (args[i].equals("-ot") && args.length > i) {

                outputType = gdal.GetDataTypeByName(args[++i]);

                if (outputType == gdalconstConstants.GDT_Unknown) {

                    System.err.println("FAILURE: Unknown output pixel type: "
                            + args[i]);
                    Usage();
                }

            } else if (args[i].equals("-txe") && args.length > i + 1) {

                minX[0] = Double.parseDouble(args[++i]);
                maxX[0] = Double.parseDouble(args[++i]);
                isXExtentSet[0] = true;

            } else if (args[i].equals("-tye") && args.length > i + 1) {

                minY[0] = Double.parseDouble(args[++i]);
                maxY[0] = Double.parseDouble(args[++i]);
                isYExtentSet[0] = true;

            } else if (args[i].equals("-outsize") && args.length > i + 1) {

                sizeX = Integer.parseInt(args[++i]);
                sizeY = Integer.parseInt(args[++i]);

            } else if (args[i].equals("-co") && args.length > i) {

                if (createOptions == null) {
                    createOptions = args[++i];
                } else {
                    createOptions += ':' + args[++i];
                }

            } else if (args[i].equals("-zfield") && args.length > i) {

                burnAttribute = args[++i];

            } else if (args[i].equals("-where") && args.length > i) {

                where = args[++i];

            } else if (args[i].equals("-l") && args.length > i) {

                if (layers == null) {
                    layers = args[++i];
                } else {
                    layers += ':' + args[++i];
                }

            } else if (args[i].equals("-sql") && args.length > i) {

                SQL = args[++i];

            } else if (args[i].equals("-spat") && args.length > i + 3) {

                double clipMinX = Double.parseDouble(args[i + 1]);
                double clipMinY = Double.parseDouble(args[i + 2]);
                double clipMaxX = Double.parseDouble(args[i + 3]);
                double clipMaxY = Double.parseDouble(args[i + 4]);
                i += 4;
                clipSrc = new Geometry(ogr.wkbPolygon);
                clipSrc.AddPoint(clipMinX, clipMinY);
                clipSrc.AddPoint(clipMinX, clipMaxY);
                clipSrc.AddPoint(clipMaxX, clipMaxY);
                clipSrc.AddPoint(clipMaxX, clipMinY);
                clipSrc.AddPoint(clipMinX, clipMinY);

            } else if (args[i].equals("-clipsrc") && args.length > i) {

                hasClipSrc = true;

                try {

                    double clipMinX = Double.parseDouble(args[i + 1]);
                    double clipMinY = Double.parseDouble(args[i + 2]);
                    double clipMaxX = Double.parseDouble(args[i + 3]);
                    double clipMaxY = Double.parseDouble(args[i + 4]);
                    i += 4;
                    clipSrc = new Geometry(ogr.wkbPolygon);
                    clipSrc.AddPoint(clipMinX, clipMinY);
                    clipSrc.AddPoint(clipMinX, clipMaxY);
                    clipSrc.AddPoint(clipMaxX, clipMaxY);
                    clipSrc.AddPoint(clipMaxX, clipMinY);
                    clipSrc.AddPoint(clipMinX, clipMinY);

                } catch (NumberFormatException e) {

                    if (args[i].substring(0, 6).equals("POLYGON")
                            || args[i].substring(0, 11).equals("MULTIPOLYGON")) {

                        clipSrc = ogr.CreateGeometryFromWkt(args[++i]);

                    } else if (args[i].equals("spat_extent")) {

                        ++i;

                    } else {

                        clipSrcDS = args[++i];
                    }
                }

            } else if (args[i].equals("-clipsrcsql") && args.length > i) {

                clipSQL = args[++i];

            } else if (args[i].equals("-clipsrclayer") && args.length > i) {

                clipLayer = args[++i];

            } else if (args[i].equals("-clipsrcwhere") && args.length > i) {

                clipLayer = args[++i];

            } else if (args[i].equals("-a_srs") && args.length > i) {

                SpatialReference enteredSRS = new SpatialReference();

                if (enteredSRS.SetFromUserInput(args[i + 1]) != 0) {
                    System.err.println("Failed to process SRS definition: "
                            + args[i + 1]);
                    Usage();
                }
                i++;

                outputSRS = enteredSRS.ExportToWkt();

            } else if (args[i].equals("-a") && args.length > i) {

                if (algorithmAndOptions == null) {
                    algorithmAndOptions = args[++i];
                } else {
                    algorithmAndOptions += ':' + args[++i];
                }

            } else if (args[i].substring(0, 1).equals("-")) {

                System.err.println("FAILURE: Option " + args[i]
                        + "incomplete, or not recognised");
                Usage();

            } else if (sourceFilename == null) {

                sourceFilename = args[i];

            } else if (outputFilename == null) {

                outputFilename = args[i];

            } else {

                Usage();

            }
        }

        if (sourceFilename == null || outputFilename == null
                || (SQL == null && layers == null)) {

            Usage();
        }

        /*
         * Open Input
         */

        if (hasClipSrc && clipSrcDS != null) {

            clipSrc = LoadGeometry(clipSrcDS, clipSQL, clipLayer, clipWhere);

            if (clipSrc == null) {

                System.out.println("FAILURE: cannot load source clip geometry");
                Usage();
            }

        } else if (hasClipSrc && clipSrcDS == null) {

            clipSrc = spatialFilter.Clone();

            if (clipSrc == null) {

                System.out
                        .println("FAILURE: "
                                + "-clipsrc must be used with -spat option or \n"
                                + "a bounding box, WKT string or datasource must be specified");
                Usage();
            }
        }

        /*
         * Find the output driver.
         */

        Driver driver = gdal.GetDriverByName(outputFormat);

        if (driver == null) {

            System.out.println("FAILURE: Output driver '" + outputFormat
                    + "' not recognized.");
            Usage();
        }

        /*
         * Open input datasource.
         */

        DataSource srcDS = ogr.Open(sourceFilename);

        if (srcDS == null) {

            System.out.println("Unable to open input datasource '"
                    + sourceFilename);
            System.out.println(gdal.GetLastErrorMsg());
            System.exit(3);
        }

        /*
         * Create target raster file.
         */

        Dataset dstDS = null;
        String[] layerList = layers.split(":");
        int layerCount = layerList.length;
        int bandCount = layerCount;

        if (SQL != null) {
            bandCount++;
        }

        if (sizeX == 0) {
            sizeX = 256;
        }

        if (sizeY == 0) {
            sizeY = 256;
        }

        String[] optionList = createOptions == null ? null : createOptions
                .split(":");

        dstDS = driver.Create(outputFilename, sizeX, sizeY, 1, outputType,
                optionList);

        if (dstDS == null) {

            System.out.println("Unable to create dataset '" + outputFilename);
            System.out.println(gdal.GetLastErrorMsg());
            System.exit(3);
        }

        /*
         * Use terminal progress report
         */
        if (quiet == false) {

            progressCallback = new TermProgressCallback();
        }

        /*
         * Process SQL request.
         */

        if (SQL != null) {

            Layer srcLayer = srcDS.ExecuteSQL(SQL);

            if (srcLayer != null) {

                ProcessLayer(srcLayer, dstDS, clipSrc, sizeX, sizeY, 1,
                        isXExtentSet, isYExtentSet, minX, maxX, minY, maxY,
                        burnAttribute, outputType, algorithmAndOptions, quiet,
                        progressCallback);
            }
        }

        /*
         * Process each layer.
         */

        for (int i = 0; i < layerList.length; i++) {

            Layer srcLayer = srcDS.GetLayerByName(layerList[i]);

            if (srcLayer == null) {

                System.out.println("Unable to find layer '" + layerList[i]);
                continue;
            }

            if (where != null) {

                if (srcLayer.SetAttributeFilter(where) != gdalconstConstants.CE_None) {
                    break;
                }
            }

            if (spatialFilter != null) {

                srcLayer.SetSpatialFilter(spatialFilter);
            }

            if (outputSRS == null) {

                SpatialReference srs = srcLayer.GetSpatialRef();

                if (srs != null) {

                    outputSRS = srs.ExportToWkt();
                }
            }

            ProcessLayer(srcLayer, dstDS, clipSrc, sizeX, sizeY, i + 1
                    + bandCount - layerCount, isXExtentSet, isYExtentSet, minX,
                    maxX, minY, maxY, burnAttribute, outputType,
                    algorithmAndOptions, quiet, progressCallback);
        }

        /*
         * Apply geotransformation matrix.
         */

        double[] geoTransform = new double[6];

        geoTransform[0] = minX[0];
        geoTransform[1] = (maxX[0] - minX[0]) / sizeX;
        geoTransform[2] = 0.0;
        geoTransform[3] = minY[0];
        geoTransform[4] = 0.0;
        geoTransform[5] = (maxY[0] - minY[0]) / sizeY;

        dstDS.SetGeoTransform(geoTransform);

        /*
         * Apply SRS definition if set.
         */

        if (outputSRS != null) {

            dstDS.SetProjection(outputSRS);
        }

        /*
         * Cleanup.
         */

        srcDS.delete();
        dstDS.delete();

        gdal.GDALDestroyDriverManager();
    }
}

class GDALGridScaledProgress extends ProgressCallback {
    private double pctMin;
    private double pctMax;
    private ProgressCallback mainCbk;

    public GDALGridScaledProgress(double pctMin, double pctMax,
            ProgressCallback mainCbk) {
        this.pctMin = pctMin;
        this.pctMax = pctMax;
        this.mainCbk = mainCbk;
    }

    public int run(double dfComplete, String message) {
        return mainCbk.run(pctMin + dfComplete * (pctMax - pctMin), message);
    }
};
