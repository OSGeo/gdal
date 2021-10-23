.. _rfc-22:

================================================================================
RFC 22: RPC Georeferencing
================================================================================

Author: Frank Warmerdam

Contact: warmerdam@pobox.com

Status: Adopted, Implemented

Summary
-------

It is proposed that GDAL support an additional mechanism for geolocation
of imagery based on rational polynomial coefficients (RPCs) represented
as metadata.

Many modern raw satellite products are distributed with RPCs, including
products from GeoEye, and DigitalGlobe. RPCs provide a higher systematic
description of georeferencing over an image, and also contain
information on the viewing geometry that in theory makes orthocorrection
(given a DEM) and some 3D operations like building height computation
possible.

RPC Domain Metadata
-------------------

Datasets with RPCs will include the following dataset level metadata
items in the "RPC" domain to identify the rational polynomials.

-  ERR_BIAS: Error - Bias. The RMS bias error in meters per horizontal
   axis of all points in the image (-1.0 if unknown)
-  ERR_RAND: Error - Random. RMS random error in meters per horizontal
   axis of each point in the image (-1.0 if unknown)
-  LINE_OFF: Line Offset
-  SAMP_OFF: Sample Offset
-  LAT_OFF: Geodetic Latitude Offset
-  LONG_OFF: Geodetic Longitude Offset
-  HEIGHT_OFF: Geodetic Height Offset
-  LINE_SCALE: Line Scale
-  SAMP_SCALE: Sample Scale
-  LAT_SCALE: Geodetic Latitude Scale
-  LONG_SCALE: Geodetic Longitude Scale
-  HEIGHT_SCALE: Geodetic Height Scale
-  LINE_NUM_COEFF (1-20): Line Numerator Coefficients. Twenty
   coefficients for the polynomial in the Numerator of the rn equation.
   (space separated)
-  LINE_DEN_COEFF (1-20): Line Denominator Coefficients. Twenty
   coefficients for the polynomial in the Denominator of the rn
   equation. (space separated)
-  SAMP_NUM_COEFF (1-20): Sample Numerator Coefficients. Twenty
   coefficients for the polynomial in the Numerator of the cn equation.
   (space separated)
-  SAMP_DEN_COEFF (1-20): Sample Denominator Coefficients. Twenty
   coefficients for the polynomial in the Denominator of the cn
   equation. (space separated)

These fields are directly derived from the document prospective GeoTIFF
RPC document at:

`http://geotiff.maptools.org/rpc_prop.html <http://geotiff.maptools.org/rpc_prop.html>`__

The line and pixel offset expressed with LINE_OFF and SAMP_OFF are with
respect to the center of the pixel (#5993)

Updating NITF Driver
--------------------

-  Already supports RPCs in this model, but will be modified to put them
   in the RPC domain instead of the primary metadata domain.
-  Add support for reading Digital Globe .RPB files.
-  No support for writing RPCs for now.

Updating GeoTIFF Driver
-----------------------

-  Upgrade to support reading Digital Globe .RPB files.
-  Possible support reading Space Imaging (GeoEye?) rpc.txt files.
-  Support reading RPC TIFF tag (per
   `http://geotiff.maptools.org/rpc_prop.html <http://geotiff.maptools.org/rpc_prop.html>`__)
-  Support writing RPC TIFF tag.
-  Support writing .RPB files (if RPB=YES or PROFILE not GDALGeoTIFF).

Changes to GenImgProj Transformer
---------------------------------

Currently it is difficult to reliably create a warp transformer based on
RPCs using GDALGenImgProjTransformer() as it will use a geotransform in
preference to RPCs if available. Many images with useful RPC information
also include a geotransform (approximate or accurate). It is therefore
proposed to modify the GDALCreateGenImgProjTransformer() function to
make it practical to provide more direction in the creation of the
transformer. The proposed new function is:

::

   void *
   GDALCreateGenImgProjTransformer2( GDALDatasetH hSrcDS, GDALDatasetH hDstDS, 
                                     char **papszOptions );

Supported Options:

-  SRC_SRS: WKT SRS to be used as an override for hSrcDS.
-  DST_SRS: WKT SRS to be used as an override for hDstDS.
-  GCPS_OK: If false, GCPs will not be used, default is TRUE.
-  MAX_GCP_ORDER: the maximum order to use for GCP derived polynomials
   if possible. The default is to autoselect based on the number of
   GCPs. A value of -1 triggers use of Thin Plate Spline instead of
   polynomials.
-  METHOD: may have a value which is one of GEOTRANSFORM,
   GCP_POLYNOMIAL, GCP_TPS, GEOLOC_ARRAY, RPC to force only one
   geolocation method to be considered on the source dataset.
-  RPC_HEIGHT: A fixed height to be used with RPC calculations.

This replaces the older function which did not include support for
passing arbitrary options, and was thus not easily extended. The old
function will be re-implemented with a call to the new functions.

The most important addition is the METHOD option which can be set to
specifically use one of the image to georeferenced coordinate system
methods instead of leaving it up to the code to pick the one it thinks
is best.

Changes to gdalwarp and gdaltransform
-------------------------------------

In order to facilitate passing transformer options into the updated
GDALCreateGenImgProjTransformer2(), the gdalwarp and gdaltransform
programs (built on this function) will be updated to include a -to
(transformer option) switch, and to use the new function.

Preserving Geolocation Through Translation
------------------------------------------

The RPC information needs to be copied and preserved through
translations that do not alter the spatial arrangement of the data. To
that end RPC metadata copying will be added to:

-  VRT driver's CreateCopy().
-  GDALDriver's default CreateCopy().
-  GDALPamDataset::CopyInfo()
-  gdal_translate will be updated to copy RPC metadata to the
   intermediate internal VRT if, and only if, no resizing or subsetting
   is being done.

Changes to RPC Transformer
--------------------------

-  Implement iterative "back transform" from pixel/line to
   lat/long/height instead of simple linear approximator.
-  Add support for RPC_HEIGHT offset, so all Z values to transformer are
   assumed to be relative to this offset (normally really and average
   elevation for the scene).
-  Make RPC Transformer serializable (in VRT files, etc).

Backward Compatibility Issues
-----------------------------

Previously the NITF driver returned RPC metadata in the default domain.
With the implementation of this RFC for GDAL 1.6.0 any applications
using this metadata would need to consult the RPC domain instead. The
RPC\_ prefix on the metadata values has also been removed.

The GDALCreateGenImgProjTransformer() function is preserved, so no
compatibility issues are anticipated by the addition of the new
generalized factory.

SWIG Bindings Issues
--------------------

-  The raw access is by the established metadata api, so no changes are
   needed for this.
-  The Warp API is only bound at a high level, so there should be no
   changes in this regard.
-  For testing purposes it is desirable to provide a binding around the
   GDAL transformer API. The following planned binding is based loosely
   on OGRCoordinateTransformation API binding. So far I have only found
   the TransformPoint( bDstToSrc, x, y, z ) entry point to be useful in
   Python and even that ends up returning a (bSuccess, (x, y, z)) result
   which is somewhat awkward. Is there a better way of doing this?

::

   /************************************************************************/
   /*                             Transformer                              */
   /************************************************************************/

   %rename (Transformer) GDALTransformerInfoShadow;
   class GDALTransformerInfoShadow {
   private:
     GDALTransformerInfoShadow();
   public:
   %extend {

     GDALTransformerInfoShadow( GDALDatasetShadow *src, GDALDatasetShadow *dst,
                                char **options ) {
       GDALTransformerInfoShadow *obj = (GDALTransformerInfoShadow*) 
          GDALCreateGenImgProjTransformer2( (GDALDatasetH)src, (GDALDatasetH)dst, 
                                            options );
       return obj;
     }

     ~GDALTransformerInfoShadow() {
       GDALDestroyTransformer( self );
     }

   // Need to apply argin typemap second so the numinputs=1 version gets applied
   // instead of the numinputs=0 version from argout.
   %apply (double argout[ANY]) {(double inout[3])};
   %apply (double argin[ANY]) {(double inout[3])};
     int TransformPoint( int bDstToSrc, double inout[3] ) {
       int nRet, nSuccess = TRUE;

       nRet = GDALUseTransformer( self, bDstToSrc, 
                                  1, &inout[0], &inout[1], &inout[2], 
                                  &nSuccess );

       return nRet && nSuccess;
     }
   %clear (double inout[3]);

     int TransformPoint( double argout[3], int bDstToSrc, 
                         double x, double y, double z = 0.0 ) {
       int nRet, nSuccess = TRUE;
       
       argout[0] = x;
       argout[1] = y;
       argout[2] = z;
       nRet = GDALUseTransformer( self, bDstToSrc, 
                                  1, &argout[0], &argout[1], &argout[2], 
                                  &nSuccess );

       return nRet && nSuccess;
     }
     
   #ifdef SWIGCSHARP
     %apply (double *inout) {(double*)};
     %apply (double *inout) {(int*)};
   #endif
     int TransformPoints( int bDstToSrc, 
                          int nCount, double *x, double *y, double *z,
                          int *panSuccess ) {
       int nRet;

       nRet = GDALUseTransformer( self, bDstToSrc, nCount, x, y, z, panSuccess );

       return nRet;
     }
   #ifdef SWIGCSHARP
     %clear (double*);
     %clear (int*);
   #endif

   } /*extend */
   };

Documentation
-------------

In addition to standard API documentation, the RPC metadata mechanism
will be introduced into the "GDAL Data Model" document.

Implementation
--------------

This work will be implemented by Frank Warmerdam with support from the
Canadian Nuclear Safety Commission.

Testing
-------

-  A test script for the transformer API covering RPC, GCP_TPS,
   GCP_POLYNOMIAL, GEOLOC and GEOTRANSFORM methods will be implemented.
-  A test script for reading and writing RPB, and GeoTIFF RPC tags will
   be written.
