.. _rfc-94:

=============================================================
RFC 94: Numeric fields width/precision metadata
=============================================================

============== =============================================
Author:        Alessandro Pasotti
Contact:       elpaso @ itopen.it
Started:       2023-Feb-17
Status:        Development
Target:        GDAL 3.7
============== =============================================

Summary
-------

The document proposes and describes the introduction of a couple of new
vector driver metadata that return information for numeric real
fields about how the precision and width have been calculated by the
driver and have to be interpreted.

Motivation
----------

Applications (for example OGR/GDAL utils and QGIS) may require information
about the field width and precision in order to convert between different
formats or to validate user input on editing.

Different drivers may calculate the width and precision differently by including
or not the decimal separator and/or the minus sign into the calculated length.

There is currently no way for an application to access this information and this
can lead to loss of width or precision while converting between formats or when the
application decides to stay on the safe side and reduce the width reported by GDAL.

Additionally, attention must be paid to the meaning of "width" and "precision":
OGR "width" corresponds to SQL "precision" and OGR "precision" corresponds to SQL "scale".


For reference:

- https://trac.osgeo.org/gdal/ticket/6960
- https://issues.qgis.org/issues/11755
- https://issues.qgis.org/issues/15188#note-8
- https://github.com/qgis/QGIS/issues/51849


Current drivers behavior
------------------------

Here is a list of the drivers and how they behave with respect to the width and precision,
(for databases we refer to the `NUMERIC` data type).

================== ================================== =====================
 Driver             Width Includes Decimal Separator   Width Includes Sign
================== ================================== =====================
 Shapefile          YES                                YES
 MapInfo            YES                                YES
 PostgreSQL         NO                                 NO
 MySQL              NO                                 NO
 MSSQL              NO                                 NO
 OCI                NO                                 NO
 GPKG               N/A                                N/A
 CSV (from .csvt)   YES                                YES
 HANA               ?                                  ?
 FlatGeoBuf         NO                                 NO
 FileGDB            N/A                                N/A
 GML                NO                                 NO
 MEM                N/A                                N/A
================== ================================== =====================


Notes about specific drivers
............................

+ GPKG: SQLite column affinity storage is 8-byte IEEE floating point number
+ GML: `xsd:decimal` with `totalDigits` and `fractionDigits`, `xs:totalDigits`
  defines the maximum number of digits of decimal and derived datatypes
  (both after and before the decimal point, not counting the decimal point itself).
  `xs:fractionDigits`` defines the maximum number of fractional digits (i.e.,
  digits that are after the decimal point) of an xs:decimal datatype.
+ FlatGeoBuf: for Float fields, OGR_width = flatgeobuf_precision and OGR_precision = flatgeobuf_scale
  (if flatgeobuf_scale != -1, or 0 if flatgeobuf_scale == -1)
+ FileGDB: Scale is the number of digits to the right of the decimal point in a number.
  For example, the number 56.78 has a scale of 2. Scale applies only to fields that are double.
  Scale is always returned as 0 from personal or File geodatabase fields.
  Precision is the number of digits in a number. For example, the number 56.78 has a precision of 4.
  Precision is only valid for fields that are numeric. Precision is always returned as 0 from personal or
  File geodatabase fields

Technical details
-----------------

The vector drivers will expose a metadata entry to define if the width
of the fields includes the decimal separator and/or the sign.

If the metadata entry is undefined the feature is not supported (there is no
client-accessible width or precision constraint for numeric fields).

Example API:

.. code-block:: c++

    /** Capability set by a vector driver that supports field width and precision.
    *
    * This capability reflects that a vector driver includes the decimal separator
    * in the field width.
    *
    * See GDAL_DMD_NUMERIC_FIELD_WIDTH_INCLUDES_SIGN for a related capability flag.
    * @since GDAL 3.7
    */
    #define GDAL_DMD_NUMERIC_FIELD_WIDTH_INCLUDES_DECIMAL_SEPARATOR "DMD_NUMERIC_FIELD_WIDTH_INCLUDES_DECIMAL_SEPARATOR"

    /** Capability set by a vector driver that supports field width and precision.
    *
    * This capability reflects that a vector driver includes the sign
    * in the field width.
    *
    * See GDAL_DMD_NUMERIC_FIELD_WIDTH_INCLUDES__DECIMAL_SEPARATOR for a related capability flag.
    * @since GDAL 3.7
    */
    #define GDAL_DMD_NUMERIC_FIELD_WIDTH_INCLUDES_SIGN "DMD_NUMERIC_FIELD_WIDTH_INCLUDES_SIGN"



Efficiency considerations
--------------------------

None.


Backward compatibility
----------------------

None.

SWIG Bindings
-------------

This implementation will be exposed to bindings.

Testing
-------

A C++ test will be added to the test suite.


Voting history
--------------


