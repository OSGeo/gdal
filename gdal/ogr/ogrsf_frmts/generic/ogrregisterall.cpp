/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Function to register all known OGR drivers.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRRegisterAll()                           */
/************************************************************************/

void OGRRegisterAll()
{
    GDALAllRegister();
}

void OGRRegisterAllInternal()
{

#ifdef SHAPE_ENABLED
    RegisterOGRShape();
#endif
#ifdef TAB_ENABLED
    RegisterOGRTAB();
#endif
#ifdef NTF_ENABLED
    RegisterOGRNTF();
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
#ifdef REC_ENABLED
    RegisterOGRREC();
#endif
#ifdef MEM_ENABLED
    RegisterOGRMEM();
#endif
#ifdef BNA_ENABLED
    RegisterOGRBNA();
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
#endif
#ifdef ILI_ENABLED
    RegisterOGRILI1();
    RegisterOGRILI2();
#endif
#ifdef GMT_ENABLED
    RegisterOGRGMT();
#endif
#ifdef SQLITE_ENABLED
    RegisterOGRGeoPackage();
    RegisterOGRSQLite();
#endif
#ifdef DODS_ENABLED
    RegisterOGRDODS();
#endif
#ifdef ODBC_ENABLED
    RegisterOGRODBC();
#endif
#ifdef WASP_ENABLED
    RegisterOGRWAsP();
#endif

/* Register before PGeo and Geomedia drivers */
/* that don't work well on Linux */
#ifdef MDB_ENABLED
    RegisterOGRMDB();
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
#ifdef INGRES_ENABLED
    RegisterOGRIngres();
#endif
#ifdef SDE_ENABLED
    RegisterOGRSDE();
#endif
/* Register OpenFileGDB before FGDB as it is more capable for read-only */
#ifdef OPENFILEGDB_ENABLED
    RegisterOGROpenFileGDB();
#endif
#ifdef FGDB_ENABLED
    RegisterOGRFileGDB();
#endif
#ifdef XPLANE_ENABLED
    RegisterOGRXPlane();
#endif
#ifdef DWGDIRECT_ENABLED
    RegisterOGRDXFDWG();
#endif
#ifdef DXF_ENABLED
    RegisterOGRDXF();
#endif
#ifdef GRASS_ENABLED
    RegisterOGRGRASS();
#endif
#ifdef FME_ENABLED
    RegisterOGRFME();
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
#ifdef GTM_ENABLED
    RegisterOGRGTM();
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
#ifdef SUA_ENABLED
    RegisterOGRSUA();
#endif
#ifdef OPENAIR_ENABLED
    RegisterOGROpenAir();
#endif
#ifdef PDS_ENABLED
    RegisterOGRPDS();
#endif
#ifdef WFS_ENABLED
    RegisterOGRWFS();
#endif
#ifdef SOSI_ENABLED 
    RegisterOGRSOSI(); 
#endif
#ifdef HTF_ENABLED
    RegisterOGRHTF();
#endif
#ifdef AERONAVFAA_ENABLED
    RegisterOGRAeronavFAA();
#endif
#ifdef GEOMEDIA_ENABLED
    RegisterOGRGeomedia();
#endif
#ifdef EDIGEO_ENABLED
    RegisterOGREDIGEO();
#endif
#ifdef GFT_ENABLED
    RegisterOGRGFT();
#endif
#ifdef GME_ENABLED
    RegisterOGRGME();
#endif
#ifdef SVG_ENABLED
    RegisterOGRSVG();
#endif
#ifdef COUCHDB_ENABLED
    RegisterOGRCouchDB();
#endif
#ifdef IDRISI_ENABLED
    RegisterOGRIdrisi();
#endif
#ifdef ARCGEN_ENABLED
    RegisterOGRARCGEN();
#endif
#ifdef SEGUKOOA_ENABLED
    RegisterOGRSEGUKOOA();
#endif
#ifdef SEGY_ENABLED
    RegisterOGRSEGY();
#endif
#ifdef FREEXL_ENABLED
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
#ifdef WALK_ENABLED
    RegisterOGRWalk();
#endif
#ifdef CARTODB_ENABLED
    RegisterOGRCartoDB();
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

/* Put TIGER and AVCBIN at end since they need poOpenInfo->GetSiblingFiles() */
#ifdef TIGER_ENABLED
    RegisterOGRTiger();
#endif
#ifdef AVCBIN_ENABLED
    RegisterOGRAVCBin();
    RegisterOGRAVCE00();
#endif

} /* OGRRegisterAll */
