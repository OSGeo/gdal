.. _gdal_bash_completion:

================================================================================
Bash completion for ``gdal``
================================================================================

.. versionadded:: 3.11

If Bash is used as an interactive shell, users can benefit from completion,
using the <TAB> key. Bash completion is enabled by first sourcing the
:file:`${install_prefix}/share/bash-completion/completions/gdal` file, which
should be automatically done on a number of binary distributions of GDAL.

That script can also be sourced when using :program:`zsh` as a shell (starting
with GDAL 3.11.1)

Examples of completion
++++++++++++++++++++++

.. example::
   :title: Listing sub-commands of "gdal":

   .. code-block:: console

        $ gdal <TAB><TAB>
        ==>
        convert   info      pipeline  raster    vector


.. example::
   :title: Completion of a sub-command from its initial letters:

   .. code-block:: console

        $ gdal r<TAB>
        ==>
        $ gdal raster


.. example::
   :title: Listing sub-commands of "gdal raster":

   .. code-block:: console

        $ gdal raster<TAB><TAB>
        ==>
        convert    edit       info       pipeline   reproject


.. example::
   :title: Listing switches of "gdal raster"

   .. code-block:: console

        $ gdal raster -<TAB><TAB>
        ==>
        --approx-stats   -f               --help           --if             --json-usage     --min-max        --no-fl          --no-md          --oo             --stats
        --checksum       --format         --hist           --input          --list-mdd       --mm             --no-gcp         --no-nodata      --open-option    --subdataset
        --drivers        -h               -i               --input-format   --mdd            --no-ct          --no-mask        --of             --output-format  --version

.. example::
   :title: Listing allowed values for a switch

   .. code-block:: console

        $ gdal raster info --of=<TAB><TAB>
        ==>
        json  text

.. example::
   :title: Listing allowed creation options, restricted to those valid for the output format, once the output filename has been specified

   .. code-block:: console

        $ gdal raster convert in.tif out.tif --co <TAB><TAB>
        ==>
        ALPHA=                           ENDIANNESS=                      JXL_EFFORT=                      PIXELTYPE=                       SOURCE_PRIMARIES_RED=            TIFFTAG_TRANSFERRANGE_BLACK=
        BIGTIFF=                         GEOTIFF_KEYS_FLAVOR=             JXL_LOSSLESS=                    PREDICTOR=                       SOURCE_WHITEPOINT=               TIFFTAG_TRANSFERRANGE_WHITE=
        BLOCKXSIZE=                      GEOTIFF_VERSION=                 LZMA_PRESET=                     PROFILE=                         SPARSE_OK=                       TILED=
        [ ... snip ... ]


.. example::
   :title: Listing known configuration options starting with AWS

   .. code-block:: console

        $ gdal --config AWS_<TAB><TAB>
        ==>
        AWS_ACCESS_KEY_ID=                       AWS_DEFAULT_REGION=                      AWS_REQUEST_PAYER=                       AWS_STS_ENDPOINT=
        AWS_CONFIG_FILE=                         AWS_HTTPS=                               AWS_ROLE_ARN=                            AWS_STS_REGION=
        AWS_CONTAINER_AUTHORIZATION_TOKEN=       AWS_MAX_KEYS=                            AWS_ROLE_SESSION_NAME=                   AWS_STS_REGIONAL_ENDPOINTS=
        AWS_CONTAINER_AUTHORIZATION_TOKEN_FILE=  AWS_NO_SIGN_REQUEST=                     AWS_S3_ENDPOINT=                         AWS_TIMESTAMP=
        AWS_CONTAINER_CREDENTIALS_FULL_URI=      AWS_PROFILE=                             AWS_SECRET_ACCESS_KEY=                   AWS_VIRTUAL_HOSTING=
        AWS_DEFAULT_PROFILE=                     AWS_REGION=                              AWS_SESSION_TOKEN=                       AWS_WEB_IDENTITY_TOKEN_FILE=


.. example::
   :title: Auto-completion of EPSG CRS codes

   .. code-block:: console

        $ gdal raster reproject --dst-crs EPSG:432<TAB>
        ==>
        4322 -- WGS 72                  4324 -- WGS 72BE                4326 -- WGS 84                  4327 -- WGS 84 (geographic 3D)  4328 -- WGS 84 (geocentric)     4329 -- WGS 84 (3D)

.. example::
   :title: Auto-completion of filenames in a cloud storage (assuming credentials are properly set up)

   .. code-block:: console

        $ gdal raster info /vsis3/my_bucket/b<TAB><TAB>
        ==>
        /vsis3/my_bucket/byte.tif      /vsis3/my_bucket/byte2.tif
