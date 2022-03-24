.. _configoptions:

================================================================================
Configuration options
================================================================================

This page discusses runtime configuration options for GDAL. These are distinct from
options to the build-time configure script. Runtime configuration options apply
on all platforms, and are evaluated at runtime. They can be set programmatically,
by commandline switches or in the environment by the user.

Configuration options are normally used to alter the default behavior of GDAL/OGR
drivers and in some cases the GDAL/OGR core. They are essentially global
variables the user can set.

How to set configuration options?
----------------------------------

One example of a configuration option is the :decl_configoption:`GDAL_CACHEMAX`
option. It controls the size
of the GDAL block cache, in megabytes. It can be set in the environment on Unix
(bash/bourne) shell like this:

::

    export GDAL_CACHEMAX=64

Or just for this command, like this:
::

    GDAL_CACHEMAX=64 gdal_translate 64 in.tif out.tif


In a DOS/Windows command shell it is done like this:

::

    set GDAL_CACHEMAX=64

It can also be set on the commandline for most GDAL and OGR utilities with the
``--config`` switch, though in a few cases these switches are not evaluated in
time to affect behavior.

::

    gdal_translate --config GDAL_CACHEMAX 64 in.tif out.tif

In C/C++ configuration switches can be set programmatically with
:cpp:func:`CPLSetConfigOption`:

.. code-block:: c

    #include "cpl_conv.h"
    ...
        CPLSetConfigOption( "GDAL_CACHEMAX", "64" );

Normally a configuration option applies to all threads active in a program, but
they can be limited to only the current thread with
:cpp:func:`CPLSetThreadLocalConfigOption`

.. code-block:: c

    CPLSetThreadLocalConfigOption( "GTIFF_DIRECT_IO", "YES" );

For boolean options, the values YES, TRUE or ON can be used to turn the option on;
NO, FALSE or OFF to turn it off.


.. _gdal_configuration_file:

GDAL configuration file
-----------------------

.. versionadded:: 3.3

On driver registration, loading of configuration is attempted from a set of
predefined files.

The following locations are tried by :cpp:func:`CPLLoadConfigOptionsFromPredefinedFiles`:

 - the location pointed by the environment variable (or configuration option)
   GDAL_CONFIG_FILE is attempted first. If it set, the next steps are not
   attempted

 - for Unix builds, the location pointed by ${sysconfdir}/gdal/gdalrc is first
   attempted (where ${sysconfdir} evaluates to ${prefix}/etc, unless the
   ``--sysconfdir`` switch of ./configure has been invoked). Then  $(HOME)/.gdal/gdalrc
   is tried, potentially overriding what was loaded with the sysconfdir

 - for Windows builds, the location pointed by $(USERPROFILE)/.gdal/gdalrc
   is attempted.

A configuration file is a text file in a .ini style format, that lists
configuration options and their values. Lines starting with `#` are comment lines.

Example:

.. code-block::

    [configoptions]
    # set BAR as the value of configuration option FOO
    FOO=BAR


Configuration options set in the configuration file can later be overridden
by calls to :cpp:func:`CPLSetConfigOption` or  :cpp:func:`CPLSetThreadLocalConfigOption`,
or through the ``--config`` command line switch.

The value of environment variables set before GDAL starts will be used instead
of the value set in the configuration files.

.. _list_config_options:

List of configuration options and where they apply
--------------------------------------------------

.. note::
    This list is known to be incomplete. It depends on proper annotation of configuration
    options where they are mentioned elsewhere in the documentation.
    If you want to help to extend it, use the ``:decl_configoption:`NAME```
    syntax in places where a configuration option is mentioned.


.. include:: configoptions_index_generated.rst
