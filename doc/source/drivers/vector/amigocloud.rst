.. _vector.amigocloud:

================================================================================
AmigoCloud
================================================================================

.. shortname:: AmigoCloud

.. build_dependencies:: libcurl

This driver can connect to the AmigoCloud API services. GDAL/OGR must be built
with Curl support in order for the AmigoCloud driver to be compiled.

The driver supports read and write operations.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Dataset name syntax
-------------------

The minimal syntax to open a AmigoCloud datasource is:

.. code-block::

   AmigoCloud:[project_id]

Additional optional parameters can be specified after the ':' sign.
Currently the following one is supported :

-  **datasets=dataset_id1[,dataset_id2, ..]**: A list of AmigoCloud
   dataset IDs. This is necessary when you need to access a particular
   AmigoCloud dataset.

If several parameters are specified, they must be separated by a space.

If no dataset_id is provided, the driver will print list of available
datasets for given project.

For example: **"AmigoCloud:1234 datasets"**

.. code-block::

    List of available datasets for project id: 1234
    | id        | name
    |-----------|-------------------
    | 5551      | points
    | 5552      | lines

Authentication
--------------

All the access permissions are defined by AmigoCloud backend.

Authenticated access is obtained by specifying the API key given in the
AmigoCloud dashboard web interface. It is specified with the :config:`AMIGOCLOUD_API_KEY`
configuration option.

Geometry
--------

The OGR driver will report as many geometry fields as available in the
layer, following RFC 41.

Filtering
---------

The driver will forward any spatial filter set with
:cpp:func:`OGRLayer::SetSpatialFilter` to the server. It also makes the same
for attribute filters set with :cpp:func:`OGRLayer::SetAttributeFilter`.

Write support
-------------

Dataset creation and deletion is possible.

Write support is only enabled when the datasource is opened in update
mode.

The mapping between the operations of the AmigoCloud service and the OGR
concepts is the following :

- :cpp:func:`OGRLayer::CreateFeature` <==> ``INSERT`` operation
- :cpp:func:`OGRLayer::SetFeature` <==> ``UPDATE`` operation
- :cpp:func:`OGRLayer::DeleteFeature` <==> ``DELETE`` operation
- :cpp:func:`GDALDataset::CreateLayer` <==> ``CREATE TABLE`` operation
- :cpp:func:`GDALDataset::DeleteLayer` <==> ``DROP TABLE`` operation

When inserting a new feature with CreateFeature(), and if the command is
successful, OGR will fetch the returned amigo_id (GUID) and use hash
value of it as the OGR FID.

The above operations are by default issued to the server synchronously
with the OGR API call. This however can cause performance penalties when
issuing a lot of commands due to many client/server exchanges.

Layer creation options
----------------------

|about-layer-creation-options|
The following layer creation options are available:

-  .. lco:: OVERWRITE
      :choices: YES, NO
      :default: NO

      Whether to overwrite an existing table with
      the layer name to be created.

-  .. lco:: GEOMETRY_NULLABLE
      :choices: YES, NO
      :default: YES

      Whether the values of the geometry column can be NULL.

Configuration options
---------------------

|about-config-options|
The following configuration options are available:

-  .. config:: AMIGOCLOUD_API_URL
      :default: https://www.amigocloud.com/api/v1

      Can be used to point to another server.

-  .. config:: AMIGOCLOUD_API_KEY

      See usage examples.

Examples
--------

.. example::
   :title: Different ways to provide AmigoCloud API token

   .. code-block:: bash
   
       ogrinfo --config AMIGOCLOUD_API_KEY abcdefghijklmnopqrstuvw -al "AmigoCloud:1234 datasets=987"
       ogrinfo -oo AMIGOCLOUD_API_KEY=abcdefghijklmnopqrstuvw -al "AmigoCloud:1234 datasets=987"
       env AMIGOCLOUD_API_KEY=abcdefghijklmnopqrstuvw ogrinfo -al "AmigoCloud:1234 datasets=987"
   
   .. code-block::
   
       export AMIGOCLOUD_API_KEY=abcdefghijklmnopqrstuvw
       ogrinfo -al "AmigoCloud:1234 datasets=987"
   
.. example::
   :title: Show list of datasets

   .. code-block:: console
   
       $ ogrinfo -ro "AmigoCloud:1234 datasets"
       List of available datasets for project id: 1234
       | id        | name
       |-----------|-------------------
       | 5551      | points
       | 5552      | lines

.. example:: 
   :title: Accessing data from a list of datasets

   .. code-block:: bash
   
       ogrinfo -ro "AmigoCloud:1234 datasets=1234,1235"

.. example::
   :title: Creating and populating a table from a shapefile

   .. code-block::
   
       ogr2ogr -f AmigoCloud "AmigoCloud:1234" myshapefile.shp

.. example::
   :title: Append the data to an existing table (dataset id: 12345) from a shapefile

   .. code-block:: bash
   
       ogr2ogr -f AmigoCloud "AmigoCloud:1234 datasets=12345" myshapefile.shp
   
   or
   
   .. code-block:: bash
   
       ogr2ogr -append -f AmigoCloud "AmigoCloud:1234 datasets=12345" myshapefile.shp

.. example::
   :title: Overwriting the data of an existing table (dataset id: 12345) with data from a shapefile

   .. code-block:: bash
   
       ogr2ogr -append -doo OVERWRITE=YES -f AmigoCloud "AmigoCloud:1234 datasets=12345" myshapefile.shp

.. example::
   :title: Delete existing dataset (dataset id: 12345) and create a new one with data from a shapefile

   .. code-block:: bash
   
       ogr2ogr -overwrite -f AmigoCloud "AmigoCloud:1234 datasets=12345" myshapefile.shp

.. example::
   :title: Overwriting the data of an existing table (dataset id: 12345) with data from a shapefile. 
   
   Filter the only the records with values of the field "visited_on"
   after 2017-08-20
   
   .. code-block:: bash
   
       ogr2ogr -append -doo OVERWRITE=YES -f AmigoCloud "AmigoCloud:1234 datasets=12345" -where "visited_on > '2017-08-20'" myshapefile.shp

See Also
--------

-  `AmigoCloud API Token management <https://www.amigocloud.com/accounts/tokens>`__
-  `AmigoCloud API Browser <https://www.amigocloud.com/api/v1/>`__
