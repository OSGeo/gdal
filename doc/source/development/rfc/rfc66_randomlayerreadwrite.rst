.. _rfc-66:

=======================================================================================
RFC 66 : OGR random layer read/write capabilities
=======================================================================================

Author: Even Rouault

Contact: even.rouault at spatialys.com

Status: Implemented

Implementing version: 2.2

Summary
-------

This RFC introduces a new API to be able to iterate over vector features
at dataset level, in addition to the existing capability of doing it at
the layer level. The existing capability of writing features in layers
in random order, that is supported by most drivers with output
capabilities, is formalized with a new dataset capability flag.

Rationale
---------

Some vector formats mix features that belong to different layers in an
interleaved way, which make the current feature iteration per layer
rather inefficient (this requires for each layer to read the whole
file). One example of such drivers is the OSM driver. For this driver, a
hack had been developed in the past to be able to use the
OGRLayer::GetNextFeature() method, but with a really particular
semantics. See "Interleaved reading" paragraph of :ref:`vector.osm` for more
details. A similar need arises with the development of a new driver,
GMLAS (for GML Application Schemas), that reads GML files with arbitrary
element nesting, and thus can return them in a apparent random order,
because it works in a streaming way. For example, let's consider the
following simplified XML content :

::

   <A>
       ...
       <B>
           ...
       </B>
       ...
   </A>

The driver will be first able to complete the building of feature B
before emitting feature A. So when reading sequences of this pattern,
the driver will emit features in the order B,A,B,A,...

Changes
-------

C++ API
~~~~~~~

Two new methods are added at the GDALDataset level :

GetNextFeature():

::

   /**
    \brief Fetch the next available feature from this dataset.

    The returned feature becomes the responsibility of the caller to
    delete with OGRFeature::DestroyFeature().

    Depending on the driver, this method may return features from layers in a
    non sequential way. This is what may happen when the
    ODsCRandomLayerRead capability is declared (for example for the
    OSM and GMLAS drivers). When datasets declare this capability, it is strongly
    advised to use GDALDataset::GetNextFeature() instead of
    OGRLayer::GetNextFeature(), as the later might have a slow, incomplete or stub
    implementation.
    
    The default implementation, used by most drivers, will
    however iterate over each layer, and then over each feature within this
    layer.

    This method takes into account spatial and attribute filters set on layers that
    will be iterated upon.

    The ResetReading() method can be used to start at the beginning again.

    Depending on drivers, this may also have the side effect of calling
    OGRLayer::GetNextFeature() on the layers of this dataset.

    This method is the same as the C function GDALDatasetGetNextFeature().

    @param ppoBelongingLayer a pointer to a OGRLayer* variable to receive the
                             layer to which the object belongs to, or NULL.
                             It is possible that the output of *ppoBelongingLayer
                             to be NULL despite the feature not being NULL.
    @param pdfProgressPct    a pointer to a double variable to receive the
                             percentage progress (in [0,1] range), or NULL.
                             On return, the pointed value might be negative if
                             determining the progress is not possible.
    @param pfnProgress       a progress callback to report progress (for
                             GetNextFeature() calls that might have a long duration)
                             and offer cancellation possibility, or NULL
    @param pProgressData     user data provided to pfnProgress, or NULL
    @return a feature, or NULL if no more features are available.
    @since GDAL 2.2
   */

   OGRFeature* GDALDataset::GetNextFeature( OGRLayer** ppoBelongingLayer,
                                            double* pdfProgressPct,
                                            GDALProgressFunc pfnProgress,
                                            void* pProgressData )

and ResetReading():

::

   /** 
    \brief Reset feature reading to start on the first feature.

    This affects GetNextFeature().

    Depending on drivers, this may also have the side effect of calling
    OGRLayer::ResetReading() on the layers of this dataset.

    This method is the same as the C function GDALDatasetResetReading().
    
    @since GDAL 2.2
   */
   void        GDALDataset::ResetReading();

New capabilities
~~~~~~~~~~~~~~~~

The following 2 new dataset capabilities are added :

::

   #define ODsCRandomLayerRead     "RandomLayerRead"   /**< Dataset capability for GetNextFeature() returning features from random layers */
   #define ODsCRandomLayerWrite    "RandomLayerWrite " /**< Dataset capability for supporting CreateFeature on layer in random order */

C API
~~~~~

The above 2 new methods are available in the C API with :

::

   OGRFeatureH CPL_DLL GDALDatasetGetNextFeature( GDALDatasetH hDS,
                                                  OGRLayerH* phBelongingLayer,
                                                  double* pdfProgressPct,
                                                  GDALProgressFunc pfnProgress,
                                                  void* pProgressData )

   void CPL_DLL GDALDatasetResetReading( GDALDatasetH hDS );

Discussion about a few design choices of the new API
----------------------------------------------------

Compared to OGRLayer::GetNextFeature(), GDALDataset::GetNextFeature()
has a few differences :

-  it returns the layer which the feature belongs to. Indeed, there's no
   easy way from a feature to know which layer it belongs too (since in
   the data model, features can exist outside of any layer). One
   possibility would be to correlate the OGRFeatureDefn\* object of the
   feature with the one of the layer, but that is a bit inconvenient to
   do (and theoretically, one could imagine several layers sharing the
   same feature definition object, although this probably never happen
   in any in-tree driver).
-  even if the feature returned is not NULL, the returned layer might be
   NULL. This is just a provision for now, since that cannot currently
   happen. This could be interesting to address schema-less datasources
   where basically each feature could have a different schema (GeoJSON
   for example) without really belonging to a clearly identified layer.
-  it returns a progress percentage. When using OGRLayer API, one has to
   count the number of features returned with the total number returned
   by GetFeatureCount(). For the use cases we want to address knowing
   quickly the total number of features of the dataset is not doable.
   But knowing the position of the file pointer regarding the total size
   of the size is easy. Hence the decision to make GetNextFeature()
   return the progress percentage. Regarding the choice of the range
   [0,1], this is to be consistent with the range accepted by GDAL
   progress functions.
-  it accepts a progress and cancellation callback. One could wonder why
   this is needed given that GetNextFeature() is an "elementary" method
   and that it can already returns the progress percentage. However, in
   some circumstances, it might take a rather long time to complete a
   GetNextFeature() call. For example in the case of the OSM driver, as
   an optimization you can ask the driver to return features of a subset
   of layers. For example all layers except nodes. But generally the
   nodes are at the beginning of the file, so before you get the first
   feature, you have typically to process 70% of the whole file. In the
   GMLAS driver, the first GetNextFeature() call is also the opportunity
   to do a preliminary quick scan of the file to determine the SRS of
   geometry columns, hence having progress feedback is welcome.

The progress percentage output is redundant with the progress callback
mechanism, and the latter could be used to get the former, however it
may be a bit convoluted. It would require doing things like:

::

   int MyProgress(double pct, const char* msg, void* user_data)
   {
       *(double*)user_data = pct;
       return TRUE;
   }

   myDS->GetNextFeature(&poLayer, MyProgress, &pct)

SWIG bindings (Python / Java / C# / Perl) changes
-------------------------------------------------

GDALDatasetGetNextFeature is mapped as gdal::Dataset::GetNextFeature()
and GDALDatasetResetReading as gdal::Dataset::ResetReading().

Regarding gdal::Dataset::GetNextFeature(), currently only Python has
been modified to return both the feature and its belonging layer. Other
bindings just return the feature for now (would need specialized
typemaps)

Drivers
-------

The OSM and GMLAS driver are updated to implement the new API.

Existing drivers that support ODsCRandomLayerWrite are updated to
advertise it (that is most drivers that have layer creation
capabilities, with the exceptions of KML, JML and GeoJSON).

Utilities
---------

ogr2ogr / GDALVectorTranslate() is changed internally to remove the hack
that was used for the OSM driver to use the new API, when
ODsCRandomLayerRead is advertized. It checks if the output driver
advertises ODsCRandomLayerWrite, and if it does not, emit a warning, but
still goes on proceeding with the conversion using random layer
reading/writing.

ogrinfo is extended to accept a -rl (for random layer) flag that
instructs it to use the GDALDataset::GetNextFeature() API. It was
considered to use it automatically when ODsCRandomLayerRead was
advertized, but the output can be quite... random and thus not very
practical for the user.

Documentation
-------------

All new methods/functions are documented.

Test Suite
----------

The specialized GetNextFeature() implementation of the OSM and GMLAS
driver is tested in their respective tests. The default implementation
of GDALDataset::GetNextFeature() is tested in the MEM driver tests.

Compatibility Issues
--------------------

None for existing users of the C/C++ API.

Since there is a default implementation, the new functions/methods can
be safely used on drivers that don't have a specialized implementation.

The addition of the new virtual methods GDALDataset::ResetReading() and
GDALDataset::GetNextFeature() may cause issues for out-of-tree drivers
that would already use internally such method names, but with different
semantics, or signatures. We have encountered such issues with a few
in-tree drivers, and fixed them.

Implementation
--------------

The implementation will be done by Even Rouault, and is mostly triggered
by the needs of the new GMLAS driver (initial development funded by the
European Earth observation programme Copernicus).

The proposed implementation is in
`https://github.com/rouault/gdal2/tree/gmlas_randomreadwrite <https://github.com/rouault/gdal2/tree/gmlas_randomreadwrite>`__
(commit:
`https://github.com/rouault/gdal2/commit/8447606d68b9fac571aa4d381181ecfffed6d72c <https://github.com/rouault/gdal2/commit/8447606d68b9fac571aa4d381181ecfffed6d72c>`__)

Voting history
--------------

+1 from TamasS, HowardB, JukkaR, DanielM and EvenR.
