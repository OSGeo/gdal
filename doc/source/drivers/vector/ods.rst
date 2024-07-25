.. _vector.ods:

ODS - Open Document Spreadsheet
===============================

.. shortname:: ODS

.. build_dependencies:: libexpat

This driver can read, write and update spreadsheets in Open Document
Spreadsheet format, used by applications like OpenOffice / LibreOffice /
KSpread / etc...

The driver is only available if GDAL/OGR is compiled against the Expat
library.

Each sheet is presented as a OGR layer. No geometry support is available
directly (but you may use the OGR VRT capabilities for that).

Note 1 : spreadsheets with passwords are not supported.

Note 2 : when updating an existing document, all existing styles,
formatting, formulas and other concepts (charts, drawings, macros, ...)
not understood by OGR will be lost : the document is re-written from
scratch from the OGR data model.

Driver capabilities
-------------------

.. supports_create::

.. supports_virtualio::

Open options
------------

|about-open-options|
The following open options are available:

-  .. oo:: HEADERS
      :choices: FORCE, DISABLE, AUTO
      :default: AUTO
      :since: 3.8

      By default, the driver
      will read the first lines of each sheet to detect if the first line
      might be the name of columns. If set to FORCE, the driver will
      consider the first line as the header line. If set to
      DISABLE, it will be considered as the first feature. Otherwise
      auto-detection will occur.

-  .. oo:: FIELD_TYPES
      :choices: STRING, AUTO
      :default: AUTO
      :since: 3.8

      By default, the driver will
      try to detect the data type of fields. If set to STRING, all fields
      will be of String type.

Configuration options
---------------------

|about-config-options|
The following configuration options are available:

-  .. config:: OGR_ODS_HEADERS
      :choices: FORCE, DISABLE, AUTO
      :default: AUTO

      By default, the driver
      will read the first lines of each sheet to detect if the first line
      might be the name of columns. If set to FORCE, the driver will
      consider the first line as the header line. If set to
      DISABLE, it will be considered as the first feature. Otherwise
      auto-detection will occur.

-  .. config:: OGR_ODS_FIELD_TYPES
      :choices: STRING, AUTO
      :default: AUTO

      By default, the driver will try
      to detect the data type of fields. If set to STRING, all fields will
      be of String type.
