Caching of remote pixel data is possible by setting a <Cache> element in the
WMS (or WMTS) configuration file.

Before GDAL 3.9, if the <Path> sub-element of <Cache> was not specified, the
directory of the cache was ``./gdalwmscache`` (that is to say a ``gdalwmscache``
subdirectory of the current directory), unless the :config:`GDAL_DEFAULT_WMS_CACHE_PATH`
configuration option is specified.

Starting with GDAL 3.9, the directory of the cache is set according to the
following logic (first listed criterion is prioritary over following ones):

- Value of the <Path> sub-element of <Cache>, if specified.

- ``${GDAL_DEFAULT_WMS_CACHE_PATH}`` if :config:`GDAL_DEFAULT_WMS_CACHE_PATH` is set.

- ``${XDG_CACHE_HOME}/gdalwmscache`` if the ``XDG_CACHE_HOME`` configuration option is set.

- On Unix, ``${HOME}/.cache/gdalwmscache`` if the ``HOME`` configuration option is set.

- On Windows, ``${USERPROFILE}/.cache/gdalwmscache`` if the ``USERPROFILE`` configuration option is set.

- ``${CPL_TMPDIR}/gdalwmscache_${USER}`` if :config:`CPL_TMPDIR` and ``USER`` configuration options are set.

   If ``CPL_TMPDIR`` is not set, then ``TMPDIR`` is used, or ``TEMP``

   If ``USER`` is not set, ``USERNAME`` is used if set.
   If neither ``USERNAME`` or ``USER`` are set, the md5sum of the filename of the configuration file is used)

- ``./gdalwmscache_{md5sum(filename)}`` if none of the above mentioned configuration options are set.

Note that if the <Unique> element is set to true (which is its default value),
a subdirectory whose name is the md5sum of the filename of the configuration file
is appended to the caching directory.

The actual caching directory can be got by querying the ``CACHE_PATH`` metadata
item on the dataset.
