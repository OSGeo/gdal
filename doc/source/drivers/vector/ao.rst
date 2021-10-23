.. _vector.ao:

================================================================================
ESRI ArcObjects
================================================================================

.. shortname:: AO

.. build_dependencies:: ESRI ArcObjects

.. deprecated_driver:: version_targeted_for_removal: 3.5
   env_variable: GDAL_ENABLE_DEPRECATED_DRIVER_AO

Overview
--------

The OGR ArcObjects driver provides read-only access to ArcObjects based
datasources. Since it uses the ESRI SDK, it has the requirement of needing an
ESRI license to run. Nevertheless, this also means that the driver has full
knowledge of ESRI abstractions. Among these, you have:

* GeoDatabases:

    * Personal GeoDatabase (.mdb)
    * File GeoDatabase (.gdb)
    * Enterprise GeoDatabase (.sde).

* ESRI Shapefiles

Although it has not been extended to do this yet (there hasn't been a need), it
can potentially also support the following GeoDatabase Abstractions

* Annotation and Dimension feature classes
* Relationship Classes
* Networks (GN and ND)
* Topologies
* Terrains
* Representations
* Parcel Fabrics

You can try those above and they may work - but they have not been
tested. Note the abstractions above cannot be supported with the Open
FileGeoDatabase API.

Requirements
------------

* An ArcView license or ArcEngine license (or higher) - Required to run.

* The ESRI libraries installed. This typically happens if you have
  ArcEngine or ArcGIS Desktop or Server installed - Required to compile. Note
  that this code should also compile using the ArcEngine \*nix SDKs, however I
  do not have access to these and thus I have not tried it myself

Usage
-----

Prefix the Datasource with "AO:"

Read from FileGDB and load into PostGIS:

.. code-block::

    ogr2ogr -overwrite -skipfailures -f "PostgreSQL" PG:"host=myhost user=myuser dbname=mydb password=mypass" AO:"C:\somefolder\BigFileGDB.gdb" "MyFeatureClass"

Get detailed info of Personal GeoDatabase:

.. code-block::

    ogrinfo -al AO:"C:\somefolder\PersonalGDB.mdb"

Get detailed info of Enterprise GeoDatabase (.sde contains target
version to connect to):

.. code-block::

    ogrinfo -al AO:"C:\somefolder\MySDEConnection.sde"

Building Notes
--------------

Read the `GDAL Windows Building example for Plugins <http://trac.osgeo.org/gdal/wiki/BuildingOnWindows>`__.
You will find a similar section in :file:`nmake.opt` for ArcObjects.
After you are done, go to the :file:`$gdal_source_root/ogr/ogrsf_frmts/arcobjects*`
folder and execute:

.. code-block::

    nmake /f makefile.vc plugin
    nmake /f makefile.vc plugin-install

Known Issues
------------

Date and blob fields have not been implemented. It is probably just a
few lines of code, I just have not had time (or need) to do it.
