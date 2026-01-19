.. option:: --skip-errors

    .. versionadded:: 3.12

    Whether failures to write feature(s) should be ignored. Note that this option
    sets the size of the transaction unit to one feature at a time, which may
    cause severe slowdown when inserting into databases.
