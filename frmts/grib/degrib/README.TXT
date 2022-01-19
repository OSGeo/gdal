PROVENANCE
----------

This directory contains a *modified* version of degrib 2.14:
   http://www.mdl.nws.noaa.gov/~degrib/download/archive/degrib-20171016.tar.gz
With metaname.cpp update to degrib 2.25:
  https://lamp.mdl.nws.noaa.gov/lamp/Data/degrib/download/archive/degrib-20200921.tar.gz
  (accessed from https://vlab.noaa.gov/web/mdl/degrib-archive)

and a *modified* version of g2clib 1.6.0:
   http://www.nco.ncep.noaa.gov/pmb/codes/GRIB2/g2clib-1.6.0.tar (Last updated: 02/18/2016)

Note: the degrib-20171016.tar.gz includes a bundled version of the g2clib library,
at version 1.0.5

Up-to-date versions of degrib can be found at
    https://www.weather.gov/mdl/degrib_archive
    https://sats.nws.noaa.gov/~degrib/download/archive

Up-to-date versions of g2clib can be found at
    http://www.nco.ncep.noaa.gov/pmb/codes/GRIB2/


GRIB2 tables
------------

Starting with GDAL 3.5, GRIB2 tables 4.2 (Parameter Number by Product discipline
and Parameter category) and 4.5 (Fixed surface types and units) are available as
CSV files in the "data" resource directory of GDAL.

They result from a merge between tables that used to be hardcoded in degrib
metaname.cpp file (those files have been extracted with the extract_tables.py
script run on metaname.cpp file, before removing the tables from it), and which
are located in frmts/grib/degrib/data, with CSV tables originating from WMO
(https://github.com/wmo-im/GRIB2). The merge is done with the merge_degrib_and_wmo_tables.py
script. To update to a newer version of the WMO tables, update the wmo_github_tag
variable at the top of the script.

When adding a new table, it must be referenced in the GDAL_DATA_FILES variable
of the gdal.cmake file, to be installed.

The following tables can be found:

* grib2_center.csv: Content of Table 0 "National/International originating centers"
  (https://www.nco.ncep.noaa.gov/pmb/docs/on388/table0.html)

  That table has the following columns:
  - code: Index between 0 and 255 of the originating center
  - name: Name of the originating center

* grib2_subcenter.csv: Content of Table C "National subcenters"
  (https://www.nco.ncep.noaa.gov/pmb/docs/on388/tablec.html)

  That table has the following columns:
  - center_code: Index between 0 and 255 of the originating center
  - subcenter_code: Index between 0 and 255 of the national subcenter
  - name: Name of the national subcenter

* grib2_table_4_2_{product_type}_{parameter_category}.csv: Content of Table 4.2 for
  product_type and parameter_category. e.g grib2_table_4_2_0_1.csv corresponds to
  https://www.nco.ncep.noaa.gov/pmb/docs/grib2/grib2_doc/grib2_table4-2-0-1.shtml,
  that is product_type=0=Meteorological products and
  parameter_category=1=Moisture category.

  Those tables have the following columns:
  - subcat: Index between 0 and 255 of the parameter
  - short_name: Short parameter name
  - name: Longer parameter name
  - unit: Parameter unit
  - unit_conv: Unit conversion rule, whose values can be one of:
      + UC_NONE: No conversion allowed
      + UC_K2F: Kelvin degrees are converted to Fahrenheit (if English unit system)
                or Celcius (metric system)
      + UC_M2Feet: Meters are converted to Feet if English unit system
      + UC_M2Inch: Meters are converted to Inch if English unit system
      + UC_MS2Knots: m/s are converted to Knots if English unit system
      + UC_LOG10: log10(x) is converted to x if English or metric unit system
      + UC_UVIndex: multiply Watts/ m**2 by 40 for the UV index if English unit system
      + UC_M2StatuteMile: Meters are converted to Statute Mile if English unit system

* grib2_table_4_2_local_index.csv: That table identifies for various (center, subcenter)
  a local table that contains parameter definitions for parameters that are
  in the range of codes reserved for local use. That index table is sparsely populated.

  That table has the following columns:
  - center_code: Index between 0 and 255 of the originating center
  - subcenter_code: Index between 0 and 255 of the national subcenter
  - filename: Filename of the local table, typically named grib2_table_4_2_local_{center_name}.csv

  subcenter_code might be an empty string to match all subcenters of a given center.
  Note that records are processed from the top to the bottom of the file.
  Consider the following content:

  center_code           subcenter_code          filename
  7                     5                       grib2_table_4_2_local_HPC.csv
  7                                             grib2_table_4_2_local_NCEP.csv

  This means that the NCEP table will be used for center_code=7, unless
  subcenter_code=5 in which case the HPC table will be used.

* grib2_table_4_2_local_{center_name}.csv: Those tables contain parameter definitions
  that are specific to a center/subcenter.

  Those tables have the following columns:
  - prod: Index of the product type
  - cat: Index of the parameter category
  - subcat: Index between 0 and 255 of the parameter
  - short_name: Short parameter name
  - name: Longer parameter name
  - unit: Parameter unit
  - unit_conv: Unit conversion rule
