.. _rfc-104:

===================================================================
RFC 104: Adding a "gdal" front-end command line interface
===================================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2024-Nov-06
Status:        Adopted, implemented in 3.11
Target:        GDAL 3.11 for initial version (full scope will likely take more development cycles)
============== =============================================

Summary
-------

This RFC introduces a single :program:`gdal` front-end command line interface
(CLI), that exposes sub-commands, adopts a consistent naming of options,
introduces new capabilities (pipelines) and a concept of algorithms that can be
run through the CLI or that can be automatically discovered and
invoked programmatically.

This RFC gives the general principles and how they will be implemented on a
subset of the envisioned commands. The initial candidate implementation will
definitely not cover the full spectrum. Given that its size is already 10,000 new
lines of code at time of writing, we need to limit its scope for reasonable
reviewability. Extra functionality will be added progressively after RFC adoption
and initial implementation.

Motivation
----------

As of 2024, GDAL has 26 years of existence, and throughout the years, various
utilities have been added by diverse contributors, leading to inconsistent
naming of utilities (underscore or not?), options and input/output parameter.
The GDAL User Survey of October-November 2024 shows that a significant proportion
of GDAL users do so through the command line interface, and they suffer from
inconsistencies, hence it is legitimate to enhance their experience.

This RFC adds a new single :program:`gdal` front-end CLI whose sub-commands will
match the functionality of existing utilities and re-use
the battle-tested underlying implementations as much as possible.

The existing utilities will remain for backwards compatible reasons.
This RFC takes inspiration from `PDAL applications <https://pdal.io/en/2.8.1/apps/index.html>`__
and `rasterio 'rio' utilities <https://rasterio.readthedocs.io/en/stable/api/rasterio.html>`__.

Examples
--------

Before going to the theory and details, let's have a look at the following examples
which reflect the state of the candidate implementation.

Short usage help message of "gdal"
++++++++++++++++++++++++++++++++++

  .. code-block:: shell

    $ gdal
    ERROR 1: gdal: Missing subcommand name.
    Usage: gdal <subcommand>
    where <subcommand> is one of:
      - convert:  Convert a dataset (shortcut for 'gdal raster convert' or 'gdal vector convert').
      - info:     Return information on a dataset (shortcut for 'gdal raster info' or 'gdal vector info').
      - pipeline: Execute a pipeline (shortcut for 'gdal vector pipeline').
      - raster:   Raster commands.
      - vector:   Vector commands.

    'gdal <FILENAME>' can also be used as a shortcut for 'gdal info <FILENAME>'.
    And 'gdal read <FILENAME> ! ...' as a shortcut for 'gdal pipeline <FILENAME> ! ...'.

    For more details, consult https://gdal.org/programs/index.html


Short usage help message of "gdal raster info"
++++++++++++++++++++++++++++++++++++++++++++++

  .. code-block:: shell

    $ gdal raster info
    ERROR 1: info: Positional arguments starting at 'INPUT' have not been specified.
    Usage: gdal raster info [OPTIONS] <INPUT>
    Try 'gdal raster info --help' for help.


Detailed usage help message of "gdal raster info"
+++++++++++++++++++++++++++++++++++++++++++++++++

  .. code-block:: shell

    $ gdal raster info --help
    Usage: gdal raster info [OPTIONS] <INPUT>

    Return information on a raster dataset.

    Positional arguments:
      -i, --input <INPUT>                                  Input raster dataset [required]

    Common Options:
      -h, --help                                           Display help message and exit
      --json-usage                                         Display usage as JSON document and exit

    Options:
      -f, --of, --format, --output-format <OUTPUT-FORMAT>  Output format. OUTPUT-FORMAT=json|text (default: json)
      --mm, --min-max                                      Compute minimum and maximum value
      --stats                                              Retrieve or compute statistics, using all pixels
                                                           Mutually exclusive with --approx-stats
      --approx-stats                                       Retrieve or compute statistics, using a subset of pixels
                                                           Mutually exclusive with --stats
      --hist                                               Retrieve or compute histogram

    Advanced Options:
      --oo, --open-option <KEY=VALUE>                      Open options [may be repeated]
      --if, --input-format <INPUT-FORMAT>                  Input formats [may be repeated]
      --no-gcp                                             Suppress ground control points list printing
      --no-md                                              Suppress metadata printing
      --no-ct                                              Suppress color table printing
      --no-fl                                              Suppress file list printing
      --checksum                                           Compute pixel checksum
      --list-mdd                                           List all metadata domains available for the dataset
      --mdd <MDD>                                          Report metadata for the specified domain. 'all' can be used to report metadata in all domains

    Esoteric Options:
      --no-nodata                                          Suppress retrieving nodata value
      --no-mask                                            Suppress mask band information
      --subdataset <SUBDATASET>                            Use subdataset of specified index (starting at 1), instead of the source dataset itself


A few invocations of "gdal raster info [OPTIONS] <FILENAME>"
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  .. code-block:: shell

    $ gdal raster info byte.tif
    [ ... JSON output stripped ... ]

    $ gdal raster info -i byte.tif
    [ ... JSON output stripped ... ]

    $ gdal raster info --input byte.tif
    [ ... JSON output stripped ... ]

    $ gdal raster info --input=byte.tif
    [ ... JSON output stripped ... ]

    $ gdal raster info byte.tif --stats --format=text
    [ ... text output stripped ... ]


Using just ``gdal info <FILENAME>``

  .. code-block:: shell

    $ gdal raster byte.tif
    [ ... JSON output stripped ... ]


And cherry-on-the-cake ``gdal <FILENAME>``

  .. code-block:: shell

    $ gdal byte.tif
    [ ... JSON output stripped ... ]


"gdal [info] <FILENAME>" on dataset with mixed raster and vector content
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  .. code-block:: shell

    $ gdal info mixed.gpkg
    ERROR 1: 'mixed.gpkg' has both raster and vector content. Please use 'gdal raster info' or 'gdal vector info'.

    $ gdal mixed.gpkg
    ERROR 1: 'mixed.gpkg' has both raster and vector content. Please use 'gdal raster info' or 'gdal vector info'.


A few invocations of "gdal raster convert"
++++++++++++++++++++++++++++++++++++++++++

  .. code-block:: shell

    $ gdal raster convert byte.tif out.tif

    $ gdal raster convert byte.tif out.tif --co=TILED=YES,COMPRESS=LZW
    ERROR 1: File 'out.tif' already exists. Specify the --overwrite option to overwrite it.

    $ gdal raster convert --input=byte.tif --output=out.tif --co=TILED=YES,COMPRESS=LZW --overwrite

    $ gdal raster convert -i byte.tif -o out.tif --co=TILED=YES,COMPRESS=LZW --overwrite --progress
    0...10...20...30...40...50...60...70...80...90...100 - done.


Similarly to "gdal info" resolving automatically to "gdal raster info" or "gdal vector info"
based on dataset content, "gdal convert" will also detect which subcommand must be used:

  .. code-block:: shell

    $ gdal convert byte.tif out.tif --overwrite


But:

  .. code-block:: shell

    $ gdal convert mixed.gpkg out.tif --overwrite
    ERROR 1: 'mixed.gpkg' has both raster and vector content. Please use 'gdal raster convert' or 'gdal vector convert'.


Help message of "gdal vector"
++++++++++++++++++++++++++++++++++++++++++++++

  .. code-block:: shell

    $ gdal vector
    ERROR 1: vector: Missing subcommand name.
    Usage: gdal vector <SUBCOMMAND>
    where <SUBCOMMAND> is one of:
      - convert:   Convert a vector dataset.
      - filter:    Filter a vector dataset.
      - info:      Return information on a vector dataset.
      - pipeline:  Process a vector dataset.
      - reproject: Reproject a vector dataset.


A few invocations of "gdal vector convert"
++++++++++++++++++++++++++++++++++++++++++

  .. code-block:: shell

    $ gdal vector convert poly.gpkg poly.parquet

    $ gdal vector convert poly.gpkg poly.parquet --lco COMPRESSION=SNAPPY
    ERROR 1: File 'poly.parquet' already exists. Specify the --overwrite option to overwrite it.

    $ gdal vector convert multilayer.gpkg output.gpkg -l my_input_layer --output-layer=new_layer --update --progress
    0...10...20...30...40...50...60...70...80...90...100 - done.

    $ gdal convert poly.gpkg poly.parquet --overwrite


JSON-formatted detailed usage of "gdal vector convert"
++++++++++++++++++++++++++++++++++++++++++++++++++++++

This mode is rather aimed at application developers that would want to dynamically
generate graphical user interfaces for GDAL algorithms.

  .. code-block:: shell

    $ gdal vector convert --json-usage


  .. code-block:: json

    {
      "name":"convert",
      "full_path":[
        "vector",
        "convert"
      ],
      "description":"Convert a vector dataset.",
      "sub_algorithms":[
      ],
      "input_arguments":[
        {
          "name":"output-format",
          "type":"string",
          "description":"Output format",
          "min_count":0,
          "max_count":1,
          "category":"Base",
          "metadata":{
            "required_capabilities":[
              "DCAP_VECTOR",
              "DCAP_CREATE"
            ]
          }
        },
        {
          "name":"open-option",
          "type":"string_list",
          "description":"Open options",
          "min_count":0,
          "max_count":2147483647,
          "category":"Advanced"
        },
        {
          "name":"input-format",
          "type":"string_list",
          "description":"Input formats",
          "min_count":0,
          "max_count":2147483647,
          "category":"Advanced",
          "metadata":{
            "required_capabilities":[
              "DCAP_VECTOR"
            ]
          }
        },
        {
          "name":"input",
          "type":"dataset",
          "description":"Input vector dataset",
          "min_count":1,
          "max_count":1,
          "category":"Base",
          "dataset_type":[
            "vector"
          ],
          "input_flags":[
            "name",
            "dataset"
          ]
        },
        {
          "name":"creation-option",
          "type":"string_list",
          "description":"Creation option",
          "min_count":0,
          "max_count":2147483647,
          "category":"Base"
        },
        {
          "name":"layer-creation-option",
          "type":"string_list",
          "description":"Layer creation option",
          "min_count":0,
          "max_count":2147483647,
          "category":"Base"
        },
        {
          "name":"overwrite",
          "type":"boolean",
          "description":"Whether overwriting existing output is allowed",
          "default":false,
          "min_count":0,
          "max_count":1,
          "category":"Base"
        },
        {
          "name":"update",
          "type":"boolean",
          "description":"Whether updating existing dataset is allowed",
          "default":false,
          "min_count":0,
          "max_count":1,
          "category":"Base"
        },
        {
          "name":"overwrite-layer",
          "type":"boolean",
          "description":"Whether overwriting existing layer is allowed",
          "default":false,
          "min_count":0,
          "max_count":1,
          "category":"Base"
        },
        {
          "name":"append",
          "type":"boolean",
          "description":"Whether appending to existing layer is allowed",
          "default":false,
          "min_count":0,
          "max_count":1,
          "category":"Base"
        },
        {
          "name":"input-layer",
          "type":"string_list",
          "description":"Input layer name(s)",
          "min_count":0,
          "max_count":2147483647,
          "category":"Base"
        },
        {
          "name":"output-layer",
          "type":"string",
          "description":"Output layer name",
          "min_count":0,
          "max_count":1,
          "category":"Base"
        }
      ],
      "output_arguments":[
      ],
      "input_output_arguments":[
        {
          "name":"output",
          "type":"dataset",
          "description":"Output vector dataset",
          "min_count":1,
          "max_count":1,
          "category":"Base",
          "dataset_type":[
            "vector"
          ],
          "input_flags":[
            "name",
            "dataset"
          ],
          "output_flags":[
            "dataset"
          ]
        }
      ]
    }

.. _rfc104_gdal_vector_pipeline_examples:

A few invocations of "gdal vector pipeline"
+++++++++++++++++++++++++++++++++++++++++++

  .. code-block:: shell

     # The use of the '!' as a step separator is to prevent Unix or Windows shells from
     # trying to use other processes for the "reproject" or "write" steps.
     # Below is a single-process pipeline.
     $ gdal vector pipeline read poly.gpkg ! reproject --dst-crs=EPSG:4326 ! write out.parquet --overwrite

     # Alternative without the "vector" and "pipeline" subcommands, and with --progress
     $ gdal read poly.gpkg ! reproject --dst-crs=EPSG:4326 ! write out.parquet --overwrite  --progress

     # Alternative using an explicit --pipeline switch, and given the quoting, we can use the '|' character
     $ gdal vector pipeline --pipeline="read poly.gpkg | reproject --dst-crs=EPSG:4326 | write out.parquet --overwrite"

     # Works also as a quoted positional argument, and without the "vector" subcommand
     $ gdal pipeline --progress "read poly.gpkg | reproject --dst-crs=EPSG:4326 | write out.parquet --overwrite"


Detailed usage help message of "gdal vector pipeline"
+++++++++++++++++++++++++++++++++++++++++++++++++++++

  .. code-block:: shell

    $ gdal vector pipeline --help
    Usage: gdal vector pipeline [OPTIONS] <PIPELINE>

    Process a vector dataset.

    Positional arguments:

    Common Options:
      -h, --help    Display help message and exit
      --json-usage  Display usage as JSON document and exit
      --progress    Display progress bar

    <PIPELINE> is of the form: read [READ-OPTIONS] ( ! <STEP-NAME> [STEP-OPTIONS] )* ! write [WRITE-OPTIONS]

    Example: 'gdal vector pipeline --progress ! read in.gpkg ! \
                   reproject --dst-crs=EPSG:32632 ! write out.gpkg --overwrite'

    Potential steps are:

    * read [OPTIONS] <INPUT>
    ------------------------

    Read a vector dataset.

    Positional arguments:
      -i, --input <INPUT>                                  Input vector dataset [required]

    Options:
      -l, --layer, --input-layer <INPUT-LAYER>             Input layer name(s) [may be repeated]

    Advanced Options:
      --if, --input-format <INPUT-FORMAT>                  Input formats [may be repeated]
      --oo, --open-option <KEY=VALUE>                      Open options [may be repeated]

    * filter [OPTIONS]
    ------------------

    Filter.

    Options:
      --bbox <BBOX>                                        Bounding box as xmin,ymin,xmax,ymax

    * reproject [OPTIONS]
    ---------------------

    Reproject.

    Options:
      -s, --src-crs <SRC-CRS>                              Source CRS
      -d, --dst-crs <DST-CRS>                              Destination CRS [required]

    * write [OPTIONS] <OUTPUT>
    --------------------------

    Write a vector dataset.

    Positional arguments:
      -o, --output <OUTPUT>                                Output vector dataset [required]

    Options:
      -f, --of, --format, --output-format <OUTPUT-FORMAT>  Output format
      --co, --creation-option <KEY=VALUE>                  Creation option [may be repeated]
      --lco, --layer-creation-option <KEY=VALUE>           Layer creation option [may be repeated]
      --overwrite                                          Whether overwriting existing output is allowed
      --update                                             Whether updating existing dataset is allowed
      --overwrite-layer                                    Whether overwriting existing layer is allowed
      --append                                             Whether appending to existing layer is allowed
      -l, --output-layer <OUTPUT-LAYER>                    Output layer name


The filter and reproject steps can also be used as direct "gdal vector" standalone
subcommands, in which case they are augmented with the options of the 'read' and
'write' steps:

  .. code-block:: shell

    $ gdal vector reproject --help
    Usage: gdal vector reproject [OPTIONS] <INPUT> <OUTPUT>

    Reproject a vector dataset.

    Positional arguments:
      -i, --input <INPUT>                                  Input vector dataset [required]
      -o, --output <OUTPUT>                                Output vector dataset [required]

    Common Options:
      -h, --help                                           Display help message and exit
      --json-usage                                         Display usage as JSON document and exit
      --progress                                           Display progress bar

    Options:
      -l, --layer, --input-layer <INPUT-LAYER>             Input layer name(s) [may be repeated]
      -f, --of, --format, --output-format <OUTPUT-FORMAT>  Output format
      --co, --creation-option <KEY>=<VALUE>                Creation option [may be repeated]
      --lco, --layer-creation-option <KEY>=<VALUE>         Layer creation option [may be repeated]
      --overwrite                                          Whether overwriting existing output is allowed
      --update                                             Whether to open existing dataset in update mode
      --overwrite-layer                                    Whether overwriting existing layer is allowed
      --append                                             Whether appending to existing layer is allowed
      --output-layer <OUTPUT-LAYER>                        Output layer name
      -s, --src-crs <SRC-CRS>                              Source CRS
      -d, --dst-crs <DST-CRS>                              Destination CRS [required]

    Advanced Options:
      --if, --input-format <INPUT-FORMAT>                  Input formats [may be repeated]
      --oo, --open-option <KEY=VALUE>                      Open options [may be repeated]


CLI specification
-----------------

Subcommand syntax
+++++++++++++++++

.. code-block:: shell

        gdal <subcommand> [<subsubcommand>]... [<options>]... [<positional arguments>]...

where subcommand is something like ``raster``, ``vector``, etc. with potential
sub-subcommand like ``info``, ``convert``, etc.

Option naming conventions
+++++++++++++++++++++++++

* One-letter short names preceded with dash: ``-z``

  When a value is specified, it must be separated with a space: ``-z <value>``

* Longer names preceded with two dashes and using dash to separate words,
  lower-case capitalized: ``--long-name``

  When a single value is expected, it must be separated with a space or equal sign:
  ``--long-name <value>`` or ``--long-name=<value>``.

  In the rest of the document, we will use the version with a space separator,
  but equal sign is also accepted.

Repeated values / multi-valued options
++++++++++++++++++++++++++++++++++++++

Existing GDAL command line utilities have an inconsistent strategy regarding
how to specify repeated values (band indices, nodata values, etc.), sometimes
with the switch being repeated many times, sometimes with a single switch but
the values being grouped together and separated with spaces or commas.

With this RFC, for arguments of list types, 2 variants will be supported:

- values passed at the same time (packed values), separated by a ``,`` (comma):
  ``--co KEY1=VALUE1,KEY2=VALUE2``

- or values are passed one by one with the option being repeated:
  ``--co KEY1=VALUE1 --co KEY2=VALUE2``

In some cases, in particular when a fixed number of values is expected, or
if the order of values in the list matters, like a bounding-box argument,
the argument can be declared to accept packed values only, like in
``--bbox <xmin>,<ymin>,<xmax>,<ymax>``

Specification of input and output files/datasets
+++++++++++++++++++++++++++++++++++++++++++++++++

Two possibilities will be offered:

* positional arguments with input(s) first, output last

  .. code-block:: shell

        gdal <subcommand> <input1> [<input2>]... <output>

* using ``-i / --input`` and ``-o / --output``

  .. code-block:: shell

        gdal <subcommand> -i <input1> [-i <input2>]... -o <output>

Reserved switches
+++++++++++++++++

The following switches are reserved. Meaning that if a subcommand uses them,
it must be with their below semantics and syntax.

* ``-h``, ``--help``: display detailed help synopsis

* ``-i <name>``, ``--input <name>``: specify input file/dataset

* ``-o <name>``, ``--output <name>``: specify output file/dataset

* ``--overwrite``: whether overwriting the output file is allowed. Defaults to no, that is execution will fail if the output file already exists.

* ``-f <format>``, ``--of <format>``: output format. Value is a (not always so)
  "short" driver name: ``GTiff``, ``COG``, ``GPKG``, ``ESRI Shapefile``.
  Also used by ``gdal info`` to select JSON vs text output

* ``--if <format>``: input format. Value is a short driver name.
  Used when autodetection of the appropriate driver fails.

* ``-b <band_number>``, ``--band <band_number>``: specify input raster band number.
  May be repeated for utilities supporting multiple bands

* ``-l <name>``, ``--layer <name>``: specify input vector layer name.
  May be repeated for utilities supporting multiple layers

* ``--co <NAME>=<VALUE>``: driver specific creation option. May be repeated.

* ``--oo <NAME>=<VALUE>``: driver specific open option. May be repeated.

* ``--ot {Byte|UInt16|...}``: output data type (for raster output)

* ``--bbox <xmin>,<ymin>,<xmax>,<ymax>``: as used by ``gdal vector info``,
  ``gdal vector convert``, ``gdal raster convert``

* ``--src-crs <crs_spec>``: Override source CRS specification. Accept ``--s_srs`` as hidden alias for old CLI compatibility.

* ``--dst-crs <crs_spec>``: Define target CRS specification. Accept ``--t_srs`` as hidden alias for old CLI compatibility.

* ``--override-crs <crs_spec>``: Override CRS without reprojection. Accept ``--a_srs`` as hidden alias for old CLI compatibility.

gdal info
+++++++++

This subcommand will merge together :ref:`gdalinfo`, :ref:`ogrinfo` and :ref:`gdalmdiminfo`.
It will :cpp:func:`GDALDataset::Open` the specified dataset in raster and vector mode.
If the dataset is only a raster one, it will automatically resolve as the sub-subcommand "gdal raster info".
If the dataset is only a vector one, it will automatically resolve as the sub-subcommand as "gdal vector info".

In this automated mode, no switch besides open options can be specified, given that we don't know yet in which mode to open.

If the dataset has both raster and vector content, an error will be emitted, inviting the user to specify explicitly the raster or vector mode.

Example:

  .. code-block:: shell

        gdal info my.tif

        gdal info my.gpkg

The main :program:`gdal` utility will also accept ``gdal [OPTIONS] <FILENAME>``
as a shortcut for ``gdal info [OPTIONS] <FILENAME>``.

gdal raster info
++++++++++++++++

Equivalent of existing :ref:`gdalinfo`

Synopsis: ``gdal raster info [-i <filename>] [other options] <filename>``

Example:

  .. code-block:: shell

        gdal raster info my.gpkg

Switches:

* ``-f json|text``, ``--of json|text``: output format. Will default to JSON.

* ``--min-max``

* ``--stats``

* ``--approx-stats``

* ``--hist``

* ``--no-gcp``

* ``--no-md``

* ``--no-ct``

* ``--no-fl``

* ``--no-nodata``

* ``--no-mask``

* ``--checksum``

* ``--list-mdd``

* ``--mdd <domain>|all``

* ``--subdataset <num>``

gdal vector info
++++++++++++++++

Equivalent of existing :ref:`ogrinfo`

Synopsis: ``gdal vector info [-i <filename>] [other options] <filename> [<layername>]...``

Example:

  .. code-block:: shell

        gdal vector info my.gpkg

Switches:

* ``-f json|text``, ``--of json|text``: output format. Will default to JSON.

* ``--sql <statement>``

* ``-l <name>``, ``--layer <name>``

* ``--update``: New default will be read-only

* ``--interleaved-layers``: a.k.a random layer reading mode (ogrinfo ``-al``), for OSM and GMLAS mostly.

* ``--where <statement>``

* ``--dialect <dialectname>``

* ``--bbox <xmin>,<ymin>,<xmax>,<ymax>``


gdal multidim info
++++++++++++++++++

Equivalent of existing :ref:`gdalmdiminfo`

Details will be fleshed out in the pull request implementing it.

gdal raster convert
+++++++++++++++++++

Equivalent of existing :ref:`gdal_translate`

Initial options below. More to be added.

.. code-block::

    Positional arguments:
      -i, --input <INPUT>                                  Input raster dataset [required]
      -o, --output <OUTPUT>                                Output raster dataset (created by algorithm) [required]

    Common Options:
      -h, --help                                           Display help message and exit
      --json-usage                                         Display usage as JSON document and exit
      --progress                                           Display progress bar

    Options:
      -f, --of, --format, --output-format <OUTPUT-FORMAT>  Output format
      --co, --creation-option <KEY=VALUE>                  Creation option [may be repeated]
      --overwrite                                          Whether overwriting existing output is allowed
                                                           Mutually exclusive with --append
      --append                                             Append as a subdataset to existing output
                                                           Mutually exclusive with --overwrite

    Advanced Options:
      --oo, --open-option <KEY=VALUE>                      Open options [may be repeated]
      --if, --input-format <INPUT-FORMAT>                  Input formats [may be repeated]


gdal vector convert
+++++++++++++++++++

Equivalent of existing :ref:`ogr2ogr`

Initial options below. More to be added, but presumably not all existing options
of ``ogr2ogr``.

.. code-block::

    Positional arguments:
      -i, --input <INPUT>                                  Input vector dataset [required]
      -o, --output <OUTPUT>                                Output vector dataset [required]

    Common Options:
      -h, --help                                           Display help message and exit
      --json-usage                                         Display usage as JSON document and exit
      --progress                                           Display progress bar

    Options:
      -f, --of, --format, --output-format <OUTPUT-FORMAT>  Output format
      --co, --creation-option <KEY=VALUE>                  Creation option [may be repeated]
      --lco, --layer-creation-option <KEY=VALUE>           Layer creation option [may be repeated]
      --overwrite                                          Whether overwriting existing output is allowed
      --update                                             Whether updating existing dataset is allowed
      --overwrite-layer                                    Whether overwriting existing layer is allowed
      --append                                             Whether appending to existing layer is allowed
      -l, --layer, --input-layer <INPUT-LAYER>             Input layer name(s) [may be repeated]
      --output-layer <OUTPUT-LAYER>                        Output layer name

    Advanced Options:
      --oo, --open-option <KEY=VALUE>                      Open options [may be repeated]
      --if, --input-format <INPUT-FORMAT>                  Input formats [may be repeated]


gdal vector pipeline
++++++++++++++++++++

"Equivalent" of existing :ref:`ogr2ogr`

Refer to above :ref:`examples <rfc104_gdal_vector_pipeline_examples>`.

A pipeline is the succession of several processing steps. One issue with ``ogr2ogr``
is that it offers tons of different processings that can be combined together,
but it is not always obvious to know in which order they are applied. In some
cases, we had to duplicate options, like ``-clipsrc`` and ``-clipdst`` to offer
a way of clipping geometries before or after reprojection. It can be more natural
to explicitly specified in which order operations should be conducted, like

- read the input dataset
- filter on a bounding box (in the CRS of the input layer)
- reproject to some other CRS
- clip geometries to a rectangle (in the new CRS)
- ... some other operation ...
- write to final file.

Available steps currently are:

- "read": required to be first. Possibility to select all, one or a subset of input layers
- "filter": filtering by bounding box, or where clause
- "reproject"
- "write": required to be last

More steps to be added in follow-up pull requests.

There might be a loss of efficiency in having separate steps that iterate over
(on-the-fly / streamed) features returned by the previous step(s) and generating
new (on-the-fly / streamed) ones. In the most simple cases, we might be able to
"compile" steps into GDALVectorTranslate() single invocation. That might be done
in follow-up pull requests to the initial candidate implementation of this RFC.

Further enhancements might support non-linear pipelines (that is forming a
directed acyclic graph), and strategies to multi-thread some processing (for example,
a reprojection step could acquire N batches of X features from its source layer,
and then reproject each batch in a dedicated thread. But at the expense of a greater
usage of RAM to be able to store N * X features at once.)

gdal multidim convert
+++++++++++++++++++++

Equivalent of existing :ref:`gdalmdimtranslate`

Details will be fleshed out in the pull request implementing it.

gdal warp ?
+++++++++++

Equivalent, or subset, of existing :ref:`gdalwarp`

.. note::

    In the User Survey, a number of users have expressed a wish to have
    gdal_translate and gdalwarp functionality merged together. This RFC does not
    attempt at addressing that. Or should it... ? That'd be a huge topic

Note that warp is also a bit of a misnomer as gdalwarp can mosaic.

Details will be fleshed out in the pull request implementing it.

gdal raster contour
+++++++++++++++++++

Equivalent of existing :ref:`gdal_contour`

Details will be fleshed out in the pull request implementing it.


gdal vector rasterize
+++++++++++++++++++++

Equivalent of existing :ref:`gdal_rasterize`

Details will be fleshed out in the pull request implementing it.


gdal raster create
++++++++++++++++++

Details will be fleshed out in the pull request implementing it.

gdal raster footprint
+++++++++++++++++++++

Equivalent of existing :ref:`gdal_footprint`

Details will be fleshed out in the pull request implementing it.

gdal dem
++++++++

Equivalent of existing :ref:`gdaldem`

Including ``viewhsed`` (equivalent of existing :ref:`gdal_viewshed`) as a subcommand,
along side with the current modes of gdaldem: ``hillshade``, ``slope``, etc.

Details will be fleshed out in the pull request implementing it.

gdal grid
+++++++++

Equivalent of existing :ref:`gdal_grid`

grid is a vector to raster operation: should it be a top-level operation, or
a sub-subcommand of the ``raster`` or ``vector`` ones ?

Details will be fleshed out in the pull request implementing it.

gdal raster mosaic
++++++++++++++++++

Equivalent of existing :ref:`gdalbuildvrt` and  :ref:`gdal_translate`.

Details will be fleshed out in the pull request implementing it.

gdal raster tileindex
+++++++++++++++++++++

Equivalent of existing :ref:`gdaltindex`

Details will be fleshed out in the pull request implementing it.

gdal vector tileindex
+++++++++++++++++++++

Equivalent of existing :ref:`ogrtindex`

Details will be fleshed out in the pull request implementing it.

gdal raster cleanborder
++++++++++++++++++++++++

Equivalent of existing :ref:`nearblack`

Details will be fleshed out in the pull request implementing it.

Implementation details
----------------------

New C++ classes: GDALAlgorithm and related classes
++++++++++++++++++++++++++++++++++++++++++++++++++

GDALAlgorithm
*************

A new abstract C++ class, ``GDALAlgorithm``, is added. A GDALAlgorithm can be:

- either a leaf, which performs some processing. e.g. ``convert`` in ``gdal raster convert``

- or a node that lists several available sub-algorithms. e.g. the top-level
  ``gdal`` command, its ``raster`` or ``vector`` subcommands, or their ``info``
  or ``convert`` sub-subcommands).

A GDALAlgorithm has the following main methods:

- ``bool ParseCommandLineArguments(const std::vector<std::string>& args)``:
  args is the list of arguments after the command name. So in
  ``gdal raster info --format=text byte.tif``, args should be set to
  ``["--format=txt", "byte.tif"]``.

  This parsing is done "at hand", that is not using the ``p-ranav/argparser``
  framework we have used in GDAL 3.9 and 3.10 development cycles to renovate
  our existing CLI utilities. I have come to this conclusion because some of
  the behavior I needed to implement would have been to complicate to integrate
  within argparser, some behavior being very GDAL specific (dealing with dataset
  objects, various syntactic sugar), with a very low chance of being candidates
  for argparser upstream inclusion (or accepted by upstream).
  Extending argparser was felt more difficult to actually re-implement the
  functionality we needed.

- ``bool ValidateArguments()``: checks that all required arguments are set,
  that no mutually exclusive arguments are set, and other consistency checks.
  This method is called both by ``ParseCommandLineArguments`` and ``Run``.
  It is mostly of use for non-CLI usages where users directly instantiate a
  GDALAlgorithm instance and manually set their arguments without using
  ``ParseCommandLineArguments``.

- ``bool Run(GDALProgressFunc pfnProgress, void* pUserData)``: actually run the
  algorithm. If invoked on a node, it forwards execution down to the actual
  leaf. ``Run()`` is non-virtual: implementations need to implement ``RunImpl()``
  which is invoked by ``Run()``

- ``bool Close()``: close datasets and get back potential error status resulting
  from that. This is for example used by the :file:`gdal.cpp` main binary to
  determine the process status code, if an error would occur during flushing
  to disk of the output dataset after successful ``Run()`` execution.

Non-CLI users can invoke the following methods:

- ``std::vector<std::unique_ptr<GDALAlgorithmArg>> &GetArgs()``: returns the
  list of potential arguments. They are initially in a un-set state (with a
  default value) and can be set by calling the ``GDALAlgorithm::Set()`` methods

- ``GDALAlgorithmArg *GetArg(const std::string &osName)``: return an available
  argument from its name.

.. note::

    This draws loose inspiration from PDAL Kernel and Stage concepts, or
    QGIS processing algorithms.

GDALAlgorithmArg
****************

Models an argument of an algorithm.

An argument has the following properties:

- a (long) name (long meaning 2 letters or more)

- an optional one-letter CLI short name

- a description to display in help messages

- a list of advertized optional alternate of long names

- a list of hidden optional alternate of long names (for backward compatibility, dealing with common typos/variations like "srs" vs "crs")

- a type among: boolean, string, integer, real, dataset, list of string, list of integer, list of real, list of dataset.

- a category for usage presentation: Common, Basic, Advanced, Esoteric, or a custom name

- if it has a role as an input argument: 99% of arguments are input arguments.

- if it has a role as an output argument: this is for example the case for algorithms whose output is a dataset.
  The "output" dataset is typically both an input and output argument.
  The ``std::string GDALArgDatasetValue::name`` member is an input value of an algorithm.
  The ``GDALDataset* GDALArgDatasetValue::poDS`` member is an output value of an algorithm, that
  can be used in non-CLI contexts

- an optional name for the mutually exclusion group to which it belongs

- a minimum number of occurrences for its values: 0 (optional), 1 (requires), 2 or more (multi valued)

- a maximum number of occurrences for its values: only allowed to be greater than 1 for list-types of arguments

- a pointer to a variable of the type consistent with its type (``bool*``,
  ``std::string*``, ``int*``, ``double*``, ``GDALArgDatasetValue``,
  ``std::vector<std::string>*``, ``std::vector<int>*``, ``std::vector<double>*``,
  ``std::vector<GDALArgDatasetValue>*``). The value pointed by this pointer is
  modified by the ``Set()`` methods.

- an optional declared default value (if no declared default value, nor explicitly set value, the initial state of the variable pointed by the above mentioned pointer will be the value, that is a kind of implicit default value)

- whether it has been explicitly set.

- ``key: list of string`` free metadata. For example the ``format`` argument
  uses the ``required_capabilities`` key and ``DCAP_RASTER``, ``DCAP_VECTOR``,
  ``DCAP_CREATE``, etc. as values to declare the expected capability of the
  specified input/output format.

- a list of action callbacks that are triggered by the ``Set`` method.

- a list of validation callbacks that are triggered by the ``Set`` method.
  Each callback may return false to mean that the new value is invalid.

GDALArgDatasetValue
*******************

This class models an argument that holds a GDALDataset. It stores a dataset
name or a ``GDALDataset*`` pointer itself (with information if its ownership
is transferred to the GDALArgDatasetValue instance).
This is done this way to be compatible both of CLI usage where only dataset names
are specified, or programmatic usages where passing dataset names or passing/getting
``GDALDataset*`` pointers might be preferred. That later mode is also used by
``gdal vector pipeline`` to bind together the output of a step to the input of
the following step.

``GDALArgDatasetValue`` has also ``inputFlags`` and ``outputFlags`` properties
to indicate if it supports specifying only the dataset name, only the dataset object,
or both when it is used as an input, or if it generates the dataset name,
the dataset object or both when it is used as an output.

The GDALAlgorithm class itself has logic, triggered during the validation phase,
to open the ``GDALDataset*`` from its name for input arguments, taking into
account potential open options and allowed input formats.
It has also very specific logic to realize that if both the ``input`` and
``output`` arguments point to the same dataset name, a single ``GDALDataset*``
instance must be set onto both for drivers that require it (typically SQLite
based ones).

GDALAlgorithmRegistry
*********************

Instances of this class store a list of C++ types implementing GDALAlgorithm.

It is used by GDALAlgorithm itself for nodes to reference their potential children.

It is extended by a GDALGlobalAlgorithmRegistry to offer a singleton that lists all
top-level nodes (``raster``, ``vector``, etc.). Potentially code external to GDAL
could register a new command available for use by :program:`gdal` in a GDAL plugin.

It is also used by the ``GDALVectorPipelineAlgorithm`` to list its potential steps.

C API
+++++

The C API will map most of the functionality of ``GDALAlgorithm``,
``GDALAlgorithmArg``, ``GDALArgDatasetValue`` and ``GDALAlgorithmRegistry``.

Below is an extract of the beginning of https://github.com/rouault/gdal/blob/rfc104/gcore/gdalalgorithm.h

.. code-block:: c

    /** Type of an argument */
    typedef enum GDALAlgorithmArgType
    {
        /** Boolean type. Value is a bool. */
        GAAT_BOOLEAN,
        /** Single-value string type. Value is a std::string */
        GAAT_STRING,
        /** Single-value integer type. Value is a int */
        GAAT_INTEGER,
        /** Single-value real type. Value is a double */
        GAAT_REAL,
        /** Dataset type. Value is a GDALArgDatasetValue */
        GAAT_DATASET,
        /** Multi-value string type. Value is a std::vector<std::string> */
        GAAT_STRING_LIST,
        /** Multi-value integer type. Value is a std::vector<int> */
        GAAT_INTEGER_LIST,
        /** Multi-value real type. Value is a std::vector<double> */
        GAAT_REAL_LIST,
        /** Multi-value dataset type. Value is a std::vector<GDALArgDatasetValue> */
        GAAT_DATASET_LIST,
    } GDALAlgorithmArgType;

    /** Return whether the argument type is a list / multi-valued one. */
    bool CPL_DLL GDALAlgorithmArgTypeIsList(GDALAlgorithmArgType type);

    /** Return the string representation of the argument type */
    const char CPL_DLL *GDALAlgorithmArgTypeName(GDALAlgorithmArgType type);

    /** Opaque C type for GDALArgDatasetValue */
    typedef struct GDALArgDatasetValueHS *GDALArgDatasetValueH;

    /** Opaque C type for GDALAlgorithmArg */
    typedef struct GDALAlgorithmArgHS *GDALAlgorithmArgH;

    /** Opaque C type for GDALAlgorithm */
    typedef struct GDALAlgorithmHS *GDALAlgorithmH;

    /** Opaque C type for GDALAlgorithmRegistry */
    typedef struct GDALAlgorithmRegistryHS *GDALAlgorithmRegistryH;

    /************************************************************************/
    /*                  GDALAlgorithmRegistryH API                          */
    /************************************************************************/

    GDALAlgorithmRegistryH CPL_DLL GDALGetGlobalAlgorithmRegistry(void);

    void CPL_DLL GDALAlgorithmRegistryRelease(GDALAlgorithmRegistryH);

    char CPL_DLL **GDALAlgorithmRegistryGetAlgNames(GDALAlgorithmRegistryH);

    GDALAlgorithmH CPL_DLL GDALAlgorithmRegistryInstantiateAlg(
        GDALAlgorithmRegistryH, const char *pszAlgName);

    /************************************************************************/
    /*                        GDALAlgorithmH API                            */
    /************************************************************************/

    void CPL_DLL GDALAlgorithmRelease(GDALAlgorithmH);

    const char CPL_DLL *GDALAlgorithmGetName(GDALAlgorithmH);

    const char CPL_DLL *GDALAlgorithmGetDescription(GDALAlgorithmH);

    const char CPL_DLL *GDALAlgorithmGetLongDescription(GDALAlgorithmH);

    const char CPL_DLL *GDALAlgorithmGetHelpFullURL(GDALAlgorithmH);

    bool CPL_DLL GDALAlgorithmHasSubAlgorithms(GDALAlgorithmH);

    char CPL_DLL **GDALAlgorithmGetSubAlgorithmNames(GDALAlgorithmH);

    GDALAlgorithmH CPL_DLL
    GDALAlgorithmInstantiateSubAlgorithm(GDALAlgorithmH, const char *pszSubAlgName);

    bool CPL_DLL GDALAlgorithmParseCommandLineArguments(GDALAlgorithmH,
                                                        CSLConstList papszArgs);

    GDALAlgorithmH CPL_DLL GDALAlgorithmGetActualAlgorithm(GDALAlgorithmH);

    bool CPL_DLL GDALAlgorithmRun(GDALAlgorithmH, GDALProgressFunc pfnProgress,
                                  void *pProgressData);

    bool CPL_DLL GDALAlgorithmFinalize(GDALAlgorithmH);

    char CPL_DLL *GDALAlgorithmGetUsageAsJSON(GDALAlgorithmH);

    char CPL_DLL **GDALAlgorithmGetArgNames(GDALAlgorithmH);

    GDALAlgorithmArgH CPL_DLL GDALAlgorithmGetArg(GDALAlgorithmH,
                                                  const char *pszArgName);

    /************************************************************************/
    /*                      GDALAlgorithmArgH API                           */
    /************************************************************************/

    void CPL_DLL GDALAlgorithmArgRelease(GDALAlgorithmArgH);

    const char CPL_DLL *GDALAlgorithmArgGetName(GDALAlgorithmArgH);

    GDALAlgorithmArgType CPL_DLL GDALAlgorithmArgGetType(GDALAlgorithmArgH);

    const char CPL_DLL *GDALAlgorithmArgGetDescription(GDALAlgorithmArgH);

    const char CPL_DLL *GDALAlgorithmArgGetShortName(GDALAlgorithmArgH);

    char CPL_DLL **GDALAlgorithmArgGetAliases(GDALAlgorithmArgH);

    const char CPL_DLL *GDALAlgorithmArgGetMetaVar(GDALAlgorithmArgH);

    const char CPL_DLL *GDALAlgorithmArgGetCategory(GDALAlgorithmArgH);

    bool CPL_DLL GDALAlgorithmArgIsPositional(GDALAlgorithmArgH);

    bool CPL_DLL GDALAlgorithmArgIsRequired(GDALAlgorithmArgH);

    int CPL_DLL GDALAlgorithmArgGetMinCount(GDALAlgorithmArgH);

    int CPL_DLL GDALAlgorithmArgGetMaxCount(GDALAlgorithmArgH);

    bool CPL_DLL GDALAlgorithmArgGetPackedValuesAllowed(GDALAlgorithmArgH);

    bool CPL_DLL GDALAlgorithmArgGetRepeatedArgAllowed(GDALAlgorithmArgH);

    char CPL_DLL **GDALAlgorithmArgGetChoices(GDALAlgorithmArgH);

    bool CPL_DLL GDALAlgorithmArgIsExplicitlySet(GDALAlgorithmArgH);

    bool CPL_DLL GDALAlgorithmArgHasDefaultValue(GDALAlgorithmArgH);

    bool CPL_DLL GDALAlgorithmArgIsHiddenForCLI(GDALAlgorithmArgH);

    bool CPL_DLL GDALAlgorithmArgIsOnlyForCLI(GDALAlgorithmArgH);

    bool CPL_DLL GDALAlgorithmArgIsInput(GDALAlgorithmArgH);

    bool CPL_DLL GDALAlgorithmArgIsOutput(GDALAlgorithmArgH);

    const char CPL_DLL *GDALAlgorithmArgGetMutualExclusionGroup(GDALAlgorithmArgH);

    bool CPL_DLL GDALAlgorithmArgGetAsBoolean(GDALAlgorithmArgH);

    const char CPL_DLL *GDALAlgorithmArgGetAsString(GDALAlgorithmArgH);

    GDALArgDatasetValueH
        CPL_DLL GDALAlgorithmArgGetAsDatasetValue(GDALAlgorithmArgH);

    int CPL_DLL GDALAlgorithmArgGetAsInteger(GDALAlgorithmArgH);

    double CPL_DLL GDALAlgorithmArgGetAsDouble(GDALAlgorithmArgH);

    char CPL_DLL **GDALAlgorithmArgGetAsStringList(GDALAlgorithmArgH);

    const int CPL_DLL *GDALAlgorithmArgGetAsIntegerList(GDALAlgorithmArgH,
                                                        size_t *pnCount);

    const double CPL_DLL *GDALAlgorithmArgGetAsDoubleList(GDALAlgorithmArgH,
                                                          size_t *pnCount);

    bool CPL_DLL GDALAlgorithmArgSetAsBoolean(GDALAlgorithmArgH, bool);

    bool CPL_DLL GDALAlgorithmArgSetAsString(GDALAlgorithmArgH, const char *);

    bool CPL_DLL GDALAlgorithmArgSetAsDatasetValue(GDALAlgorithmArgH hArg,
                                                   GDALArgDatasetValueH value);

    bool CPL_DLL GDALAlgorithmArgSetDataset(GDALAlgorithmArgH hArg, GDALDatasetH);

    bool CPL_DLL GDALAlgorithmArgSetAsInteger(GDALAlgorithmArgH, int);

    bool CPL_DLL GDALAlgorithmArgSetAsDouble(GDALAlgorithmArgH, double);

    bool CPL_DLL GDALAlgorithmArgSetAsStringList(GDALAlgorithmArgH, CSLConstList);

    bool CPL_DLL GDALAlgorithmArgSetAsIntegerList(GDALAlgorithmArgH, size_t nCount,
                                                  const int *pnValues);

    bool CPL_DLL GDALAlgorithmArgSetAsDoubleList(GDALAlgorithmArgH, size_t nCount,
                                                 const double *pnValues);

    /** Binary-or combination of GDAL_OF_RASTER, GDAL_OF_VECTOR,
     * GDAL_OF_MULTIDIM_RASTER, possibly with GDAL_OF_UPDATE.
     */
    typedef int GDALArgDatasetType;

    GDALArgDatasetType CPL_DLL GDALAlgorithmArgGetDatasetType(GDALAlgorithmArgH);

    /** Bit indicating that the name component of GDALArgDatasetValue is accepted. */
    #define GADV_NAME (1 << 0)
    /** Bit indicating that the dataset component of GDALArgDatasetValue is accepted. */
    #define GADV_OBJECT (1 << 1)

    int CPL_DLL GDALAlgorithmArgGetDatasetInputFlags(GDALAlgorithmArgH);

    int CPL_DLL GDALAlgorithmArgGetDatasetOutputFlags(GDALAlgorithmArgH);

    /************************************************************************/
    /*                    GDALArgDatasetValueH API                          */
    /************************************************************************/

    GDALArgDatasetValueH CPL_DLL GDALArgDatasetValueCreate(void);

    void CPL_DLL GDALArgDatasetValueRelease(GDALArgDatasetValueH);

    const char CPL_DLL *GDALArgDatasetValueGetName(GDALArgDatasetValueH);

    GDALDatasetH CPL_DLL GDALArgDatasetValueGetDatasetRef(GDALArgDatasetValueH);

    GDALDatasetH
        CPL_DLL GDALArgDatasetValueGetDatasetIncreaseRefCount(GDALArgDatasetValueH);

    void CPL_DLL GDALArgDatasetValueSetName(GDALArgDatasetValueH, const char *);

    void CPL_DLL GDALArgDatasetValueSetDataset(GDALArgDatasetValueH, GDALDatasetH);

SWIG API
++++++++

All the above C API will be directly mapped to equivalent SWIG classes and methods.

It will be available in a new `swig/include/Algorithm.i <https://github.com/rouault/gdal/blob/rfc104/swig/include/Algorithm.i>`__
file.`

It will be used by our Python autotest suite, as most of the testing
will be done through that way.

``gdal`` binary
+++++++++++++++

gdal.cpp is a ~ 50 line of code launcher script that queries the ``gdal`` main
algorithm, passes it to it the command line arguments and execute the
``GDALAlgorithm::Run`` method.

Out of scope
------------

* This RFC only addresses existing C++ utilities. Python utilities that would be
  migrated in the future as C++ utilities should follow this RFC.

* The very specific :ref:`sozip` utility will not follow this RFC. It has been
  design to mimic the existing standard ``zip`` utility.

Backward compatibility
----------------------

Fully backwards compatible. Existing utilities will remain for now, and if the
project decides to retire them in the future, that will likely go through a
multi-year deprecation period. Such decision will be made later, depending on
the maturity of the new unified CLI approach, and its adoption status by the
community.

Before GDAL 3.11 release, we'll need to decide if we advertise the already
implemented commands as stable or experimental. My feeling is that it might be
prudent to label them as experimental for now, but release them as part of
3.11.0 to get broader feedback from users, before stabilizing command and option
names.

Testing
-------

Testing of the parsing logic of GDALAlgorithm, setting argument values will
be done in C++ in :file:`autotest/cpp/test_gdal_algorithm.cpp`.
Testing of the commands and subcommands of ``gdal`` will be done in Python
in :file:`autotest/utilities`.

Documentation
-------------

The new ``gdal`` utility and its commands will be documented in https://gdal.org/programs

Staffing
--------

The candidate implementation will be done by Even Rouault. Full scope will
likely require a team effort, at least if we want to have a significant subset
ready for 3.11. Otherwise it might take several release cycles. At the very least
we'll need double checking of all naming to avoid adding new inconsistencies!

Related issues and PRs
----------------------

* Candidate implementation: https://github.com/OSGeo/gdal/pull/11303

Voting history
--------------

+1 from PSC members JukkaR, DanielM, JavierJS, HowardB and EvenR



.. below is an allow-list for spelling checker.

.. spelling:word-list::
    acyclic
    CLI
    Subcommand
    subcommand
    subcommands
    multidim
    tileindex
    cleanborder
    ranav
    argparser
    GDALAlgorithm
    GDALAlgorithmArg
    GDALArgDatasetValue
    reviewability
