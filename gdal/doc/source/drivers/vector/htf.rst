.. _vector.htf:

HTF - Hydrographic Transfer Format
==================================

.. shortname:: HTF

.. built_in_by_default::

This driver reads files containing sounding data following the
Hydrographic Transfer Format (HTF), which is used by the Australian
Hydrographic Office (AHO).

The driver has been developed based on HTF 2.02 specification.

The file must be georeferenced in UTM WGS84 to be considered valid by
the driver.

The driver returns 2 spatial layers : a layer named "polygon" and a
layer name "sounding". There is also a "hidden" layer, called
"metadata", that can be fetched with GetLayerByName(), and which
contains a single feature, made of the header lines of the file.

Polygons are used to distinguish between differing survey categories,
such that any significant changes in position/depth accuracy and/or a
change in the seafloor coverage will dictate a separate bounding polygon
contains polygons.

The "polygon" layer contains the following fields :

-  *DESCRIPTION* : Defines the polygons of each region of similar survey
   criteria or theme.
-  *IDENTIFIER* : Unique polygon identifier for this transmittal.
-  *SEAFLOOR_COVERAGE* : All significant seafloor features detected
   (full ensonification/sweep) or full coverage not achieved and
   uncharted features may exist.
-  *POSITION_ACCURACY* : +/- NNN.n meters at 95% CI (2.45) with respect
   to the given datum.
-  *DEPTH_ACCURACY* : +/- NN.n meters at 95% CI (2.00) at critical
   depths.

The "sounding" layer should contain - at minimum - the following 20
fields :

-  *REJECTED_SOUNDING* : if 0 sounding is valid or if 1 the sounding has
   been rejected (flagged).
-  *LINE_NAME* : Survey line name/number as a unique identifier within
   the survey.
-  *FIX_NUMBER* : Sequential sounding fix number, unique within the
   survey.
-  *UTC_DATE* : UTC date for the sounding CCYYMMDD.
-  *UTC_TIME* : UTC time for the sounding HHMMSS.ss.
-  *LATITUDE* : Latitude position of the sounding +/-NN.nnnnnn (degrees
   of arc, south is negative).
-  *LONGITUDE* : Longitude position of the sounding +/-NNN.nnnnnn
   (degrees of arc, west is negative).
-  *EASTING* : Grid coordinate position of the sounding in meters
   NNNNNNN.n.
-  *NORTHING* : Grid coordinate position of the sounding in meters
   NNNNNNN.n.
-  *DEPTH* : Reduced sounding value in meters with corrections applied
   as indicated in the relevant fields, soundings are positive and
   drying heights are negative +/-NNNN.nn meters.
-  *POSITIONING_SENSOR* : Indicate position system number populated in
   the HTF header record.
-  *DEPTH_SENSOR* : Indicate depth sounder system number populated in
   the HTF header record.
-  *TPE_POSITION* : Total propagated error of the horizontal component
   for the sounding.
-  *TPE_DEPTH* : Total propagated error of the vertical component for
   the sounding.
-  *NBA FLAG* : No Bottom at Flag, if 0 not NBA depth or if 1 Depth is
   NBA, deeper water probably exists.
-  *TIDE* : Value of the tidal correction applied +/- NN.nn meters.
-  *DEEP_WATER_CORRECTION* : Value of the deep water sounding velocity
   applied +/- NN.nn meters.
-  *VERTICAL BIAS_CORRECTION* : Value of the vertical bias applied +/-
   NN.nn meters. eg transducer depth correction
-  *SOUND_VELOCITY* : Measured sound velocity used to process sounding
   in meters per second IIII.
-  *PLOTTED_SOUNDING* : if 0 then the reduced depth did not appear on
   the original fairsheet or id 1 then the reduced depth appeared on the
   original fairsheet.

Some fields may be never set, depending on the value of the Field
Population Key. Extra fields may also be added.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

See Also
--------

-  `HTF - Hydrographic Transfer Format home
   page <http://www.hydro.gov.au/tools/htf/htf.htm>`__
-  `HTF Technical
   Specification <http://www.hydro.gov.au/tools/htf/htf.pdf>`__
