.. _vector.fme:

FMEObjects Gateway
==================

.. shortname:: FME

.. build_dependencies:: FME

.. deprecated_driver:: version_targeted_for_removal: 3.5
   env_variable: GDAL_ENABLE_DEPRECATED_DRIVER_FME

Feature sources supported by FMEObjects are supported for reading by OGR
if the FMEObjects gateway is configured, and if a licensed copy of
FMEObjects is installed and accessible.

To using the FMEObjects based readers the data source name passed should
be the name of the FME reader to use, a colon and then the actual data
source name (i.e. the filename). For instance,
"NTF:F:\DATA\NTF\2144.NTF" would indicate the NTF reader should be used
to read the file There are a number of special cases:

-  A data source ending in .fdd will be assumed to be an "FME Datasource
   Definition" file which will contain the reader name, the data source
   name, and then a set of name/value pairs of lines for the macros
   suitable to pass to the createReader() call.
-  A datasource named PROMPT will result in prompting the user for
   information using the regular FME dialogs. This only works on
   Windows.
-  A datasource named "PROMPT:filename" will result in prompting, and
   then having the resulting definition saved to the indicate files in
   .fdd format. The .fdd extension will be forced on the filename. This
   only works on Windows.

Each FME feature type will be treated as a layer through OGR, named by
the feature type. With some limitations FME coordinate systems are
supported. All FME geometry types should be properly supported. FME
graphical attributes (color, line width, etc) are not converted into OGR
Feature Style information.

Caching
-------

In order to enable fast access to large datasets without having to
retranslate them each time they are accessed, the FMEObjects gateway
supports a mechanism to cache features read from FME readers in "Fast
Feature Stores", a native vector format for FME with a spatial index for
fast spatial searches. These cached files are kept in the directory
indicated by the OGRFME_TMPDIR environment variable (or TMPDIR or /tmp
or C:\\ if that is not available).

The cached feature files will have the prefix FME_OLEDB\_ and a master
index is kept in the file ogrfmeds.ind. To clear away the index delete
all these files. Do not just delete some.

By default features in the cache are re-read after 3600s (60 minutes).
Cache retention times can be altered at compile time by altering the
fme2ogr.h include file.

Input from the SDE and ORACLE readers are not cached. These sources are
treated specially in a number of other ways as well.

Caveats
-------

#. Establishing an FME session is quite an expensive operation, on a
   350Mhz Linux system this can be in excess of 10s.
#. Old files in the feature cache are cleaned up, but only on subsequent
   visits to the FMEObjects gateway code in OGR. This means that if
   unused the FMEObjects gateway will leave old cached features around
   indefinitely.

Build/Configuration
-------------------

To include the FMEObjects gateway in an OGR build it is necessary to
have FME loaded on the system. The *--with-fme=*\ **$FME_HOME**
configuration switch should be supplied to configure. The FMEObjects
gateway is not explicitly linked against (it is loaded later when it may
be needed) so it is practical to distribute an OGR binary build with
FMEObjects support without distributing FMEObjects. It will just "work"
for people who have FMEObjects in the path.

The FMEObjects gateway has been tested on Linux and Windows.

--------------

More information on the FME product line, and how to purchase a license
for the FME software (enabling FMEObjects support) can be found on the
Safe Software web site at `www.safe.com <http://www.safe.com/>`__.
Development of this driver was financially supported by Safe Software.
