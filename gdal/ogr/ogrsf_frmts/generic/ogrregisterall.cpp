/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Function to register all known OGR drivers.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
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
    OGRSFDriverRegistrar::GetRegistrar()->AutoLoadDrivers();

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
#ifdef TIGER_ENABLED
    RegisterOGRTiger();
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
    RegisterOGRSQLite();
#endif
#ifdef DODS_ENABLED
    RegisterOGRDODS();
#endif
#ifdef ODBC_ENABLED
    RegisterOGRODBC();
#endif    
#ifdef PGEO_ENABLED
    RegisterOGRPGeo();
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
#ifdef PCIDSK_ENABLED
    RegisterOGRPCIDSK();
#endif
#ifdef SDE_ENABLED
    RegisterOGRSDE();
#endif
#ifdef XPLANE_ENABLED
    RegisterOGRXPlane();
#endif
#ifdef AVCBIN_ENABLED
    RegisterOGRAVCBin();
    RegisterOGRAVCE00();
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
#ifdef GPSBABEL_ENABLED
    RegisterOGRGPSBabel();
#endif
} /* OGRRegisterAll */

