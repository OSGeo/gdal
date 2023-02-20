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

The document proposes and describes the introduction of a new method
to the `OGRFieldDefn` class that returns information for numeric real
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
 MapInfo            ?                                  ?
 PostgreSQL         NO                                 NO
 MySQL              NO                                 NO
 MSSQL              NO                                 NO
 OCI                NO                                 NO
 GPKG               N/A                                N/A
 CSV (from .csvt)   YES                                YES
 HANA               ?                                  ?
 FlatGeoBuf         ?                                  ?
 FileGDB            ?                                  ?
 GML                ?                                  ?
 MEM                ?                                  ?
================== ================================== =====================


Notes about specific drivers
............................

+ GPKG: SQLite column affinity storage is 8-byte IEEE floating point number
+ GML: `xsd:decimal` with `totalDigits` and `fractionDigits`, I could not find any detail in the specs.


Technical details
-----------------

The implementation details are still to be defined, a possible
approach would be to define a flag with values to define if
the decimal place is included in the width and if the minus sign
is included in the width.

Additional flag values related to the field definition can be added if
needed.

Example API:

.. code-block:: c++

    /** Field width includes decimal separator */
    #define OGR_F_WIDTH_INCLUDES_DECIMAL_SEPARATOR 0x1

    /** Field width includes sign */
    #define OGR_F_WIDTH_INCLUDES_SIGN 0x2


    /************************************************************************/
    /*                        OGR_Fld_GetWidthPrecisionFlags()              */
    /************************************************************************/
    /**
    * \brief Returns flags representing information about how the width and
    *        precision for this field must be interpreted.
    *
    * This function returns zero for fields of types other than OFTReal or OFTRealList.
    *
    * This function is the same as the CPP method OGRFieldDefn::GetWidthPrecisionFlags().
    *
    * @param hDefn handle to the field definition to get width and precision flags from.
    * @return the width and precision flags.
    */

    int OGR_Fld_GetWidthPrecisionFlags(OGRFieldDefnH hDefn)

    {
        return OGRFieldDefn::FromHandle(hDefn)->GetWidthPrecisionFlags();
    }

    /************************************************************************/
    /*                        OGR_Fld_SetWidthPrecisionFlags()              */
    /************************************************************************/
    /**
    * \brief Set the flags representing information about how the width and
    *        precision for this field must be interpreted.
    *
    * Calling this function on fields of types other than OFTReal or OFTRealList
    * does nothing.
    *
    * This function is the same as the CPP method OGRFieldDefn::SetWidthPrecisionFlags().
    *
    * @param hDefn handle to the field definition to set precision to.
    * @param nFlags the new width and precision flags.
    */

    void OGR_Fld_SetWidthPrecisionFlags(OGRFieldDefnH hDefn, int nFlags)
    {
        return OGRFieldDefn::FromHandle(hDefn)->SetWidthPrecisionFlags(nFlags);
    }


Efficiency considerations
--------------------------

Field definitions will have to set an additional integer field for real and list of
reals fields, only if the driver has any of the flags set, the default for the flags
will be zero (no flags set).


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


