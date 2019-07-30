.. _rfc-39:

=========================================================================
RFC 39: OGR Layer Algebra
=========================================================================

Author: Ari Jolma

Contact: ari dot jolma at aalto dot fi

Status: Adopted, implemented in GDAL 1.10

Summary
-------

It is proposed that the OGR layer class and the C API contains methods
for commonly needed overlay analysis methods.

The basic functionality for spatial analysis with GDAL is provided by
GEOS. However, GEOS operates on geometries and typically people work
with geospatial data layers. Vector data layers are represented in GDAL
by OGRLayer objects. Thus, there is a need for spatial analaysis
operations that work on layers.

Unfortunately there is no standard for spatial analysis operations API,
but it is possible to create a useful set by using existing software as
example.

The methods are fundamentally dependent on comparison of all the
features of two layers. There would possibly be huge performance
improvements achievable with layer specific spatial indexes. This is
considered out of the scope of these methods and belonging to the
general problem of iterating features in a layer and accessing features
randomly. For these reasons these methods should be only considered
convenience methods and not replacements for analysis in relational
databases for example.

Implementation
--------------

The methods are implemented by new methods in OGRLayer class
(ogrsf_frmts.h and ogrlayer.cpp) and new calls in the C API (ogr_api.h).
The Swig bindings (ogr.i) are also extended with these methods.

The patch with the changes to OGR core and to the Swig bindings is
attached to this page. The patch has been superficially tested but it is
not written or formatted according to the GDAL tradition.

Backward Compatibility
----------------------

Proposed additions will have an impact on C binary compatibility because
they change the API.

C++ binary interface will be broken (due to the addition of a new
members in OGRLayer class).

The changes are purely extensions and have no impact on existing code.

Impact on drivers
-----------------

The changes do not have any impacts on drivers.

Timeline
--------

Ari Jolma is responsible to implement this proposal. New API should be
available in GDAL 1.11.

There needs to be a discussion on the names of the methods and on the
internal logic of the methods (this refers especially to the handling of
attributes and error conditions).

In addition to the methods in the attached patch, there should be some
discussion on additional methods. For example Append and Buffer methods
could be easily added to the set. An illustration of what is currently
available in the common software is for example this page:
`http://courses.washington.edu/gis250/lessons/Model_Builder/ <http://courses.washington.edu/gis250/lessons/Model_Builder/>`__

Comments on performance
-----------------------

Profiling Intersection of a layer of 46288 line string features with a
layer of one polygon feature (~1/3 of features within and many only
partly within the one feature) showed that when the method layer was a
Shapefile, most of the time was spent in reading the feature from the
Shapefile. When the method layer was copied into memory, most of the
time (83 %) was spent in OGRLineString::getEnvelope. The 6th version of
the patch contains a test against a pre-computed layer envelope, which
speeds up the computation in this case ~30% (from 2.44 s to 1.76 in my
machine). Still the most of the time (82 %) is spent in
OGRLineString::getEnvelope.

Voting history
--------------

(June 2012) +1 from Even, Frank, Howard, Tamas, Daniel
