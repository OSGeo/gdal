.. option:: -if <format>

    Format/driver name to be attempted to open the input file(s). It is generally
    not necessary to specify it, but it can be used to skip automatic driver
    detection, when it fails to select the appropriate driver.
    This option can be repeated several times to specify several candidate drivers.
    Note that it does not force those drivers to open the dataset. In particular,
    some drivers have requirements on file extensions.

    .. versionadded:: 3.2
