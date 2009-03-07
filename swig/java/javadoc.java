/* This file contains the Javadoc for all classes */

/* Class gdal */

/**
 * Class gdal is an uninstanciable class providing various utility functions as static methods.
 * <p>
 * In particular, it provides :
 * <ul>
 * <li>gdal.<a href="#AllRegister()">AllRegister()</a> and gdal.<a href="#Open(java.lang.String, int)">Open()</a> methods.
 * <li>bindings for various GDAL algorithms.
 * <li>bindings for some general purpose CPL functions.
 * </ul>
 */
public class gdal

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
 * is for generating overviews via Dataset.<a href="Dataset.html#BuildOverviews(java.lang.String, int[], org.gdal.gdal.ProgressCallback)">BuildOverviews()</a>, but it
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
 *
 * @see Dataset#BuildOverviews(java.lang.String resampling, int[] overviewlist, ProgressCallback callback)
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
 * @return 1 if the passed type is complex (one of gdalconst.GDT_CInt16, GDT_CInt32, 
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
 * open and shared Dataset's, and if the <a href="MajorObject.html#GetDescription()">GetDescription()</a> name for one
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

/**
 * Parse an XML string into tree form.
 *
 * The passed document is parsed into a  XMLNode tree representation. 
 * If the document is not well formed XML then NULL is returned, and errors
 * are reported via CPLError().  No validation beyond wellformedness is
 * done.
 *
 * If the document has more than one "root level" element then those after the 
 * first will be attached to the first as siblings (via the psNext pointers)
 * even though there is no common parent.  A document with no XML structure
 * (no angle brackets for instance) would be considered well formed, and 
 * returned as a single CXT_Text node.  
 * 
 * @param xmlString the document to parse. 
 *
 * @return parsed tree or null on error. 
 */
public class gdal:public static XMLNode ParseXMLString(String xmlString)

/**
 * Convert tree into string document.
 *
 * This function converts a XMLNode tree representation of a document
 * into a flat string representation.  White space indentation is used
 * visually preserve the tree structure of the document.
 *
 * @param xmlnode the root of the tree to serialize
 *
 * @return the document on success or null on failure. 
 */
public class gdal:public static String SerializeXMLTree(XMLNode xmlnode)


/* Class Dataset */

/**
 * Class Dataset is an uninstanciable class providing various methods to access a set of associated raster bands, usually from one file.
 * <p>
 * A dataset encapsulating one or more raster bands. Details are further discussed in the <a href="http://gdal.org/gdal_datamodel.html#GDALDataset">GDAL Data Model</a>.
 * <p>
 * Dataset objects are returned by methods from other classes, such as
 * gdal.<a href="gdal.html#Open(java.lang.String, int)">Open()</a> or
 * Driver.<a href="Driver.html#Create(java.lang.String, int, int, int, int, java.lang.String[])">Create()</a> /
 * Driver.<a href="Driver.html#CreateCopy(java.lang.String, org.gdal.gdal.Dataset)">CreateCopy()</a>
 */
public class Dataset


/**
 * Add a band to a dataset.
 *
 * This method will add a new band to the dataset if the underlying format
 * supports this action.  Except VRT and MEM drivers, most formats do not.
 *
 * Note that the new Band object is not returned.  It may be fetched
 * after successful completion of the method by calling 
 * ds.GetRasterBand(ds.GetRasterCount()-1) as the newest
 * band will always be the last band.
 *
 * @param datatype the data type of the pixels in the new band. 
 *
 * @param options a vector of options strings, each being "NAME=VALUE".  The supported
 * options are format specific.  null may be passed by default.
 *
 * @return gdalconst.CE_None on success or gdalconst.CE_Failure on failure.  
 */
public class Dataset:public int AddBand(int datatype, java.util.Vector options)

/**
 * Add a band to a dataset.
 *
 * Same as below with options == null
 *
 * @see #AddBand(int datatype, java.util.Vector options)
 */
public class Dataset:public int AddBand(int datatype)

/**
 * Add a band to a dataset.
 *
 * Same as below with datatype == gdalconst.GDT_Byte and options == null
 *
 * @see #AddBand(int datatype, java.util.Vector options)
 */
public class Dataset:public int AddBand()


/**
 * Build raster overview(s).
 *
 * If the operation is unsupported for the indicated dataset, then 
 * gdalconst.CE_Failure is returned, and gdal.GetLastErrorNo() will return 
 * gdalconst.CPLE_NotSupported.
 * <p>
 *
 * For example, to build overview level 2, 4 and 8 on all bands the following
 * call could be made:
 * <pre>
 *   ds.BuildOverviews( "NEAREST", new int[] { 2, 4, 8 }, null );
 * </pre>
 *
 * @param resampling one of "NEAREST", "GAUSS", "AVERAGE", 
 * "AVERAGE_MAGPHASE" or "NONE" controlling the downsampling method applied.
 * @param overviewlist the list of overview decimation factors to build. 
 * @param callback for reporting progress or null
 *
 * @return gdalconst.CE_None on success or gdalconst.CE_Failure if the operation doesn't work.
 *
 * @see gdal#RegenerateOverviews(Band srcBand, Band[] overviewBands, String resampling, ProgressCallback callback)
 */
public class Dataset:public int BuildOverviews(String resampling, int[] overviewlist, ProgressCallback callback)

/**
 * Build raster overview(s).
 *
 * Same as below with callback == null
 *
 * @see #BuildOverviews(String resampling, int[] overviewlist, ProgressCallback callback)
 */
public class Dataset:public int BuildOverviews(String resampling, int[] overviewlist)

/**
 * Build raster overview(s).
 *
 * Same as below with resampling == "NEAREST" and callback == null
 *
 * @see #BuildOverviews(String resampling, int[] overviewlist, ProgressCallback callback)
 */
public class Dataset:public int BuildOverviews(int[] overviewlist)

/**
 * Build raster overview(s).
 *
 * Same as below with resampling == "NEAREST"
 *
 * @see #BuildOverviews(String resampling, int[] overviewlist, ProgressCallback callback)
 */
public class Dataset:public int BuildOverviews(int[] overviewlist, ProgressCallback callback)

/**
 * Adds a mask band to the current band.
 *
 * The default implementation of the CreateMaskBand() method is implemented
 * based on similar rules to the .ovr handling implemented using the
 * GDALDefaultOverviews object. A TIFF file with the extension .msk will
 * be created with the same basename as the original file, and it will have
 * as many bands as the original image (or just one for GMF_PER_DATASET).
 * The mask images will be deflate compressed tiled images with the same
 * block size as the original image if possible.
 *
 * @since GDAL 1.5.0
 *
 * @return gdalconst.CE_None on success or gdalconst.CE_Failure on an error.
 *
 * @see <a href="http://trac.osgeo.org/gdal/wiki/rfc15_nodatabitmask">RFC 15 - No data bit mask</a>
 *
 */
public class Dataset:public int CreateMaskBand(int nFlags)

/**
  * Frees the native resource associated to a Dataset object and close the file.
  *
  * This method will delete the underlying C++ object. After it has been called,
  * all native resources will have been destroyed, so it will be illegal to use
  * any derived relative Java objects, such as Band objects of this Dataset.
  * <p>
  * The delete() method <b>must</b> be called when a dataset has been opened in update
  * or creation mode, otherwise data might not be properly flushed to the disk.
  */
public class Dataset:public void delete()

/**
 * Flush all write cached data to disk.
 *
 * Any raster (or other GDAL) data written via GDAL calls, but buffered
 * internally will be written to disk.
 * <p>
 * Calling this method is generally not sufficient to ensure that the file is in
 * a consistent state. You <b>must</b> call <a href="#delete()">delete()</a> for that
 *
 * @see #delete()
 */
public class Dataset:public void FlushCache()


/**
 * Fetch the driver to which this dataset relates.
 *
 * @return the driver on which the dataset was created with gdal.Open() or
 * Driver.Create().
 */
public class Dataset:public Driver GetDriver()

/**
 *  Fetch a band object for a dataset.
 *
 * @param nBandId the index number of the band to fetch, from 1 to
                  GetRasterCount().
 * @return the nBandId th band object
 */
public class Dataset:public Band GetRasterBand(int nBandId)

/**
 * Fetch the projection definition string for this dataset.
 *
 * The returned string defines the projection coordinate system of the
 * image in OpenGIS WKT format.  It should be suitable for use with the 
 * OGRSpatialReference class.
 *
 * When a projection definition is not available an empty (but not null)
 * string is returned.
 *
 * @return the projection string.
 *
 * @see <a href="http://www.gdal.org/ogr/osr_tutorial.html">OSR tutorial</a>
 */
public class Dataset:public String GetProjection()

/**
 * Fetch the projection definition string for this dataset.
 *
 * The returned string defines the projection coordinate system of the
 * image in OpenGIS WKT format.  It should be suitable for use with the 
 * OGRSpatialReference class.
 *
 * When a projection definition is not available an empty (but not null)
 * string is returned.
 *
 * @return the projection string.
 *
 * @see <a href="http://www.gdal.org/ogr/osr_tutorial.html">OSR tutorial</a>
 */
public class Dataset:public String GetProjectionRef()


/**
 * Set the projection reference string for this dataset.
 *
 * The string should be in OGC WKT or PROJ.4 format.  An error may occur
 * because of incorrectly specified projection strings, because the dataset
 * is not writable, or because the dataset does not support the indicated
 * projection.  Many formats do not support writing projections.
 *
 * @param projection projection reference string.
 *
 * @return gdalconst.CE_Failure if an error occurs, otherwise gdalconst.CE_None.
 */
public class Dataset:public int SetProjection(String projection)


/**
 * Fetch the affine transformation coefficients.
 *
 * Fetches the coefficients for transforming between pixel/line (P,L) raster
 * space, and projection coordinates (Xp,Yp) space.
 * <p>
 * <pre>
 *   Xp = geoTransformArray[0] + P*geoTransformArray[1] + L*geoTransformArray[2];
 *   Yp = geoTransformArray[3] + P*geoTransformArray[4] + L*geoTransformArray[5];
 * </pre>
 * <p>
 * In a north up image, geoTransformArray[1] is the pixel width, and
 * geoTransformArray[5] is the pixel height.  The upper left corner of the
 * upper left pixel is at position (geoTransformArray[0],geoTransformArray[3]).
 * <p>
 * The default transform is (0,1,0,0,0,1) and should be returned even when
 * an error occurs, such as for formats that don't support
 * transformation to projection coordinates.
 * <p>
 * NOTE: GetGeoTransform() isn't expressive enough to handle the variety of
 * OGC Grid Coverages pixel/line to projection transformation schemes.
 * Eventually this method will be depreciated in favour of a more general
 * scheme.
 *
 * @param geoTransformArray an existing six double array into which the
 * transformation will be placed.
 */
public class Dataset:public void GetGeoTransform(double[] geoTransformArray)


/**
 * Fetch the affine transformation coefficients.
 *
 * Same as below, except the geotransform array is returned by the method
 *
 * @see #GetGeoTransform(double[] geoTransformArray)
 */
public class Dataset:public double[] GetGeoTransform()

/**
 * Set the affine transformation coefficients.
 *
 * See <a href="#GetGeoTransform(double[])">#GetGeoTransform()</a> for details on the meaning of the geoTransformArray
 * coefficients.
 *
 * @param geoTransformArray a six double array containing the transformation
 * coefficients to be written with the dataset.
 *
 * @return gdalconst.CE_None on success, or gdalconst.CE_Failure if this transform cannot be
 * written.
 *
 * @see #GetGeoTransform(double[] geoTransformArray)
 */
public class Dataset:public int SetGeoTransform(double[] geoTransformArray)

/**
 * Fetch files forming dataset.
 *
 * Returns a list of files believed to be part of this dataset.  If it returns
 * an empty list of files it means there is believed to be no local file
 * system files associated with the dataset (for instance a virtual dataset).
 * <p>
 * The returned filenames will normally be relative or absolute paths 
 * depending on the path used to originally open the dataset.
 *
 * @return null or a vector of strings of file names. 
 */
public class Dataset:public java.util.Vector GetFileList()

/**
 * Get number of GCPs. 
 *
 * @return number of GCPs for this dataset.  Zero if there are none.
 */
public class Dataset:public int GetGCPCount()

/**
 * Get output projection for GCPs. 
 *
 * The projection string follows the normal rules from <a href="#GetProjectionRef()">GetProjectionRef()</a>.
 * 
 * @return projection string or "" if there are no GCPs. 
 */
public class Dataset:public String GetGCPProjection()

/**
 * Fetch GCPs.
 *
 * Add to the provided vector the GCPs of the dataset
 *
 * @param gcpVector non null Vector object
 */ 
public class Dataset:public void GetGCPs(java.util.Vector gcpVector)

/**
 * Fetch GCPs.
 *
 * @return a vector of GCP objects
 */
public class Dataset:public java.util.Vector GetGCPs()


/**
 * Assign GCPs.
 *
 * This method assigns the passed set of GCPs to this dataset, as well as
 * setting their coordinate system.  Internally copies are made of the
 * coordinate system and list of points, so the caller remains resposible for
 * deallocating these arguments if appropriate. 
 * <p>
 * Most formats do not support setting of GCPs, even foramts that can 
 * handle GCPs.  These formats will return CE_Failure. 
 *
 * @param gcpArray array of GCP objects being assigned
 *
 * @param GCPProjection the new OGC WKT coordinate system to assign for the 
 * GCP output coordinates.  This parameter should be "" if no output coordinate
 * system is known.
 *
 * @return gdalconst.CE_None on success, gdalconst.CE_Failure on failure (including if action is
 * not supported for this format). 
 */ 
public class Dataset:public int SetGCPs(GCP[] gcpArray, String GCPProjection)

/**
 * Fetch raster width in pixels.
 *
 * @return the width in pixels of raster bands in this Dataset.
 */
public class Dataset:public int getRasterXSize()

/**
 * Fetch raster width in pixels.
 *
 * @return the width in pixels of raster bands in this Dataset.
 */
public class Dataset:public int GetRasterXSize()

/**
 * Fetch raster height in pixels.
 *
 * @return the heigt in pixels of raster bands in this Dataset.
 */
public class Dataset:public int getRasterYSize()

/**
 * Fetch raster height in pixels.
 *
 * @return the heigt in pixels of raster bands in this Dataset.
 */
public class Dataset:public int GetRasterYSize()

/**
 * Fetch the number of raster bands on this dataset.
 *
 * @return the number of raster bands.
 */
public class Dataset:public int getRasterCount()

/**
 * Fetch the number of raster bands on this dataset.
 *
 * @return the number of raster bands.
 */
public class Dataset:public int GetRasterCount()

/**
 * Read a region of image data from multiple bands.
 *
 * This method allows reading a region of one or more Band's from
 * this dataset into a buffer. It automatically takes care of data type
 * translation if the data type (buf_type) of the buffer is different than
 * that of the Band.
 * The method also takes care of image decimation / replication if the
 * buffer size (buf_xsize x buf_ysize) is different than the size of the
 * region being accessed (xsize x ysize).
 * <p>
 * The nPixelSpace, nLineSpace and nBandSpace parameters allow reading into or
 * writing from various organization of buffers. 
 * <p>
 * For highest performance full resolution data access, read and write
 * on "block boundaries" as returned by GetBlockSize(), or use the
 * ReadBlock() and WriteBlock() methods.
 *
 * @param xoff The pixel offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the left side.
 *
 * @param yoff The line offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the top.
 *
 * @param xsize The width of the region of the band to be accessed in pixels.
 *
 * @param ysize The height of the region of the band to be accessed in lines.

 * @param buf_xsize the width of the buffer image into which the desired region
 * is to be read.
 *
 * @param buf_ysize the height of the buffer image into which the desired
 * region is to be read.
 *
 * @param buf_type the type of the pixel values in the nioBuffer data buffer.  The
 * pixel values will automatically be translated to/from the Band
 * data type as needed.
 *
 * @param nioBuffer The buffer into which the data should be read, or from which
 * it should be written.  This buffer must contain at least
 * buf_xsize * buf_ysize * nBandCount words of type buf_type.  It is organized
 * in left to right,top to bottom pixel order.  Spacing is controlled by the
 * nPixelSpace, and nLineSpace parameters.
 *
 * @param band_list the list of band numbers being read/written.
 * Note band numbers are 1 based.   This may be null to select the first 
 * nBandCount bands.
 *
 * @param nPixelSpace The byte offset from the start of one pixel value in
 * pData to the start of the next pixel value within a scanline.  If defaulted
 * (0) the size of the datatype eBufType is used.
 *
 * @param nLineSpace The byte offset from the start of one scanline in
 * pData to the start of the next.  If defaulted the size of the datatype
 * eBufType * nBufXSize is used.
 *
 * @param nBandSpace the byte offset from the start of one bands data to the
 * start of the next.  If defaulted (zero) the value will be 
 * nLineSpace * nBufYSize implying band sequential organization
 * of the data buffer. 
 *
 * @return gdalconst.CE_Failure if the access fails, otherwise gdalconst.CE_None.
 */
public class Dataset:public int ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int[] band_list, int nPixelSpace, int nLineSpace, int nBandSpace)

/**
 * Read a region of image data from multiple bands.
 *
 * Same as below with nBandSpace == 0
 *
 * @see #ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int[] band_list, int nPixelSpace, int nLineSpace, int nBandSpace)
 */
public class Dataset:public int ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int[] band_list, int nPixelSpace, int nLineSpace)

/**
 * Read a region of image data from multiple bands.
 *
 * Same as below with nLineSpace == 0 and nBandSpace == 0
 *
 * @see #ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int[] band_list, int nPixelSpace, int nLineSpace, int nBandSpace)
 */
public class Dataset:public int ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int[] band_list, int nPixelSpace)

/**
 * Read a region of image data from multiple bands.
 *
 * Same as below with nPixelSpace == 0, nLineSpace == 0 and nBandSpace == 0
 *
 * @see #ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int[] band_list, int nPixelSpace, int nLineSpace, int nBandSpace)
 */
public class Dataset:public int ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int[] band_list)

/**
 * Write a region of image data from multiple bands.
 *
 * This method allows writing data from a buffer into a region 
 * of the Band's.  It automatically takes care of data type
 * translation if the data type (buf_type) of the buffer is different than
 * that of the Band.
 * The method also takes care of image decimation / replication if the
 * buffer size (buf_xsize x buf_ysize) is different than the size of the
 * region being accessed (xsize x ysize).
 * <p>
 * The nPixelSpace, nLineSpace and nBandSpace parameters allow reading into or
 * writing from various organization of buffers. 
 * <p>
 * For highest performance full resolution data access, read and write
 * on "block boundaries" as returned by GetBlockSize(), or use the
 * ReadBlock() and WriteBlock() methods.
 *
 * @param xoff The pixel offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the left side.
 *
 * @param yoff The line offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the top.
 *
 * @param xsize The width of the region of the band to be accessed in pixels.
 *
 * @param ysize The height of the region of the band to be accessed in lines.

 * @param buf_xsize the width of the buffer image from which the desired region is to be written.
 *
 * @param buf_ysize the height of the buffer image from which the desired region is to be written.
 *
 * @param buf_type the type of the pixel values in the nioBuffer data buffer.  The
 * pixel values will automatically be translated to/from the Band
 * data type as needed.
 *
 * @param nioBuffer The buffer into which the data should be read, or from which
 * it should be written.  This buffer must contain at least
 * buf_xsize * buf_ysize * nBandCount words of type buf_type.  It is organized
 * in left to right,top to bottom pixel order.  Spacing is controlled by the
 * nPixelSpace, and nLineSpace parameters.
 *
 * @param band_list the list of band numbers being read/written.
 * Note band numbers are 1 based.   This may be null to select the first 
 * nBandCount bands.
 *
 * @param nPixelSpace The byte offset from the start of one pixel value in
 * pData to the start of the next pixel value within a scanline.  If defaulted
 * (0) the size of the datatype eBufType is used.
 *
 * @param nLineSpace The byte offset from the start of one scanline in
 * pData to the start of the next.  If defaulted the size of the datatype
 * eBufType * nBufXSize is used.
 *
 * @param nBandSpace the byte offset from the start of one bands data to the
 * start of the next.  If defaulted (zero) the value will be 
 * nLineSpace * nBufYSize implying band sequential organization
 * of the data buffer. 
 *
 * @return gdalconst.CE_Failure if the access fails, otherwise gdalconst.CE_None.
 */
public class Dataset:public int WriteRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int[] band_list, int nPixelSpace, int nLineSpace, int nBandSpace)

/**
 * Write a region of image data from multiple bands.
 *
 * Same as below with nBandSpace == 0
 *
 * @see #WriteRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int[] band_list, int nPixelSpace, int nLineSpace, int nBandSpace)
 */
public class Dataset:public int WriteRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int[] band_list, int nPixelSpace, int nLineSpace)

/**
 * Write a region of image data from multiple bands.
 *
 * Same as below with nLineSpace == 0 and nBandSpace == 0
 *
 * @see #WriteRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int[] band_list, int nPixelSpace, int nLineSpace, int nBandSpace)
 */
public class Dataset:public int WriteRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int[] band_list, int nPixelSpace)

/**
 * Write a region of image data from multiple bands.
 *
 * Same as below with nPixelSpace == 0, nLineSpace == 0 and nBandSpace == 0
 *
 * @see #WriteRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int[] band_list, int nPixelSpace, int nLineSpace, int nBandSpace)
 */
public class Dataset:public int WriteRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int[] band_list)


/* Class Band */

/**
 * Class Band is an uninstanciable class providing various methods to access a single raster band (or channel).
 * <p>
 * Band objects are returned by methods from other classes, such as
 * Dataset.<a href="Dataset.html#GetRasterBand(int)">GetRasterBand()</a>
 */
public class Band

/**
 * Compute checksum for image region. 
 *
 * Computes a 16bit (0-65535) checksum from a region of raster data on the raster band.
 * Floating point data is converted to 32bit integer 
 * so decimal portions of such raster data will not affect the checksum.
 * Real and Imaginary components of complex bands influence the result. 
 *
 * @param xoff pixel offset of window to read.
 * @param yoff line offset of window to read.
 * @param xsize pixel size of window to read.
 * @param ysize line size of window to read.
 *
 * @return Checksum value. 
 */
public class Band:public int Checksum(int xoff, int yoff, int xsize, int ysize)

/**
 * Compute checksum for while image.
 *
 * Computes a 16bit (0-65535) checksum from data on the raster band.
 * Floating point data is converted to 32bit integer 
 * so decimal portions of such raster data will not affect the checksum.
 * Real and Imaginary components of complex bands influence the result. 
 *
 * @return Checksum value. 
 */
public class Band:public int Checksum()

/**
 * Compute mean and standard deviation values.
 *
 * @param meanAndStdDevArray the allocated array of 2 doubles in which the mean value (meanAndStdDevArray[0])
 *                           and the standard deviation (meanAndStdDevArray[1]) are returned
 * @param samplestep step in number of lines
 */
public class Band:public void ComputeBandStats(double[] meanAndStdDevArray, int samplestep)

/**
 * Compute mean and standard deviation values.
 *
 * Same as below with samplestep == 1
 *
 * @see #ComputeBandStats(double[] meanAndStdDevArray, int samplestep)
 */
public class Band:public void ComputeBandStats(double[] meanAndStdDevArray)

/**
 * Compute the min/max values for a band.
 * 
 * If approximate is OK, then the band's GetMinimum()/GetMaximum() will
 * be trusted.  If it doesn't work, a subsample of blocks will be read to
 * get an approximate min/max.  If the band has a nodata value it will
 * be excluded from the minimum and maximum.
 * <p>
 * If approx_ok is 0, then all pixels will be read and used to compute
 * an exact range.
 * 
 * @param minMaxArray the allocated array of 2 doubles in which the minimum (minMaxArray[0]) and the
 * maximum (minMaxArray[1]) are returned.
 * @param approx_ok 1 if an approximate (faster) answer is OK, otherwise 0.
 */
public class Band:public void ComputeRasterMinMax(double[] minMaxArray, int approx_ok)

/**
 * Compute the min/max values for a band.
 * 
 * Same as below with approx_ok == 0
 *
 * @see #ComputeRasterMinMax(double[] minMaxArray, int approx_ok)
 */
public class Band:public void ComputeRasterMinMax(double[] minMaxArray)

/**
 * Compute image statistics. 
 *
 * Returns the minimum, maximum, mean and standard deviation of all
 * pixel values in this band.  If approximate statistics are sufficient,
 * the approx_ok flag can be set to true in which case overviews, or a
 * subset of image tiles may be used in computing the statistics.  
 * <p>
 * Once computed, the statistics will generally be "set" back on the 
 * raster band using SetStatistics(). 
 *
 * @param approx_ok If true statistics may be computed based on overviews
 * or a subset of all tiles. 
 * 
 * @param min Allocated array of one double into which to load image minimum (may be null).
 *
 * @param max Allocated array of one double into which to load image maximum (may be null).
 *
 * @param mean Allocated array of one double into which to load image mean (may be null).
 *
 * @param stddev Allocated array of one double into which to load image standard deviation 
 * (may be null).
 *
 * @param callback for reporting algorithm progress. May be null
 *
 * @return gdalconst.CE_None on success, or gdalconst.CE_Failure if an error occurs or processing
 * is terminated by the user.
 */
public class Band:public int ComputeStatistics(boolean approx_ok, double[] min, double[] max, double[] mean, double[] stddev, ProgressCallback callback)

/**
 * Compute image statistics. 
 *
 * Same as below with callback == null
 *
 * @see #ComputeStatistics(boolean approx_ok, double[] min, double[] max, double[] mean, double[] stddev, ProgressCallback callback)
 */
public class Band:public int ComputeStatistics(boolean approx_ok, double[] min, double[] max, double[] mean, double[] stddev)

/**
 * Compute image statistics. 
 *
 * Same as below with stddev == null and callback == null
 *
 * @see #ComputeStatistics(boolean approx_ok, double[] min, double[] max, double[] mean, double[] stddev, ProgressCallback callback)
 */
public class Band:public int ComputeStatistics(boolean approx_ok, double[] min, double[] max, double[] mean)

/**
 * Compute image statistics. 
 *
 * Same as below with mean == null, stddev == null and callback == null
 *
 * @see #ComputeStatistics(boolean approx_ok, double[] min, double[] max, double[] mean, double[] stddev, ProgressCallback callback)
 */
public class Band:public int ComputeStatistics(boolean approx_ok, double[] min, double[] max)

/**
 * Compute image statistics. 
 *
 * Same as below with max == null, mean == null, stddev == null and callback == null
 *
 * @see #ComputeStatistics(boolean approx_ok, double[] min, double[] max, double[] mean, double[] stddev, ProgressCallback callback)
 */
public class Band:public int ComputeStatistics(boolean approx_ok, double[] min)

/**
 * Compute image statistics. 
 *
 * Same as below with min == null, max == null, mean == null, stddev == null and callback == null
 *
 * @see #ComputeStatistics(boolean approx_ok, double[] min, double[] max, double[] mean, double[] stddev, ProgressCallback callback)
 */
public class Band:public int ComputeStatistics(boolean approx_ok)

/**
 * Adds a mask band to the current band.
 *
 * The default implementation of the CreateMaskBand() method is implemented
 * based on similar rules to the .ovr handling implemented using the
 * GDALDefaultOverviews object. A TIFF file with the extension .msk will
 * be created with the same basename as the original file, and it will have
 * as many bands as the original image (or just one for GMF_PER_DATASET).
 * The mask images will be deflate compressed tiled images with the same
 * block size as the original image if possible.
 *
 * @since GDAL 1.5.0
 *
 * @return gdalconst.CE_None on success or gdalconst.CE_Failure on an error.
 *
 * @see <a href="http://trac.osgeo.org/gdal/wiki/rfc15_nodatabitmask">RFC 15 - No data bit mask</a>
 *
 */
public class Band:public int CreateMaskBand(int nFlags)

/** 
 * Fill this band with a constant value.
 *
 * GDAL makes no guarantees
 * about what values pixels in newly created files are set to, so this
 * method can be used to clear a band to a specified "default" value.
 * The fill value is passed in as a double but this will be converted
 * to the underlying type before writing to the file. An optional
 * second argument allows the imaginary component of a complex
 * constant value to be specified.
 * 
 * @param real_fill Real component of fill value
 * @param imag_fill Imaginary component of fill value, defaults to zero
 * 
 * @return gdalconst.CE_Failure if the write fails, otherwise gdalconst.CE_None
 */
public class Band:public int Fill(double real_fill, double imag_fill)

/** 
 * Fill this band with a constant value.
 *
 * Same as below with image_fill == 0
 *
 * @see #Fill(double real_fill, double imag_fill)
 */
public class Band:public int Fill(double real_fill)

/**
 * Flush raster data cache.
 *
 * This call will recover memory used to cache data blocks for this raster
 * band, and ensure that new requests are referred to the underlying driver.
 */
public class Band:public void FlushCache()

/**
 * Fetch the band number.
 *
 * This method returns the band that this Band object represents
 * within its dataset.  This method may return a value of 0 to indicate
 * Band objects without an apparently relationship to a dataset,
 * such as Band's serving as overviews.
 *
 * @return band number (1+) or 0 if the band number isn't known.
 */
public class Band:public int GetBand()

/**
 * Fetch the "natural" block size of this band.
 *
 * GDAL contains a concept of the natural block size of rasters so that
 * applications can organized data access efficiently for some file formats.
 * The natural block size is the block size that is most efficient for
 * accessing the format.  For many formats this is simple a whole scanline
 * in which case pnBlockXSize[0] is set to GetXSize(), and pnBlockXSize[1] is set to 1.
 * <p>
 * However, for tiled images this will typically be the tile size.
 * <p>
 * Note that the X and Y block sizes don't have to divide the image size
 * evenly, meaning that right and bottom edge blocks may be incomplete.
 * See <a href="#ReadBlock_Direct(int, int, java.nio.ByteBuffer)">ReadBlock_Direct()</a> for an example of code dealing with these issues.
 *
 * @param pnBlockXSize allocated array of 1 integer to put the X block size into or null.
 *
 * @param pnBlockYSize allocated array of 1 integer to put the Y block size into or null.
 */
public class Band:public void GetBlockSize(int[] pnBlockXSize, int[] pnBlockYSize)

/**
 * Fetch the "natural" block width of this band
 * @return the X block size
 * @see #GetBlockSize(int[] pnBlockXSize, int[] pnBlockYSize)
 */
public class Band:public int GetBlockXSize()

/**
 * Fetch the "natural" block height of this band
 * @return the Y block size
 * @see #GetBlockSize(int[] pnBlockXSize, int[] pnBlockYSize)
 */
public class Band:public int GetBlockYSize()

/**
 * How should this band be interpreted as color?
 *
 * gdalconst.GCI_Undefined is returned when the format doesn't know anything
 * about the color interpretation. 
 *
 * @return color interpretation value for band.
 */
public class Band:public int GetColorInterpretation()

/**
 * Fetch the color table associated with band.
 *
 * If there is no associated color table, the return result is null.  The
 * returned color table remains owned by the Band object.
 * It should not be modified by the caller.
 *
 * @return color table, or null.
 */
public class Band:public ColorTable GetColorTable()

/**
 * Return the data type of the band.
 *
 * A value such as gdalconst.GDT_Byte, gdalconst.GDT_Int16, ...
 * @return the data type of the band.
 */
public class Band:public int getDataType()


/**
 * Fetch default raster histogram. 
 *
 * The default method in GDALRasterBand will compute a default histogram. This
 * method is overriden by derived classes (such as GDALPamRasterBand, VRTDataset, HFADataset...)
 * that may be able to fetch efficiently an already stored histogram.
 * <p>
 * For example,
 * <pre>
 *  double[] dfMin = new double[1];
 *  double[] dfMax = new double[1];
 *  int[][] panHistogram = new int[1][];
 *  int eErr = hBand.GetDefaultHistogram(dfMin, dfMax, panHistogram, true, new TermProgressCallback());
 *  if( eErr == gdalconstConstants.CE_None )
 *  {
 *      int iBucket;
 *      int nBucketCount = panHistogram[0].length;
 *      System.out.print( "  " + nBucketCount + " buckets from " +
 *                         dfMin[0] + " to " + dfMax[0] + ":\n  " );
 *      for( iBucket = 0; iBucket < nBucketCount; iBucket++ )
 *          System.out.print( panHistogram[0][iBucket] + " ");
 *      System.out.print( "\n" );
 *  }
 * </pre>
 *
 * @param min_ret allocated array of one double that will contain the lower bound of the histogram.
 * @param max_ret allocated array of one double that will contain the upper bound of the histogram.
 * @param histogram_ret allocated array of one int[] into which the histogram totals are placed.
 * @param force true to force the computation. If false and no default histogram is available, the method will return CE_Warning
 * @param callback for reporting algorithm progress. May be null
 *
 * @return gdalconst.CE_None on success, gdalconst.CE_Failure if something goes wrong, or 
 * gdalconst.CE_Warning if no default histogram is available.
 */
public class Band:public int GetDefaultHistogram(double[] min_ret, double[] max_ret, int[][] histogram_ret, boolean force, ProgressCallback callback)

/**
 * Fetch default raster histogram. 
 *
 * Same as below with callback == null
 *
 * @see #GetDefaultHistogram(double[] min_ret, double[] max_ret, int[][] histogram_ret, boolean force, ProgressCallback callback)
 */
public class Band:public int GetDefaultHistogram(double[] min_ret, double[] max_ret, int[][] histogram_ret, boolean force)

/**
 * Fetch default raster histogram. 
 *
 * Same as below with force == true and callback == null
 *
 * @see #GetDefaultHistogram(double[] min_ret, double[] max_ret, int[][] histogram_ret, boolean force, ProgressCallback callback)
 */
public class Band:public int GetDefaultHistogram(double[] min_ret, double[] max_ret, int[][] histogram_ret)

/**
 * Fetch default Raster Attribute Table.
 *
 * A RAT will be returned if there is a default one associated with the
 * band, otherwise null is returned.  The returned RAT is owned by the
 * band and should not be altered by the application. 
 * 
 * @return null, or a pointer to an internal RAT owned by the band.
 */
public class Band:public RasterAttributeTable GetDefaultRAT()

/**
 * Compute raster histogram. 
 *
 * Note that the bucket size is (dfMax-dfMin) / nBuckets.  
 * <p>
 * For example to compute a simple 256 entry histogram of eight bit data, 
 * the following would be suitable.  The unusual bounds are to ensure that
 * bucket boundaries don't fall right on integer values causing possible errors
 * due to rounding after scaling. 
 * <pre>
 *     int anHistogram = new int[256];
 * 
 *     band.GetHistogram( -0.5, 255.5, 256, anHistogram, false, false, null);
 * </pre>
 * <p>
 * Note that setting approx_ok will generally result in a subsampling of the
 * file, and will utilize overviews if available.  It should generally 
 * produce a representative histogram for the data that is suitable for use
 * in generating histogram based luts for instance.  Generally bApproxOK is
 * much faster than an exactly computed histogram.
 *
 * @param min the lower bound of the histogram.
 * @param max the upper bound of the histogram.
 * @param histogram allocated array into which the histogram totals are placed.
 * @param include_out_of_range if true values below the histogram range will
 * mapped into anHistogram[0], and values above will be mapped into 
 * anHistogram[anHistogram.length-1] otherwise out of range values are discarded.
 * @param approx_ok true if an approximate, or incomplete histogram OK.
 * @param callback for reporting algorithm progress. May be null
 *
 * @return gdalconst.CE_None on success, or gdalconst.CE_Failure if something goes wrong. 
 */
public class Band:public int GetHistogram(double min, double max, int[] histogram, boolean include_out_of_range, boolean approx_ok, ProgressCallback callback)

/**
 * Compute raster histogram. 
 *
 * Same as below with callback == null
 *
 * @see #GetHistogram(double min, double max, int[] histogram, boolean include_out_of_range, boolean approx_ok, ProgressCallback callback)
 */
public class Band:public int GetHistogram(double min, double max, int[] histogram, boolean include_out_of_range, boolean approx_ok)

/**
 * Compute raster histogram. 
 *
 * Same as below with include_out_of_range == 0, approx_ok == true and callback == null
 *
 * @see #GetHistogram(double min, double max, int[] histogram, boolean include_out_of_range, boolean approx_ok, ProgressCallback callback)
 */
public class Band:public int GetHistogram(double min, double max, int[] histogram)

/**
 * Compute raster histogram. 
 *
 * Same as below with include_out_of_range == 0, approx_ok == true and callback == null
 *
 * @see #GetHistogram(double min, double max, int[] histogram, boolean include_out_of_range, boolean approx_ok, ProgressCallback callback)
 */
public class Band:public int GetHistogram(int[] histogram)

/**
 * Return the mask band associated with the band.
 *
 * The GDALRasterBand class includes a default implementation of GetMaskBand() that
 * returns one of four default implementations :
 * <ul>
 * <li>If a corresponding .msk file exists it will be used for the mask band.</li>
 * <li>If the dataset has a NODATA_VALUES metadata item, an instance of the
 *     new GDALNoDataValuesMaskBand class will be returned.
 *     GetMaskFlags() will return GMF_NODATA | GMF_PER_DATASET. @since GDAL 1.6.0</li>
 * <li>If the band has a nodata value set, an instance of the new
 *     GDALNodataMaskRasterBand class will be returned.
 *     GetMaskFlags() will return GMF_NODATA.</li>
 * <li>If there is no nodata value, but the dataset has an alpha band that seems
 *     to apply to this band (specific rules yet to be determined) and that is
 *     of type GDT_Byte then that alpha band will be returned, and the flags
 *     GMF_PER_DATASET and GMF_ALPHA will be returned in the flags.</li>
 * <li>If neither of the above apply, an instance of the new GDALAllValidRasterBand
 *     class will be returned that has 255 values for all pixels.
 *     The null flags will return GMF_ALL_VALID.</li>
 * </ul>
 * <p>
 * Note that the GetMaskBand() should always return a GDALRasterBand mask, even if it is only
 * an all 255 mask with the flags indicating GMF_ALL_VALID. 
 *
 * @return a valid mask band.
 *
 * @since GDAL 1.5.0
 *
 * @see <a href="http://trac.osgeo.org/gdal/wiki/rfc15_nodatabitmask">RFC 15 - No data bit mask</a>
 *
 */
public class Band:public Band GetMaskBand()

/**
 * Return the status flags of the mask band associated with the band.
 *
 * The GetMaskFlags() method returns an bitwise OR-ed set of status flags with
 * the following available definitions that may be extended in the future:
 * <ul>
 * <li>GMF_ALL_VALID(0x01): There are no invalid pixels, all mask values will be 255.
 *     When used this will normally be the only flag set.</li>
 * <li>GMF_PER_DATASET(0x02): The mask band is shared between all bands on the dataset.</li>
 * <li>GMF_ALPHA(0x04): The mask band is actually an alpha band and may have values
 *     other than 0 and 255.</li>
 * <li>GMF_NODATA(0x08): Indicates the mask is actually being generated from nodata values.
 *     (mutually exclusive of GMF_ALPHA)</li>
 * </ul>
 *
 * The GDALRasterBand class includes a default implementation of GetMaskBand() that
 * returns one of four default implementations :
 * <ul>
 * <li>If a corresponding .msk file exists it will be used for the mask band.</li>
 * <li>If the dataset has a NODATA_VALUES metadata item, an instance of the
 *     new GDALNoDataValuesMaskBand class will be returned.
 *     GetMaskFlags() will return GMF_NODATA | GMF_PER_DATASET. @since GDAL 1.6.0</li>
 * <li>If the band has a nodata value set, an instance of the new
 *     GDALNodataMaskRasterBand class will be returned.
 *     GetMaskFlags() will return GMF_NODATA.</li>
 * <li>If there is no nodata value, but the dataset has an alpha band that seems
 *     to apply to this band (specific rules yet to be determined) and that is
 *     of type GDT_Byte then that alpha band will be returned, and the flags
 *     GMF_PER_DATASET and GMF_ALPHA will be returned in the flags.</li>
 * <li>If neither of the above apply, an instance of the new GDALAllValidRasterBand
 *     class will be returned that has 255 values for all pixels.
 *     The null flags will return GMF_ALL_VALID.</li>
 * </ul>
 *
 * @since GDAL 1.5.0
 *
 * @return a valid mask band.
 *
 * @see <a href="http://trac.osgeo.org/gdal/wiki/rfc15_nodatabitmask">RFC 15 - No data bit mask</a>
 *
 */
public class Band:public int GetMaskFlags()

/**
 * Fetch the minimum value for this band.
 * 
 * For file formats that don't know this intrinsically, no value will be returned
 *
 * @param val empty allocated array of type Doube[] of size 1. val[0] will contain a Double object
 * with the minimum value if available, other val[0] will contain null
 */
public class Band:public void GetMinimum(Double[] val)

/**
 * Fetch the maximum value for this band.
 * 
 * For file formats that don't know this intrinsically, no value will be returned
 *
 * @param val empty allocated array of type Doube[] of size 1. val[0] will contain a Double object
 * with the maximum value if available, other val[0] will contain null
 */
public class Band:public void GetMaximum(Double[] val)

/**
 * Fetch the raster value offset.
 *
 * This value (in combination with the GetScale() value) is used to
 * transform raw pixel values into the units returned by GetUnits().  
 * For example this might be used to store elevations in GUInt16 bands
 * with a precision of 0.1, and starting from -100. 
 * <p>
 * Units value = (raw value * scale) + offset
 * <p>
 * For file formats that don't know this intrinsically, no value will be returned
 *
 * @param val empty allocated array of type Doube[] of size 1. val[0] will contain a Double object
 * with the offset value if available, other val[0] will contain null
 */
public class Band:public void GetOffset(Double[] val)

/**
 * Fetch the raster value scale.
 *
 * This value (in combination with the GetOffset() value) is used to
 * transform raw pixel values into the units returned by GetUnits().  
 * For example this might be used to store elevations in GUInt16 bands
 * with a precision of 0.1, and starting from -100. 
 * <p>
 * Units value = (raw value * scale) + offset
 * <p>
 * For file formats that don't know this intrinsically, no value will be returned
 *
 * @param val empty allocated array of type Doube[] of size 1. val[0] will contain a Double object
 * with the scale value if available, other val[0] will contain null
 */
public class Band:public void GetScale(Double[] val)

/**
 * Fetch the no data value for this band.
 * 
 * The no data value for a band is generally a special marker
 * value used to mark pixels that are not valid data.  Such pixels should
 * generally not be displayed, nor contribute to analysis operations.
 *
 * @param val empty allocated array of type Doube[] of size 1. val[0] will contain a Double object
 * with the no data value if available, other val[0] will contain null
 */
public class Band:public void GetNoDataValue(Double[] val)

/**
 * Fetch overview raster band object.
 * 
 * @param i overview index between 0 and GetOverviewCount()-1.
 * 
 * @return overview Band.
 */
public class Band:public Band GetOverview(int i)

/**
 * Return the number of overview layers available.
 *
 * @return overview count, zero if none.
 */
public class Band:public int GetOverviewCount()

/**
 * Fetch the list of category names for this raster.
 *
 * Raster values without 
 * associated names will have an empty string in the returned list.  The
 * first entry in the list is for raster values of zero, and so on. 
 * 
 * @return vector of names, or null if none.
 */
public class Band:public java.util.Vector GetRasterCategoryNames()

/**
 * How should this band be interpreted as color?
 *
 * gdalconst.GCI_Undefined is returned when the format doesn't know anything
 * about the color interpretation. 
 *
 * @return color interpretation value for band.
 */
public class Band:public int GetRasterColorInterpretation()

/**
 * Fetch the color table associated with band.
 *
 * If there is no associated color table, the return result is null.  The
 * returned color table remains owned by the Band object.
 * It should not be modified by the caller.
 *
 * @return color table, or null.
 */
public class Band:public ColorTable GetRasterColorTable()

/**
 * Return the data type of the band.
 *
 * A value such as gdalconst.GDT_Byte, gdalconst.GDT_Int16, ...
 * @return the data type of the band.
 */
public class Band:public int GetRasterDataType()

/**
 * Fetch image statistics. 
 *
 * Returns the minimum, maximum, mean and standard deviation of all
 * pixel values in this band.  If approximate statistics are sufficient,
 * the approx_ok flag can be set to true in which case overviews, or a
 * subset of image tiles may be used in computing the statistics.  
 * <p>
 * If force is false results will only be returned if it can be done 
 * quickly (ie. without scanning the data).  If force is false and 
 * results cannot be returned efficiently, the method will return CE_Warning
 * but no warning will have been issued.   This is a non-standard use of
 * the CE_Warning return value to indicate "nothing done". 
 * <p>
 * Note that file formats using PAM (Persistent Auxilary Metadata) services
 * will generally cache statistics in the .pam file allowing fast fetch
 * after the first request. 
 *
 * @param approx_ok If true statistics may be computed based on overviews
 * or a subset of all tiles. 
 * 
 * @param force If true statistics will only be returned if it can
 * be done without rescanning the image. 
 *
 * @param min Allocated array of one double into which to load image minimum (may be null).
 *
 * @param max Allocated array of one double into which to load image maximum (may be null).
 *
 * @param mean Allocated array of one double into which to load image mean (may be null).
 *
 * @param stddev Allocated array of one double into which to load image standard deviation 
 * (may be null).
 *
 * @return gdalconst.CE_None on success, gdalconst.CE_Warning if no values returned, 
 * gdalconst.CE_Failure if an error occurs.
 */
public class Band:public int GetStatistics(boolean approx_ok, boolean force, double[] min, double[] max, double[] mean, double[] stddev)

/**
 * Fetch image statistics. 
 *
 * Same as below but boolean value of true should be replaced with 1, and false with 0.
 *
 * @see #GetStatistics(boolean approx_ok, boolean force, double[] min, double[] max, double[] mean, double[] stddev)
 */
public class Band:public int GetStatistics(int approx_ok, int force, double[] min, double[] max, double[] mean, double[] stddev)

/**
 * Return raster unit type.
 *
 * Return a name for the units of this raster's values.  For instance, it
 * might be "m" for an elevation model in meters, or "ft" for feet.  If no 
 * units are available, a value of "" will be returned.
 *
 * @return unit name string.
 */
public class Band:public String GetUnitType()

/**
 * Fetch XSize of raster. 
 *
 * @return the width in pixels of this band.
 */
public class Band:public int getXSize()

/**
 * Fetch XSize of raster. 
 *
 * @return the width in pixels of this band.
 */

public class Band:public int GetXSize()

/**
 * Fetch YSize of raster. 
 *
 * @return the height in pixels of this band.
 */
public class Band:public int getYSize()

/**
 * Fetch YSize of raster. 
 *
 * @return the height in pixels of this band.
 */
public class Band:public int GetYSize()

/**
 * Check for arbitrary overviews.
 *
 * This returns true if the underlying datastore can compute arbitrary 
 * overviews efficiently, such as is the case with OGDI over a network. 
 * Datastores with arbitrary overviews don't generally have any fixed
 * overviews, but the RasterIO() method can be used in downsampling mode
 * to get overview data efficiently.
 *
 * @return true if arbitrary overviews available (efficiently), otherwise
 * false. 
 */
public class Band:public boolean HasArbitraryOverviews()

/**
 * Read a region of image data for this band.
 *
 * This method allows reading a region of a GDALRasterBand into a buffer.  It
 * automatically takes care of data type translation if the data type
 * (buf_type) of the buffer is different than that of the GDALRasterBand.
 * The method also takes care of image decimation / replication if the
 * buffer size (buf_xsize x buf_ysize) is different than the size of the
 * region being accessed (xsize x yszie).
 *<p>
 * The nPixelSpace and nLineSpace parameters allow reading into or
 * writing from unusually organized buffers.  This is primarily used
 * for buffers containing more than one bands raster data in interleaved
 * format. 
 *<p>
 * Some formats may efficiently implement decimation into a buffer by
 * reading from lower resolution overview images.
 * <p>
 * For highest performance full resolution data access, read and write
 * on "block boundaries" as returned by GetBlockSize(), or use the
 * ReadBlock() and WriteBlock() methods.
 *
 * @param xoff The pixel offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the left side.
 *
 * @param yoff The line offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the top.
 *
 * @param xsize The width of the region of the band to be accessed in pixels.
 *
 * @param ysize The height of the region of the band to be accessed in lines.
 *
 * @param buf_xsize the width of the buffer image into which the desired region is
 * to be read, or from which it is to be written.
 *
 * @param buf_ysize the height of the buffer image into which the desired region is
 * to be read, or from which it is to be written.
 *
 * @param buf_type the type of the pixel values in the pData data buffer.  The
 * pixel values will automatically be translated to/from the GDALRasterBand
 * data type as needed.
 *
 * @param nioBuffer The buffer into which the data should be read.
 * This buffer must contain at least buf_xsize *
 * buf_ysize words of type buf_type.  It is organized in left to right,
 * top to bottom pixel order.  Spacing is controlled by the nPixelSpace,
 * and nLineSpace parameters.
 *
 * @param nPixelSpace The byte offset from the start of one pixel value in
 * pData to the start of the next pixel value within a scanline.  If defaulted
 * (0) the size of the datatype eBufType is used.
 *
 * @param nLineSpace The byte offset from the start of one scanline in
 * pData to the start of the next.  If defaulted the size of the datatype
 * eBufType * nBufXSize is used.
 *
 * @return CE_Failure if the access fails, otherwise CE_None.
 */
public class Band:public int ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int nPixelSpace, int nLineSpace)

/**
 * Read a region of image data for this band.
 *
 * Same as below with nLineSpace == 0
 *
 * @see #ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int nPixelSpace, int nLineSpace)
 */
public class Band:public int ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int nPixelSpace)

/**
 * Read a region of image data for this band.
 *
 * Same as below with nPixelSpace == 0 and nLineSpace == 0
 *
 * @see #ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int nPixelSpace, int nLineSpace)
 */
public class Band:public int ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer)

/**
 * Read a region of image data for this band.
 *
 * Same as below with buf_type == gdalconst.GDT_Byte, nPixelSpace == 0 and nLineSpace == 0
 *
 * @see #ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int nPixelSpace, int nLineSpace)
 */
public class Band:public int ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, java.nio.ByteBuffer nioBuffer)

/**
 * Read a region of image data for this band.
 *
 * Same as below with buf_xsize = xsize, buf_ysize = ysize, buf_type == gdalconst.GDT_Byte, nPixelSpace == 0 and nLineSpace == 0
 *
 * @see #ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int nPixelSpace, int nLineSpace)
 */
public class Band:public int ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize, java.nio.ByteBuffer nioBuffer)

/**
 * Read a region of image data for this band.
 *
 * Same as below but buffer is allocated by the method
 *
 * @return a newly allocated byte buffer with the read region
 *
 * @see #ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize, java.nio.ByteBuffer nioBuffer)
 */
public class Band:public java.nio.ByteBuffer ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize)

/**
 * Read a region of image data for this band.
 *
 * Same as below with buf_xsize = xsize, buf_ysize = ysize, nPixelSpace == 0 and nLineSpace == 0 but buffer is allocated by the method
 *
 * @return a newly allocated byte buffer with the read region
 *
 * @see #ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int nPixelSpace, int nLineSpace)
 */
public class Band:public java.nio.ByteBuffer ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_type)

/**
 * Read a region of image data for this band.
 *
 * Same as below with nPixelSpace == 0 and nLineSpace == 0 but buffer is allocated by the method
 *
 * @return a newly allocated byte buffer with the read region
 *
 * @see #ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int nPixelSpace, int nLineSpace)
 */
public class Band:public java.nio.ByteBuffer ReadRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type)

/**
 * Write a region of image data for this band.
 *
 * This method allows writing data from a buffer into a region 
 * of the Band.  It
 * automatically takes care of data type translation if the data type
 * (buf_type) of the buffer is different than that of the GDALRasterBand.
 * The method also takes care of image decimation / replication if the
 * buffer size (buf_xsize x buf_ysize) is different than the size of the
 * region being accessed (xsize x yszie).
 *<p>
 * The nPixelSpace and nLineSpace parameters allow reading into or
 * writing from unusually organized buffers.  This is primarily used
 * for buffers containing more than one bands raster data in interleaved
 * format. 
 *<p>
 * Some formats may efficiently implement decimation into a buffer by
 * reading from lower resolution overview images.
 * <p>
 * For highest performance full resolution data access, read and write
 * on "block boundaries" as returned by GetBlockSize(), or use the
 * ReadBlock() and WriteBlock() methods.
 *
 * @param xoff The pixel offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the left side.
 *
 * @param yoff The line offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the top.
 *
 * @param xsize The width of the region of the band to be accessed in pixels.
 *
 * @param ysize The height of the region of the band to be accessed in lines.
 *
 * @param buf_xsize the width of the buffer image from which the desired region is to be written.
 *
 * @param buf_ysize the height of the buffer image from which the desired region is to be written.
 *
 * @param buf_type the type of the pixel values in the pData data buffer.  The
 * pixel values will automatically be translated to/from the GDALRasterBand
 * data type as needed.
 *
 * @param nioBuffer The buffer into which the data should be read.
 * This buffer must contain at least buf_xsize *
 * buf_ysize words of type buf_type.  It is organized in left to right,
 * top to bottom pixel order.  Spacing is controlled by the nPixelSpace,
 * and nLineSpace parameters.
 *
 * @param nPixelSpace The byte offset from the start of one pixel value in
 * pData to the start of the next pixel value within a scanline.  If defaulted
 * (0) the size of the datatype eBufType is used.
 *
 * @param nLineSpace The byte offset from the start of one scanline in
 * pData to the start of the next.  If defaulted the size of the datatype
 * eBufType * nBufXSize is used.
 *
 * @return CE_Failure if the access fails, otherwise CE_None.
 */
public class Band:public int WriteRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int nPixelSpace, int nLineSpace)

/**
 * Write a region of image data for this band.
 *
 * Same as below with nLineSpace == 0
 *
 * @see #WriteRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int nPixelSpace, int nLineSpace)
 */
public class Band:public int WriteRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int nPixelSpace)

/**
 * Write a region of image data for this band.
 *
 * Same as below with nPixelSpace == 0 and nLineSpace == 0
 *
 * @see #WriteRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int nPixelSpace, int nLineSpace)
 */
public class Band:public int WriteRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer)

/**
 * Write a region of image data for this band.
 *
 * Same as below with buf_type == gdalconst.GDT_Byte, nPixelSpace == 0 and nLineSpace == 0
 *
 * @see #WriteRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int nPixelSpace, int nLineSpace)
 */
public class Band:public int WriteRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, java.nio.ByteBuffer nioBuffer)

/**
 * Write a region of image data for this band.
 *
 * Same as below with buf_xsize == xsize, buf_ysize == ysize, nPixelSpace == 0 and nLineSpace == 0
 *
 * @see #WriteRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int nPixelSpace, int nLineSpace)
 */
public class Band:public int WriteRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_type, java.nio.ByteBuffer nioBuffer)

/**
 * Write a region of image data for this band.
 *
 * Same as below with buf_xsize == xsize, buf_ysize == ysize, buf_type == gdalconst.GDT_Byte, nPixelSpace == 0 and nLineSpace == 0
 *
 * @see #WriteRaster_Direct(int xoff, int yoff, int xsize, int ysize, int buf_xsize, int buf_ysize, int buf_type, java.nio.ByteBuffer nioBuffer, int nPixelSpace, int nLineSpace)
 */
public class Band:public int WriteRaster_Direct(int xoff, int yoff, int xsize, int ysize, java.nio.ByteBuffer nioBuffer)

/**
 * Read a block of image data efficiently.
 *
 * This method accesses a "natural" block from the raster band without
 * resampling, or data type conversion.  For a more generalized, but
 * potentially less efficient access use RasterIO().
 *
 * @param nXBlockOff the horizontal block offset, with zero indicating
 * the left most block, 1 the next block and so forth. 
 *
 * @param nYBlockOff the vertical block offset, with zero indicating
 * the left most block, 1 the next block and so forth.
 *
 * @param nioBuffer the buffer into which the data will be read.  The buffer
 * must be large enough to hold GetBlockXSize()*GetBlockYSize() words
 * of type GetRasterDataType().
 *
 * @return gdalconst.CE_None on success or gdalconst.CE_Failure on an error.
 *
 */
public class Band:public int ReadBlock_Direct(int nXBlockOff, int nYBlockOff, java.nio.ByteBuffer nioBuffer)

/**
 * Write a block of image data efficiently.
 *
 * This method accesses a "natural" block from the raster band without
 * resampling, or data type conversion.  For a more generalized, but
 * potentially less efficient access use RasterIO().
 *
 * @param nXBlockOff the horizontal block offset, with zero indicating
 * the left most block, 1 the next block and so forth. 
 *
 * @param nYBlockOff the vertical block offset, with zero indicating
 * the left most block, 1 the next block and so forth.
 *
 * @param nioBuffer the buffer from which the data will be written.  The buffer
 * must be large enough to hold GetBlockXSize()*GetBlockYSize() words
 * of type GetRasterDataType().
 *
 * @return gdalconst.CE_None on success or gdalconst.CE_Failure on an error.
 *
 */
public class Band:public int WriteBlock_Direct(int nXBlockOff, int nYBlockOff, java.nio.ByteBuffer nioBuffer)

/**
 * Set color interpretation of a band.
 *
 * @param eColorInterp the new color interpretation to apply to this band.
 * 
 * @return gdalconst.CE_None on success or gdalconst.CE_Failure if method is unsupported by format.
 */
public class Band:public int SetColorInterpretation(int eColorInterp)

/**
 * Set color interpretation of a band.
 *
 * @param eColorInterp the new color interpretation to apply to this band.
 * 
 * @return gdalconst.CE_None on success or gdalconst.CE_Failure if method is unsupported by format.
 */
public class Band:public int SetRasterColorInterpretation(int eColorInterp)

/**
 * Set the raster color table. 
 *
 * @param colorTable the color table to apply.  This may be null to clear the color 
 * table (where supported).
 *
 * @return gdalconst.CE_None on success, or gdalconst.CE_Failure on failure.  If the action is
 * unsupported by the driver, a value of CE_Failure is returned, but no
 * error is issued.
 */
public class Band:public int SetColorTable(ColorTable colorTable)

/**
 * Set the raster color table. 
 *
 * @param colorTable the color table to apply.  This may be null to clear the color 
 * table (where supported).
 *
 * @return gdalconst.CE_None on success, or gdalconst.CE_Failure on failure.  If the action is
 * unsupported by the driver, a value of CE_Failure is returned, but no
 * error is issued.
 */
public class Band:public int SetRasterColorTable(ColorTable colorTable)

/**
 * Set default histogram
 *
 */
public class Band:public int SetDefaultHistogram(double min, double max, int[] histogram)

/**
 * Set default Raster Attribute Table.
 *
 * Associates a default RAT with the band.  If not implemented for the
 * format a CPLE_NotSupported error will be issued.
 *
 * @param table the RAT to assign to the band.
 *
 * @return gdalconst.CE_None on success or gdalconst.CE_Failure if unsupported or otherwise 
 * failing.
 */
public class Band:public int SetDefaultRAT(RasterAttributeTable table)

/**
 * Set the no data value for this band. 
 *
 * To clear the nodata value, just set it with an "out of range" value.
 * Complex band no data values must have an imagery component of zero.
 *
 * @param nodataValue the value to set.
 *
 * @return gdalconst.CE_None on success, or gdalconst.CE_Failure on failure.  If unsupported
 * by the driver, CE_Failure is returned by no error message will have
 * been emitted.
 */
public class Band:public int SetNoDataValue(double nodataValue)

/**
 * Set the category names for this band.
 *
 * See the GetCategoryNames() method for more on the interpretation of
 * category names. 
 *
 * @param names a vector of strings with category names.  May
 * be ,ull to just clear the existing list. 
 *
 * @return gdalconst.CE_None on success, or gdalconst.CE_Failure on failure.  If unsupported
 * by the driver, CE_Failure is returned by no error message will have
 * been emitted.
 */
public class Band:public int SetRasterCategoryNames(java.util.Vector names)

/**
 * Set statistics on band.
 *
 * This method can be used to store min/max/mean/standard deviation
 * statistics on a raster band.  
 * <p>
 * The default implementation stores them as metadata, and will only work 
 * on formats that can save arbitrary metadata.  This method cannot detect
 * whether metadata will be properly saved and so may return CE_None even
 * if the statistics will never be saved.
 * 
 * @param min minimum pixel value.
 * 
 * @param max maximum pixel value.
 *
 * @param mean mean (average) of all pixel values.		
 *
 * @param stddev Standard deviation of all pixel values.
 *
 * @return gdalconst.CE_None on success or gdalconst.CE_Failure on failure. 
 */
public class Band:public int SetStatistics(double min, double max, double mean, double stddev)

/* Class Driver */

/**
 * Class Driver is an uninstanciable class providing various methods for a format specific driver.
 * <p>
 * An instance of this class is created for each supported format, and manages information about the format.
 * This roughly corresponds to a file format, though some drivers may be gateways to many formats through a secondary multi-library.
 * <p>
 * Drivers are loaded and registered with the gdal.<a href="./gdal.html#AllRegister()">AllRegister()</a> method
 */
public class org.gdal.gdal.Driver

/**
 * Create a new dataset with this driver.
 *
 * What argument values are legal for particular drivers is driver specific,
 * and there is no way to query in advance to establish legal values (except
 * querying driver.GetMetadataItem(gdalconst.DMD_CREATIONOPTIONLIST)
 * <p>
 * That function will try to validate the creation option list passed to the driver
 * with the GDALValidateCreationOptions() method. This check can be disabled
 * by defining the configuration option GDAL_VALIDATE_CREATION_OPTIONS=NO.
 * <p>
 * At the end of dataset manipulation, the delete() method <b>must</b> be called
 * on the returned dataset otherwise data might not be properly flushed to the disk.
 *
 * @param name the name of the dataset to create.
 * @param xsize width of created raster in pixels.
 * @param ysize height of created raster in pixels.
 * @param nBands number of bands.
 * @param eType type of raster.
 * @param options list of driver specific control parameters (may be null)
 *
 * @return null on failure, or a Dataset object
 */
public class org.gdal.gdal.Driver:public Dataset Create(String name, int xsize, int ysize, int nBands, int eType, String[] options)

/**
 * Create a new dataset with this driver.
 *
 * Same as below but options are passed as a Vector of String.
 *
 * @see #Create(String name, int xsize, int ysize, int nBands, int eType, String[] options)
 */
public class org.gdal.gdal.Driver:public Dataset Create(String name, int xsize, int ysize, int nBands, int eType, java.util.Vector options)

/**
 * Create a new dataset with this driver.
 *
 * Same as below with eType == gdalconst.GDT_Byte
 *
 * @see #Create(String name, int xsize, int ysize, int nBands, int eType, String[] options)
 */
public class org.gdal.gdal.Driver:public Dataset Create(String name, int xsize, int ysize, int nBands, String[] options)

/**
 * Create a new dataset with this driver.
 *
 * Same as below with options == null
 *
 * @see #Create(String name, int xsize, int ysize, int nBands, int eType, String[] options)
 */
public class org.gdal.gdal.Driver:public Dataset Create(String name, int xsize, int ysize, int nBands, int eType)

/**
 * Create a new dataset with this driver.
 *
 * Same as below with eType == gdalconst.GDT_Byte and options == null
 *
 * @see #Create(String name, int xsize, int ysize, int nBands, int eType, String[] options)
 */
public class org.gdal.gdal.Driver:public Dataset Create(String name, int xsize, int ysize, int nBands)

/**
 * Create a new dataset with this driver.
 *
 * Same as below with nbands == 1, eType == gdalconst.GDT_Byte and options == null
 *
 * @see #Create(String name, int xsize, int ysize, int nBands, int eType, String[] options)
 */
public class org.gdal.gdal.Driver:public Dataset Create(String name, int xsize, int ysize)

/**
 * Create a copy of a dataset.
 *
 * This method will attempt to create a copy of a raster dataset with the
 * indicated filename, and in this drivers format.  Band number, size, 
 * type, projection, geotransform and so forth are all to be copied from
 * the provided template dataset.  
 * <p>
 * Note that many sequential write once formats (such as JPEG and PNG) don't
 * implement the Create() method but do implement this CreateCopy() method.
 * If the driver doesn't implement CreateCopy(), but does implement Create()
 * then the default CreateCopy() mechanism built on calling Create() will
 * be used.
 * <p>
 * It is intended that CreateCopy() will often be used with a source dataset
 * which is a virtual dataset allowing configuration of band types, and
 * other information without actually duplicating raster data (see the VRT driver).
 * This is what is done by the gdal_translate utility for example.
 * <p>
 * That function will try to validate the creation option list passed to the driver
 * with the GDALValidateCreationOptions() method. This check can be disabled
 * by defining the configuration option GDAL_VALIDATE_CREATION_OPTIONS=NO.
 * <p>
 * At the end of dataset manipulation, the delete() method <b>must</b> be called
 * on the returned dataset otherwise data might not be properly flushed to the disk.
 *
 * @param name the name for the new dataset. 
 * @param src_ds the dataset being duplicated. 
 * @param strict 1 if the copy must be strictly equivelent, or more
 * normally 0 indicating that the copy may adapt as needed for the 
 * output format. 
 * @param options additional format dependent options controlling 
 * creation of the output file. 
 * @param callback for reporting algorithm progress. May be null
 *
 * @return a pointer to the newly created dataset (may be read-only access).
 */
public class org.gdal.gdal.Driver:public Dataset CreateCopy(String name, Dataset src_ds, int strict, java.util.Vector options, ProgressCallback callback)

/**
 * Create a copy of a dataset.
 *
 * Same as below with callback == null
 *
 * @see #CreateCopy(String name, Dataset src_ds, int strict, java.util.Vector options, ProgressCallback callback)
 */
public class org.gdal.gdal.Driver:public Dataset CreateCopy(String name, Dataset src_ds, int strict, java.util.Vector options)

/**
 * Create a copy of a dataset.
 *
 * Same as below with strict == 1 and callback == null
 *
 * @see #CreateCopy(String name, Dataset src_ds, int strict, java.util.Vector options, ProgressCallback callback)
 */
public class org.gdal.gdal.Driver:public Dataset CreateCopy(String name, Dataset src_ds, Vector options)

/**
 * Create a copy of a dataset.
 *
 * Same as below with options == null and callback == null
 *
 * @see #CreateCopy(String name, Dataset src_ds, int strict, java.util.Vector options, ProgressCallback callback)
 */
public class org.gdal.gdal.Driver:public Dataset CreateCopy(String name, Dataset src_ds, int strict)

/**
 * Create a copy of a dataset.
 *
 * Same as below with strict == 1, options == null and callback == null
 *
 * @see #CreateCopy(String name, Dataset src_ds, int strict, java.util.Vector options, ProgressCallback callback)
 */
public class org.gdal.gdal.Driver:public Dataset CreateCopy(String name, Dataset src_ds)

/**
 * Create a copy of a dataset.
 *
 * Same as below with callback == null and options as an array of strings
 *
 * @see #CreateCopy(String name, Dataset src_ds, int strict, java.util.Vector options, ProgressCallback callback)
 */
public class org.gdal.gdal.Driver:public Dataset CreateCopy(String name, Dataset src_ds, int strict, String[] options)

/**
 * Create a copy of a dataset.
 *
 * Same as below with strict == 1, callback == null and options as an array of strings
 *
 * @see #CreateCopy(String name, Dataset src_ds, int strict, java.util.Vector options, ProgressCallback callback)
 */
public class org.gdal.gdal.Driver:public Dataset CreateCopy(String name, Dataset src_ds, String[] options)

/**
 * Delete named dataset.
 *
 * The driver will attempt to delete the named dataset in a driver specific
 * fashion.  Full featured drivers will delete all associated files,
 * database objects, or whatever is appropriate.  The default behaviour when
 * no driver specific behaviour is provided is to attempt to delete the
 * passed name as a single file.
 * <p>
 * It is unwise to have open dataset handles on this dataset when it is
 * deleted.
 *
 * @param name name of dataset to delete.
 *
 * @return gdalconst.CE_None on success, or gdalconst.CE_Failure if the operation fails.
 */
public class org.gdal.gdal.Driver:public int Delete(String name)

/**
 * Rename a dataset.
 *
 * Rename a dataset. This may including moving the dataset to a new directory
 * or even a new filesystem.  
 * <p>
 * It is unwise to have open dataset handles on this dataset when it is
 * being renamed. 
 *
 * @param newName new name for the dataset.
 * @param oldName old name for the dataset.
 *
 * @return gdalconst.CE_None on success, or gdalconst.CE_Failure if the operation fails.
 */
public class org.gdal.gdal.Driver:public int Rename(String newName, String oldName)


/**
 * Register a driver for use.
 *
 * Normally this method is used by format specific C callable registration
 * entry points such as GDALRegister_GTiff() rather than being called
 * directly by application level code.
 * <p>
 * If this driver is already
 * registered, then no change is made, and the index of the existing driver
 * is returned.  Otherwise the driver list is extended, and the new driver
 * is added at the end.
 *
 * @return the index of the new installed driver.
 */
public class org.gdal.gdal.Driver:public int Register()

/**
 * Deregister the driver.
 */
public class org.gdal.gdal.Driver:public void Deregister()

/**
 * Return the short name of a driver.
 *
 * This is the string that can be
 * passed to the GDALGetDriverByName() function.
 * <p>
 * For the GeoTIFF driver, this is "GTiff"
 *
 * @return the short name of the driver.
 */
public class org.gdal.gdal.Driver:public String getShortName()

/**
 * Return the long name of a driver
 *
 * For the GeoTIFF driver, this is "GeoTIFF"
 *
 * @return the long name of the driver or empty string.
 */
public class org.gdal.gdal.Driver:public String getLongName()

/**
 * Return the URL to the help that describes the driver
 *
 * That URL is relative to the GDAL documentation directory.
 * <p>
 * For the GeoTIFF driver, this is "frmt_gtiff.html"
 *
 * @return the URL to the help that describes the driver or null
 */
public class org.gdal.gdal.Driver:public String getHelpTopic()