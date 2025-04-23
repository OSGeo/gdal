.. option:: --co <NAME>=<VALUE>

    Many formats have one or more optional dataset creation options that can be
    used to control particulars about the file created. For instance,
    the GeoPackage driver supports creation options to control the version.

    May be repeated.

    The dataset creation options available vary by format driver, and some
    simple formats have no creation options at all. A list of options
    supported for a format can be listed with the
    :ref:`--formats <vector_common_options>`
    command line option but the documentation for the format is the
    definitive source of information on driver creation options.
    See :ref:`vector_drivers` format
    specific documentation for legal creation options for each format.

    Note that dataset creation options are different from layer creation options.
