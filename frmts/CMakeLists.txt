# CMake4GDAL project is distributed under MIT license. See accompanying file LICENSE.txt.

add_library(gdal_frmts OBJECT gdalallregister.cpp)
include(GdalDriverHelper)
include(CMakeDependentOption)

# JPEG must be defined before GTiff
gdal_dependent_format(jpeg "JPEG image format" "GDAL_USE_JPEG OR GDAL_USE_JPEG_INTERNAL")

# base driver provide frmt core functions
gdal_optional_format(raw "Raw formats:EOSAT FAST Format, FARSITE LCP and Vexcel MFF2 Image")
gdal_format(iso8211 "iso8211 library")
# JPEG must be defined before GTiff
gdal_format(gtiff "GeoTIFF image format")
set_package_properties(TIFF PROPERTIES PURPOSE "gdal_GTIFF: GeoTIFF image format")
gdal_format(mem "Read/write data in Memory")
gdal_format(vrt "Virtual GDAL Datasets")
# Note: derived is derived of vrt
add_subdirectory(derived)

# default formats

if (CMAKE_BUILD_TYPE MATCHES "Debug" OR GDAL_ENABLE_DRIVER_NULL)
  gdal_optional_format(null "NULL dummy driver")
endif ()

gdal_format(hfa "Erdas Imagine .img")
gdal_optional_format(sdts "SDTS translator")
gdal_optional_format(nitf "National Imagery Transmission Format")
gdal_optional_format(gxf "GXF")
gdal_optional_format(aaigrid "Arc/Info ASCII Grid Format.")
gdal_optional_format(ceos "CEOS translator")
gdal_optional_format(ceos2 "ASI CEOS translator" DRIVER_NAME_OPTION "SAR_CEOS")
gdal_optional_format(xpm "XPM image format")
gdal_optional_format(dted "Military Elevation Data")
gdal_optional_format(jdem "JDEM driver")
gdal_optional_format(envisat "Envisat")
gdal_optional_format(elas "Earth Resources Laboratory Applications Software")
gdal_optional_format(fit "FIT driver")
gdal_optional_format(l1b "NOAA Polar Orbiter Level 1b Data Set (AVHRR)")
gdal_optional_format(rs2 "RS2 -- RadarSat 2 XML Product")
gdal_optional_format(ilwis "Raster Map")
gdal_optional_format(rmf "RMF --- Raster Matrix Format")
gdal_optional_format(leveller "Daylon Leveller heightfield")
gdal_optional_format(sgi "SGI Image driver")
gdal_optional_format(srtmhgt "SRTM HGT File Read Support")
gdal_optional_format(idrisi "Idrisi Raster Format")
gdal_optional_format(gsg "Implements the Golden Software Surfer 7 Binary Grid Format.")
gdal_optional_format(ers "ERMapper .ERS")
gdal_optional_format(jaxapalsar "JAXA PALSAR Level 1.1 and Level 1.5 processed products support")
gdal_optional_format(dimap "SPOT Dimap Driver")
gdal_optional_format(gff "Ground-based SAR Applitcations Testbed File Format driver")
gdal_optional_format(cosar "COSAR -- TerraSAR-X Complex SAR Data Product")
gdal_optional_format(pds "USGS Astrogeology ISIS Cube (Version 2)")
gdal_optional_format(adrg "ADRG reader and ASRP/USRP Reader")
gdal_optional_format(coasp "DRDC Configurable Airborne SAR Processor (COASP) data reader")
gdal_optional_format(tsx "TerraSAR-X XML Product Support")
gdal_optional_format(terragen "Terragen&trade; Terrain File")
gdal_optional_format(blx "Magellan BLX Topo File Format")
gdal_optional_format(msgn "Meteosat Second Generation (MSG) Native Archive Format (.nat)")
gdal_optional_format(til "EarthWatch .TIL Driver")
gdal_optional_format(r "R Object Data Store")
gdal_optional_format(northwood "NWT_GRD/NWT_GRC -- Northwood/Vertical Mapper File Format")
gdal_optional_format(saga "SAGA GIS Binary Driver")
gdal_optional_format(xyz "ASCII Gridded XYZ")
gdal_dependent_format(heif "HEIF" "GDAL_USE_HEIF")
gdal_optional_format(esric "ESRI compact cache")
gdal_optional_format(hf2 "HF2/HFZ heightfield raster")
gdal_optional_format(kmlsuperoverlay "")
gdal_optional_format(ctg "CTG driver")
gdal_optional_format(zmap "ZMAP")
gdal_optional_format(ngsgeoid "NOAA NGS Geoid Height Grids")
gdal_optional_format(iris "IRIS driver")
gdal_optional_format(map "OziExplorer .MAP")
gdal_optional_format(cals "CALS type 1")
set_package_properties(TIFF PROPERTIES PURPOSE "gdal_CALS: CALS type 1 driver")
gdal_optional_format(safe "SAFE -- Sentinel-1 SAFE XML Product")
gdal_optional_format(sentinel2 "Driver for Sentinel-2 Level-1B, Level-1C and Level-2A products.")
gdal_optional_format(prf "PHOTOMOD Raster File")
gdal_optional_format(mrf "Meta raster format")
gdal_optional_format(wmts "OGC Web Map Tile Service")
gdal_optional_format(grib "WMO General Regularly-distributed Information in Binary form")
gdal_optional_format(bmp "Microsoft Windows Device Independent Bitmap")
gdal_optional_format(daas "Airbus DS Intelligence Data As A Service(DAAS)")
gdal_optional_format(tga "TGA")
gdal_optional_format(stacta "STACTA")
gdal_optional_format(ogcapi "OGCAPI")

# optional Formats
gdal_optional_format(bsb "Maptech/NOAA BSB Nautical Chart Format")
gdal_dependent_format(aigrid "Arc/Info Binary Grid Format" "OGR_ENABLE_DRIVER_AVC")
gdal_optional_format(arg "ARG: Azavea Raster Grid")
gdal_optional_format(usgsdem "USGS ASCII DEM (and CDED)")
gdal_optional_format(airsar "AirSAR Polarimetric Format")
gdal_optional_format(ozi "OZF2/OZFX3 raster")
gdal_optional_format(pcidsk "PCI Geomatics Database File")
gdal_optional_format(sigdem "Scaled Integer Gridded DEM .sigdem Driver")
gdal_dependent_format(msg "Meteosat Second Generation" "GDAL_USE_PUBLICDECOMPWT")
gdal_optional_format(rik "RIK -- Swedish Grid Maps")
gdal_optional_format(stacit "STACIT")
gdal_optional_format(pdf "Geospatial PDF") # write only if none of GDAL_USE_POPPLER, GDAL_USE_PODOFO or GDAL_USE_PDFIUM
                                           # is set

# formats with external library dependency
gdal_dependent_format(png "PNG image format" "GDAL_USE_PNG OR GDAL_USE_PNG_INTERNAL")
gdal_dependent_format(gif "Graphics Interchange Format" "GDAL_USE_GIF OR GDAL_USE_GIF_INTERNAL")

gdal_dependent_format(wcs "OGC Web Coverage Service" "GDAL_USE_CURL" SKIP_ADD_SUBDIRECTORY)
gdal_dependent_format(http "HTTP driver" "GDAL_USE_CURL" SKIP_ADD_SUBDIRECTORY)
add_subdirectory(wcs)

gdal_dependent_format(netcdf "NetCDF network Common Data Form" "GDAL_USE_NETCDF")
gdal_optional_format(zarr "ZARR")
gdal_dependent_format(eeda "Earth Engine Data API" "GDAL_USE_CURL")
gdal_dependent_format(fits "FITS Driver" "GDAL_USE_CFITSIO")
gdal_dependent_format(rasdaman "Rasdaman GDAL driver" "GDAL_USE_RASDAMAN")
gdal_dependent_format(hdf5 "Hierarchical Data Format Release 5 (HDF5)" "GDAL_USE_HDF5")
gdal_dependent_format(plmosaic "PLMosaic (Planet Labs Mosaics API)" "GDAL_USE_CURL")
gdal_dependent_format(wms "Web Map Services" "GDAL_USE_CURL")
gdal_dependent_format(gta "Generic Tagged Arrays" "GDAL_USE_GTA")
gdal_dependent_format(webp "WebP" "GDAL_USE_WEBP")
gdal_dependent_format(hdf4 "Hierarchical Data Format Release 4 (HDF4)" "GDAL_USE_HDF4")
gdal_dependent_format(rasterlite "Rasterlite - Rasters in SQLite DB" "GDAL_USE_SQLITE3;OGR_ENABLE_DRIVER_SQLITE")
gdal_dependent_format(mbtiles "MBTile" "GDAL_USE_SQLITE3;OGR_ENABLE_DRIVER_GPKG")
gdal_dependent_format(postgisraster "PostGIS Raster driver" "GDAL_USE_POSTGRESQL")
gdal_dependent_format(dds "DirectDraw Surface" "GDAL_USE_CRNLIB")
gdal_dependent_format(kea "Kea" "GDAL_USE_KEA;GDAL_USE_HDF5")
gdal_dependent_format(openjpeg "JPEG2000 driver based on OpenJPEG library" "GDAL_USE_OPENJPEG" DRIVER_NAME_OPTION
                      "JP2OpenJPEG")
gdal_dependent_format(tiledb "TileDB tiledb.io" "GDAL_USE_TILEDB")
gdal_dependent_format(exr "EXR support via OpenEXR library" "GDAL_USE_OPENEXR")
gdal_dependent_format(pcraster "PCRaster CSF 2.0 raster file driver" "GDAL_USE_LIBCSF OR GDAL_USE_LIBCSF_INTERNAL")
gdal_dependent_format(rdb "RIEGL RDB Map Pixel (.mpx) driver" "rdb_FOUND")

# ######################################################################################################################
# driver with proprietary libraries Kakadu software SDK
gdal_dependent_format(jp2kak "JPEG-2000 (based on Kakadu)" "GDAL_USE_KDU")
gdal_dependent_format(jpipkak "JPIP Streaming" "GDAL_USE_KDU")
# Luratech SDK
gdal_dependent_format(jp2lura "JPEG-2000 (based on Luratech)" "GDAL_USE_LURATECH")
# ESRI ArcSDE C API SDK
gdal_dependent_format(sde "ESRI ArcSDE Raster" "HAVE_SDE")
# LizardTech's decoding software development kit (DSDK)
gdal_dependent_format(mrsid "Multi-resolution Seamless Image Database" "GDAL_USE_MRSID")
gdal_dependent_format(georaster "Oracle Spatial GeoRaster" "GDAL_USE_ORACLE" DRIVER_NAME_OPTION GEOR)
gdal_dependent_format(ecw "ERDAS JPEG2000 (.jp2)" "GDAL_USE_ECW")

# Register all frmt drivers according to FRMT_<driver> flags
gdal_standard_includes(gdal_frmts)
add_dependencies(gdal_frmts generate_gdal_version_h)
if (ENABLE_GNM)
  target_compile_definitions(gdal_frmts PRIVATE -DGNM_ENABLED)
  target_include_directories(gdal_frmts PRIVATE $<TARGET_PROPERTY:gnm_frmts,SOURCE_DIR>)
endif ()
target_include_directories(
  gdal_frmts PRIVATE $<TARGET_PROPERTY:gdal_vrt,SOURCE_DIR> $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>
                     $<TARGET_PROPERTY:ogr_MEM,SOURCE_DIR>)
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:gdal_frmts>)
set_property(TARGET gdal_frmts PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})

# Special case for HTTP driver since it is included in the frmts/wcs/ directory We should ultimately make it a separate
# frmts/http/ to clean up things.
if (GDAL_ENABLE_DRIVER_HTTP AND NOT GDAL_ENABLE_DRIVER_HTTP_PLUGIN)
  target_compile_definitions(gdal_frmts PRIVATE -DFRMT_http)
endif ()
