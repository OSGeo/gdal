	GDAL Binary Distribution for Windows
	====================================

This distribution contains files necessary to use GDAL utility applications,
as well as library and include files necessary to compile and link other
programs against GDAL.  It also include support for using GDAL from Python 2.0.

Local information about GDAL can be found in html\index.html.

eg. 
  start html\index.html

Central information can be found at:

  http://www.remotesensing.org/gdal


Installation
------------

It is sufficient to unzip this distribution somewhere on your harddrive, and
add the GDALxxx\BIN directory to your path (where xxx is the version). 
Alternatively, just copy the contents of the BIN directory to somewhere in
your path.

For those wanting to develop with GDAL using VC++ commandline tools, a small
setup script has been written to update the environment appropriately. To 
use it, edit SETUP_GDAL.BAT, and modify the GDAL_DIR environment to point to
where GDAL has been put.  Then run SETUP_GDAL.BAT in a DOS window before
trying to compile against GDAL. 

Utility Programs
----------------

The following GDAL and OGR utility programs should be available in the bin
directory.  Visit the GDAL web page for more information on the utility
programs. 

  gdalinfo.exe: Report summary information about a GDAL supported raster file.
  gdal_translate.exe: Translate GDAL supported raster files into other formats.
  gdaladdo: Add overviews to selected GDAL supported formats. 
  ogrinfo: Dump information about OGR supported vector formats.
  ogr2ogr: Translate OGR supported vector formats to OGR supported formats.


More Detailed Manifest
----------------------

BIN		All distributed executables, and the DLLs required for them.
		The GDAL11.DLL is the main GDAL/OGR DLL.  PROJ.DLL is the
		PROJ.4 DLL used by GDAL for reprojection.

HTML		Web pages, a snapshot of the www.remotesensing.org/gdal web
		pages at the time the distribution was prepared.

INCLUDE		GDAL and OGR include files.  

LIB		GDAL/OGR libraries.  GDAL.LIB is the omnibus GDAL/OGR static
		library.  GDAL_I.LIB is the stub (interface) library for the
		GDAL11.DLL.  Link against GDAL_I.LIB to use the DLL. 

DATA		Data files required by OGRSpatialReference and the GDAL 
		GeoTIFF driver for looking up EPSG coordinate systems.  Define
		GEOTIFF_CSV to point to the directory where these files are 
		found. 

PYMOD		Python extensions for GDAL.  Add this directory to your 
		PYTHONPATH to use from python.

Supported Formats
-----------------

The html\formats_list.html document contains a generic list of supported 
formats for GDAL, but the following issues should be kept in mind for standard
builds on Windows:

GDAL:
 o OGDI is omitted.
 o FITS is omitted.
 o GRASS omitted.
 o JPEG, PNG, GeoTIFF are all using internal library code. 

OGR:
 o OGDI omitted.
 o PostgreSQL omitted.

