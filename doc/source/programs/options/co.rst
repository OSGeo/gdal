.. WARNING: if modifying that file, please edit gdal_translate.rst too

.. option:: -co <NAME>=<VALUE>

    Many formats have one or more optional creation options that can be
    used to control particulars about the file created. For instance,
    the GeoTIFF driver supports creation options to control compression,
    and whether the file should be tiled.

    The creation options available vary by format driver, and some
    simple formats have no creation options at all. A list of options
    supported for a format can be listed with the
    :ref:`--format <raster_common_options_format>`
    command line option but the documentation for the format is the
    definitive source of information on driver creation options.
    See :ref:`raster_drivers` format
    specific documentation for legal creation options for each format.
