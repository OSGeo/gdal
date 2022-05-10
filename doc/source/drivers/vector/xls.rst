.. _vector.xls:

XLS - MS Excel format
=====================

.. shortname:: XLS

.. build_dependencies:: libfreexl

This driver reads spreadsheets in MS Excel format. GDAL/OGR must be
built against the FreeXL library (GPL/LPL/MPL licensed), and the driver
has the same restrictions as the FreeXL library itself as far as which
and how Excel files are supported. (At the time of writing - with FreeXL
1.0.0a -, it means in particular that formulas are not supported.)

Each sheet is presented as a OGR layer. No geometry support is available
directly (but you may use the OGR VRT capabilities for that).

Configuration options
---------------------

The following :ref:`configuration options <configoptions>` are 
available:

-  :decl_configoption:`OGR_XLS_HEADERS` = FORCE / DISABLE / AUTO : By default, the driver
   will read the first lines of each sheet to detect if the first line
   might be the name of columns. If set to FORCE, the driver will
   consider the first line will be taken as the header line. If set to
   DISABLE, it will be considered as the first feature. Otherwise
   auto-detection will occur.
-  :decl_configoption:`OGR_XLS_FIELD_TYPES` = STRING / AUTO : By default, the driver will try
   to detect the data type of fields. If set to STRING, all fields will
   be of String type.

See Also
--------

-  `Homepage of the FreeXL
   library <https://www.gaia-gis.it/fossil/freexl/index>`__
