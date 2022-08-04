.. _rfc-13:

================================================================================
RFC 13: Improved Feature Insertion/Update/Delete Performance in Batch Mode
================================================================================

::

   Author: Konstantin Baumann

   Contact: konstantin.baumann@hpi.uni-potsdam.de

   Status: Withdrawn

*Withdrawn*

I have withdrawn this RFC based on some comments from
`Frank <http://lists.osgeo.org/pipermail/gdal-dev/2007-May/013132.html>`__
and
`Tamas <http://lists.osgeo.org/pipermail/gdal-dev/2007-May/013130.html>`__
on GDAL-dev.

*Summary*

Some OGR drivers can dramatically increase the speed of and optimize the
insertion, update, and deletion of a set of features, if the driver
knows, that there is a whole set of features that should/could be
inserted, updated, or deleted at once (instead of just one by one).

*CreateFeatures()*

The following new virtual method is added to the OGRLayer class, with an
analogous C function:

::

   virtual OGRErr CreateFeatures( OGRFeature** papoFeatures, int iFeatureCount );

A default implementation is given as below:

::

   OGRErr OGRLayer::CreateFeatures(
       OGRFeature **papoFeatures,
       int iFeatureCount
   ) {
       for(int i = 0; i < iFeatureCount; ++i) {
           OGRErr error = CreateFeature( papoFeatures[i] );
           if( error != OGRERR_NONE ) return error;
       }
       return OGRERR_NONE;
   }

This triggers the old behavior of an unoptimized insertion.

Individual drivers can override the default implementation and can
implement an optimized algorithm for inserting a set of features.

*SetFeatures()*

The following new virtual method is added to the OGRLayer class, with an
analogous C function:

::

   virtual OGRErr SetFeatures( OGRFeature** papoFeatures, int iFeatureCount );

A default implementation is given as below:

::

   OGRErr OGRLayer::SetFeatures(
       OGRFeature **papoFeatures,
       int iFeatureCount
   ) {
       for(int i = 0; i < iFeatureCount; ++i) {
           OGRErr error = SetFeature( papoFeatures[i] );
           if( error != OGRERR_NONE ) return error;
       }
       return OGRERR_NONE;
   }

This triggers the old behavior of an unoptimized update.

Individual drivers can override the default implementation and can
implement an optimized algorithm for updating a set of features.

*DeleteFeatures()*

The following new virtual method is added to the OGRLayer class, with an
analogous C function:

::

   virtual OGRErr DeleteFeatures( long *panFIDs, int iFIDCount );

A default implementation is given as below:

::

   OGRErr OGRLayer::DeleteFeatures(
       long *panFIDs,
       int iFIDCount
   ) {
       for(int i = 0; i < iFIDCount; ++i) {
           OGRErr error = DeleteFeature( panFIDs[i] );
           if( error != OGRERR_NONE ) return error;
       }
       return OGRERR_NONE;
   }

This triggers the old behavior of an unoptimized deletion.

Individual drivers can override the default implementation and can
implement an optimized algorithm for deleting a set of features.

*C API functions*

The following C functions are added:

::

   OGRErr OGR_L_CreateFeatures( OGRFeature** papoFeatures, int iFeatureCount );
   OGRErr OGR_L_SetFeatures( OGRFeature** papoFeatures, int iFeatureCount );
   OGRErr OGR_L_DeleteFeatures( long* panFIDs, int iFIDCount );

However, there are some issues with adding plain C arrays to the public
OGR interface due to the SWIG based wrapping, see for example `GDAL-Dev
Mail from
Tamas <http://lists.gdal.org/pipermail/gdal-dev/2007-May/013092.html>`__...

*Additional Notes*

Based in this new interface functions, I was able to increase the
insertion speed of features in the MySQL driver from 40 per second to up
to 800-2000 per second. I think other drivers can benefit from this
change, too.

See also ticket #1633.

*Implementation Plan*

A patch for the describe additions can be trivially provided.

I can provide another patch based on this interface which contains the
optimized implementation for the MySQL driver.

*History*

14-May-2007: initial version created

15-May-2007: SetFeatures() added

16-May-2007: DeleteFeatures() added

17-May-2007: C API functions added; SWIG wrapping issues mentioned

23-May-2007: Withdrawn due some concerns on GDAL-dev
