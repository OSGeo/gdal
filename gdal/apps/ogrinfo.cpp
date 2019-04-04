/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Simple client for viewing OGR driver data.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_port.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>

#include "commonutils.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal_version.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_version.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"


CPL_CVSID("$Id$")

bool bVerbose = true;
bool bSuperQuiet = false;
bool bSummaryOnly = false;
GIntBig nFetchFID = OGRNullFID;
char** papszOptions = nullptr;

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage( const char* pszErrorMsg = nullptr )
{
    printf("Usage: ogrinfo [--help-general] [-ro] [-q] [-where restricted_where|@filename]\n"
           "               [-spat xmin ymin xmax ymax] [-geomfield field] [-fid fid]\n"
           "               [-sql statement|@filename] [-dialect sql_dialect] [-al] [-rl] [-so] [-fields={YES/NO}]\n"
           "               [-geom={YES/NO/SUMMARY}] [[-oo NAME=VALUE] ...]\n"
           "               [-nomd] [-listmdd] [-mdd domain|`all`]*\n"
           "               [-nocount] [-noextent] [-wkt_format WKT1|WKT2|...]\n"
           "               datasource_name [layer [layer ...]]\n");

    if( pszErrorMsg != nullptr )
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    exit(1);
}

/************************************************************************/
/*                       GDALInfoPrintMetadata()                        */
/************************************************************************/
static void GDALInfoPrintMetadata( GDALMajorObjectH hObject,
                                   const char *pszDomain,
                                   const char *pszDisplayedname,
                                   const char *pszIndent )
{
    bool bIsxml = false;

    if( pszDomain != nullptr && STARTS_WITH_CI(pszDomain, "xml:") )
        bIsxml = true;

    char **papszMetadata = GDALGetMetadata(hObject, pszDomain);
    if( CSLCount(papszMetadata) > 0 )
    {
        printf("%s%s:\n", pszIndent, pszDisplayedname);
        for( int i = 0; papszMetadata[i] != nullptr; i++ )
        {
            if( bIsxml )
                printf("%s%s\n", pszIndent, papszMetadata[i]);
            else
                printf("%s  %s\n", pszIndent, papszMetadata[i]);
        }
    }
}

/************************************************************************/
/*                       GDALInfoReportMetadata()                       */
/************************************************************************/
static void GDALInfoReportMetadata( GDALMajorObjectH hObject,
                                    bool bListMDD,
                                    bool bShowMetadata,
                                    char **papszExtraMDDomains )
{
    const char* pszIndent = "";

    /* -------------------------------------------------------------------- */
    /*      Report list of Metadata domains                                 */
    /* -------------------------------------------------------------------- */
    if( bListMDD )
    {
        char** papszMDDList = GDALGetMetadataDomainList(hObject);
        char** papszIter = papszMDDList;

        if( papszMDDList != nullptr )
            printf("%sMetadata domains:\n", pszIndent);
        while( papszIter != nullptr && *papszIter != nullptr )
        {
            if( EQUAL(*papszIter, "") )
                printf("%s  (default)\n", pszIndent);
            else
                printf("%s  %s\n", pszIndent, *papszIter);
            papszIter ++;
        }
        CSLDestroy(papszMDDList);
    }

    if( !bShowMetadata )
        return;

    /* -------------------------------------------------------------------- */
    /*      Report default Metadata domain.                                 */
    /* -------------------------------------------------------------------- */
    GDALInfoPrintMetadata(hObject, nullptr, "Metadata", pszIndent);

    /* -------------------------------------------------------------------- */
    /*      Report extra Metadata domains                                   */
    /* -------------------------------------------------------------------- */
    if( papszExtraMDDomains != nullptr )
    {
        char **papszExtraMDDomainsExpanded = nullptr;

        if( EQUAL(papszExtraMDDomains[0], "all") &&
            papszExtraMDDomains[1] == nullptr )
        {
            char** papszMDDList = GDALGetMetadataDomainList(hObject);
            char** papszIter = papszMDDList;

            while( papszIter != nullptr && *papszIter != nullptr )
            {
                if( !EQUAL(*papszIter, "") )
                {
                    papszExtraMDDomainsExpanded = CSLAddString(papszExtraMDDomainsExpanded, *papszIter);
                }
                papszIter ++;
            }
            CSLDestroy(papszMDDList);
        }
        else
        {
            papszExtraMDDomainsExpanded = CSLDuplicate(papszExtraMDDomains);
        }

        for( int iMDD = 0;
             papszExtraMDDomainsExpanded != nullptr &&
               papszExtraMDDomainsExpanded[iMDD] != nullptr;
             iMDD++ )
        {
            char pszDisplayedname[256];
            snprintf(pszDisplayedname, 256, "Metadata (%s)",
                     papszExtraMDDomainsExpanded[iMDD]);
            GDALInfoPrintMetadata(hObject, papszExtraMDDomainsExpanded[iMDD],
                                  pszDisplayedname, pszIndent);
        }

        CSLDestroy(papszExtraMDDomainsExpanded);
    }
}

/************************************************************************/
/*                           ReportOnLayer()                            */
/************************************************************************/

static void ReportOnLayer( OGRLayer * poLayer, const char *pszWHERE,
                           const char* pszGeomField,
                           OGRGeometry *poSpatialFilter,
                           bool bListMDD,
                           bool bShowMetadata,
                           char** papszExtraMDDomains,
                           bool bFeatureCount,
                           bool bExtent,
                           const char* pszWKTFormat )
{
    OGRFeatureDefn      *poDefn = poLayer->GetLayerDefn();

/* -------------------------------------------------------------------- */
/*      Set filters if provided.                                        */
/* -------------------------------------------------------------------- */
    if( pszWHERE != nullptr )
    {
        if( poLayer->SetAttributeFilter(pszWHERE) != OGRERR_NONE )
        {
            printf("FAILURE: SetAttributeFilter(%s) failed.\n", pszWHERE);
            exit(1);
        }
    }

    if( poSpatialFilter != nullptr )
    {
        if( pszGeomField != nullptr )
        {
            const int iGeomField = poDefn->GetGeomFieldIndex(pszGeomField);
            if( iGeomField >= 0 )
                poLayer->SetSpatialFilter(iGeomField, poSpatialFilter);
            else
                printf("WARNING: Cannot find geometry field %s.\n",
                       pszGeomField);
        }
        else
        {
            poLayer->SetSpatialFilter(poSpatialFilter);
        }
    }

/* -------------------------------------------------------------------- */
/*      Report various overall information.                             */
/* -------------------------------------------------------------------- */
    if( !bSuperQuiet )
    {
        printf("\n");
        printf("Layer name: %s\n", poLayer->GetName());
    }

    GDALInfoReportMetadata(static_cast<GDALMajorObjectH>(poLayer),
                           bListMDD,
                           bShowMetadata,
                           papszExtraMDDomains);

    if( bVerbose )
    {
        const int nGeomFieldCount =
            poLayer->GetLayerDefn()->GetGeomFieldCount();
        if( nGeomFieldCount > 1 )
        {
            for(int iGeom = 0;iGeom < nGeomFieldCount; iGeom ++ )
            {
                OGRGeomFieldDefn* poGFldDefn =
                    poLayer->GetLayerDefn()->GetGeomFieldDefn(iGeom);
                printf("Geometry (%s): %s\n", poGFldDefn->GetNameRef(),
                       OGRGeometryTypeToName(poGFldDefn->GetType()));
            }
        }
        else
        {
            printf("Geometry: %s\n",
                   OGRGeometryTypeToName(poLayer->GetGeomType()));
        }

        if( bFeatureCount )
            printf("Feature Count: " CPL_FRMT_GIB "\n",
                   poLayer->GetFeatureCount());

        OGREnvelope oExt;
        if( bExtent && nGeomFieldCount > 1 )
        {
            for( int iGeom = 0;iGeom < nGeomFieldCount; iGeom ++ )
            {
                if( poLayer->GetExtent(iGeom, &oExt, TRUE) == OGRERR_NONE )
                {
                    OGRGeomFieldDefn* poGFldDefn =
                        poLayer->GetLayerDefn()->GetGeomFieldDefn(iGeom);
                    CPLprintf("Extent (%s): (%f, %f) - (%f, %f)\n",
                              poGFldDefn->GetNameRef(),
                              oExt.MinX, oExt.MinY, oExt.MaxX, oExt.MaxY);
                }
            }
        }
        else if( bExtent && poLayer->GetExtent(&oExt, TRUE) == OGRERR_NONE )
        {
            CPLprintf("Extent: (%f, %f) - (%f, %f)\n",
                      oExt.MinX, oExt.MinY, oExt.MaxX, oExt.MaxY);
        }

        const auto displayDataAxisMapping = [](const OGRSpatialReference* poSRS) {
            const auto mapping = poSRS->GetDataAxisToSRSAxisMapping();
            printf("Data axis to CRS axis mapping: ");
            for( size_t i = 0; i < mapping.size(); i++ )
            {
                if( i > 0 )
                {
                    printf(",");
                }
                printf("%d", mapping[i]);
            }
            printf("\n");
        };

        if( nGeomFieldCount > 1 )
        {
            for(int iGeom = 0;iGeom < nGeomFieldCount; iGeom ++ )
            {
                OGRGeomFieldDefn* poGFldDefn =
                    poLayer->GetLayerDefn()->GetGeomFieldDefn(iGeom);
                OGRSpatialReference* poSRS = poGFldDefn->GetSpatialRef();
                char *pszWKT = nullptr;
                if( poSRS == nullptr )
                {
                    pszWKT = CPLStrdup("(unknown)");
                }
                else
                {
                    CPLString osWKTFormat("FORMAT=");
                    osWKTFormat += pszWKTFormat;
                    const char* const apszWKTOptions[] =
                        { osWKTFormat.c_str(), "MULTILINE=YES", nullptr };
                    poSRS->exportToWkt(&pszWKT, apszWKTOptions);
                }

                printf("SRS WKT (%s):\n%s\n",
                       poGFldDefn->GetNameRef(), pszWKT);
                CPLFree(pszWKT);
                if( poSRS )
                {
                    displayDataAxisMapping(poSRS);
                }
            }
        }
        else
        {
            char *pszWKT = nullptr;
            auto poSRS = poLayer->GetSpatialRef();
            if( poSRS == nullptr )
            {
                pszWKT = CPLStrdup("(unknown)");
            }
            else
            {
                poSRS->exportToPrettyWkt(&pszWKT);
            }

            printf("Layer SRS WKT:\n%s\n", pszWKT);
            CPLFree(pszWKT);
            if( poSRS )
            {
                displayDataAxisMapping(poSRS);
            }
        }

        if( strlen(poLayer->GetFIDColumn()) > 0 )
            printf("FID Column = %s\n",
                   poLayer->GetFIDColumn());

        for(int iGeom = 0;iGeom < nGeomFieldCount; iGeom ++ )
        {
            OGRGeomFieldDefn* poGFldDefn =
                poLayer->GetLayerDefn()->GetGeomFieldDefn(iGeom);
            if( nGeomFieldCount == 1 &&
                EQUAL(poGFldDefn->GetNameRef(), "") &&
                poGFldDefn->IsNullable() )
                break;
            printf("Geometry Column ");
            if( nGeomFieldCount > 1 )
                printf("%d ", iGeom + 1);
            if( !poGFldDefn->IsNullable() )
                printf("NOT NULL ");
            printf("= %s\n", poGFldDefn->GetNameRef());
        }

        for( int iAttr = 0; iAttr < poDefn->GetFieldCount(); iAttr++ )
        {
            OGRFieldDefn *poField = poDefn->GetFieldDefn(iAttr);
            const char* pszType = (poField->GetSubType() != OFSTNone)
                ? CPLSPrintf(
                      "%s(%s)",
                      poField->GetFieldTypeName(poField->GetType()),
                      poField->GetFieldSubTypeName(poField->GetSubType()))
                : poField->GetFieldTypeName(poField->GetType());
            printf("%s: %s (%d.%d)",
                   poField->GetNameRef(),
                   pszType,
                   poField->GetWidth(),
                   poField->GetPrecision());
            if( !poField->IsNullable() )
                printf(" NOT NULL");
            if( poField->GetDefault() != nullptr )
                printf(" DEFAULT %s", poField->GetDefault());
            printf("\n");
        }
    }

/* -------------------------------------------------------------------- */
/*      Read, and dump features.                                        */
/* -------------------------------------------------------------------- */

    if( nFetchFID == OGRNullFID && !bSummaryOnly )
    {
        OGRFeature *poFeature = nullptr;
        while( (poFeature = poLayer->GetNextFeature()) != nullptr )
        {
            if( !bSuperQuiet )
                poFeature->DumpReadable(nullptr, papszOptions);
            OGRFeature::DestroyFeature(poFeature);
        }
    }
    else if( nFetchFID != OGRNullFID )
    {
        OGRFeature *poFeature = poLayer->GetFeature(nFetchFID);
        if( poFeature == nullptr )
        {
            printf("Unable to locate feature id " CPL_FRMT_GIB
                   " on this layer.\n",
                   nFetchFID);
        }
        else
        {
            poFeature->DumpReadable(nullptr, papszOptions);
            OGRFeature::DestroyFeature(poFeature);
        }
    }
}

/************************************************************************/
/*                             RemoveBOM()                              */
/************************************************************************/

/* Remove potential UTF-8 BOM from data (must be NUL terminated) */
static void RemoveBOM(GByte* pabyData)
{
    if( pabyData[0] == 0xEF && pabyData[1] == 0xBB && pabyData[2] == 0xBF )
    {
        memmove(pabyData, pabyData + 3,
                strlen(reinterpret_cast<char *>(pabyData) + 3) + 1);
    }
}

static void RemoveSQLComments(char*& pszSQL)
{
    char** papszLines = CSLTokenizeStringComplex(pszSQL, "\r\n", FALSE, FALSE);
    CPLString osSQL;
    for( char** papszIter = papszLines; papszIter && *papszIter; ++papszIter )
    {
        if( !STARTS_WITH(*papszIter, "--") )
        {
            osSQL += *papszIter;
            osSQL += " ";
        }
    }
    CSLDestroy(papszLines);
    CPLFree(pszSQL);
    pszSQL = CPLStrdup(osSQL);
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

#define CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(nExtraArg) \
    do { if (iArg + nExtraArg >= nArgc) \
        Usage(CPLSPrintf("%s option requires %d argument(s)", \
                         papszArgv[iArg], nExtraArg)); } while( false )

MAIN_START(nArgc, papszArgv)
{
    // Check strict compilation and runtime library version as we use C++ API.
    if( !GDAL_CHECK_VERSION(papszArgv[0]) )
        exit(1);

    EarlySetConfigOptions(nArgc, papszArgv);

    OGRRegisterAll();

/* -------------------------------------------------------------------- */
/*      Processing command line arguments.                              */
/* -------------------------------------------------------------------- */
    nArgc = OGRGeneralCmdLineProcessor(nArgc, &papszArgv, 0);

    if( nArgc < 1 )
        exit(-nArgc);

    char *pszWHERE = nullptr;
    const char *pszDataSource = nullptr;
    char **papszLayers = nullptr;
    OGRGeometry *poSpatialFilter = nullptr;
    int nRepeatCount = 1;
    bool bAllLayers = false;
    char *pszSQLStatement = nullptr;
    const char *pszDialect = nullptr;
    int nRet = 0;
    const char* pszGeomField = nullptr;
    char **papszOpenOptions = nullptr;
    char **papszExtraMDDomains = nullptr;
    bool bListMDD = false;
    bool bShowMetadata = true;
    bool bFeatureCount = true;
    bool bExtent = true;
    bool bDatasetGetNextFeature = false;
    bool bReadOnly = false;
    bool bUpdate = false;
    const char* pszWKTFormat = "WKT2";

    for( int iArg = 1; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and "
                   "is running against GDAL %s\n",
                   papszArgv[0], GDAL_RELEASE_NAME,
                   GDALVersionInfo("RELEASE_NAME"));
            CSLDestroy(papszArgv);
            return 0;
        }
        else if( EQUAL(papszArgv[iArg], "--help") )
        {
            Usage();
        }
        else if( EQUAL(papszArgv[iArg], "-ro") )
        {
            bReadOnly = true;
        }
        else if( EQUAL(papszArgv[iArg], "-update") )
        {
            bUpdate = true;
        }
        else if( EQUAL(papszArgv[iArg], "-q") ||
                 EQUAL(papszArgv[iArg], "-quiet"))
        {
            bVerbose = false;
        }
        else if( EQUAL(papszArgv[iArg], "-qq") )
        {
            /* Undocumented: mainly only useful for AFL testing */
            bVerbose = false;
            bSuperQuiet = true;
        }
        else if( EQUAL(papszArgv[iArg], "-fid") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            nFetchFID = CPLAtoGIntBig(papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg], "-spat") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(4);

            OGRLinearRing oRing;
            oRing.addPoint(CPLAtof(papszArgv[iArg+1]),
                           CPLAtof(papszArgv[iArg+2]));
            oRing.addPoint(CPLAtof(papszArgv[iArg+1]),
                           CPLAtof(papszArgv[iArg+4]));
            oRing.addPoint(CPLAtof(papszArgv[iArg+3]),
                           CPLAtof(papszArgv[iArg+4]));
            oRing.addPoint(CPLAtof(papszArgv[iArg+3]),
                           CPLAtof(papszArgv[iArg+2]));
            oRing.addPoint(CPLAtof(papszArgv[iArg+1]),
                           CPLAtof(papszArgv[iArg+2]));

            poSpatialFilter = new OGRPolygon();
            static_cast<OGRPolygon *>(poSpatialFilter)->addRing(&oRing);
            iArg += 4;
        }
        else if( EQUAL(papszArgv[iArg], "-geomfield") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszGeomField = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg], "-where") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            iArg++;
            CPLFree(pszWHERE);
            GByte* pabyRet = nullptr;
            if( papszArgv[iArg][0] == '@' &&
                VSIIngestFile(nullptr, papszArgv[iArg] + 1, &pabyRet,
                              nullptr, 1024*1024) )
            {
                RemoveBOM(pabyRet);
                pszWHERE = reinterpret_cast<char *>(pabyRet);
            }
            else
            {
                pszWHERE = CPLStrdup(papszArgv[iArg]);
            }
        }
        else if( EQUAL(papszArgv[iArg], "-sql") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            iArg++;
            CPLFree(pszSQLStatement);
            GByte* pabyRet = nullptr;
            if( papszArgv[iArg][0] == '@' &&
                VSIIngestFile(nullptr, papszArgv[iArg] + 1, &pabyRet,
                              nullptr, 1024*1024) )
            {
                RemoveBOM(pabyRet);
                pszSQLStatement = reinterpret_cast<char *>(pabyRet);
                RemoveSQLComments(pszSQLStatement);
            }
            else
            {
                pszSQLStatement = CPLStrdup(papszArgv[iArg]);
            }
        }
        else if( EQUAL(papszArgv[iArg], "-dialect") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszDialect = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg], "-rc") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            nRepeatCount = atoi(papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg], "-al") )
        {
            bAllLayers = true;
        }
        else if( EQUAL(papszArgv[iArg], "-so") ||
                 EQUAL(papszArgv[iArg], "-summary")  )
        {
            bSummaryOnly = true;
        }
        else if( STARTS_WITH_CI(papszArgv[iArg], "-fields=") )
        {
            char* pszTemp =
                static_cast<char *>(CPLMalloc(32 + strlen(papszArgv[iArg])));
            snprintf(pszTemp,
                    32 + strlen(papszArgv[iArg]),
                    "DISPLAY_FIELDS=%s", papszArgv[iArg] + strlen("-fields="));
            papszOptions = CSLAddString(papszOptions, pszTemp);
            CPLFree(pszTemp);
        }
        else if( STARTS_WITH_CI(papszArgv[iArg], "-geom=") )
        {
            char* pszTemp =
                static_cast<char *>(CPLMalloc(32 + strlen(papszArgv[iArg])));
            snprintf(pszTemp,
                    32 + strlen(papszArgv[iArg]),
                    "DISPLAY_GEOMETRY=%s", papszArgv[iArg] + strlen("-geom="));
            papszOptions = CSLAddString(papszOptions, pszTemp);
            CPLFree(pszTemp);
        }
        else if( EQUAL(papszArgv[iArg], "-oo") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszOpenOptions = CSLAddString(papszOpenOptions,
                                            papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg], "-nomd") )
        {
            bShowMetadata = false;
        }
        else if( EQUAL(papszArgv[iArg], "-listmdd") )
        {
            bListMDD = true;
        }
        else if( EQUAL(papszArgv[iArg], "-mdd") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszExtraMDDomains = CSLAddString(papszExtraMDDomains,
                                               papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg], "-nocount") )
        {
            bFeatureCount = false;
        }
        else if( EQUAL(papszArgv[iArg], "-noextent") )
        {
            bExtent = false;
        }
        else if( EQUAL(papszArgv[iArg], "-rl"))
        {
            bDatasetGetNextFeature = true;
        }
        else if( EQUAL(papszArgv[iArg], "-wkt_format") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszWKTFormat = papszArgv[++iArg];
        }

        else if( papszArgv[iArg][0] == '-' )
        {
            Usage(CPLSPrintf("Unknown option name '%s'", papszArgv[iArg]));
        }
        else if( pszDataSource == nullptr )
        {
            pszDataSource = papszArgv[iArg];
        }
        else
        {
            papszLayers = CSLAddString(papszLayers, papszArgv[iArg]);
            bAllLayers = false;
        }
    }

    if( pszDataSource == nullptr )
        Usage("No datasource specified.");

    if( pszDialect != nullptr && pszWHERE != nullptr &&
        pszSQLStatement == nullptr )
        printf("Warning: -dialect is ignored with -where. Use -sql instead");

    if( bDatasetGetNextFeature && pszSQLStatement )
    {
        Usage("-rl is incompatible with -sql");
    }

#ifdef __AFL_HAVE_MANUAL_CONTROL
    while (__AFL_LOOP(1000)) {
#endif
/* -------------------------------------------------------------------- */
/*      Open data source.                                               */
/* -------------------------------------------------------------------- */
    GDALDataset *poDS = static_cast<GDALDataset *>(GDALOpenEx(
        pszDataSource,
        ((bReadOnly || pszSQLStatement == nullptr) &&
         !bUpdate ? GDAL_OF_READONLY : GDAL_OF_UPDATE) | GDAL_OF_VECTOR,
        nullptr, papszOpenOptions, nullptr));
    if( poDS == nullptr && !bReadOnly && !bUpdate &&
        pszSQLStatement == nullptr )
    {
        // In some cases (empty geopackage for example), opening in read-only
        // mode fails, so retry in update mode
        if( GDALIdentifyDriverEx(pszDataSource, GDAL_OF_VECTOR,
                                 nullptr, nullptr) )
        {
            poDS = static_cast<GDALDataset *>(GDALOpenEx(
                pszDataSource,
                GDAL_OF_UPDATE | GDAL_OF_VECTOR, nullptr,
                papszOpenOptions, nullptr));
        }
    }
    if( poDS == nullptr && !bReadOnly && !bUpdate &&
        pszSQLStatement != nullptr )
    {
        poDS = static_cast<GDALDataset *>(GDALOpenEx(
            pszDataSource,
            GDAL_OF_READONLY | GDAL_OF_VECTOR, nullptr,
            papszOpenOptions, nullptr));
        if( poDS != nullptr && bVerbose )
        {
            printf("Had to open data source read-only.\n");
#ifdef __AFL_HAVE_MANUAL_CONTROL
            bReadOnly = true;
#endif
        }
    }

    GDALDriver *poDriver = nullptr;
    if( poDS != nullptr )
        poDriver = poDS->GetDriver();

/* -------------------------------------------------------------------- */
/*      Report failure                                                  */
/* -------------------------------------------------------------------- */
    if( poDS == nullptr )
    {
        printf("FAILURE:\n"
               "Unable to open datasource `%s' with the following drivers.\n",
               pszDataSource);
#ifdef __AFL_HAVE_MANUAL_CONTROL
        continue;
#else
        OGRSFDriverRegistrar *poR = OGRSFDriverRegistrar::GetRegistrar();
        for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
        {
            printf("  -> %s\n", poR->GetDriver(iDriver)->GetDescription());
        }

        nRet = 1;
        goto end;
#endif
    }

    CPLAssert(poDriver != nullptr);

/* -------------------------------------------------------------------- */
/*      Some information messages.                                      */
/* -------------------------------------------------------------------- */
    if( bVerbose )
        printf("INFO: Open of `%s'\n"
               "      using driver `%s' successful.\n",
               pszDataSource, poDriver->GetDescription());

    if( bVerbose && !EQUAL(pszDataSource,poDS->GetDescription()) )
    {
        printf("INFO: Internal data source name `%s'\n"
               "      different from user name `%s'.\n",
               poDS->GetDescription(), pszDataSource);
    }

    GDALInfoReportMetadata(static_cast<GDALMajorObjectH>(poDS),
                           bListMDD,
                           bShowMetadata,
                           papszExtraMDDomains);

    if( bDatasetGetNextFeature )
    {
        nRepeatCount = 0;  // skip layer reporting.

/* -------------------------------------------------------------------- */
/*      Set filters if provided.                                        */
/* -------------------------------------------------------------------- */
        if( pszWHERE != nullptr || poSpatialFilter != nullptr )
        {
            for( int iLayer = 0; iLayer < poDS->GetLayerCount(); iLayer++ )
            {
                OGRLayer *poLayer = poDS->GetLayer(iLayer);

                if( poLayer == nullptr )
                {
                    printf("FAILURE: Couldn't fetch advertised layer %d!\n",
                           iLayer);
                    exit(1);
                }

                if( pszWHERE != nullptr )
                {
                    if( poLayer->SetAttributeFilter(pszWHERE) != OGRERR_NONE )
                    {
                        printf("WARNING: SetAttributeFilter(%s) "
                               "failed on layer %s.\n",
                               pszWHERE, poLayer->GetName());
                    }
                }

                if( poSpatialFilter != nullptr )
                {
                    if( pszGeomField != nullptr )
                    {
                        OGRFeatureDefn *poDefn = poLayer->GetLayerDefn();
                        const int iGeomField =
                            poDefn->GetGeomFieldIndex(pszGeomField);
                        if( iGeomField >= 0 )
                            poLayer->SetSpatialFilter(iGeomField,
                                                      poSpatialFilter);
                        else
                            printf("WARNING: Cannot find geometry field %s.\n",
                                   pszGeomField);
                    }
                    else
                    {
                        poLayer->SetSpatialFilter(poSpatialFilter);
                    }
                }
            }
        }

        std::set<OGRLayer*> oSetLayers;
        while( true )
        {
            OGRLayer* poLayer = nullptr;
            OGRFeature* poFeature = poDS->GetNextFeature(&poLayer, nullptr,
                                                         nullptr, nullptr);
            if( poFeature == nullptr )
                break;
            if( papszLayers == nullptr || poLayer == nullptr ||
                CSLFindString(papszLayers, poLayer->GetName()) >= 0 )
            {
                if( bVerbose && poLayer != nullptr &&
                    oSetLayers.find(poLayer) == oSetLayers.end() )
                {
                    oSetLayers.insert(poLayer);
                    const bool bSummaryOnlyBackup = bSummaryOnly;
                    bSummaryOnly = true;
                    ReportOnLayer(poLayer, nullptr, nullptr, nullptr,
                                  bListMDD, bShowMetadata,
                                  papszExtraMDDomains,
                                  bFeatureCount,
                                  bExtent,
                                  pszWKTFormat);
                    bSummaryOnly = bSummaryOnlyBackup;
                }
                if( !bSuperQuiet && !bSummaryOnly )
                    poFeature->DumpReadable(nullptr, papszOptions);
            }
            OGRFeature::DestroyFeature(poFeature);
        }
    }

/* -------------------------------------------------------------------- */
/*      Special case for -sql clause.  No source layers required.       */
/* -------------------------------------------------------------------- */
    else if( pszSQLStatement != nullptr )
    {
        nRepeatCount = 0;  // skip layer reporting.

        if( CSLCount(papszLayers) > 0 )
            printf("layer names ignored in combination with -sql.\n");

        OGRLayer *poResultSet =
            poDS->ExecuteSQL(
                pszSQLStatement,
                pszGeomField == nullptr ? poSpatialFilter : nullptr,
                pszDialect);

        if( poResultSet != nullptr )
        {
            if( pszWHERE != nullptr )
            {
                if( poResultSet->SetAttributeFilter(pszWHERE) != OGRERR_NONE )
                {
                    printf("FAILURE: SetAttributeFilter(%s) failed.\n",
                           pszWHERE);
                    exit(1);
                }
            }

            if( pszGeomField != nullptr )
                ReportOnLayer(poResultSet, nullptr,
                              pszGeomField, poSpatialFilter,
                              bListMDD, bShowMetadata, papszExtraMDDomains,
                              bFeatureCount, bExtent, pszWKTFormat);
            else
                ReportOnLayer(poResultSet, nullptr, nullptr, nullptr,
                              bListMDD, bShowMetadata, papszExtraMDDomains,
                              bFeatureCount, bExtent, pszWKTFormat);
            poDS->ReleaseResultSet(poResultSet);
        }
    }

    // coverity[tainted_data]
    for( int iRepeat = 0; iRepeat < nRepeatCount; iRepeat++ )
    {
        if( papszLayers == nullptr || *papszLayers == nullptr )
        {
            if( iRepeat == 0 )
                CPLDebug("OGR", "GetLayerCount() = %d\n",
                         poDS->GetLayerCount());

/* -------------------------------------------------------------------- */
/*      Process each data source layer.                                 */
/* -------------------------------------------------------------------- */
            for( int iLayer = 0; iLayer < poDS->GetLayerCount(); iLayer++ )
            {
                OGRLayer *poLayer = poDS->GetLayer(iLayer);

                if( poLayer == nullptr )
                {
                    printf("FAILURE: Couldn't fetch advertised layer %d!\n",
                           iLayer);
                    exit(1);
                }

                if( !bAllLayers )
                {
                    printf("%d: %s", iLayer + 1, poLayer->GetName());

                    const char* pszTitle = poLayer->GetMetadataItem("TITLE");
                    if( pszTitle )
                    {
                        printf(" (title: %s)", pszTitle);
                    }

                    const int nGeomFieldCount =
                        poLayer->GetLayerDefn()->GetGeomFieldCount();
                    if( nGeomFieldCount > 1 )
                    {
                        printf(" (");
                        for( int iGeom = 0; iGeom < nGeomFieldCount; iGeom++ )
                        {
                            if( iGeom > 0 )
                                printf(", ");
                            OGRGeomFieldDefn* poGFldDefn =
                                poLayer->GetLayerDefn()->
                                    GetGeomFieldDefn(iGeom);
                            printf(
                                "%s",
                                OGRGeometryTypeToName(
                                    poGFldDefn->GetType()));
                        }
                        printf(")");
                    }
                    else if( poLayer->GetGeomType() != wkbUnknown )
                        printf(" (%s)",
                               OGRGeometryTypeToName(
                                   poLayer->GetGeomType()));

                    printf("\n");
                }
                else
                {
                    if( iRepeat != 0 )
                        poLayer->ResetReading();

                    ReportOnLayer(poLayer, pszWHERE,
                                  pszGeomField, poSpatialFilter,
                                  bListMDD, bShowMetadata, papszExtraMDDomains,
                                  bFeatureCount, bExtent, pszWKTFormat);
                }
            }
        }
        else
        {
/* -------------------------------------------------------------------- */
/*      Process specified data source layers.                           */
/* -------------------------------------------------------------------- */

            for( char** papszIter = papszLayers;
                 *papszIter != nullptr;
                 ++papszIter )
            {
                OGRLayer *poLayer = poDS->GetLayerByName(*papszIter);

                if( poLayer == nullptr )
                {
                    printf("FAILURE: Couldn't fetch requested layer %s!\n",
                           *papszIter);
                    exit(1);
                }

                if( iRepeat != 0 )
                    poLayer->ResetReading();

                ReportOnLayer(poLayer, pszWHERE, pszGeomField, poSpatialFilter,
                              bListMDD, bShowMetadata, papszExtraMDDomains,
                              bFeatureCount, bExtent, pszWKTFormat);
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Close down.                                                     */
/* -------------------------------------------------------------------- */
    GDALClose(poDS);

#ifdef __AFL_HAVE_MANUAL_CONTROL
    }
#else
end:
#endif

    CSLDestroy(papszArgv);
    CSLDestroy(papszLayers);
    CSLDestroy(papszOptions);
    CSLDestroy(papszOpenOptions);
    CSLDestroy(papszExtraMDDomains);
    if( poSpatialFilter )
        OGRGeometryFactory::destroyGeometry(poSpatialFilter);
    CPLFree(pszSQLStatement);
    CPLFree(pszWHERE);

    OGRCleanupAll();

    return nRet;
}
MAIN_END
