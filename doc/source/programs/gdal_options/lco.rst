.. option:: --layer-creation-option <NAME>=<VALUE>

    Many formats have one or more optional layer creation options that can be
    used to control particulars about the layer created. For instance,
    the GeoPackage driver supports layer creation options to control the
    feature identifier or geometry column name, setting the identifier or
    description, etc.

    May be repeated.

    The layer creation options available vary by format driver, and some
    simple formats have no layer creation options at all. A list of options
    supported for a format can be listed with the
    :ref:`--formats <vector_common_options>`
    command line option but the documentation for the format is the
    definitive source of information on driver creation options.
    See :ref:`vector_drivers` format
    specific documentation for legal creation options for each format.

    Note that layer creation options are different from dataset creation options.
