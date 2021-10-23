/******************************************************************************
 * Project:  ogr linear referencing utility
 * Purpose:  main source file
 * Author:   Dmitry Baryshnikov (aka Bishop), polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (C) 2014 NextGIS
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

#include "commonutils.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "gdal_version.h"
#include "gdal.h"
#include "gdal_alg.h"
#include "ogr_api.h"
#include "ogr_geos.h"
#include "ogr_p.h"
#include "ogrsf_frmts.h"

#include <limits>
#include <map>
#include <set>
#include <vector>

CPL_CVSID("$Id$")

#if defined(HAVE_GEOS)
#if GEOS_VERSION_MAJOR > 3 || (GEOS_VERSION_MAJOR == 3 && GEOS_VERSION_MINOR >= 2)
#define HAVE_GEOS_PROJECT
#endif
#endif

#define FIELD_START "beg"
#define FIELD_FINISH "end"
#define FIELD_SCALE_FACTOR "scale"
constexpr double DELTA = 0.00000001; // - delta
#ifdef HAVE_GEOS_PROJECT
constexpr double TOLERANCE_DEGREE = 0.00008983153;
constexpr double TOLERANCE_METER = 10.0;
#endif

enum operation
{
    op_unknown = 0,
    op_create,
    op_get_pos,
    op_get_coord,
    op_get_subline
};

typedef struct _curve_data
{
    OGRLineString* pPart;
    double dfBeg, dfEnd, dfFactor;
    bool IsInside( const double& dfDist ) const {
        return (dfDist + DELTA >= dfBeg) && (dfDist - DELTA <= dfEnd);
    }
} CURVE_DATA;

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/
static void Usage( const char* pszAdditionalMsg,
                   bool bShort = true ) CPL_NO_RETURN;

static void Usage( const char* pszAdditionalMsg, bool bShort )
{
    OGRSFDriverRegistrar *poR = OGRSFDriverRegistrar::GetRegistrar();

    printf("Usage: ogrlineref [--help-general] [-progress] [-quiet]\n"
        "               [-f format_name] [[-dsco NAME=VALUE] ...] [[-lco NAME=VALUE]...]\n"
        "               [-create]\n"
        "               [-l src_line_datasource_name] [-ln layer_name] [-lf field_name]\n"
        "               [-p src_repers_datasource_name] [-pn layer_name] [-pm pos_field_name] [-pf field_name]\n"
        "               [-r src_parts_datasource_name] [-rn layer_name]\n"
        "               [-o dst_datasource_name] [-on layer_name]  [-of field_name] [-s step]\n"
        "               [-get_pos] [-x long] [-y lat]\n"
        "               [-get_coord] [-m position] \n"
        "               [-get_subline] [-mb position] [-me position]\n");

    if( bShort )
    {
        printf("\nNote: ogrlineref --long-usage for full help.\n");
        if( pszAdditionalMsg )
            fprintf(stderr, "\nFAILURE: %s\n", pszAdditionalMsg);
        exit(1);
    }

    printf(
        "\n -f format_name: output file format name, possible values are:\n");

    for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
    {
        GDALDriver *poDriver = poR->GetDriver(iDriver);

        if( CPLTestBool(CSLFetchNameValueDef(poDriver->GetMetadata(),
                                             GDAL_DCAP_CREATE, "FALSE")) )
            printf("     -f \"%s\"\n", poDriver->GetDescription());
    }

    printf(
        " -progress: Display progress on terminal. Only works if input layers have the \n"
        "                                          \"fast feature count\" capability\n"
        " -dsco NAME=VALUE: Dataset creation option (format specific)\n"
        " -lco  NAME=VALUE: Layer creation option (format specific)\n"
        " -l src_line_datasource_name: Datasource of line path name\n"
        " -ln layer_name: Layer name in datasource (optional)\n"
        " -lf field_name: Field name for unique paths in layer (optional)\n"
        " -p src_repers_datasource_name: Datasource of repers name\n"
        " -pn layer_name: Layer name in datasource (optional)\n"
        " -pm pos_field_name: Line position field name\n"
        " -pf field_name: Field name for correspondence repers of separate paths in layer (optional)\n"
        " -r src_parts_datasource_name: Parts datasource name\n"
        " -rn layer_name: Layer name in datasource (optional)\n"
        " -o dst_datasource_name: Parts datasource name\n"
        " -on layer_name: Layer name in datasource (optional)\n"
        " -of field_name: Field name for correspondence parts of separate paths in layer (optional)\n"
        " -s step: part size in m\n"
        );

    if( pszAdditionalMsg )
        fprintf(stderr, "\nFAILURE: %s\n", pszAdditionalMsg);

    exit(1);
}

static void Usage( bool bShort = true )
{
    Usage(nullptr, bShort);
}

/************************************************************************/
/*                         SetupTargetLayer()                           */
/************************************************************************/

static OGRLayer* SetupTargetLayer( OGRLayer * poSrcLayer, GDALDataset *poDstDS,
                                   char **papszLCO, const char *pszNewLayerName,
                                   const char* pszOutputSepFieldName = nullptr )
{
    const CPLString szLayerName =
        pszNewLayerName == nullptr
        ? CPLGetBasename(poDstDS->GetDescription())
        : pszNewLayerName;

    /* -------------------------------------------------------------------- */
    /*      Get other info.                                                 */
    /* -------------------------------------------------------------------- */
    OGRFeatureDefn *poSrcFDefn = poSrcLayer->GetLayerDefn();

    /* -------------------------------------------------------------------- */
    /*      Find requested geometry fields.                                 */
    /* -------------------------------------------------------------------- */

    OGRSpatialReference *poOutputSRS = poSrcLayer->GetSpatialRef();

    /* -------------------------------------------------------------------- */
    /*      Find the layer.                                                 */
    /* -------------------------------------------------------------------- */

    // GetLayerByName() can instantiate layers that would have been
    // 'hidden' otherwise, for example, non-spatial tables in a
    // PostGIS-enabled database, so this apparently useless command is
    // not useless... (#4012)
    CPLPushErrorHandler(CPLQuietErrorHandler);
    OGRLayer *poDstLayer = poDstDS->GetLayerByName(szLayerName);
    CPLPopErrorHandler();
    CPLErrorReset();

    if( poDstLayer != nullptr )
    {
        const int nLayerCount = poDstDS->GetLayerCount();
        int iLayer = -1;  // Used after for.
        for( iLayer = 0; iLayer < nLayerCount; iLayer++ )
        {
            OGRLayer *poLayer = poDstDS->GetLayer(iLayer);
            if( poLayer == poDstLayer )
                break;
        }

        if( iLayer == nLayerCount )
            // Should not happen with an ideal driver.
            poDstLayer = nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      If the layer does not exist, then create it.                    */
    /* -------------------------------------------------------------------- */
    if( poDstLayer == nullptr )
    {
        if( !poDstDS->TestCapability(ODsCCreateLayer) )
        {
            fprintf(stderr,
                    "Layer %s not found, and "
                    "CreateLayer not supported by driver.\n",
                    szLayerName.c_str());
            return nullptr;
        }

        OGRwkbGeometryType eGType = wkbLineString;

        CPLErrorReset();

        if( poDstDS->TestCapability(ODsCCreateGeomFieldAfterCreateLayer) )
        {
            eGType = wkbNone;
        }

        poDstLayer =
            poDstDS->CreateLayer(szLayerName, poOutputSRS,
                                 static_cast<OGRwkbGeometryType>(eGType),
                                 papszLCO);

        if( poDstLayer == nullptr )
            return nullptr;

        if( poDstDS->TestCapability(ODsCCreateGeomFieldAfterCreateLayer) )
        {
            OGRGeomFieldDefn oGFldDefn(poSrcFDefn->GetGeomFieldDefn(0));
            if( poOutputSRS != nullptr )
                oGFldDefn.SetSpatialRef(poOutputSRS);
            oGFldDefn.SetType(wkbLineString);
            poDstLayer->CreateGeomField(&oGFldDefn);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Otherwise we will append to it, if append was requested.        */
    /* -------------------------------------------------------------------- */
    else
    {
        fprintf(stderr, "FAILED: Layer %s already exists.\n",
                szLayerName.c_str());
        return nullptr;
    }

    // Create beg, end, scale factor fields.
    OGRFieldDefn oFieldDefn_Beg(FIELD_START, OFTReal);
    if( poDstLayer->CreateField(&oFieldDefn_Beg) != OGRERR_NONE )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Create %s field failed!",
                 oFieldDefn_Beg.GetNameRef());
        return nullptr;
    }

    OGRFieldDefn oFieldDefn_End(FIELD_FINISH, OFTReal);
    if( poDstLayer->CreateField(&oFieldDefn_End) != OGRERR_NONE )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Create %s field failed!",
                 oFieldDefn_End.GetNameRef());
        return nullptr;
    }

    OGRFieldDefn oFieldDefn_SF(FIELD_SCALE_FACTOR, OFTReal);
    if( poDstLayer->CreateField(&oFieldDefn_SF) != OGRERR_NONE )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Create %s field failed!",
                 oFieldDefn_SF.GetNameRef());
        return nullptr;
    }

    if( pszOutputSepFieldName != nullptr )
    {
        OGRFieldDefn oSepField(pszOutputSepFieldName, OFTString);
        oSepField.SetWidth(254);
        if( poDstLayer->CreateField(&oSepField) != OGRERR_NONE )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Create %s field failed!",
                     oSepField.GetNameRef());
            return nullptr;
        }
    }

    // Now that we've created a field, GetLayerDefn() won't return NULL.
    OGRFeatureDefn *poDstFDefn = poDstLayer->GetLayerDefn();

    // Sanity check: if it fails, the driver is buggy.
    if( poDstFDefn != nullptr && poDstFDefn->GetFieldCount() != 3 )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "The output driver has claimed to have added the %s field, "
                 "but it did not!",
                 oFieldDefn_Beg.GetNameRef());
    }

    return poDstLayer;
}

/* -------------------------------------------------------------------- */
/*                  CheckDestDataSourceNameConsistency()                */
/* -------------------------------------------------------------------- */

static
void CheckDestDataSourceNameConsistency(const char* pszDestFilename,
                                        const char* pszDriverName)
{
    char* pszDestExtension = CPLStrdup(CPLGetExtension(pszDestFilename));

    // TODO: Would be good to have driver metadata like for GDAL drivers.
    static const char* apszExtensions[][2] = { { "shp"    , "ESRI Shapefile" },
                                               { "dbf"    , "ESRI Shapefile" },
                                               { "sqlite" , "SQLite" },
                                               { "db"     , "SQLite" },
                                               { "mif"    , "MapInfo File" },
                                               { "tab"    , "MapInfo File" },
                                               { "s57"    , "S57" },
                                               { "bna"    , "BNA" },
                                               { "csv"    , "CSV" },
                                               { "gml"    , "GML" },
                                               { "kml"    , "KML" },
                                               { "kmz"    , "LIBKML" },
                                               { "json"   , "GeoJSON" },
                                               { "geojson", "GeoJSON" },
                                               { "dxf"    , "DXF" },
                                               { "gdb"    , "FileGDB" },
                                               { "pix"    , "PCIDSK" },
                                               { "sql"    , "PGDump" },
                                               { "gtm"    , "GPSTrackMaker" },
                                               { "gmt"    , "GMT" },
                                               { "pdf"    , "PDF" },
                                               { nullptr, nullptr }
                                              };
    static const char* apszBeginName[][2] =  { { "PG:"      , "PG" },
                                               { "MySQL:"   , "MySQL" },
                                               { "CouchDB:" , "CouchDB" },
                                               { "GFT:"     , "GFT" },
                                               { "MSSQL:"   , "MSSQLSpatial" },
                                               { "ODBC:"    , "ODBC" },
                                               { "OCI:"     , "OCI" },
                                               { "SDE:"     , "SDE" },
                                               { "WFS:"     , "WFS" },
                                               { nullptr, nullptr }
                                             };

    for( int i = 0; apszExtensions[i][0] != nullptr; i++ )
    {
        if( EQUAL(pszDestExtension, apszExtensions[i][0]) &&
            !EQUAL(pszDriverName, apszExtensions[i][1]) )
        {
            fprintf(stderr,
                    "Warning: The target file has a '%s' extension, "
                    "which is normally used by the %s driver,\n"
                    "but the requested output driver is %s. "
                    "Is it really what you want ?\n",
                    pszDestExtension,
                    apszExtensions[i][1],
                    pszDriverName);
            break;
        }
    }

    for( int i = 0; apszBeginName[i][0] != nullptr; i++ )
    {
        if( EQUALN(pszDestFilename, apszBeginName[i][0],
                   strlen(apszBeginName[i][0])) &&
            !EQUAL(pszDriverName, apszBeginName[i][1]) )
        {
            fprintf(stderr,
                    "Warning: The target file has a name which is normally "
                    "recognized by the %s driver,\n"
                    "but the requested output driver is %s. "
                    "Is it really what you want ?\n",
                    apszBeginName[i][1],
                    pszDriverName);
            break;
        }
    }

    CPLFree(pszDestExtension);
}

//------------------------------------------------------------------------
// AddFeature
//------------------------------------------------------------------------

static OGRErr AddFeature( OGRLayer* const poOutLayer, OGRLineString* pPart,
                          double dfFrom, double dfTo, double dfScaleFactor,
                          bool bQuiet,
                          const char* pszOutputSepFieldName = nullptr,
                          const char* pszOutputSepFieldValue = nullptr )
{
    OGRFeature *poFeature =
        OGRFeature::CreateFeature(poOutLayer->GetLayerDefn());

    poFeature->SetField(FIELD_START, dfFrom);
    poFeature->SetField(FIELD_FINISH, dfTo);
    poFeature->SetField(FIELD_SCALE_FACTOR, dfScaleFactor);

    if( pszOutputSepFieldName != nullptr )
    {
        poFeature->SetField(pszOutputSepFieldName, pszOutputSepFieldValue);
    }

    poFeature->SetGeometryDirectly(pPart);

    if( poOutLayer->CreateFeature(poFeature) != OGRERR_NONE )
    {
        if( !bQuiet )
            printf("Failed to create feature in shapefile.\n");
        return OGRERR_FAILURE;
    }

    OGRFeature::DestroyFeature(poFeature);

    return OGRERR_NONE;
}

//------------------------------------------------------------------------
// CreateSubline
//------------------------------------------------------------------------
static OGRErr CreateSubline( OGRLayer* const poPkLayer,
                             double dfPosBeg,
                             double dfPosEnd,
                             OGRLayer* const poOutLayer,
                             CPL_UNUSED int bDisplayProgress,
                             bool bQuiet )
{
    // Get step
    poPkLayer->ResetReading();
    OGRFeature* pFeature = poPkLayer->GetNextFeature();
    if( nullptr != pFeature )
    {
        // FIXME: Clang Static Analyzer rightly found that the following
        // code is dead
        // dfBeg = pFeature->GetFieldAsDouble(FIELD_START);
        // dfEnd = pFeature->GetFieldAsDouble(FIELD_FINISH);
        OGRFeature::DestroyFeature(pFeature);
    }
    else
    {
        fprintf(stderr, "Get step for positions %f - %f failed\n",
                dfPosBeg, dfPosEnd);
        return OGRERR_FAILURE;
    }
    // Get second part.
    double dfBeg = 0.0;
    double dfEnd = 0.0;
    pFeature = poPkLayer->GetNextFeature();
    if( nullptr != pFeature )
    {
        dfBeg = pFeature->GetFieldAsDouble(FIELD_START);
        dfEnd = pFeature->GetFieldAsDouble(FIELD_FINISH);
        OGRFeature::DestroyFeature(pFeature);
    }
    else
    {
        fprintf(stderr, "Get step for positions %f - %f failed\n",
                dfPosBeg, dfPosEnd);
        return OGRERR_FAILURE;
    }
    const double dfStep = dfEnd - dfBeg;

    // Round input to step
    const double dfPosBegLow = floor(dfPosBeg / dfStep) * dfStep;
    const double dfPosEndHigh = ceil(dfPosEnd / dfStep) * dfStep;

    CPLString szAttributeFilter;
    szAttributeFilter.Printf("%s >= %f AND %s <= %f",
                             FIELD_START, dfPosBegLow,
                             FIELD_FINISH, dfPosEndHigh);
    // TODO: ExecuteSQL should be faster.
    poPkLayer->SetAttributeFilter(szAttributeFilter);
    poPkLayer->ResetReading();

    std::map<double, OGRFeature *> moParts;

    while( (pFeature = poPkLayer->GetNextFeature()) != nullptr )
    {
        double dfStart = pFeature->GetFieldAsDouble(FIELD_START);
        moParts[dfStart] = pFeature;
    }


    if( moParts.empty() )
    {
        fprintf(stderr, "Get parts for positions %f - %f failed\n",
                dfPosBeg, dfPosEnd);
        return OGRERR_FAILURE;
    }

    OGRLineString SubLine;
    if( moParts.size() == 1 )
    {
        std::map<double, OGRFeature *>::iterator IT = moParts.begin();
        const double dfStart = IT->first;
        double dfPosBegCorr = dfPosBeg - dfStart;
        const double dfSF = IT->second->GetFieldAsDouble(FIELD_SCALE_FACTOR);
        dfPosBegCorr *= dfSF;

        const double dfPosEndCorr = (dfPosEnd - dfStart) * dfSF;

        OGRLineString *pLine = IT->second->GetGeometryRef()->toLineString();

        OGRLineString *pSubLine =
            pLine->getSubLine(dfPosBegCorr, dfPosEndCorr, FALSE);

        OGRFeature::DestroyFeature(IT->second);
        // Store.
        return AddFeature(poOutLayer, pSubLine, dfPosBeg, dfPosEnd,
                          1.0, bQuiet);
    }
    else
    {
        int nCounter = static_cast<int>(moParts.size());
        std::map<double, OGRFeature *>::iterator IT = moParts.begin();
        OGRLineString *pOutLine = new OGRLineString();
        // Get first part.
        const double dfStart = IT->first;
        double dfPosBegCorr = dfPosBeg - dfStart;
        double dfSF = IT->second->GetFieldAsDouble(FIELD_SCALE_FACTOR);
        dfPosBegCorr *= dfSF;

        OGRLineString *pLine = IT->second->GetGeometryRef()->toLineString();

        OGRLineString *pSubLine =
            pLine->getSubLine(dfPosBegCorr, pLine->get_Length(), FALSE);

        pOutLine->addSubLineString(pSubLine);
        delete pSubLine;
        OGRFeature::DestroyFeature(IT->second);

        ++IT;
        nCounter--;

        while( nCounter > 1 )
        {
            pLine = IT->second->GetGeometryRef()->toLineString();
            pOutLine->addSubLineString(pLine);
            OGRFeature::DestroyFeature(IT->second);
            ++IT;
            nCounter--;
        }

        // Get last part
        double dfPosEndCorr = dfPosEnd - IT->first;
        dfSF = IT->second->GetFieldAsDouble(FIELD_SCALE_FACTOR);
        dfPosEndCorr *= dfSF;

        pLine = IT->second->GetGeometryRef()->toLineString();

        pSubLine = pLine->getSubLine(0, dfPosEndCorr, FALSE);

        pOutLine->addSubLineString(pSubLine);
        delete pSubLine;

        OGRFeature::DestroyFeature(IT->second);
        // Store
        return AddFeature(poOutLayer, pOutLine, dfPosBeg, dfPosEnd,
                          1.0, bQuiet);
    }
}

//------------------------------------------------------------------------
// Project
//------------------------------------------------------------------------
#ifdef HAVE_GEOS_PROJECT
static double Project( OGRLineString* pLine, OGRPoint* pPoint )
{
    if( nullptr == pLine || nullptr == pPoint )
        return -1;
    OGRPoint TestPoint;
    pLine->StartPoint(&TestPoint);
    if( TestPoint.Equals(pPoint) )
        return 0.0;
    pLine->EndPoint(&TestPoint);
    if( TestPoint.Equals(pPoint) )
        return pLine->get_Length();

    return pLine->Project(pPoint);
}
#endif

//------------------------------------------------------------------------
// CreatePartsFromLineString
//------------------------------------------------------------------------
#ifdef HAVE_GEOS_PROJECT
static OGRErr CreatePartsFromLineString(
    OGRLineString* pPathGeom, OGRLayer* const poPkLayer, int nMValField,
    double dfStep, OGRLayer* const poOutLayer, int bDisplayProgress,
    bool bQuiet, const char* pszOutputSepFieldName = nullptr,
    const char* pszOutputSepFieldValue = nullptr )
{
    // Check repers/milestones/reference points type
    OGRwkbGeometryType eGeomType = poPkLayer->GetGeomType();
    if( wkbFlatten(eGeomType) != wkbPoint )
    {
        fprintf(stderr, "Unsupported geometry type %s for path\n",
                OGRGeometryTypeToName(eGeomType));
        return OGRERR_FAILURE;
    }

    double dfTolerance = 1.0;
    OGRSpatialReference* pSpaRef = pPathGeom->getSpatialReference();
    if( pSpaRef->IsGeographic() )
    {
        dfTolerance = TOLERANCE_DEGREE;
    }
    else
    {
        dfTolerance = TOLERANCE_METER;
    }

    // Create sorted list of repers.
    std::map<double, OGRPoint*> moRepers;
    poPkLayer->ResetReading();
    OGRFeature* pReperFeature = nullptr;
    double dfTestDistance = 0.0;
    while( (pReperFeature = poPkLayer->GetNextFeature()) != nullptr )
    {
        const double dfReperPos = pReperFeature->GetFieldAsDouble(nMValField);
        OGRGeometry* pGeom = pReperFeature->GetGeometryRef();
        if( nullptr != pGeom )
        {
            OGRPoint* pPt = pGeom->clone()->toPoint();
            if( !bQuiet )
            {
                if( moRepers.find(dfReperPos) != moRepers.end() )
                {
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "The distance %f is already present in repers file!",
                        dfReperPos);
                }
            }
            // Check if reper is inside the path
            dfTestDistance = Project(pPathGeom, pPt);
            if( dfTestDistance < 0 )
            {
                if( !bQuiet )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "The distance %f is out of path!", dfReperPos);
                }
                delete pPt;
            }
            else
            {
                const double dfDist = pPathGeom->Distance(pPt);
                if( dfDist < dfTolerance )
                    moRepers[dfReperPos] = pPt;
                else
                    delete pPt;
            }
        }
        OGRFeature::DestroyFeature(pReperFeature);
    }

    if( moRepers.size() < 2 )
    {
        fprintf(stderr, "Not enough repers to proceed\n");
        return OGRERR_FAILURE;
    }

    // Check direction.
    if( !bQuiet )
    {
        fprintf(stdout, "Check path direction\n");
    }

    // Get distance along path from pt1 and pt2.
    // If pt1 distance > pt2 distance, reverse path
    std::map<double, OGRPoint*>::const_iterator IT;
    IT = moRepers.begin();
    double dfPosition = IT->first;
    double dfBeginPosition = IT->first;
    OGRPoint *pt1 = IT->second;
    ++IT;
    OGRPoint *pt2 = IT->second;

    double dfDistance1 = Project(pPathGeom, pt1);
    double dfDistance2 = Project(pPathGeom, pt2);

    if( dfDistance1 > dfDistance2 )
    {
        if( !bQuiet )
        {
            fprintf(stderr,
                    "Warning: The path is opposite the repers direction. "
                    "Let's reverse path\n");
        }
        pPathGeom->reversePoints();

        dfDistance1 = Project(pPathGeom, pt1);
        dfDistance2 = Project(pPathGeom, pt2);
    }

    OGRLineString* pPart = nullptr;

    std::vector<CURVE_DATA> astSubLines;

    if( !bQuiet )
    {
        fprintf(stdout, "Create parts\n");
    }

    // Get first part
    // If first point is not at the beginning of the path
    // The first part should be from the beginning of the path
    // to the first point. length == part.getLength
    OGRPoint *pPtBeg = nullptr;
    OGRPoint *pPtEnd = nullptr;
    double dfPtBegPosition = 0.0;
    double dfPtEndPosition = 0.0;

    if( dfDistance1 > DELTA )
    {
        pPart = pPathGeom->getSubLine(0, dfDistance1, FALSE);
        if( nullptr != pPart )
        {
            double dfLen = pPart->get_Length();
            if( pSpaRef->IsGeographic() )
            {
                //convert to UTM/WGS84
                OGRPoint pt;
                pPart->Value(dfLen / 2, &pt);
                const int nZoneEnv =
                    static_cast<int>(30 + (pt.getX() + 3.0) / 6.0 + 0.5);
                int nEPSG;
                if( pt.getY() > 0 )
                {
                    nEPSG = 32600 + nZoneEnv;
                }
                else
                {
                    nEPSG = 32700 + nZoneEnv;
                }
                OGRSpatialReference SpatRef;
                SpatRef.importFromEPSG(nEPSG);
                OGRGeometry *pTransformPart = pPart->clone();
                if( pTransformPart->transformTo(&SpatRef) == OGRERR_NONE )
                {
                    OGRLineString* pTransformPartLS =
                        pTransformPart->toLineString();
                    dfLen = pTransformPartLS->get_Length();
                }

                CURVE_DATA data = { pPart, dfPosition - dfLen, dfPosition,
                                    pPart->get_Length() / dfLen };
                astSubLines.push_back(data);

                pPtBeg = new OGRPoint();
                pPart->getPoint(0, pPtBeg);
                dfPtBegPosition = dfPosition - dfLen;

                // AddFeature(poOutLayer, pPart, dfPosition - dfLen, dfPosition,
                //            pPart->get_Length() / dfLen, bQuiet);
                delete pTransformPart;
            }
            else
            {
                CURVE_DATA data = {
                    pPart, dfPosition - dfLen, dfPosition, 1.0
                };
                astSubLines.push_back(data);
                // AddFeature(poOutLayer, pPart, dfPosition - dfLen, dfPosition,
                //            1.0, bQuiet);
                pPtBeg = new OGRPoint();
                pPart->getPoint(0, pPtBeg);
                dfPtBegPosition = dfPosition - dfLen;
            }
        }
    }

    if( dfDistance2 - dfDistance1 > DELTA )
    {
        pPart = pPathGeom->getSubLine(dfDistance1, dfDistance2, FALSE);
        if( nullptr != pPart )
        {
            CURVE_DATA data = {
                pPart, dfPosition, IT->first,
                pPart->get_Length() / (IT->first - dfPosition)
            };
            astSubLines.push_back(data);
            // AddFeature(poOutLayer, pPart, dfPosition, IT->first,
            //            pPart->get_Length() / (IT->first - dfPosition),
            //            bQuiet);
        }
    }

    GDALProgressFunc pfnProgress = nullptr;
    void *pProgressArg = nullptr;

    double dfFactor = 1.0 / moRepers.size();
    if( bDisplayProgress )
    {
        pfnProgress = GDALScaledProgress;
        pProgressArg =
            GDALCreateScaledProgress(0.0, 1.0, GDALTermProgress, nullptr);
    }

    int nCount = 2;
    dfDistance1 = dfDistance2;
    dfPosition = IT->first;
    ++IT;  // Get third point

    double dfEndPosition = 0.0;
    while( IT != moRepers.end() )
    {
        if( bDisplayProgress )
        {
            pfnProgress(nCount * dfFactor, "", pProgressArg);
            nCount++;
        }

        dfEndPosition = IT->first;

        dfDistance2 = Project(pPathGeom, IT->second);

        if(dfDistance2 - dfDistance1 > DELTA)
        {
            pPart = pPathGeom->getSubLine(dfDistance1, dfDistance2, FALSE);
            if( nullptr != pPart )
            {
                CURVE_DATA data = {
                    pPart, dfPosition, IT->first,
                    pPart->get_Length() / (IT->first - dfPosition)
                };
                astSubLines.push_back(data);
                // AddFeature(poOutLayer, pPart, dfPosition, IT->first,
                //            pPart->get_Length() / (IT->first - dfPosition),
                //            bQuiet);
                dfDistance1 = dfDistance2;
                dfPosition = IT->first;
            }
        }

        ++IT;
    }

    // Get last part
    if( pPathGeom->get_Length() - dfDistance1 > DELTA )
    {
        pPart =
            pPathGeom->getSubLine(dfDistance1, pPathGeom->get_Length(), FALSE);
        if( nullptr != pPart )
        {
            double dfLen = pPart->get_Length();
            if( pSpaRef->IsGeographic() )
            {
                //convert to UTM/WGS84
                OGRPoint pt;
                pPart->Value(dfLen / 2, &pt);
                const int nZoneEnv =
                    static_cast<int>(30 + (pt.getX() + 3.0) / 6.0 + 0.5);
                int nEPSG;
                if( pt.getY() > 0 )
                {
                    nEPSG = 32600 + nZoneEnv;
                }
                else
                {
                    nEPSG = 32700 + nZoneEnv;
                }
                OGRSpatialReference SpatRef;
                SpatRef.importFromEPSG(nEPSG);
                OGRGeometry *pTransformPart = pPart->clone();
                if( pTransformPart->transformTo(&SpatRef) == OGRERR_NONE )
                {
                    OGRLineString* pTransformPartLS =
                        pTransformPart->toLineString();
                    dfLen = pTransformPartLS->get_Length();
                }
                CURVE_DATA data = {
                    pPart, dfPosition, dfPosition + dfLen,
                    pPart->get_Length() / dfLen
                };
                astSubLines.push_back(data);
                // AddFeature(poOutLayer, pPart, dfPosition, dfPosition + dfLen,
                //            pPart->get_Length() / dfLen, bQuiet);

                pPtEnd = new OGRPoint();
                pPart->getPoint(pPart->getNumPoints() - 1, pPtEnd);
                dfPtEndPosition = dfPosition + dfLen;

                delete pTransformPart;
            }
            else
            {
                CURVE_DATA data = {
                    pPart, dfPosition, dfPosition + dfLen, 1.0
                };
                astSubLines.push_back(data);
                // AddFeature(poOutLayer, pPart, dfPosition - dfLen, dfPosition,
                //            1.0, bQuiet);
                pPtEnd = new OGRPoint();
                pPart->getPoint(pPart->getNumPoints() - 1, pPtEnd);
                dfPtEndPosition = dfPosition + dfLen;
            }
        }
    }

    // Create pickets
    if( !bQuiet )
    {
        fprintf(stdout, "\nCreate pickets\n");
    }

    const double dfRoundBeg =
        pPtBeg != nullptr
        ? ceil(dfPtBegPosition / dfStep) * dfStep
        : ceil(dfBeginPosition / dfStep) * dfStep;

    if( pPtEnd != nullptr )
        dfEndPosition = dfPtEndPosition;

    dfFactor = dfStep / (dfEndPosition - dfRoundBeg);
    nCount = 0;
    for( std::map<double, OGRPoint*>::iterator oIter = moRepers.begin();
         oIter != moRepers.end();
         ++oIter )
    {
        delete oIter->second;
    }

    moRepers.clear();

    if( pPtBeg != nullptr )
        moRepers[dfPtBegPosition] = pPtBeg;
    if( pPtEnd != nullptr )
        moRepers[dfPtEndPosition] = pPtEnd;

    for( double dfDist = dfRoundBeg; dfDist <= dfEndPosition; dfDist += dfStep )
    {
        if( bDisplayProgress )
        {
            pfnProgress(nCount * dfFactor, "", pProgressArg);
            nCount++;
        }

        for( int j = 0; j < static_cast<int>(astSubLines.size()); j++ )
        {
            if( astSubLines[j].IsInside(dfDist) )
            {
                const double dfRealDist =
                    (dfDist - astSubLines[j].dfBeg) * astSubLines[j].dfFactor;
                OGRPoint *pReperPoint = new OGRPoint();
                astSubLines[j].pPart->Value(dfRealDist, pReperPoint);

                moRepers[dfDist] = pReperPoint;
                break;
            }
        }
    }

    for( int i = 0; i < static_cast<int>(astSubLines.size()); i++ )
    {
        delete astSubLines[i].pPart;
    }
    astSubLines.clear();

    if( !bQuiet )
    {
        fprintf(stdout, "\nCreate sublines\n");
    }

    IT = moRepers.begin();
    dfFactor = 1.0 / moRepers.size();
    nCount = 0;
    dfDistance1 = 0;
    dfPosition = IT->first;

    while( IT != moRepers.end() )
    {
        if( bDisplayProgress )
        {
            pfnProgress(nCount * dfFactor, "", pProgressArg);
            nCount++;
        }

        dfDistance2 = Project(pPathGeom, IT->second);

        if( dfDistance2 - dfDistance1 > DELTA )
        {
            pPart = pPathGeom->getSubLine(dfDistance1, dfDistance2, FALSE);
            if( nullptr != pPart )
            {
                AddFeature(poOutLayer, pPart, dfPosition, IT->first,
                           pPart->get_Length() / (IT->first - dfPosition),
                           bQuiet, pszOutputSepFieldName,
                           pszOutputSepFieldValue);
                dfDistance1 = dfDistance2;
                dfPosition = IT->first;
            }
        }

        ++IT;
    }

    for( std::map<double, OGRPoint*>::iterator oIter = moRepers.begin();
         oIter != moRepers.end();
         ++oIter)
    {
        delete oIter->second;
    }

    if( !bQuiet )
    {
        fprintf(stdout, "\nSuccess!\n\n");
    }

    if( nullptr != pProgressArg )
    {
        GDALDestroyScaledProgress(pProgressArg);
    }

    return OGRERR_NONE;
}
#endif

//------------------------------------------------------------------------
// CreateParts
//------------------------------------------------------------------------
#ifdef HAVE_GEOS_PROJECT
static OGRErr CreateParts(
    OGRLayer* const poLnLayer, OGRLayer* const poPkLayer, int nMValField,
    double dfStep, OGRLayer* const poOutLayer, int bDisplayProgress,
    bool bQuiet,
    const char* pszOutputSepFieldName = nullptr,
    const char* pszOutputSepFieldValue = nullptr )
{
    OGRErr eRetCode = OGRERR_FAILURE;

    //check path and get first line
    OGRwkbGeometryType eGeomType = poLnLayer->GetGeomType();
    if( wkbFlatten(eGeomType) != wkbLineString &&
        wkbFlatten(eGeomType) != wkbMultiLineString )
    {
        fprintf(stderr, "Unsupported geometry type %s for path\n",
                OGRGeometryTypeToName(eGeomType));
        return eRetCode;
    }

    poLnLayer->ResetReading();
    // Get first geometry
    // TODO: Attribute filter for path geometry.
    OGRFeature* pPathFeature = poLnLayer->GetNextFeature();
    if( nullptr != pPathFeature )
    {
        OGRGeometry* pGeom = pPathFeature->GetGeometryRef();

        if( pGeom != nullptr &&
            wkbFlatten(pGeom->getGeometryType()) == wkbMultiLineString )
        {
            if( !bQuiet )
            {
                fprintf(stdout, "\nThe geometry " CPL_FRMT_GIB
                        " is wkbMultiLineString type\n",
                        pPathFeature->GetFID());
            }

            OGRGeometryCollection* pGeomColl = pGeom->toGeometryCollection();
            for( int i = 0; i < pGeomColl->getNumGeometries(); ++i )
            {
                OGRLineString* pPath =
                    pGeomColl->getGeometryRef(i)->clone()->toLineString();
                pPath->assignSpatialReference(pGeomColl->getSpatialReference());
                eRetCode = CreatePartsFromLineString(
                    pPath, poPkLayer, nMValField, dfStep, poOutLayer,
                    bDisplayProgress, bQuiet, pszOutputSepFieldName,
                    pszOutputSepFieldValue);

                if( eRetCode != OGRERR_NONE )
                {
                    OGRFeature::DestroyFeature(pPathFeature);
                    return eRetCode;
                }
            }
        }
        else if( pGeom != nullptr &&
            wkbFlatten(pGeom->getGeometryType()) == wkbLineString )
        {
            OGRLineString* pGeomClone = pGeom->clone()->toLineString();
            eRetCode = CreatePartsFromLineString(
                pGeomClone, poPkLayer, nMValField, dfStep, poOutLayer,
                bDisplayProgress, bQuiet, pszOutputSepFieldName,
                pszOutputSepFieldValue);
            delete pGeomClone;
        }

        OGRFeature::DestroyFeature(pPathFeature);
    }

    // Should never reach

    return eRetCode;
}
#endif

//------------------------------------------------------------------------
// CreatePartsMultiple
//------------------------------------------------------------------------
#ifdef HAVE_GEOS_PROJECT
static OGRErr CreatePartsMultiple(
    OGRLayer* const poLnLayer, const char* pszLineSepFieldName,
    OGRLayer* const poPkLayer, const char* pszPicketsSepFieldName,
    int nMValField, double dfStep, OGRLayer* const poOutLayer,
    const char* pszOutputSepFieldName, int bDisplayProgress, bool bQuiet )
{
    // Read all separate field values into array
    OGRFeatureDefn *pDefn = poLnLayer->GetLayerDefn();
    const int nLineSepFieldInd = pDefn->GetFieldIndex(pszLineSepFieldName);
    if( nLineSepFieldInd == -1 )
    {
        fprintf(stderr, "The field %s not found\n", pszLineSepFieldName);
        return OGRERR_FAILURE;
    }

    poLnLayer->ResetReading();

    std::set<CPLString> asIDs;
    OGRFeature* pFeature = nullptr;
    while( (pFeature = poLnLayer->GetNextFeature()) != nullptr )
    {
        CPLString sID = pFeature->GetFieldAsString(nLineSepFieldInd);
        asIDs.insert(sID);

        OGRFeature::DestroyFeature(pFeature);
    }

    for( std::set<CPLString>::const_iterator it = asIDs.begin();
         it != asIDs.end(); ++it )
    {
        // Create select clause
        CPLString sLineWhere;
        sLineWhere.Printf("%s = '%s'", pszLineSepFieldName, it->c_str());
        poLnLayer->SetAttributeFilter(sLineWhere);

        CPLString sPkWhere;
        sPkWhere.Printf("%s = '%s'", pszPicketsSepFieldName, it->c_str());
        poPkLayer->SetAttributeFilter(sPkWhere);

        if( !bQuiet )
        {
            fprintf(stdout, "The %s %s\n", pszPicketsSepFieldName, it->c_str());
        }

        // Don't check success as we want to try all paths
        CreateParts(poLnLayer, poPkLayer, nMValField, dfStep, poOutLayer,
                    bDisplayProgress, bQuiet, pszOutputSepFieldName, *it);
    }

    return OGRERR_NONE;
}
#endif

//------------------------------------------------------------------------
// GetPosition
//------------------------------------------------------------------------
#ifdef HAVE_GEOS_PROJECT
static OGRErr GetPosition( OGRLayer* const poPkLayer,
                           double dfX,
                           double dfY,
                           int /* bDisplayProgress */,
                           int bQuiet)
{
    // Create point
    OGRPoint pt;
    pt.setX(dfX);
    pt.setY(dfY);
    pt.assignSpatialReference(poPkLayer->GetSpatialRef());

    poPkLayer->ResetReading();
    OGRLineString *pCloserPart = nullptr;
    double dfBeg = 0.0;
    double dfScale = 0.0;
    double dfMinDistance = std::numeric_limits<double>::max();
    OGRFeature* pFeature = nullptr;
    while( (pFeature = poPkLayer->GetNextFeature()) != nullptr )
    {
        OGRGeometry* pCurrentGeom = pFeature->GetGeometryRef();
        if( pCurrentGeom != nullptr )
        {
            double dfCurrentDistance = pCurrentGeom->Distance(&pt);
            if( dfCurrentDistance < dfMinDistance )
            {
                dfMinDistance = dfCurrentDistance;
                if( pCloserPart != nullptr )
                    delete pCloserPart;
                pCloserPart = pFeature->StealGeometry()->toLineString();
                dfBeg = pFeature->GetFieldAsDouble(FIELD_START);
                dfScale = pFeature->GetFieldAsDouble(FIELD_SCALE_FACTOR);
            }
        }
        OGRFeature::DestroyFeature(pFeature);
    }

    if( nullptr == pCloserPart )
    {
        fprintf(stderr, "Filed to find closest part\n");
        return OGRERR_FAILURE;
    }
    // Now we have closest part
    // Get real distance
    const double dfRealDist = Project(pCloserPart, &pt);
    delete pCloserPart;
    // Compute reference distance
    const double dfRefDist = dfBeg + dfRealDist / dfScale;
    if( bQuiet )
    {
        fprintf(stdout, "%s", CPLSPrintf("%f\n", dfRefDist));
    }
    else
    {
        fprintf(stdout, "%s", CPLSPrintf(
           "The position for coordinates lat:%f, long:%f is %f\n",
           dfY, dfX, dfRefDist));
    }

    return OGRERR_NONE;
}
#endif

//------------------------------------------------------------------------
// GetCoordinates
//------------------------------------------------------------------------
static OGRErr GetCoordinates( OGRLayer* const poPkLayer,
                              double dfPos,
                              /* CPL_UNUSED */ int /* bDisplayProgress */,
                              bool bQuiet )
{
    CPLString szAttributeFilter;
    szAttributeFilter.Printf(
        "%s < %f AND %s > %f", FIELD_START, dfPos, FIELD_FINISH, dfPos);
    // TODO: ExecuteSQL should be faster.
    poPkLayer->SetAttributeFilter(szAttributeFilter);
    poPkLayer->ResetReading();

    bool bHaveCoords = false;
    for( auto& pFeature: poPkLayer )
    {
        bHaveCoords = true;
        const double dfStart = pFeature->GetFieldAsDouble(FIELD_START);
        double dfPosCorr = dfPos - dfStart;
        const double dfSF = pFeature->GetFieldAsDouble(FIELD_SCALE_FACTOR);
        dfPosCorr *= dfSF;
        OGRLineString *pLine = pFeature->GetGeometryRef()->toLineString();

        OGRPoint pt;
        pLine->Value(dfPosCorr, &pt);

        if( bQuiet )
        {
            fprintf(stdout, "%s",
                    CPLSPrintf("%f,%f,%f\n", pt.getX(), pt.getY(), pt.getZ()));
        }
        else
        {
            fprintf(stdout, "%s", CPLSPrintf(
                "The position for distance %f is lat:%f, long:%f, height:%f\n",
                dfPos, pt.getY(), pt.getX(), pt.getZ()));
        }
    }

    if( bHaveCoords )
    {
        return OGRERR_NONE;
    }
    else
    {
        fprintf(stderr, "Get coordinates for position %f failed\n", dfPos);
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

#define CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(nExtraArg) \
    do { if( iArg + nExtraArg >= nArgc ) \
        Usage(CPLSPrintf("%s option requires %d argument(s)", \
                         papszArgv[iArg], nExtraArg)); } while( false )

MAIN_START(nArgc, papszArgv)

{
    OGRErr eErr = OGRERR_NONE;
    bool bQuiet = false;
    const char *pszFormat = "ESRI Shapefile";

    const char *pszOutputDataSource = nullptr;
    const char *pszLineDataSource = nullptr;
    const char *pszPicketsDataSource = nullptr;
    const char *pszPartsDataSource = nullptr;
    char *pszOutputLayerName = nullptr;
    const char *pszLineLayerName = nullptr;
#ifdef HAVE_GEOS_PROJECT
    const char *pszPicketsLayerName = nullptr;
    const char *pszPicketsMField = nullptr;
#endif
    const char *pszPartsLayerName = nullptr;

#ifdef HAVE_GEOS_PROJECT
    const char *pszLineSepFieldName = nullptr;
    const char *pszPicketsSepFieldName = nullptr;
    const char *pszOutputSepFieldName = "uniq_uid";
#endif

    char **papszDSCO = nullptr;
    char **papszLCO = nullptr;

    operation stOper = op_unknown;
#ifdef HAVE_GEOS_PROJECT
    double dfX = -100000000.0;
    double dfY = -100000000.0;
#endif
    double dfPos = -100000000.0;

    int bDisplayProgress = FALSE;

    double dfPosBeg = -100000000.0;
    double dfPosEnd = -100000000.0;
#ifdef HAVE_GEOS_PROJECT
    double dfStep = -100000000.0;
#endif

    // Check strict compilation and runtime library version as we use C++ API.
    if( ! GDAL_CHECK_VERSION(papszArgv[0]) )
        exit(1);

    EarlySetConfigOptions(nArgc, papszArgv);

    OGRRegisterAll();

/* -------------------------------------------------------------------- */
/*      Processing command line arguments.                              */
/* -------------------------------------------------------------------- */
    nArgc = OGRGeneralCmdLineProcessor( nArgc, &papszArgv, 0 );

    if( nArgc < 1 )
        exit( -nArgc );

    for( int iArg = 1; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and "
                   "is running against GDAL %s\n",
                   papszArgv[0], GDAL_RELEASE_NAME,
                   GDALVersionInfo("RELEASE_NAME"));
            CSLDestroy( papszArgv );
            return 0;
        }
        else if( EQUAL(papszArgv[iArg],"--help") )
        {
            Usage();
        }
        else if( EQUAL(papszArgv[iArg], "--long-usage") )
        {
            Usage(false);
        }

        else if( EQUAL(papszArgv[iArg], "-q") ||
                 EQUAL(papszArgv[iArg], "-quiet") )
        {
            bQuiet = true;
        }
        else if( (EQUAL(papszArgv[iArg], "-f") ||
                  EQUAL(papszArgv[iArg], "-of")) )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // bFormatExplicitlySet = TRUE;
            // coverity[tainted_data]
            pszFormat = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg], "-dsco") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            papszDSCO = CSLAddString(papszDSCO, papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg],"-lco") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            papszLCO = CSLAddString(papszLCO, papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg], "-create") )
        {
            stOper = op_create;
        }
        else if( EQUAL(papszArgv[iArg], "-get_pos") )
        {
            stOper = op_get_pos;
        }
        else if( EQUAL(papszArgv[iArg], "-get_coord") )
        {
            stOper = op_get_coord;
        }
        else if( EQUAL(papszArgv[iArg], "-get_subline") )
        {
            stOper = op_get_subline;
        }
        else if( EQUAL(papszArgv[iArg], "-l") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            pszLineDataSource = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg], "-ln") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            pszLineLayerName = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg], "-lf") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
#ifdef HAVE_GEOS_PROJECT
            // coverity[tainted_data]
            pszLineSepFieldName = papszArgv[++iArg];
#else
            fprintf(stderr,
                    "GEOS support not enabled or incompatible version.\n");
            exit(1);
#endif
        }
        else if( EQUAL(papszArgv[iArg], "-p") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            pszPicketsDataSource = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg], "-pn") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
#ifdef HAVE_GEOS_PROJECT
            // coverity[tainted_data]
            pszPicketsLayerName = papszArgv[++iArg];
#else
            fprintf(stderr,
                    "GEOS support not enabled or incompatible version.\n");
            exit(1);
#endif
        }
        else if( EQUAL(papszArgv[iArg], "-pm") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
#ifdef HAVE_GEOS_PROJECT
            // coverity[tainted_data]
            pszPicketsMField = papszArgv[++iArg];
#else
            fprintf(stderr,
                    "GEOS support not enabled or incompatible version.\n");
            exit(1);
#endif
        }
        else if( EQUAL(papszArgv[iArg], "-pf") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
#ifdef HAVE_GEOS_PROJECT
            // coverity[tainted_data]
            pszPicketsSepFieldName = papszArgv[++iArg];
#else
            fprintf(stderr,
                    "GEOS support not enabled or incompatible version.\n");
            exit(1);
#endif
        }
        else if( EQUAL(papszArgv[iArg], "-r") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            pszPartsDataSource = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg], "-rn") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            pszPartsLayerName = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg], "-o") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            pszOutputDataSource = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg], "-on") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            pszOutputLayerName = CPLStrdup(papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg], "-of") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
#ifdef HAVE_GEOS_PROJECT
            // coverity[tainted_data]
            pszOutputSepFieldName = papszArgv[++iArg];
#else
            fprintf(stderr,
                    "GEOS support not enabled or incompatible version.\n");
            exit(1);
#endif
        }
        else if( EQUAL(papszArgv[iArg], "-x") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
#ifdef HAVE_GEOS_PROJECT
            // coverity[tainted_data]
            dfX = CPLAtofM(papszArgv[++iArg]);
#else
            fprintf(stderr,
                    "GEOS support not enabled or incompatible version.\n");
            exit(1);
#endif
        }
        else if( EQUAL(papszArgv[iArg], "-y") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
#ifdef HAVE_GEOS_PROJECT
            // coverity[tainted_data]
            dfY = CPLAtofM(papszArgv[++iArg]);
#else
            fprintf(stderr,
                    "GEOS support not enabled or incompatible version.\n");
            exit(1);
#endif
        }
        else if( EQUAL(papszArgv[iArg], "-m") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            dfPos = CPLAtofM(papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg], "-mb") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            dfPosBeg = CPLAtofM(papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg], "-me") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            dfPosEnd = CPLAtofM(papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg], "-s") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
#ifdef HAVE_GEOS_PROJECT
            // coverity[tainted_data]
            dfStep = CPLAtofM(papszArgv[++iArg]);
#else
            fprintf(stderr,
                    "GEOS support not enabled or incompatible version.\n");
            exit(1);
#endif
        }
        else if( EQUAL(papszArgv[iArg], "-progress") )
        {
            bDisplayProgress = TRUE;
        }
        else if( papszArgv[iArg][0] == '-' )
        {
            Usage(CPLSPrintf("Unknown option name '%s'", papszArgv[iArg]));
        }
    }

    if( stOper == op_create )
    {
#ifdef HAVE_GEOS_PROJECT
        if( pszOutputDataSource == nullptr )
            Usage("no output datasource provided");
        else if( pszLineDataSource == nullptr )
            Usage("no path datasource provided");
        else if( pszPicketsDataSource == nullptr )
            Usage("no repers datasource provided");
        else if( pszPicketsMField == nullptr )
            Usage("no position field provided");
        else if( dfStep == -100000000.0 )
            Usage("no step provided");

    /* -------------------------------------------------------------------- */
    /*      Open data source.                                               */
    /* -------------------------------------------------------------------- */

        GDALDataset *poLnDS = reinterpret_cast<GDALDataset *>(
            OGROpen(pszLineDataSource, FALSE, nullptr));

    /* -------------------------------------------------------------------- */
    /*      Report failure                                                  */
    /* -------------------------------------------------------------------- */
        if( poLnDS == nullptr )
        {
            OGRSFDriverRegistrar *poR = OGRSFDriverRegistrar::GetRegistrar();

            fprintf(stderr, "FAILURE:\n"
                    "Unable to open path datasource `%s' with "
                    "the following drivers.\n",
                    pszLineDataSource);

            for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
            {
                fprintf(stderr, "  -> %s\n",
                        poR->GetDriver(iDriver)->GetDescription());
            }

            exit(1);
        }

        GDALDataset *poPkDS = reinterpret_cast<GDALDataset *>(
            OGROpen(pszPicketsDataSource, FALSE, nullptr));
    /* -------------------------------------------------------------------- */
    /*      Report failure                                                  */
    /* -------------------------------------------------------------------- */
        if( poPkDS == nullptr )
        {
            OGRSFDriverRegistrar *poR = OGRSFDriverRegistrar::GetRegistrar();

            fprintf(stderr, "FAILURE:\n"
                    "Unable to open repers datasource `%s' "
                    "with the following drivers.\n",
                    pszPicketsDataSource);

            for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
            {
                fprintf(stderr, "  -> %s\n",
                        poR->GetDriver(iDriver)->GetDescription());
            }

            exit(1);
        }

    /* -------------------------------------------------------------------- */
    /*      Find the output driver.                                         */
    /* -------------------------------------------------------------------- */

        if( !bQuiet )
            CheckDestDataSourceNameConsistency(pszOutputDataSource, pszFormat);

        OGRSFDriverRegistrar *poR = OGRSFDriverRegistrar::GetRegistrar();

        GDALDriver *poDriver = poR->GetDriverByName(pszFormat);
        if( poDriver == nullptr )
        {
            fprintf(stderr, "Unable to find driver `%s'.\n", pszFormat);
            fprintf(stderr,  "The following drivers are available:\n");

            for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
            {
                fprintf(stderr,  "  -> `%s'\n",
                        poR->GetDriver(iDriver)->GetDescription());
            }
            exit(1);
        }

        if( !CPLTestBool(CSLFetchNameValueDef(poDriver->GetMetadata(),
                                              GDAL_DCAP_CREATE, "FALSE")) )
        {
            fprintf(stderr,
                    "%s driver does not support data source creation.\n",
                    pszFormat);
            exit(1);
        }

    /* -------------------------------------------------------------------- */
    /*      Create the output data source.                                  */
    /* -------------------------------------------------------------------- */
        GDALDataset *poODS = poDriver->Create(
            pszOutputDataSource, 0, 0, 0, GDT_Unknown, papszDSCO);
        if( poODS == nullptr )
        {
            fprintf( stderr,  "%s driver failed to create %s\n",
                    pszFormat, pszOutputDataSource );
            exit( 1 );
        }

        OGRLayer *poLnLayer =
            pszLineLayerName == nullptr
            ? poLnDS->GetLayer(0)
            : poLnDS->GetLayerByName(pszLineLayerName);

        if( poLnLayer == nullptr )
        {
            fprintf(stderr, "Get path layer failed.\n");
            exit(1);
        }

        OGRLayer *poPkLayer =
            pszPicketsLayerName == nullptr
            ? poPkDS->GetLayer(0)
            : poPkDS->GetLayerByName(pszPicketsLayerName);

        if(poPkLayer == nullptr)
        {
            fprintf(stderr, "Get repers layer failed.\n");
            exit(1);
        }

        OGRFeatureDefn *poPkFDefn = poPkLayer->GetLayerDefn();
        int nMValField = poPkFDefn->GetFieldIndex( pszPicketsMField );

        OGRLayer *poOutLayer = nullptr;
        if( pszLineSepFieldName != nullptr &&
            pszPicketsSepFieldName != nullptr )
        {
            poOutLayer = SetupTargetLayer(poLnLayer, poODS, papszLCO,
                                          pszOutputLayerName,
                                          pszOutputSepFieldName);
            if( poOutLayer == nullptr )
            {
                fprintf(stderr, "Create output layer failed.\n");
                exit(1);
            }

            // Do the work
            eErr = CreatePartsMultiple(
                poLnLayer, pszLineSepFieldName, poPkLayer,
                pszPicketsSepFieldName, nMValField, dfStep, poOutLayer,
                pszOutputSepFieldName, bDisplayProgress, bQuiet);
        }
        else
        {
            poOutLayer = SetupTargetLayer(poLnLayer, poODS, papszLCO,
                                          pszOutputLayerName);
            if( poOutLayer == nullptr )
            {
                fprintf(stderr, "Create output layer failed.\n");
                exit(1);
            }

            // Do the work
            eErr = CreateParts(poLnLayer, poPkLayer, nMValField, dfStep,
                               poOutLayer, bDisplayProgress, bQuiet);
        }

        GDALClose(poLnDS);
        GDALClose(poPkDS);
        GDALClose(poODS);

        if( nullptr != pszOutputLayerName )
            CPLFree(pszOutputLayerName);
#else  // HAVE_GEOS_PROJECT
        fprintf(stderr, "GEOS support not enabled or incompatible version.\n");
        exit(1);
#endif  // HAVE_GEOS_PROJECT
    }
    else if(stOper == op_get_pos)
    {
#ifdef HAVE_GEOS_PROJECT
        if( pszPartsDataSource == nullptr )
            Usage("no parts datasource provided");
        else if( dfX == -100000000.0 || dfY == -100000000.0 )
            Usage("no coordinates provided");

        GDALDataset *poPartsDS = reinterpret_cast<GDALDataset *>(
            OGROpen(pszPartsDataSource, FALSE, nullptr));
    /* -------------------------------------------------------------------- */
    /*      Report failure                                                  */
    /* -------------------------------------------------------------------- */
        if( poPartsDS == nullptr )
        {
            OGRSFDriverRegistrar *poR = OGRSFDriverRegistrar::GetRegistrar();

            fprintf(stderr, "FAILURE:\n"
                    "Unable to open parts datasource `%s' with "
                    "the following drivers.\n",
                    pszPicketsDataSource);

            for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
            {
                fprintf(stderr, "  -> %s\n",
                        poR->GetDriver(iDriver)->GetDescription() );
            }

            exit(1);
        }

        OGRLayer *poPartsLayer =
            pszPartsLayerName == nullptr
            ? poPartsDS->GetLayer(0)
            : poPartsDS->GetLayerByName(pszPartsLayerName);

        if( poPartsLayer == nullptr )
        {
            fprintf(stderr, "Get parts layer failed.\n");
            exit(1);
        }

        // Do the work
        eErr = GetPosition(poPartsLayer, dfX, dfY, bDisplayProgress, bQuiet);

        GDALClose(poPartsDS);

#else  // HAVE_GEOS_PROJECT
        fprintf(stderr, "GEOS support not enabled or incompatible version.\n");
        exit(1);
#endif  // HAVE_GEOS_PROJECT
    }
    else if( stOper == op_get_coord )
    {
        if( pszPartsDataSource == nullptr )
            Usage("no parts datasource provided");
        else if( dfPos == -100000000.0 )
            Usage("no position provided");

        GDALDataset *poPartsDS = reinterpret_cast<GDALDataset *>(
            OGROpen(pszPartsDataSource, FALSE, nullptr));
    /* -------------------------------------------------------------------- */
    /*      Report failure                                                  */
    /* -------------------------------------------------------------------- */
        if( poPartsDS == nullptr )
        {
            OGRSFDriverRegistrar *poR = OGRSFDriverRegistrar::GetRegistrar();

            fprintf(stderr, "FAILURE:\n"
                    "Unable to open parts datasource `%s' with "
                    "the following drivers.\n",
                    pszPicketsDataSource);

            for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
            {
                fprintf(stderr, "  -> %s\n",
                        poR->GetDriver(iDriver)->GetDescription());
            }

            exit(1);
        }

        OGRLayer *poPartsLayer =
            pszPartsLayerName == nullptr
            ? poPartsDS->GetLayer(0)
            : poPartsDS->GetLayerByName(pszPartsLayerName);

        if( poPartsLayer == nullptr )
        {
            fprintf(stderr, "Get parts layer failed.\n");
            exit(1);
        }
        // Do the work
        eErr = GetCoordinates(poPartsLayer, dfPos, bDisplayProgress, bQuiet);

        GDALClose(poPartsDS);
    }
    else if( stOper == op_get_subline )
    {
        if( pszOutputDataSource == nullptr )
            Usage("no output datasource provided");
        else if( pszPartsDataSource == nullptr )
            Usage("no parts datasource provided");
        else if( dfPosBeg == -100000000.0 )
            Usage("no begin position provided");
        else if( dfPosEnd == -100000000.0 )
            Usage("no end position provided");

        // Open data source.
        GDALDataset *poPartsDS = reinterpret_cast<GDALDataset *>(
            OGROpen(pszPartsDataSource, FALSE, nullptr));

        // Report failure.
        if( poPartsDS == nullptr )
        {
            OGRSFDriverRegistrar *poR = OGRSFDriverRegistrar::GetRegistrar();

            fprintf(stderr, "FAILURE:\n"
                    "Unable to open parts datasource `%s' with "
                    "the following drivers.\n",
                    pszLineDataSource);

            for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
            {
                fprintf(stderr, "  -> %s\n",
                        poR->GetDriver(iDriver)->GetDescription());
            }

            exit(1);
        }

        // Find the output driver.

        if( !bQuiet )
            CheckDestDataSourceNameConsistency(pszOutputDataSource, pszFormat);

        OGRSFDriverRegistrar *poR = OGRSFDriverRegistrar::GetRegistrar();

        GDALDriver *poDriver = poR->GetDriverByName(pszFormat);
        if( poDriver == nullptr )
        {
            fprintf(stderr, "Unable to find driver `%s'.\n", pszFormat);
            fprintf(stderr, "The following drivers are available:\n");

            for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
            {
                fprintf(stderr, "  -> `%s'\n",
                        poR->GetDriver(iDriver)->GetDescription());
            }
            exit(1);
        }

        if( !CPLTestBool(CSLFetchNameValueDef(poDriver->GetMetadata(),
                                              GDAL_DCAP_CREATE, "FALSE")) )
        {
            fprintf(stderr,
                    "%s driver does not support data source creation.\n",
                    pszFormat);
            exit(1);
        }

        // Create the output data source.
        GDALDataset *poODS =
            poDriver->Create(pszOutputDataSource, 0, 0, 0,
                             GDT_Unknown, papszDSCO);
        if( poODS == nullptr )
        {
            fprintf(stderr, "%s driver failed to create %s\n",
                pszFormat, pszOutputDataSource);
            exit(1);
        }

        OGRLayer *poPartsLayer =
            pszLineLayerName == nullptr
            ? poPartsDS->GetLayer(0)
            : poPartsDS->GetLayerByName(pszLineLayerName);

        if( poPartsLayer == nullptr )
        {
            fprintf(stderr, "Get parts layer failed.\n");
            exit(1);
        }

        OGRLayer *poOutLayer =
            SetupTargetLayer(poPartsLayer, poODS, papszLCO, pszOutputLayerName);
        if( poOutLayer == nullptr )
        {
            fprintf(stderr, "Create output layer failed.\n");
            exit(1);
        }

        // Do the work.
        eErr = CreateSubline(poPartsLayer, dfPosBeg, dfPosEnd, poOutLayer,
                             bDisplayProgress, bQuiet);

        GDALClose(poPartsDS);
        GDALClose(poODS);

        if( nullptr != pszOutputLayerName )
            CPLFree(pszOutputLayerName);
    }
    else
    {
        Usage("no operation provided");
    }

    CSLDestroy( papszArgv );
    CSLDestroy( papszDSCO );
    CSLDestroy( papszLCO );

    OGRCleanupAll();

#ifdef DBMALLOC
    malloc_dump(1);
#endif

    return eErr == OGRERR_NONE ? 0 : 1;
}
MAIN_END
