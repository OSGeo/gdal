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
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "ogrsf_frmts.h"

CPL_CVSID("$Id$")

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
#ifdef NTF_ENABLED
    RegisterOGRNTF();
#endif
#ifdef LVBAG_ENABLED
    RegisterOGRLVBAG();
#endif
#ifdef SDTS_ENABLED
    RegisterOGRSDTS();
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
#ifdef MEM_ENABLED
    RegisterOGRMEM();
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
#ifdef OGDI_ENABLED
    RegisterOGROGDI();
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
#ifdef GEOCONCEPT_ENABLED
    RegisterOGRGeoconcept();
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
    RegisterOGROAPIF();
#endif
#ifdef SOSI_ENABLED
    RegisterOGRSOSI();
#endif
#ifdef EDIGEO_ENABLED
    RegisterOGREDIGEO();
#endif
#ifdef SVG_ENABLED
    RegisterOGRSVG();
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
#endif // NGW_ENABLED
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

    // NOTE: you need to generally insert your own driver before that line.

    // NOTE: frmts/drivers.ini in the same directory should be kept in same
    // order as this file

/* Put TIGER and AVCBIN at end since they need poOpenInfo->GetSiblingFiles() */
#ifdef TIGER_ENABLED
    RegisterOGRTiger();
#endif
#ifdef AVC_ENABLED
    RegisterOGRAVCBin();
    RegisterOGRAVCE00();
#endif


} /* OGRRegisterAll */
