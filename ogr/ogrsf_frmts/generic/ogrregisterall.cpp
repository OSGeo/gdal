/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Function to register all known OGR drivers.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrsf_frmts.h"

/************************************************************************/
/*                           OGRRegisterAll()                           */
/************************************************************************/

void OGRRegisterAll()
{
    GDALAllRegister();
}

void OGRRegisterAllInternal()
{
    // NOTE: frmts/drivers.ini in the same directory should be kept in same
    // order as this file

#ifdef SHAPE_ENABLED
    RegisterOGRShape();
#endif
#ifdef MITAB_ENABLED
    RegisterOGRTAB();
#endif
#ifdef LVBAG_ENABLED
    RegisterOGRLVBAG();
#endif
#ifdef S57_ENABLED
    RegisterOGRS57();
#endif
#ifdef DGN_ENABLED
    RegisterOGRDGN();
#endif
#ifdef VRT_ENABLED
    RegisterOGRVRT();
#endif
#ifdef CSV_ENABLED
    RegisterOGRCSV();
#endif
#ifdef NAS_ENABLED
    RegisterOGRNAS();
#endif
#ifdef GML_ENABLED
    RegisterOGRGML();
#endif
#ifdef GPX_ENABLED
    RegisterOGRGPX();
#endif
#ifdef LIBKML_ENABLED
    RegisterOGRLIBKML();
#endif
#ifdef KML_ENABLED
    RegisterOGRKML();
#endif
#ifdef GEOJSON_ENABLED
    RegisterOGRGeoJSON();
    RegisterOGRGeoJSONSeq();
    RegisterOGRESRIJSON();
    RegisterOGRTopoJSON();
#endif
#ifdef ILI_ENABLED
    RegisterOGRILI1();
    RegisterOGRILI2();
#endif
#ifdef GMT_ENABLED
    RegisterOGRGMT();
#endif
#ifdef GPKG_ENABLED
    RegisterOGRGeoPackage();
#endif
#ifdef SQLITE_ENABLED
    RegisterOGRSQLite();
#endif
#ifdef ODBC_ENABLED
    RegisterOGRODBC();
#endif
#ifdef WASP_ENABLED
    RegisterOGRWAsP();
#endif
#ifdef PGEO_ENABLED
    RegisterOGRPGeo();
#endif
#ifdef MSSQLSPATIAL_ENABLED
    RegisterOGRMSSQLSpatial();
#endif
#ifdef PG_ENABLED
    RegisterOGRPG();
#endif
#ifdef MYSQL_ENABLED
    RegisterOGRMySQL();
#endif
#ifdef OCI_ENABLED
    RegisterOGROCI();
#endif
/* Register OpenFileGDB before FGDB as it is more capable for read-only */
#ifdef OPENFILEGDB_ENABLED
    RegisterOGROpenFileGDB();
#endif
#ifdef FGDB_ENABLED
    RegisterOGRFileGDB();
#endif
#ifdef DWG_ENABLED
    RegisterOGRDWG();
#endif
#ifdef DGNV8_ENABLED
    RegisterOGRDGNV8();
#endif
#ifdef DXF_ENABLED
    RegisterOGRDXF();
#endif
#ifdef CAD_ENABLED
    RegisterOGRCAD();
#endif
#ifdef FLATGEOBUF_ENABLED
    RegisterOGRFlatGeobuf();
#endif
#ifdef IDB_ENABLED
    RegisterOGRIDB();
#endif
#ifdef GEORSS_ENABLED
    RegisterOGRGeoRSS();
#endif
#ifdef VFK_ENABLED
    RegisterOGRVFK();
#endif
#ifdef PGDUMP_ENABLED
    RegisterOGRPGDump();
#endif
#ifdef OSM_ENABLED
    /* Register before GPSBabel, that could recognize .osm file too */
    RegisterOGROSM();
#endif
#ifdef GPSBABEL_ENABLED
    RegisterOGRGPSBabel();
#endif
#ifdef PDS_ENABLED
    RegisterOGRPDS();
#endif
#ifdef WFS_ENABLED
    RegisterOGRWFS();
#endif
#ifdef OAPIF_ENABLED
    RegisterOGROAPIF();
#endif
#ifdef SOSI_ENABLED
    RegisterOGRSOSI();
#endif
#ifdef EDIGEO_ENABLED
    RegisterOGREDIGEO();
#endif
#ifdef IDRISI_ENABLED
    RegisterOGRIdrisi();
#endif
#ifdef XLS_ENABLED
    RegisterOGRXLS();
#endif
#ifdef ODS_ENABLED
    RegisterOGRODS();
#endif
#ifdef XLSX_ENABLED
    RegisterOGRXLSX();
#endif
#ifdef ELASTIC_ENABLED
    RegisterOGRElastic();
#endif
#ifdef CARTO_ENABLED
    RegisterOGRCarto();
#endif
#ifdef AMIGOCLOUD_ENABLED
    RegisterOGRAmigoCloud();
#endif
#ifdef SXF_ENABLED
    RegisterOGRSXF();
#endif
#ifdef SELAFIN_ENABLED
    RegisterOGRSelafin();
#endif
#ifdef JML_ENABLED
    RegisterOGRJML();
#endif
#ifdef PLSCENES_ENABLED
    RegisterOGRPLSCENES();
#endif
#ifdef CSW_ENABLED
    RegisterOGRCSW();
#endif
#ifdef MONGODBV3_ENABLED
    RegisterOGRMongoDBv3();
#endif
#ifdef VDV_ENABLED
    RegisterOGRVDV();
#endif
#ifdef GMLAS_ENABLED
    RegisterOGRGMLAS();
#endif
#ifdef MVT_ENABLED
    RegisterOGRMVT();
#endif
#ifdef NGW_ENABLED
    RegisterOGRNGW();
#endif  // NGW_ENABLED
#ifdef MAPML_ENABLED
    RegisterOGRMapML();
#endif
#ifdef HANA_ENABLED
    RegisterOGRHANA();
#endif
#ifdef PARQUET_ENABLED
    RegisterOGRParquet();
#endif
#ifdef ARROW_ENABLED
    RegisterOGRArrow();
#endif
#ifdef GTFS_ENABLED
    RegisterOGRGTFS();
#endif
#ifdef PMTILES_ENABLED
    RegisterOGRPMTiles();
#endif
#ifdef JSONFG_ENABLED
    RegisterOGRJSONFG();
#endif
#ifdef MIRAMON_ENABLED
    RegisterOGRMiraMon();
#endif
#ifdef XODR_ENABLED
    RegisterOGRXODR();
#endif
#ifdef ADBC_ENABLED
    RegisterOGRADBC();
#endif

    // NOTE: you need to generally insert your own driver before that line.

    // NOTE: frmts/drivers.ini in the same directory should be kept in same
    // order as this file

/* Put AVCBIN at end since they need poOpenInfo->GetSiblingFiles() */
#ifdef AVC_ENABLED
    RegisterOGRAVCBin();
    RegisterOGRAVCE00();
#endif

    // Last but not the least
#ifdef AIVECTOR_ENABLED
    RegisterOGRAIVector();
#endif

} /* OGRRegisterAll */
