/* This file contains the Javadoc for all classes */

/**
 * Class gdal is an uninstanciable class providing various utility functions as static methods.
 * In particular, it provides :
 * <ul>
 * <li>gdal.<a href="#AllRegister()">AllRegister()</a> and gdal.<a href="#Open(java.lang.String, int)">Open()</a> methods.
 * <li>bindings for various GDAL algorithms.
 * <li>bindings for some general purpose CPL functions.
 * </ul>
 */
public class gdal:public class gdal

   /**
    * General utility option processing.
    *
    * This function is intended to provide a variety of generic commandline 
    * options for all GDAL commandline utilities.  It takes care of the following
    * commandline options:
    * <p><ul>
    *  <li>--version: report version of GDAL in use.
    *  <li>--license: report GDAL license info.
    *  <li>--formats: report all format drivers configured.
    *  <li>--format [format]: report details of one format driver. 
    *  <li>--optfile filename: expand an option file into the argument list. 
    *  <li>--config key value: set system configuration option. 
    *  <li>--debug [on/off/value]: set debug level.
    *  <li>--mempreload dir: preload directory contents into /vsimem
    *  <li>--help-general: report detailed help on general options. 
    * </ul><p>
    * The typical usage looks something like the following.  Note that the formats
    * should be registered so that the --formats and --format options will work properly.
    * <pre>
    *  public static void main( Strings[] args )
    *  { 
    *    gdal.AllRegister();
    *
    *    args = gdal.GeneralCmdLineProcessor( args, 0 );
    *  }
    * </pre>
    * @param args the argument list array
    * @param options currently unused
    *
    * @return updated argument list array.
    */
public class gdal:public static String[] GeneralCmdLineProcessor(String[] args, int options)

   /**
    * General utility option processing.
    *
    * Same as below with options == 0
    *
    * @see #GeneralCmdLineProcessor(String[] args, int options)
    */
public class gdal:public static String[] GeneralCmdLineProcessor(String[] args)

   /**
    * General utility option processing.
    *
    * Same as below but with arguments as a Vector of strings
    *
    * @return updated argument list as a new Vector of strings
    *
    * @see #GeneralCmdLineProcessor(String[] args, int options)
    */
public class gdal:public static java.util.Vector GeneralCmdLineProcessor(java.util.Vector args, int options)

   /**
    * General utility option processing.
    *
    * Same as below but with arguments as a Vector of strings and options == 0
    *
    * @return updated argument list as a new Vector of strings
    *
    * @see #GeneralCmdLineProcessor(String[] args, int options)
    */
public class gdal:public static java.util.Vector GeneralCmdLineProcessor(java.util.Vector args)


/**
 * Display a debugging message.
 *
 * The category argument is used in conjunction with the CPL_DEBUG
 * environment variable to establish if the message should be displayed.
 * If the CPL_DEBUG environment variable is not set, no debug messages
 * are emitted (use Error(gdalconst.CE_Warning,...) to ensure messages are displayed).
 * If CPL_DEBUG is set, but is an empty string or the word "ON" then all
 * debug messages are shown.  Otherwise only messages whose category appears
 * somewhere within the CPL_DEBUG value are displayed (as determinted by
 * strstr()).
 * <p>
 * Categories are usually an identifier for the subsystem producing the
 * error.  For instance "GDAL" might be used for the GDAL core, and "TIFF"
 * for messages from the TIFF translator.  
 *
 * @param msg_class name of the debugging message category.
 * @param message message to display.
 */ 
public class gdal:public static void Debug(String msg_class, String message)

/**
 * Push a new error handler.
 *
 * This pushes a new error handler on the thread-local error handler
 * stack.  This handler will be used untill removed with gdal.PopErrorHandler().
 *
 * @param callbackName handler function name : "CPLQuietErrorHandler", "CPLDefaultErrorHandler", "CPLLoggingErrorHandler"
 */
public class gdal:public static int PushErrorHandler(String callbackName)

/**
 * Push the quiet error handler.
 *
 * This pushes a new error handler on the thread-local error handler
 * stack.  This handler will be used untill removed with gdal.PopErrorHandler().
 */
public class gdal:public static int PushErrorHandler()

/**
 * Report an error.
 *
 * This function reports an error in a manner that can be hooked
 * and reported appropriate by different applications.
 * <p>
 * The msg_class argument can have the value gdalconst.CE_Warning indicating that the
 * message is an informational warning, gdalconst.CE_Failure indicating that the
 * action failed, but that normal recover mechanisms will be used or
 * CE_Fatal meaning that a fatal error has occured, and that Error()
 * should not return.  
 *
 * The default behaviour of Error() is to report errors to stderr,
 * and to abort() after reporting a gdalconst.CE_Fatal error.  It is expected that
 * some applications will want to supress error reporting, and will want to
 * install a C++ exception, or longjmp() approach to no local fatal error
 * recovery.
 *
 * Regardless of how application error handlers or the default error
 * handler choose to handle an error, the error number, and message will
 * be stored for recovery with gdal.GetLastErrorNo() and gdal.GetLastErrorMsg().
 *
 * @param msg_class one of gdalconst.CE_Warning, gdalconst.CE_Failure or gdalconst.CE_Fatal.
 * @param err_code the error number (CPLE_*) from cpl_error.h.
 * @param msg message to display..
 */
public class gdal:public static void Error(int msg_class, int err_code, String msg)

/**
 * Pop error handler off stack.
 *
 * Discards the current error handler on the error handler stack, and restores 
 * the one in use before the last gdal.PushErrorHandler() call.  This method
 * has no effect if there are no error handlers on the current threads error
 * handler stack. 
 */ 
public class gdal:public static void PopErrorHandler()

/**
 * Erase any traces of previous errors.
 *
 * This is normally used to ensure that an error which has been recovered
 * from does not appear to be still in play with high level functions.
 */
public class gdal:public static void ErrorReset()

/**
 * Apply escaping to string to preserve special characters.
 *
 * @see #EscapeString(String str, int scheme)
 */
public class gdal:public static String EscapeString(byte[] byteArray, int scheme)

/**
 * Apply escaping to string to preserve special characters.
 *
 * This function will "escape" a variety of special characters
 * to make the string suitable to embed within a string constant
 * or to write within a text stream but in a form that can be
 * reconstitued to it's original form.  The escaping will even preserve
 * zero bytes allowing preservation of raw binary data.
 * <ul>
 * <li>gdalconst.CPLES_BackslashQuotable(0): This scheme turns a binary string into 
 * a form suitable to be placed within double quotes as a string constant.
 * The backslash, quote, '\\0' and newline characters are all escaped in 
 * the usual C style. 
 *
 * <li>gdalconst.CPLES_XML(1): This scheme converts the '<', '<' and '&' characters into
 * their XML/HTML equivelent (&gt;, &lt; and &amp;) making a string safe
 * to embed as CDATA within an XML element.  The '\\0' is not escaped and 
 * should not be included in the input.
 *
 * <li>gdalconst.CPLES_URL(2): Everything except alphanumerics and the underscore are 
 * converted to a percent followed by a two digit hex encoding of the character
 * (leading zero supplied if needed).  This is the mechanism used for encoding
 * values to be passed in URLs.
 *
 * <li>gdalconst.CPLES_SQL(3): All single quotes are replaced with two single quotes.  
 * Suitable for use when constructing literal values for SQL commands where
 * the literal will be enclosed in single quotes.
 *
 * <li>gdalconst.CPLES_CSV(4): If the values contains commas, double quotes, or newlines it 
 * placed in double quotes, and double quotes in the value are doubled.
 * Suitable for use when constructing field values for .csv files.  Note that
 * gdal.UnescapeString() currently does not support this format, only 
 * gdal.EscapeString().  See cpl_csv.cpp for csv parsing support.
 * </ul>
 * @param str the string to escape.  
 * @param scheme the encoding scheme to use.  
 *
 * @return an escaped string
 */
public class gdal:public static String EscapeString(String str, int scheme)


/**
 * Fetch the last error number.
 *
 * This is the error number, not the error class.
 *
 * @return the error number of the last error to occur, or gdalconst.CPLE_None (0)
 * if there are no posted errors.
 */
public class gdal:public static int GetLastErrorNo()


/**
 * Fetch the last error type.
 *
 * This is the error class, not the error number.
 *
 * @return the error number of the last error to occur, or gdalconst.CE_None (0)
 * if there are no posted errors.
 */
public class gdal:public static int GetLastErrorType()

/**
 * Get the last error message.
 *
 * Fetches the last error message posted with CPLError(), that hasn't
 * been cleared by gdal.ErrorReset().  The returned pointer is to an internal
 * string that should not be altered or freed.
 *
 * @return the last error message, or null if there is no posted error
 * message.
 */
public class gdal:public static String GetLastErrorMsg()

/**
  * Set a configuration option for GDAL/OGR use.
  *
  * Those options are defined as a (key, value) couple. The value corresponding
  * to a key can be got later with the gdal.GetConfigOption() method.
  * <p>
  * This mechanism is similar to environment variables, but options set with
  * gdal.SetConfigOption() overrides, for gdal.GetConfigOption() point of view,
  * values defined in the environment.
  * <p>
  * If gdal.SetConfigOption() is called several times with the same key, the
  * value provided during the last call will be used.
  * <p>
  * Options can also be passed on the command line of most GDAL utilities
  * with the with '--config KEY VALUE'. For example,
  * ogrinfo --config CPL_DEBUG ON ~/data/test/point.shp
  *
  * @param key the key of the option
  * @param value the value of the option
  *
  * @see #GetConfigOption
  */
public class gdal:public static void SetConfigOption(String key, String value)

/**
  * Get the value of a configuration option.
  * 
  * The value is the value of a (key, value) option set with gdal.SetConfigOption().
  * If the given option was no defined with gdal.SetConfigOption(), it tries to find
  * it in environment variables.
  *
  * @param key the key of the option to retrieve
  * @param defaultValue a default value if the key does not match existing defined options (may be null)
  * @return the value associated to the key, or the default value if not found
  *
  * @see #SetConfigOption
  */
public class gdal:public static String GetConfigOption(String key, String defaultValue)


/**
  * Get the value of a configuration option.
  * 
  * Same as below with defaultValue == null
  *
  * @see #GetConfigOption(String key, String defaultValue)
  */
public class gdal:public static String GetConfigOption(String key)


/**
 * Generate Geotransform from GCPs. 
 *
 * Given a set of GCPs perform first order fit as a geotransform. 
 * <p>
 * Due to imprecision in the calculations the fit algorithm will often 
 * return non-zero rotational coefficients even if given perfectly non-rotated
 * inputs.  A special case has been implemented for corner corner coordinates
 * given in TL, TR, BR, BL order.  So when using this to get a geotransform
 * from 4 corner coordinates, pass them in this order. 
 * 
 * @param gcpArray the array of GCP. 
 * @param outGeoTransform the six double array in which the affine 
 * geotransformation will be returned. 
 * @param bApproxOK If 0 the function will fail if the geotransform is not 
 * essentially an exact fit (within 0.25 pixel) for all GCPs. 
 * 
 * @return 1 on success or 0 if there aren't enough points to prepare a
 * geotransform, the pointers are ill-determined or if bApproxOK is 0 
 * and the fit is poor.
 */
public class gdal:public static int GCPsToGeoTransform(GCP[] gcpArray, double[] outGeoTransform, int bApproxOK)

/**
 * Generate Geotransform from GCPs. 
 *
 * Same as below with bApproxOK == 0
 *
 * @see #GCPsToGeoTransform(GCP[] gcpArray, double[] outGeoTransform, int bApproxOK)
 */
public class gdal:public static int GCPsToGeoTransform(GCP[] gcpArray, double[] outGeoTransform)


/**
 * Compute optimal PCT for RGB image.
 *
 * This function implements a median cut algorithm to compute an "optimal"
 * pseudocolor table for representing an input RGB image.  This PCT could
 * then be used with GDALDitherRGB2PCT() to convert a 24bit RGB image into
 * an eightbit pseudo-colored image. 
 * <p>
 * This code was based on the tiffmedian.c code from libtiff (www.libtiff.org)
 * which was based on a paper by Paul Heckbert:
 * <p>
 * <pre>
 *   "Color  Image Quantization for Frame Buffer Display", Paul
 *   Heckbert, SIGGRAPH proceedings, 1982, pp. 297-307.
 * </pre>
 * <p>
 * The red, green and blue input bands do not necessarily need to come
 * from the same file, but they must be the same width and height.  They will
 * be clipped to 8bit during reading, so non-eight bit bands are generally
 * inappropriate. 
 *
 * @param red Red input band. 
 * @param green Green input band. 
 * @param blue Blue input band. 
 * @param num_colors the desired number of colors to be returned (2-256).
 * @param colors the color table will be returned in this color table object.
 * @param callback for reporting algorithm progress. May be null
 *
 * @return returns gdalconst.CE_None on success or gdalconst.CE_Failure if an error occurs. 
 */
public class gdal:public static int ComputeMedianCutPCT(Band red, Band green, Band blue, int num_colors, ColorTable colors, ProgressCallback callback)


/**
 * Compute optimal PCT for RGB image.
 *
 * Same as below with callback == null
 *
 * @see #ComputeMedianCutPCT(Band red, Band green, Band blue, int num_colors, ColorTable colors, ProgressCallback callback)
 */
public class gdal:public static int ComputeMedianCutPCT(Band red, Band green, Band blue, int num_colors, ColorTable colors)


/**
 * 24bit to 8bit conversion with dithering.
 *
 * This functions utilizes Floyd-Steinberg dithering in the process of 
 * converting a 24bit RGB image into a pseudocolored 8bit image using a
 * provided color table.  
 * <p>
 * The red, green and blue input bands do not necessarily need to come
 * from the same file, but they must be the same width and height.  They will
 * be clipped to 8bit during reading, so non-eight bit bands are generally
 * inappropriate.  Likewise the hTarget band will be written with 8bit values
 * and must match the width and height of the source bands. 
 * <p>
 * The color table cannot have more than 256 entries.
 *
 * @param red Red input band. 
 * @param green Green input band. 
 * @param blue Blue input band. 
 * @param target Output band. 
 * @param colors the color table to use with the output band. 
 * @param callback for reporting algorithm progress. May be null
 *
 * @return gdalconst.CE_None on success or gdalconst.CE_Failure if an error occurs. 
 */
public class gdal:public static int DitherRGB2PCT(Band red, Band green, Band blue, Band target, ColorTable colors, ProgressCallback callback)

/**
 * 24bit to 8bit conversion with dithering.
 *
 * Same as below with callback == null
 * @see #DitherRGB2PCT(Band red, Band green, Band blue, Band target, ColorTable colors, ProgressCallback callback)
 */
public class gdal:public static int DitherRGB2PCT(Band red, Band green, Band blue, Band target, ColorTable colors)

/**
 * Reproject image.
 *
 * This is a convenience function utilizing the GDALWarpOperation class to
 * reproject an image from a source to a destination.  In particular, this
 * function takes care of establishing the transformation function to
 * implement the reprojection, and will default a variety of other 
 * warp options. 
 * <p>
 * By default all bands are transferred, with no masking or nodata values
 * in effect.  No metadata, projection info, or color tables are transferred 
 * to the output file. 
 *
 * @param src_ds the source image file. 
 * @param dst_ds the destination image file. 
 * @param src_wkt the source projection.  If null the source projection
 * is read from from src_ds.
 * @param dst_wkt the destination projection.  If null the destination
 * projection will be read from dst_ds.
 * @param resampleAlg the type of resampling to use. (among gdalconst.GRA_*)
 * @param warpMemoryLimit the amount of memory (in bytes) that the warp
 * API is allowed to use for caching.  This is in addition to the memory
 * already allocated to the GDAL caching (as per gdal.SetCacheMax()).  May be
 * 0.0 to use default memory settings.
 * @param maxError maximum error measured in input pixels that is allowed
 * in approximating the transformation (0.0 for exact calculations).
 * @param callback for reporting progress or null
 *
 * @return gdalconst.CE_None on success or gdalconst.CE_Failure if something goes wrong.
 */
public class gdal:public static int ReprojectImage(Dataset src_ds, Dataset dst_ds, String src_wkt, String dst_wkt, int resampleAlg, double warpMemoryLimit, double maxError, ProgressCallback callback)

/**
 * Reproject image.
 *
 * Same as below with callback == null.
 * 
 * @see #ReprojectImage(Dataset src_ds, Dataset dst_ds, String src_wkt, String dst_wkt, int resampleAlg, double warpMemoryLimit, double maxError, ProgressCallback callback)
 *
 * @return gdalconst.CE_None on success or gdalconst.CE_Failure if something goes wrong.
 */
public class gdal:public static int ReprojectImage(Dataset src_ds, Dataset dst_ds, String src_wkt, String dst_wkt, int resampleAlg, double warpMemoryLimit, double maxError)

/**
 * Reproject image.
 *
 * Same as below with maxError == 0.0 and callback == null.
 * 
 * @see #ReprojectImage(Dataset src_ds, Dataset dst_ds, String src_wkt, String dst_wkt, int resampleAlg, double warpMemoryLimit, double maxError, ProgressCallback callback)
 */
public class gdal:public static int ReprojectImage(Dataset src_ds, Dataset dst_ds, String src_wkt, String dst_wkt, int resampleAlg, double warpMemoryLimit)

/**
 * Reproject image.
 *
 * Same as below with warpMemoryLimit == 0.0, maxError == 0.0 and callback == null.
 * 
 * @see #ReprojectImage(Dataset src_ds, Dataset dst_ds, String src_wkt, String dst_wkt, int resampleAlg, double warpMemoryLimit, double maxError, ProgressCallback callback)
 */
public class gdal:public static int ReprojectImage(Dataset src_ds, Dataset dst_ds, String src_wkt, String dst_wkt, int resampleAlg)

/**
 * Reproject image.
 *
 * Same as below with resampleAlg == gdalconst.GRA_NearestNeighbour, warpMemoryLimit == 0.0, maxError == 0.0 and callback == null.
 * 
 * @see #ReprojectImage(Dataset src_ds, Dataset dst_ds, String src_wkt, String dst_wkt, int resampleAlg, double warpMemoryLimit, double maxError, ProgressCallback callback)
 */
public class gdal:public static int ReprojectImage(Dataset src_ds, Dataset dst_ds, String src_wkt, String dst_wkt)

/**
 * Reproject image.
 *
 * Same as below with dst_wkt == null, resampleAlg == gdalconst.GRA_NearestNeighbour, warpMemoryLimit == 0.0, maxError == 0.0 and callback == null.
 * 
 * @see #ReprojectImage(Dataset src_ds, Dataset dst_ds, String src_wkt, String dst_wkt, int resampleAlg, double warpMemoryLimit, double maxError, ProgressCallback callback)
 */
public class gdal:public static int ReprojectImage(Dataset src_ds, Dataset dst_ds, String src_wkt)

/**
 * Reproject image.
 *
 * Same as below with src_wkt == null, dst_wkt == null, resampleAlg == gdalconst.GRA_NearestNeighbour, warpMemoryLimit == 0.0, maxError == 0.0 and callback == null.
 * 
 * @see #ReprojectImage(Dataset src_ds, Dataset dst_ds, String src_wkt, String dst_wkt, int resampleAlg, double warpMemoryLimit, double maxError, ProgressCallback callback)
 */
public class gdal:public static int ReprojectImage(Dataset src_ds, Dataset dst_ds)

/**
 * Compute the proximity of all pixels in the image to a set of pixels in the source image.
 *
 * The following options are used to define the behavior of the function.  By
 * default all non-zero pixels in srcBand will be considered the
 * "target", and all proximities will be computed in pixels.  Note
 * that target pixels are set to the value corresponding to a distance
 * of zero.
 * <p>
 * Options:
 * <dl>
 * <dt>VALUES=n[,n]*</dt> <dd>
 * A list of target pixel values to measure the distance from.  If this
 * option is not provided proximity will be computed from non-zero
 * pixel values.  Currently pixel values are internally processed as
 * integers.</dd>
 * <dt>DISTUNITS=[PIXEL]/GEO</dt> <dd>
 * Indicates whether distances will be computed in pixel units or
 * in georeferenced units.  The default is pixel units.  This also 
 * determines the interpretation of MAXDIST.</dd>
 * <dt>MAXDIST=n</dt> <dd>
 * The maximum distance to search.  Proximity distances greater than
 * this value will not be computed.  Instead output pixels will be
 * set to a nodata value.</dd>
 * <dt>NODATA=n</dt> <dd>
 * The NODATA value to use on the output band for pixels that are
 * beyond MAXDIST.  If not provided, the hProximityBand will be
 * queried for a nodata value.  If one is not found, 65535 will be used.</dd>
 * <dt>FIXED_BUF_VAL=n</dt> <dd>
 * If this option is set, all pixels within the MAXDIST threadhold are
 * set to this fixed value instead of to a proximity distance.</dd>
 * </dl>
 * @param srcBand the source band
 * @param proximityBand the destination band
 * @param options a vector of strings with the above options
 * @param callback for reporting progress or null
 *
 * @return gdalconst.CE_None on success or gdalconst.CE_Failure if something goes wrong.
 */
public class gdal:public static int ComputeProximity(Band srcBand, Band proximityBand, java.util.Vector options, ProgressCallback callback)

/**
 * Compute the proximity of all pixels in the image to a set of pixels in the source image.
 *
 * Same as below with callback = null
 *
 * @see #ComputeProximity(Band srcBand, Band proximityBand, java.util.Vector options, ProgressCallback callback)
 *
 */
public class gdal:public static int ComputeProximity(Band srcBand, Band proximityBand, java.util.Vector options)

/**
 * Compute the proximity of all pixels in the image to a set of pixels in the source image.
 *
 * Same as below with options == null and callback == null
 *
 * @see #ComputeProximity(Band srcBand, Band proximityBand, java.util.Vector options, ProgressCallback callback)
 *
 */
public class gdal:public static int ComputeProximity(Band srcBand, Band proximityBand)

/**
 * Burn geometries from the specified layer into raster.
 *
 * Rasterize all the geometric objects from a layer into a raster
 * dataset.
 * <p>
 * The transform
 * needs to transform the geometry locations into pixel/line coordinates
 * on the raster dataset.
 * <p>
 * The output raster may be of any GDAL supported datatype, though currently
 * internally the burning is done either as gdal.GDT_Byte or gdal.GDT_Float32.  This
 * may be improved in the future.  An explicit list of burn values for
 * each layer for each band must be passed in. 
 *
 * @param dataset output data, must be opened in update mode.
 * @param bandNumbers the list of bands to be updated. 
 * @param layer the layer to burn in. 
 * @param burn_values the array of values to burn into the raster.  
 * There should be as many values as in bandNumbers. If null, 255 will be used
 * @param options a vector of strings for special options controlling rasterization:
 * <dl>
 * <dt>"ATTRIBUTE":</dt> <dd>Identifies an attribute field on the features to be
 * used for a burn in value. The value will be burned into all output
 * bands. If specified, burn_values will not be used and can be a null value.</dd>
 * <dt>"CHUNKYSIZE":</dt> <dd>The height in lines of the chunk to operate on.
 * The larger the chunk size the less times we need to make a pass through all
 * the shapes. If it is not set or set to zero the default chunk size will be
 * used. Default size will be estimated based on the GDAL cache buffer size
 * using formula: cache_size_bytes/scanline_size_bytes, so the chunk will
 * not exceed the cache.</dd>
 * </dl>
 * @param callback for reporting progress or null
 *
 * @return gdalconst.CE_None on success or gdalconst.CE_Failure on error.
 */

public class gdal:public static int RasterizeLayer(Dataset dataset, int[] bandNumbers, org.gdal.ogr.Layer layer, double[] burn_values, java.util.Vector options, ProgressCallback callback)

/**
 * Burn geometries from the specified layer into raster.
 *
 * Same as below with callback == null
 *
 * @see #RasterizeLayer(Dataset dataset, int[] bandNumbers, org.gdal.ogr.Layer layer, double[] burn_values, java.util.Vector options, ProgressCallback callback)
 *
 */
public class gdal:public static int RasterizeLayer(Dataset dataset, int[] bandNumbers, org.gdal.ogr.Layer layer, double[] burn_values, java.util.Vector options)


/**
 * Burn geometries from the specified layer into raster.
 *
 * Same as below with options == null and callback == null
 *
 * @see #RasterizeLayer(Dataset dataset, int[] bandNumbers, org.gdal.ogr.Layer layer, double[] burn_values, java.util.Vector options, ProgressCallback callback)
 *
 */
public class gdal:public static int RasterizeLayer(Dataset dataset, int[] bandNumbers, org.gdal.ogr.Layer layer, double[] burn_values)


/**
 * Burn geometries from the specified layer into raster.
 *
 * Same as below with burn_values == null, options == null and callback == null
 *
 * @see #RasterizeLayer(Dataset dataset, int[] bandNumbers, org.gdal.ogr.Layer layer, double[] burn_values, java.util.Vector options, ProgressCallback callback)
 *
 */
public class gdal:public static int RasterizeLayer(Dataset dataset, int[] bandNumbers, org.gdal.ogr.Layer layer)

/**
 * Create polygon coverage from raster data.
 *
 * This function creates vector polygons for all connected regions of pixels in
 * the raster sharing a common pixel value.  Optionally each polygon may be
 * labelled with the pixel value in an attribute.  Optionally a mask band
 * can be provided to determine which pixels are eligible for processing.
 * <p>
 * Note that currently the source pixel band values are read into a
 * signed 32bit integer buffer (Int32), so floating point or complex 
 * bands will be implicitly truncated before processing.  
 * <p>
 * Polygon features will be created on the output layer, with polygon 
 * geometries representing the polygons.  The polygon geometries will be
 * in the georeferenced coordinate system of the image (based on the
 * geotransform of the source dataset).  It is acceptable for the output
 * layer to already have features.  Note that gdal.Polygonize() does not
 * set the coordinate system on the output layer.  Application code should
 * do this when the layer is created, presumably matching the raster 
 * coordinate system. 
 * <p>
 * The algorithm used attempts to minimize memory use so that very large
 * rasters can be processed.  However, if the raster has many polygons 
 * or very large/complex polygons, the memory use for holding polygon 
 * enumerations and active polygon geometries may grow to be quite large. 
 * <p>
 * The algorithm will generally produce very dense polygon geometries, with
 * edges that follow exactly on pixel boundaries for all non-interior pixels.
 * For non-thematic raster data (such as satellite images) the result will
 * essentially be one small polygon per pixel, and memory and output layer
 * sizes will be substantial.  The algorithm is primarily intended for 
 * relatively simple thematic imagery, masks, and classification results. 
 * 
 * @param srcBand the source raster band to be processed.
 * @param maskBand an optional mask band (or null).  All pixels in the mask band with a 
 * value other than zero will be considered suitable for collection as 
 * polygons.  
 * @param outLayer the vector feature layer to which the polygons should
 * be written. 
 * @param iPixValField the attribute field index indicating the feature
 * attribute into which the pixel value of the polygon should be written.
 * @param options a name/value list of additional options (none currently
 * supported. just pass null). 
 * @param callback for reporting progress or null
 * 
 * @return gdalconst.CE_None on success or gdalconst.CE_Failure on a failure.
 */
public class gdal:public static int Polygonize(Band srcBand, Band maskBand, org.gdal.ogr.Layer outLayer, int iPixValField, java.util.Vector options, ProgressCallback callback)

/**
 * Create polygon coverage from raster data.
 *
 * Same as below with callback == null
 *
 * @see #Polygonize(Band srcBand, Band maskBand, org.gdal.ogr.Layer outLayer, int iPixValField, java.util.Vector options, ProgressCallback callback)
 *
 */
public class gdal:public static int Polygonize(Band srcBand, Band maskBand, org.gdal.ogr.Layer outLayer, int iPixValField, java.util.Vector options)

/**
 * Create polygon coverage from raster data.
 *
 * Same as below with options == null and callback == null
 *
 * @see #Polygonize(Band srcBand, Band maskBand, org.gdal.ogr.Layer outLayer, int iPixValField, java.util.Vector options, ProgressCallback callback)
 *
 */
public class gdal:public static int Polygonize(Band srcBand, Band maskBand, org.gdal.ogr.Layer outLayer, int iPixValField)

/**
 * Fill selected raster regions by interpolation from the edges.
 *
 * This algorithm will interpolate values for all designated 
 * nodata pixels (marked by zeros in maskBand).  For each pixel
 * a four direction conic search is done to find values to interpolate
 * from (using inverse distance weighting).  Once all values are
 * interpolated, zero or more smoothing iterations (3x3 average
 * filters on interpolated pixels) are applied to smooth out 
 * artifacts. 
 * <p>
 * This algorithm is generally suitable for interpolating missing
 * regions of fairly continuously varying rasters (such as elevation
 * models for instance).  It is also suitable for filling small holes
 * and cracks in more irregularly varying images (like airphotos).  It
 * is generally not so great for interpolating a raster from sparse 
 * point data - see the algorithms defined in gdal_grid.h for that case.
 *
 * @param targetBand the raster band to be modified in place. 
 * @param maskBand a mask band indicating pixels to be interpolated (zero valued
 * @param maxSearchDist the maximum number of pixels to search in all 
 * directions to find values to interpolate from.
 * @param smoothingIterations the number of 3x3 smoothing filter passes to 
 * run (0 or more).
 * @param options additional name=value options in a string list (none 
 * supported at this time - just pass null).
 * @param callback for reporting progress or null
 * 
 * @return gdalconst.CE_None on success or gdalconst.CE_Failure on a failure.
 */
public class gdal:public static int FillNodata(Band targetBand, Band maskBand, double maxSearchDist, int smoothingIterations, java.util.Vector options, ProgressCallback callback)

/**
 * Fill selected raster regions by interpolation from the edges.
 *
 * Same as below with callback == null
 *
 * @see #FillNodata(Band targetBand, Band maskBand, double maxSearchDist, int smoothingIterations, java.util.Vector options, ProgressCallback callback)
 *
 */
public class gdal:public static int FillNodata(Band targetBand, Band maskBand, double maxSearchDist, int smoothingIterations, java.util.Vector options)

/**
 * Fill selected raster regions by interpolation from the edges.
 *
 * Same as below with options == null and callback == null
 *
 * @see #FillNodata(Band targetBand, Band maskBand, double maxSearchDist, int smoothingIterations, java.util.Vector options, ProgressCallback callback)
 *
 */
public class gdal:public static int FillNodata(Band targetBand, Band maskBand, double maxSearchDist, int smoothingIterations)

/** 
 * Removes small raster polygons. 
 *
 * The function removes raster polygons smaller than a provided
 * threshold size (in pixels) and replaces replaces them with the pixel value 
 * of the largest neighbour polygon.  
 * <p>
 * Polygon are determined (per GDALRasterPolygonEnumerator) as regions of
 * the raster where the pixels all have the same value, and that are contiguous
 * (connected).  
 * <p>
 * Pixels determined to be "nodata" per maskBand will not be treated as part
 * of a polygon regardless of their pixel values.  Nodata areas will never be
 * changed nor affect polygon sizes. 
 * <p>
 * Polygons smaller than the threshold with no neighbours that are as large
 * as the threshold will not be altered.  Polygons surrounded by nodata areas
 * will therefore not be altered.  
 * <p>
 * The algorithm makes three passes over the input file to enumerate the
 * polygons and collect limited information about them.  Memory use is 
 * proportional to the number of polygons (roughly 24 bytes per polygon), but
 * is not directly related to the size of the raster.  So very large raster
 * files can be processed effectively if there aren't too many polygons.  But
 * extremely noisy rasters with many one pixel polygons will end up being 
 * expensive (in memory) to process.
 * 
 * @param srcBand the source raster band to be processed.
 * @param maskBand an optional mask band.  All pixels in the mask band with a 
 * value other than zero will be considered suitable for inclusion in polygons.
 * @param dstBand the output raster band.  It may be the same as srcBand
 * to update the source in place. 
 * @param threshold raster polygons with sizes smaller than this will
 * be merged into their largest neighbour.
 * @param connectedness either 4 indicating that diagonal pixels are not
 * considered directly adjacent for polygon membership purposes or 8
 * indicating they are. 
 * @param options algorithm options in name=value list form.  None currently
 * supported. just pass null
 * @param callback for reporting progress or null
 * 
 * @return gdalconst.CE_None on success or gdalconst.CE_Failure on a failure.
 */
public class gdal:public static int SieveFilter(Band srcBand, Band maskBand, Band dstBand, int threshold, int connectedness, java.util.Vector options, ProgressCallback callback)

/**
 * Removes small raster polygons. 
 *
 * Same as below with callback == null
 *
 * @see #SieveFilter(Band srcBand, Band maskBand, Band dstBand, int threshold, int connectedness, java.util.Vector options, ProgressCallback callback)
 *
 */
public class gdal:public static int SieveFilter(Band srcBand, Band maskBand, Band dstBand, int threshold, int connectedness, java.util.Vector options)

/**
 * Removes small raster polygons. 
 *
 * Same as below with options == null and callback == null
 *
 * @see #SieveFilter(Band srcBand, Band maskBand, Band dstBand, int threshold, int connectedness, java.util.Vector options, ProgressCallback callback)
 *
 */
public class gdal:public static int SieveFilter(Band srcBand, Band maskBand, Band dstBand, int threshold, int connectedness)

/**
 * Removes small raster polygons. 
 *
 * Same as below with connectedness == 4, options == null and callback == null
 *
 * @see #SieveFilter(Band srcBand, Band maskBand, Band dstBand, int threshold, int connectedness, java.util.Vector options, ProgressCallback callback)
 *
 */
public class gdal:public static int SieveFilter(Band srcBand, Band maskBand, Band dstBand, int threshold)

/**
 * Generate downsampled overviews.
 *
 * This function will generate one or more overview images from a base
 * image using the requested downsampling algorithm.  It's primary use
 * is for generating overviews via GDALDataset::BuildOverviews(), but it
 * can also be used to generate downsampled images in one file from another
 * outside the overview architecture.
 * <p>
 * The output bands need to exist in advance. 
 * <p>
 * The full set of resampling algorithms is documented in 
 * Dataset.BuildOverviews().
 *
 * @param srcBand the source (base level) band. 
 * @param overviewBands the list of downsampled bands to be generated.
 * @param resampling Resampling algorithm (eg. "AVERAGE"). 
 * @param callback for reporting progress or null
 *
 * @return gdalconst.CE_None on success or gdalconst.CE_Failure on a failure.
 */
public class gdal:public static int RegenerateOverviews(Band srcBand, Band[] overviewBands, String resampling, ProgressCallback callback)

/**
 * Generate downsampled overviews.
 *
 * Same as below with callback == null
 *
 * @see #RegenerateOverviews(Band srcBand, Band[] overviewBands, String resampling, ProgressCallback callback)
 *
 */
public class gdal:public static int RegenerateOverviews(Band srcBand, Band[] overviewBands, String resampling)

/**
 * Generate downsampled overviews.
 *
 * Same as below with resampling == "AVERAGE" and callback == null
 *
 * @see #RegenerateOverviews(Band srcBand, Band[] overviewBands, String resampling, ProgressCallback callback)
 *
 */
public class gdal:public static int RegenerateOverviews(Band srcBand, Band[] overviewBands)

/**
 * Generate downsampled overview.
 *
 * Same as below for a unique overview band
 *
 * @see #RegenerateOverviews(Band srcBand, Band[] overviewBands, String resampling, ProgressCallback callback)
 *
 */
public class gdal:public static int RegenerateOverview(Band srcBand, Band overviewBand, String resampling, ProgressCallback callback)

/**
 * Generate downsampled overview.
 *
 * Same as below for a unique overview band and callback == null
 *
 * @see #RegenerateOverviews(Band srcBand, Band[] overviewBands, String resampling, ProgressCallback callback)
 *
 */
public class gdal:public static int RegenerateOverview(Band srcBand, Band overviewBand, String resampling)

/**
 * Generate downsampled overview.
 *
 * Same as below for a unique overview band, resampling == "AVERAGE" and callback == null
 *
 * @see #RegenerateOverviews(Band srcBand, Band[] overviewBands, String resampling, ProgressCallback callback)
 *
 */
public class gdal:public static int RegenerateOverview(Band srcBand, Band overviewBand)


/**
 * Create virtual warped dataset automatically.
 *
 * This function will create a warped virtual file representing the 
 * input image warped into the target coordinate system.  A GenImgProj
 * transformation is created to accomplish any required GCP/Geotransform
 * warp and reprojection to the target coordinate system.  The output virtual
 * dataset will be "northup" in the target coordinate system.   The
 * GDALSuggestedWarpOutput() function is used to determine the bounds and
 * resolution of the output virtual file which should be large enough to 
 * include all the input image 
 * <p>
 * Note that the constructed Dataset object will acquire one or more references 
 * to the passed in src_ds.  Reference counting semantics on the source 
 * dataset should be honoured.  That is, don't just GDALClose() it unless it 
 * was opened with GDALOpenShared(). 
 * <p>
 * The returned dataset will have no associated filename for itself.  If you
 * want to write the virtual dataset description to a file, use the
 * SetDescription() method on the dataset
 * to assign a filename before it is closed.  
 *
 * @param src_ds The source dataset. 
 *
 * @param src_wkt The coordinate system of the source image.  If null, it 
 * will be read from the source image. 
 *
 * @param dst_wkt The coordinate system to convert to.  If null no change 
 * of coordinate system will take place.  
 *
 * @param eResampleAlg One of gdalconst.GRA_NearestNeighbour, gdalconst.GRA_Bilinear, gdalconst.GRA_Cubic or 
 * gdalconst.RA_CubicSpline.  Controls the sampling method used. 
 *
 * @param maxError Maximum error measured in input pixels that is allowed in 
 * approximating the transformation (0.0 for exact calculations).
 *
 * @return null on failure, or a new virtual dataset handle on success.
 */
public class gdal:public static Dataset AutoCreateWarpedVRT(Dataset src_ds, String src_wkt, String dst_wkt, int eResampleAlg, double maxError)

/**
 * Create virtual warped dataset automatically.
 *
 * Same as below with maxError == 0.0
 *
 * @see #AutoCreateWarpedVRT(Dataset src_ds, String src_wkt, String dst_wkt, int eResampleAlg, double maxError)
 *
 */
public class gdal:public static Dataset AutoCreateWarpedVRT(Dataset src_ds, String src_wkt, String dst_wkt, int eResampleAlg)

/**
 * Create virtual warped dataset automatically.
 *
 * Same as below with eResampleAlg == gdalconst.GRA_NearestNeighbour and maxError == 0.0
 *
 * @see #AutoCreateWarpedVRT(Dataset src_ds, String src_wkt, String dst_wkt, int eResampleAlg, double maxError)
 *
 */
public class gdal:public static Dataset AutoCreateWarpedVRT(Dataset src_ds, String src_wkt, String dst_wkt)

/**
 * Create virtual warped dataset automatically.
 *
 * Same as below with dst_wkt == null, eResampleAlg == gdalconst.GRA_NearestNeighbour and maxError == 0.0
 *
 * @see #AutoCreateWarpedVRT(Dataset src_ds, String src_wkt, String dst_wkt, int eResampleAlg, double maxError)
 *
 */
public class gdal:public static Dataset AutoCreateWarpedVRT(Dataset src_ds, String src_wkt)

/**
 * Create virtual warped dataset automatically.
 *
 * Same as below with src_wkt == null, dst_wkt == null, eResampleAlg == gdalconst.GRA_NearestNeighbour and maxError == 0.0
 *
 * @see #AutoCreateWarpedVRT(Dataset src_ds, String src_wkt, String dst_wkt, int eResampleAlg, double maxError)
 *
 */
public class gdal:public static Dataset AutoCreateWarpedVRT(Dataset src_ds)

/**
 * Get runtime version information.
 *
 * Available request values:
 * <ul>
 * <li> "VERSION_NUM": Returns GDAL_VERSION_NUM formatted as a string.  ie. "1170"
 * <li> "RELEASE_DATE": Returns GDAL_RELEASE_DATE formatted as a string.  
 * ie. "20020416".
 * <li> "RELEASE_NAME": Returns the GDAL_RELEASE_NAME. ie. "1.1.7"
 * <li> "--version": Returns one line version message suitable for use in 
 * response to --version requests.  ie. "GDAL 1.1.7, released 2002/04/16"
 * <li> "LICENCE": Returns the content of the LICENSE.TXT file from the GDAL_DATA directory.
 * </ul>
 *
 * @param request the type of version info desired, as listed above.
 *
 * @return a string containing the requested information.
 */
public class gdal:public static String VersionInfo(String request)

/**
 * Get runtime version information.
 *
 * @return a string containing GDAL_VERSION_NUM formatted as a string.  ie. "1170"
 */
public class gdal:public static String VersionInfo()


/**
 * Register all known configured GDAL drivers.
 *
 * This function will drive any of the following that are configured into
 * GDAL.  Many others as well that haven't been updated in this
 * documentation (see <a href="http://gdal.org/formats_list.html">full list</a>):
 * <p>
 * <ul>
 * <li> GeoTIFF (GTiff)
 * <li> Geosoft GXF (GXF)
 * <li> Erdas Imagine (HFA)
 * <li> CEOS (CEOS)
 * <li> ELAS (ELAS)
 * <li> Arc/Info Binary Grid (AIGrid)
 * <li> SDTS Raster DEM (SDTS)
 * <li> OGDI (OGDI)
 * <li> ESRI Labelled BIL (EHdr)
 * <li> PCI .aux Labelled Raw Raster (PAux)
 * <li> HDF4 Hierachal Data Format Release 4
 * <li> HDF5 Hierachal Data Format Release 5
 * <li> GSAG Golden Software ASCII Grid
 * <li> GSBG Golden Software Binary Grid
 * </ul>
 * <p>
 * This function should generally be called once at the beginning of the application.
 */
public class gdal:public static void AllRegister()

/**
 * Get maximum cache memory.
 *
 * Gets the maximum amount of memory available to the GDALRasterBlock
 * caching system for caching GDAL read/write imagery. 
 *
 * @return maximum in bytes. 
 */
public class gdal:public static int GetCacheMax()

/**
 * Set maximum cache memory.
 *
 * This function sets the maximum amount of memory that GDAL is permitted
 * to use for GDALRasterBlock caching.
 *
 * @param newSize the maximum number of bytes for caching.  Maximum is 2GB.
 */
public class gdal:public static void SetCacheMax(int newSize)

/**
 * Get cache memory used.
 *
 * @return the number of bytes of memory currently in use by the 
 * GDALRasterBlock memory caching.
 */
public class gdal:public static int GetCacheUsed()


/**
 * Get data type size in bits.
 *
 * Returns the size of a a GDT_* type <b>in bits</b>, not bytes!
 *
 * @param eDataType type, such as gdalconst.GDT_Byte. 
 * @return the number of bits or zero if it is not recognised.
 */
public class gdal:public static int GetDataTypeSize(int eDataType)

/**
 * Is data type complex? 
 *
 * @return TRUE if the passed type is complex (one of gdalconst.GDT_CInt16, GDT_CInt32, 
 * GDT_CFloat32 or GDT_CFloat64), that is it consists of a real and imaginary
 * component. 
 */
public class gdal:public static int DataTypeIsComplex(int eDataType)

/**
 * Get name of data type.
 *
 * Returns a symbolic name for the data type.  This is essentially the
 * the enumerated item name with the GDT_ prefix removed.  So gdalconst.GDT_Byte returns
 * "Byte". These strings are useful for reporting
 * datatypes in debug statements, errors and other user output. 
 *
 * @param eDataType type to get name of.
 * @return string corresponding to type.
 */
public class gdal:public static String GetDataTypeName(int eDataType)

/**
 * Get data type by symbolic name.
 *
 * Returns a data type corresponding to the given symbolic name. This
 * function is opposite to the gdal.GetDataTypeName().
 *
 * @param dataTypeName string containing the symbolic name of the type.
 * 
 * @return GDAL data type.
 */
public class gdal:public static int GetDataTypeByName(String dataTypeName)

/**
 * Get name of color interpretation.
 *
 * Returns a symbolic name for the color interpretation.  This is derived from
 * the enumerated item name with the GCI_ prefix removed, but there are some
 * variations. So GCI_GrayIndex returns "Gray" and GCI_RedBand returns "Red".
 * The returned strings are static strings and should not be modified
 * or freed by the application.
 *
 * @param eColorInterp color interpretation to get name of.
 * @return string corresponding to color interpretation.
 */
public class gdal:public static String GetColorInterpretationName(int eColorInterp)


/**
 * Get name of palette interpretation.
 *
 * Returns a symbolic name for the palette interpretation.  This is the
 * the enumerated item name with the GPI_ prefix removed.  So GPI_Gray returns
 * "Gray".  The returned strings are static strings and should not be modified
 * or freed by the application.
 *
 * @param ePaletteInterp palette interpretation to get name of.
 * @return string corresponding to palette interpretation.
 */
public class gdal:public static String GetPaletteInterpretationName(int ePaletteInterp)

/**
 * Fetch the number of registered drivers.
 *
 * @return the number of registered drivers.
 */
public class gdal:public static int GetDriverCount()

/**
 * Fetch a driver based on the short name.
 *
 * @param name the short name, such as "GTiff", being searched for.
 *
 * @return the identified driver, or null if no match is found.
 */
public class gdal:public static Driver GetDriverByName(String name)

/**
 * Fetch driver by index.
 *
 * @param iDriver the driver index from 0 to gdal.GetDriverCount()-1.
 *
 * @return the driver identified by the index or null if the index is invalid
 */
public class gdal:public static Driver GetDriver(int iDriver)

/**
 * Open a raster file as a Dataset object.
 *
 * This function will try to open the passed file, or virtual dataset
 * name by invoking the Open method of each registered Driver in turn. 
 * The first successful open will result in a returned dataset.  If all
 * drivers fail then null is returned.
 * <p>
 * It is required that you explicitely close a dataset opened in update
 * mode with the Dataset.delete() method. Otherwise the data might not be
 * flushed to the disk. Don't rely only on Java garbage collection.
 *
 * @param name the name of the file to access.  In the case of
 * exotic drivers this may not refer to a physical file, but instead contain
 * information for the driver on how to access a dataset.
 *
 * @param eAccess the desired access, either gdalconst.GA_Update or gdalconst.GA_ReadOnly.  Many
 * drivers support only read only access.
 *
 * @return A Dataset object or null on failure.
 *
 * @see #OpenShared(String name, int eAccess)
 */
public class gdal:public static Dataset Open(String name, int eAccess)

/**
 * Open a raster file as a Dataset object.
 *
 * Same as below with eAccess == gdalconst.GA_ReadOnly
 *
 * @see #Open(String name, int eAccess)
 */
public class gdal:public static Dataset Open(String name)

/**
 * Open a raster file as a GDALDataset.
 *
 * This function works the same as gdal.Open(), but allows the sharing of
 * GDALDataset handles for a dataset with other callers to gdal.OpenShared().
 * <p>
 * In particular, gdal.OpenShared() will first consult it's list of currently
 * open and shared Dataset's, and if the GetDescription() name for one
 * exactly matches the name passed to gdal.OpenShared() it will be
 * referenced and returned.
 *
 * @param name the name of the file to access.  In the case of
 * exotic drivers this may not refer to a physical file, but instead contain
 * information for the driver on how to access a dataset.
 *
 * @param eAccess the desired access, either gdalconst.GA_Update or gdalconst.GA_ReadOnly.  Many
 * drivers support only read only access.
 *
 * @return A Dataset object or null on failure.
 *
 * @see #Open(String name, int eAccess)
 */
public class gdal:public static Dataset OpenShared(String name, int eAccess)

/**
 * Open a raster file as a Dataset object.
 *
 * Same as below with eAccess == gdalconst.GA_ReadOnly
 *
 * @see #OpenShared(String name, int eAccess)
 */
public class gdal:public static Dataset OpenShared(String name)


/**
 * Identify the driver that can open a raster file.
 *
 * This function will try to identify the driver that can open the passed file
 * name by invoking the Identify method of each registered Driver in turn. 
 * The first driver that successful identifies the file name will be returned.
 * If all drivers fail then null is returned.
 * <p>
 * In order to reduce the need for such searches touch the operating system
 * file system machinery, it is possible to give an optional list of files.
 * This is the list of all files at the same level in the file system as the
 * target file, including the target file. The filenames will not include any
 * path components, are an essentially just the output of CPLReadDir() on the
 * parent directory. If the target object does not have filesystem semantics
 * then the file list should be NULL.
 *
 * @param name the name of the file to access.  In the case of
 * exotic drivers this may not refer to a physical file, but instead contain
 * information for the driver on how to access a dataset.
 *
 * @param fileList a veector of strings.
 * These strings are filenames that are auxiliary to the main filename. The passed
 * value may be null.
 *
 * @return A Driver object or null on failure.
 */
public class gdal:public static Driver IdentifyDriver(String name, java.util.Vector fileList)

/**
 * Identify the driver that can open a raster file.
 *
 * Same as below with fileList == null
 *
 * @see #IdentifyDriver(String name, java.util.Vector fileList)
 */
public class gdal:public static Driver IdentifyDriver(String name)