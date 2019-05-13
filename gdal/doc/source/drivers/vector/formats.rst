.. _vector.formats:

OGR Vector Formats
==================

==================================================================== ============================= ======== ======================== ========================================================
Format Name                                                          Code                          Creation Georeferencing           Compiled by default
==================================================================== ============================= ======== ======================== ========================================================
`Aeronav FAA files <drv_aeronavfaa.html>`__                          AeronavFAA                    No       Yes                      Yes
`AmigoCloud API <drv_amigocloud.html>`__                             AmigoCloud                    Yes      Yes                      No, needs libcurl
`ESRI ArcObjects <drv_ao.html>`__                                    ArcObjects                    No       Yes                      No, needs ESRI ArcObjects
`Arc/Info Binary Coverage <drv_avcbin.html>`__                       AVCBin                        No       Yes                      Yes
`Arc/Info .E00 (ASCII) Coverage <drv_avce00.html>`__                 AVCE00                        No       Yes                      Yes
`Arc/Info Generate <drv_arcgen.html>`__                              ARCGEN                        No       No                       Yes
`Atlas BNA <drv_bna.html>`__                                         BNA                           Yes      No                       Yes
`AutoCAD DWG <drv_cad.html>`__                                       CAD                           No       Yes                      Yes (internal libopencad provided)
`AutoCAD DWG <drv_dwg.html>`__                                       DWG                           No       No                       No, needs Open Design Alliance Teigha library
`AutoCAD DXF <drv_dxf.html>`__                                       DXF                           Yes      No                       Yes
`Carto <drv_carto.html>`__                                           Carto                         Yes      Yes                      No, needs libcurl
`Cloudant / CouchDB <drv_cloudant.html>`__                           Cloudant                      Yes      Yes                      No, needs libcurl
`CouchDB / GeoCouch <drv_couchdb.html>`__                            CouchDB                       Yes      Yes                      No, needs libcurl
`Comma Separated Value (.csv) <drv_csv.html>`__                      CSV                           Yes      No                       Yes
`OGC CSW (Catalog Service for the Web) <drv_csw.html>`__             CSW                           No       Yes                      No, needs libcurl
`Czech Cadastral Exchange Data Format <drv_vfk.html>`__              VFK                           No       Yes                      No, needs libsqlite3
`DB2 Spatial <drv_db2.html>`__                                       DB2ODBC                       Yes      Yes                      No, needs ODBC library
`DODS/OPeNDAP <drv_dods.html>`__                                     DODS                          No       Yes                      No, needs libdap
`Google Earth Engine Data API <drv_eeda.html>`__                     EEDA                          No       Yes                      No, needs libcurl
`EDIGEO <drv_edigeo.html>`__                                         EDIGEO                        No       Yes                      Yes
`ElasticSearch <drv_elasticsearch.html>`__                           ElasticSearch                 Yes      Yes                      No, needs libcurl
`ESRI FileGDB <drv_filegdb.html>`__                                  FileGDB                       Yes      Yes                      No, needs FileGDB API library
`ESRI Personal GeoDatabase <drv_pgeo.html>`__                        PGeo                          No       Yes                      No, needs ODBC library
`ESRI ArcSDE <drv_sde.html>`__                                       SDE                           No       Yes                      No, needs ESRI SDE
`ESRIJSON <drv_esrijson.html>`__                                     ESRIJSON                      No       Yes                      Yes
`ESRI Shapefile / DBF <drv_shapefile.html>`__                        ESRI Shapefile                Yes      Yes                      Yes
`FMEObjects Gateway <drv_fme.html>`__                                FMEObjects Gateway            No       Yes                      No, needs FME
`GeoJSON <drv_geojson.html>`__                                       GeoJSON                       Yes      Yes                      Yes
`GeoJSONSeq <drv_geojsonseq.html>`__                                 GeoJSON sequences             Yes      Yes                      Yes
`Géoconcept Export <drv_geoconcept.html>`__                          Geoconcept                    Yes      Yes                      Yes
`Geomedia .mdb <drv_geomedia.html>`__                                Geomedia                      No       No                       No, needs ODBC library
`GeoPackage <drv_geopackage.html>`__                                 GPKG                          Yes      Yes                      No, needs libsqlite3
`GeoRSS <drv_georss.html>`__                                         GeoRSS                        Yes      Yes                      Yes (read support needs libexpat)
`Google Fusion Tables <drv_gft.html>`__                              GFT                           Yes      Yes                      No, needs libcurl
`GML <drv_gml.html>`__                                               GML                           Yes      Yes                      Yes (read support needs Xerces or libexpat)
`GMLAS <drv_gmlas.html>`__                                           GMLAS                         Yes      Yes                      Yes (requires Xerces)
`GMT <drv_gmt.html>`__                                               GMT                           Yes      Yes                      Yes
`GPSBabel <drv_gpsbabel.html>`__                                     GPSBabel                      Yes      Yes                      Yes (needs GPSBabel and GPX driver)
`GPX <drv_gpx.html>`__                                               GPX                           Yes      Yes                      Yes (read support needs libexpat)
`GRASS Vector Format <drv_grass.html>`__                             GRASS                         No       Yes                      No, needs libgrass
`GPSTrackMaker (.gtm, .gtz) <drv_gtm.html>`__                        GPSTrackMaker                 Yes      Yes                      Yes
`Hydrographic Transfer Format <drv_htf.html>`__                      HTF                           No       Yes                      Yes
`Idrisi Vector (.VCT) <drv_idrisi.html>`__                           Idrisi                        No       Yes                      Yes
`Informix DataBlade <drv_idb.html>`__                                IDB                           Yes      Yes                      No, needs Informix DataBlade
`INTERLIS <drv_ili.html>`__                                          "Interlis 1" and "Interlis 2" Yes      Yes                      No, needs Xerces
`INGRES <drv_ingres.html>`__                                         INGRES                        Yes      No                       No, needs INGRESS
`JML <drv_jml.html>`__                                               OpenJUMP .jml                 Yes      No                       Yes (read support needs libexpat)
`KML <drv_kml.html>`__                                               KML                           Yes      Yes                      Yes (read support needs libexpat)
`LIBKML <drv_libkml.html>`__                                         LIBKML                        Yes      Yes                      No, needs libkml
`Mapinfo File <drv_mitab.html>`__                                    MapInfo File                  Yes      Yes                      Yes
`Microstation DGN v7 <drv_dgn.html>`__                               DGN                           Yes      No                       Yes
`Microstation DGN v8 <drv_dgnv8.html>`__                             DGNv8                         Yes      No                       No, needs Open Design Alliance Teigha library
`Access MDB (PGeo and Geomedia capable) <drv_mdb.html>`__            MDB                           No       Yes                      No, needs JDK/JRE
`Memory <drv_memory.html>`__                                         Memory                        Yes      Yes                      Yes
`MongoDB <drv_mongodb.html>`__                                       MongoDB                       Yes      Yes                      No, needs Mongo C++ client legacy library
`MongoDBv3 <drv_mongodbv3.html>`__                                   MongoDBv3                     Yes      Yes                      No, needs Mongo CXX >= 3.4.0 client library
`Mapbox Vector Tiles <drv_mvt.html>`__                               MVT                           Yes      Yes                      Yes (requires SQLite and GEOS for write support)
`MySQL <drv_mysql.html>`__                                           MySQL                         No       Yes                      No, needs MySQL library
`NAS - ALKIS <drv_nas.html>`__                                       NAS                           No       Yes                      No, needs Xerces
`NetCDF <frmt_netcdf_vector.html>`__                                 netCDF                        Yes      Yes                      No, needs libnetcdf
`NextGIS Web <drv_ngw.html>`__                                       NGW                           Yes      Yes                      No, needs libcurl
`Oracle Spatial <drv_oci.html>`__                                    OCI                           Yes      Yes                      No, needs OCI library
`ODBC <drv_odbc.html>`__                                             ODBC                          No       Yes                      No, needs ODBC library
`MS SQL Spatial <drv_mssqlspatial.html>`__                           MSSQLSpatial                  Yes      Yes                      No, needs ODBC library
`Open Document Spreadsheet <drv_ods.html>`__                         ODS                           Yes      No                       No, needs libexpat
`OGDI Vectors (VPF, VMAP, DCW) <drv_ogdi.html>`__                    OGDI                          No       Yes                      No, needs OGDI library
`OpenAir <drv_openair.html>`__                                       OpenAir                       No       Yes                      Yes
`ESRI FileGDB <drv_openfilegdb.html>`__                              OpenFileGDB                   No       Yes                      Yes
`OpenStreetMap XML and PBF <drv_osm.html>`__                         OSM                           No       Yes                      No, needs libsqlite3 (and libexpat for OSM XML)
`PCI Geomatics Database File <../frmt_pcidsk.html>`__                PCIDSK                        Yes      Yes                      Yes, using internal PCIDSK SDK (from GDAL 1.7.0)
`Geospatial PDF <../frmt_pdf.html>`__                                PDF                           Yes      Yes                      Yes (read supports need libpoppler or libpodofo support)
`PDS <drv_pds.html>`__                                               PDS                           No       Yes                      Yes
`Planet Labs Scenes API <drv_plscenes.html>`__                       PLScenes                      No       Yes                      No, needs libcurl
`PostgreSQL SQL dump <drv_pgdump.html>`__                            PGDump                        Yes      Yes                      Yes
`PostgreSQL/PostGIS <drv_pg.html>`__                                 PostgreSQL/PostGIS            Yes      Yes                      No, needs PostgreSQL client library (libpq)
EPIInfo .REC                                                         REC                           No       No                       Yes
`S-57 (ENC) <drv_s57.html>`__                                        S57                           No       Yes                      Yes
`SDTS <drv_sdts.html>`__                                             SDTS                          No       Yes                      Yes
`SEG-P1 / UKOOA P1/90 <drv_segukooa.html>`__                         SEGUKOOA                      No       Yes                      Yes
`SEG-Y <drv_segy.html>`__                                            SEGY                          No       No                       Yes
`Selafin/Seraphin format <drv_selafin.html>`__                       Selafin                       Yes      Partial (only EPSG code) Yes
`Norwegian SOSI Standard <http://trac.osgeo.org/gdal/ticket/3638>`__ SOSI                          No       Yes                      No, needs FYBA library
`SQLite/SpatiaLite <drv_sqlite.html>`__                              SQLite                        Yes      Yes                      No, needs libsqlite3 or libspatialite
`SUA <drv_sua.html>`__                                               SUA                           No       Yes                      Yes
`SVG <drv_svg.html>`__                                               SVG                           No       Yes                      No, needs libexpat
`Storage and eXchange Format <drv_sxf.html>`__                       SXF                           No       Yes                      Yes
`U.S. Census TIGER/Line <drv_tiger.html>`__                          TIGER                         No       Yes                      Yes
`TopoJSON <drv_topojson.html>`__                                     TopoJSON                      No       Yes                      Yes
`UK .NTF <drv_ntf.html>`__                                           UK. NTF                       No       Yes                      Yes
`VDV-451/VDV-452/IDF <drv_vdv.html>`__                               VDV                           Yes      Yes                      Yes
`VRT - Virtual Datasource <drv_vrt.html>`__                          VRT                           No       Yes                      Yes
`Walk <drv_walk.html>`__                                             Walk                          No       Yes                      No, needs ODBC library
`WAsP .map format <drv_wasp.html>`__                                 WAsP                          Yes      Yes                      Yes
`OGC WFS (Web Feature Service) <drv_wfs.html>`__                     WFS                           Yes      Yes                      No, needs libcurl
`OGC WFS 3.0 (Web Feature Service) (experimental) <drv_wfs3.html>`__ WFS3                          No       Yes                      No, needs libcurl
`MS Excel format <drv_xls.html>`__                                   XLS                           No       No                       No, needs libfreexl
`MS Office Open XML spreadsheet <drv_xlsx.html>`__                   XLSX                          Yes      No                       No, needs libexpat
`X-Plane/Flightgear aeronautical data <drv_xplane.html>`__           XPLANE                        No       Yes                      Yes
==================================================================== ============================= ======== ======================== ========================================================
