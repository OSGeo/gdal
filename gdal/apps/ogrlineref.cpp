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

#include "ogrsf_frmts.h"
#include "ogr_p.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_api.h"
#include "gdal.h"
#include "gdal_alg.h"
#include "commonutils.h"
#include <map>
#include <vector>
#include <set>
#include <limits>
#include "cpl_error.h"
#include "ogr_geos.h"

#define FIELD_START "beg"
#define FIELD_FINISH "end"
#define FIELD_SCALE_FACTOR "scale"
#define DELTA 0.00000001 //- delta

#if defined(HAVE_GEOS)
#if GEOS_VERSION_MAJOR > 3 || (GEOS_VERSION_MAJOR == 3 && GEOS_VERSION_MINOR >= 2)
#define HAVE_GEOS_PROJECT
#endif
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
    bool IsInside(const double& dfDist) const{ return (dfDist + DELTA >= dfBeg) && (dfDist - DELTA <= dfEnd); }
} CURVE_DATA;

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/
static void Usage(const char* pszAdditionalMsg, int bShort = TRUE)
{
    OGRSFDriverRegistrar        *poR = OGRSFDriverRegistrar::GetRegistrar();


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

    if (bShort)
    {
        printf("\nNote: ogrlineref --long-usage for full help.\n");
        if (pszAdditionalMsg)
            fprintf(stderr, "\nFAILURE: %s\n", pszAdditionalMsg);
        exit(1);
    }

    printf("\n -f format_name: output file format name, possible values are:\n");

    for (int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++)
    {
        OGRSFDriver *poDriver = poR->GetDriver(iDriver);

        if (poDriver->TestCapability(ODrCCreateDataSource))
            printf("     -f \"%s\"\n", poDriver->GetName());
    }

    printf(" -progress: Display progress on terminal. Only works if input layers have the \n"
        "                                          \"fast feature count\" capability\n"
        " -dsco NAME=VALUE: Dataset creation option (format specific)\n"
        " -lco  NAME=VALUE: Layer creation option (format specific)\n"
        " -l src_line_datasource_name: Datasource of line path name\n"
        " -ln layer_name: Layer name in datasource (optional)\n"
        " -lf field_name: Field name for uniq paths in layer (optional)\n"
        " -p src_repers_datasource_name: Datasource of repers name\n"
        " -pn layer_name: Layer name in datasource (optional)\n"
        " -pm pos_field_name: Line postion field name\n"
        " -pf field_name: Field name for correspondence repers of separate paths in layer (optional)\n"
        " -r src_parts_datasource_name: Parts datasource name\n"
        " -rn layer_name: Layer name in datasource (optional)\n"
        " -o dst_datasource_name: Parts datasource name\n"
        " -on layer_name: Layer name in datasource (optional)\n"
        " -of field_name: Field name for correspondence parts of separate paths in layer (optional)\n"
        " -s step: part size in m\n"
        );

    if (pszAdditionalMsg)
        fprintf(stderr, "\nFAILURE: %s\n", pszAdditionalMsg);

    exit(1);
}

static void Usage(int bShort = TRUE)
{
    Usage(NULL, bShort);
}

/************************************************************************/
/*                         SetupTargetLayer()                           */
/************************************************************************/

static OGRLayer* SetupTargetLayer(OGRLayer * poSrcLayer, OGRDataSource *poDstDS, char **papszLCO, const char *pszNewLayerName, const char* pszOutputSepFieldName = NULL)
{
    OGRLayer    *poDstLayer;
    OGRFeatureDefn *poSrcFDefn;
    OGRSpatialReference *poOutputSRS;

    CPLString szLayerName;
    
    if (pszNewLayerName == NULL)
    {
        szLayerName = CPLGetBasename(poDstDS->GetName());
    }
    else
    {
        szLayerName = pszNewLayerName;
    }

    /* -------------------------------------------------------------------- */
    /*      Get other info.                                                 */
    /* -------------------------------------------------------------------- */
    poSrcFDefn = poSrcLayer->GetLayerDefn();

    /* -------------------------------------------------------------------- */
    /*      Find requested geometry fields.                                 */
    /* -------------------------------------------------------------------- */

    poOutputSRS = poSrcLayer->GetSpatialRef();

    /* -------------------------------------------------------------------- */
    /*      Find the layer.                                                 */
    /* -------------------------------------------------------------------- */

    /* GetLayerByName() can instanciate layers that would have been */
    /* 'hidden' otherwise, for example, non-spatial tables in a */
    /* Postgis-enabled database, so this apparently useless command is */
    /* not useless... (#4012) */
    CPLPushErrorHandler(CPLQuietErrorHandler);
    poDstLayer = poDstDS->GetLayerByName(szLayerName);
    CPLPopErrorHandler();
    CPLErrorReset();

    int iLayer = -1;
    if (poDstLayer != NULL)
    {
        int nLayerCount = poDstDS->GetLayerCount();
        for (iLayer = 0; iLayer < nLayerCount; iLayer++)
        {
            OGRLayer *poLayer = poDstDS->GetLayer(iLayer);
            if (poLayer == poDstLayer)
                break;
        }

        if (iLayer == nLayerCount)
            /* shouldn't happen with an ideal driver */
            poDstLayer = NULL;
    }

    /* -------------------------------------------------------------------- */
    /*      If the layer does not exist, then create it.                    */
    /* -------------------------------------------------------------------- */
    if (poDstLayer == NULL)
    {
        if (!poDstDS->TestCapability(ODsCCreateLayer))
        {
            fprintf(stderr,
                "Layer %s not found, and CreateLayer not supported by driver.\n",
                pszNewLayerName);
            return NULL;
        }

        OGRwkbGeometryType eGType = wkbLineString;

        CPLErrorReset();

        if (poDstDS->TestCapability(ODsCCreateGeomFieldAfterCreateLayer))
        {
            eGType = wkbNone;
        }

        poDstLayer = poDstDS->CreateLayer(pszNewLayerName, poOutputSRS,
            (OGRwkbGeometryType)eGType,
            papszLCO);

        if (poDstLayer == NULL)
            return NULL;

        if (poDstDS->TestCapability(ODsCCreateGeomFieldAfterCreateLayer))
        {
            OGRGeomFieldDefn oGFldDefn(poSrcFDefn->GetGeomFieldDefn(0));
            if (poOutputSRS != NULL)
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
            pszNewLayerName);
        return NULL;
    }

    //create beg, end, scale factor fields
    OGRFieldDefn oFieldDefn_Beg = OGRFieldDefn(FIELD_START, OFTReal);
    if (poDstLayer->CreateField(&oFieldDefn_Beg) != OGRERR_NONE)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Create %s field failed!",
            oFieldDefn_Beg.GetNameRef());
        return NULL;
    }

    OGRFieldDefn oFieldDefn_End = OGRFieldDefn(FIELD_FINISH, OFTReal);
    if (poDstLayer->CreateField(&oFieldDefn_End) != OGRERR_NONE)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Create %s field failed!",
            oFieldDefn_End.GetNameRef());
        return NULL;
    }

    OGRFieldDefn oFieldDefn_SF = OGRFieldDefn(FIELD_SCALE_FACTOR, OFTReal);
    if (poDstLayer->CreateField(&oFieldDefn_SF) != OGRERR_NONE)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Create %s field failed!",
            oFieldDefn_SF.GetNameRef());
        return NULL;
    }

    if (pszOutputSepFieldName != NULL)
    {
        OGRFieldDefn  oSepField(pszOutputSepFieldName, OFTString);
        oSepField.SetWidth(255);
        if (poDstLayer->CreateField(&oSepField) != OGRERR_NONE)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Create %s field failed!",
                oSepField.GetNameRef());
            return NULL;
        }
    }

    /* now that we've created a field, GetLayerDefn() won't return NULL */
    OGRFeatureDefn *poDstFDefn = poDstLayer->GetLayerDefn();

    /* Sanity check : if it fails, the driver is buggy */
    if (poDstFDefn != NULL && poDstFDefn->GetFieldCount() != 3)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
            "The output driver has claimed to have added the %s field, but it did not!",
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
    int i;
    char* pszDestExtension = CPLStrdup(CPLGetExtension(pszDestFilename));

    /* TODO: Would be good to have driver metadata like for GDAL drivers ! */
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
                                               { "kml"    , "KML/LIBKML" },
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
                                               { NULL, NULL }
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
                                               { NULL, NULL }
                                             };

    for(i=0; apszExtensions[i][0] != NULL; i++)
    {
        if (EQUAL(pszDestExtension, apszExtensions[i][0]) && !EQUAL(pszDriverName, apszExtensions[i][1]))
        {
            fprintf(stderr,
                    "Warning: The target file has a '%s' extension, which is normally used by the %s driver,\n"
                    "but the requested output driver is %s. Is it really what you want ?\n",
                    pszDestExtension,
                    apszExtensions[i][1],
                    pszDriverName);
            break;
        }
    }

    for(i=0; apszBeginName[i][0] != NULL; i++)
    {
        if (EQUALN(pszDestFilename, apszBeginName[i][0], strlen(apszBeginName[i][0])) &&
            !EQUAL(pszDriverName, apszBeginName[i][1]))
        {
            fprintf(stderr,
                    "Warning: The target file has a name which is normally recognized by the %s driver,\n"
                    "but the requested output driver is %s. Is it really what you want ?\n",
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

OGRErr AddFeature(OGRLayer* const poOutLayer, OGRLineString* pPart, double dfFrom, double dfTo, double dfScaleFactor, int bQuiet, const char* pszOutputSepFieldName = NULL, const char* pszOutputSepFieldValue = NULL)
{
    OGRFeature *poFeature;

    poFeature = OGRFeature::CreateFeature(poOutLayer->GetLayerDefn());

    poFeature->SetField(FIELD_START, dfFrom);
    poFeature->SetField(FIELD_FINISH, dfTo);
    poFeature->SetField(FIELD_SCALE_FACTOR, dfScaleFactor);

    if (pszOutputSepFieldName != NULL)
    {
        poFeature->SetField(pszOutputSepFieldName, pszOutputSepFieldValue);
    }

    poFeature->SetGeometryDirectly(pPart);

    if (poOutLayer->CreateFeature(poFeature) != OGRERR_NONE)
    {
        if (!bQuiet)
            printf("Failed to create feature in shapefile.\n");
        return OGRERR_FAILURE;
    }

    OGRFeature::DestroyFeature(poFeature);

    return OGRERR_NONE;
}

//------------------------------------------------------------------------
// CreateSubline
//------------------------------------------------------------------------
int CreateSubline(OGRLayer* const poPkLayer, double dfPosBeg, double dfPosEnd, OGRLayer* const poOutLayer, int bDisplayProgress, int bQuiet)
{
    OGRFeature* pFeature = NULL;
    //get step
    poPkLayer->ResetReading();
    pFeature = poPkLayer->GetNextFeature();
    //get second part
    pFeature = poPkLayer->GetNextFeature();
    if (pFeature == NULL)
    {
        fprintf(stderr, "Get step for positions %f - %f failed\n", dfPosBeg, dfPosEnd);
        return 1;
    }
    double dfBeg = pFeature->GetFieldAsDouble(FIELD_START);
    double dfEnd = pFeature->GetFieldAsDouble(FIELD_FINISH);
    double dfStep = dfEnd - dfBeg;

    //round input to step
    CPLString szAttributeFilter;
    double dfPosBegLow = floor(dfPosBeg / dfStep) * dfStep;
    double dfPosEndHigh = ceil(dfPosEnd / dfStep) * dfStep;

    szAttributeFilter.Printf("%s >= %f AND %s <= %f", FIELD_START, dfPosBegLow, FIELD_FINISH, dfPosEndHigh);
    poPkLayer->SetAttributeFilter(szAttributeFilter); //TODO: ExecuteSQL should be faster
    poPkLayer->ResetReading();

    std::map<double, OGRFeature *> moParts;
    
    while ((pFeature = poPkLayer->GetNextFeature()) != NULL)
    {
        double dfStart = pFeature->GetFieldAsDouble(FIELD_START);
        moParts[dfStart] = pFeature;
    }

    OGRLineString SubLine;
    
    if (moParts.size() == 0)
    {
        fprintf(stderr, "Get parts for positions %f - %f failed\n", dfPosBeg, dfPosEnd);
        return 1;
    }
    else if (moParts.size() == 1)
    {
        std::map<double, OGRFeature *>::iterator IT = moParts.begin();
        double dfStart = IT->first;
        double dfPosBegCorr = dfPosBeg - dfStart;
        double dfSF = IT->second->GetFieldAsDouble(FIELD_SCALE_FACTOR);
        dfPosBegCorr *= dfSF;

        double dfPosEndCorr = dfPosEnd - dfStart;
        dfPosEndCorr *= dfSF;

        OGRLineString *pLine = (OGRLineString*)IT->second->GetGeometryRef();

        OGRLineString *pSubLine = pLine->getSubLine(dfPosBegCorr, dfPosEndCorr, FALSE);

        OGRFeature::DestroyFeature(IT->second);
        //store
        if (AddFeature(poOutLayer, pSubLine, dfPosBeg, dfPosEnd, 1.0, bQuiet) == OGRERR_NONE)
            return 0;
    }
    else
    {
        int nCounter = moParts.size();
        std::map<double, OGRFeature *>::iterator IT = moParts.begin();
        OGRLineString *pOutLine = new OGRLineString();
        //get first part
        double dfStart = IT->first;
        double dfPosBegCorr = dfPosBeg - dfStart;
        double dfSF = IT->second->GetFieldAsDouble(FIELD_SCALE_FACTOR);
        dfPosBegCorr *= dfSF;

        OGRLineString *pLine = (OGRLineString*)IT->second->GetGeometryRef();

        OGRLineString *pSubLine = pLine->getSubLine(dfPosBegCorr, pLine->get_Length(), FALSE);

        pOutLine->addSubLineString(pSubLine);
        OGRFeature::DestroyFeature(IT->second);

        ++IT;
        nCounter--;

        while (nCounter > 1)
        {
            pLine = (OGRLineString*)IT->second->GetGeometryRef();
            pOutLine->addSubLineString(pLine);
            OGRFeature::DestroyFeature(IT->second);
            ++IT;
            nCounter--;
        }

        //get last part
        double dfPosEndCorr = dfPosEnd - IT->first;
        dfSF = IT->second->GetFieldAsDouble(FIELD_SCALE_FACTOR);
        dfPosEndCorr *= dfSF;

        pLine = (OGRLineString*)IT->second->GetGeometryRef();

        pSubLine = pLine->getSubLine(0, dfPosEndCorr, FALSE);

        pOutLine->addSubLineString(pSubLine);

        OGRFeature::DestroyFeature(IT->second);
        //store
        if (AddFeature(poOutLayer, pOutLine, dfPosBeg, dfPosEnd, 1.0, bQuiet) == OGRERR_NONE)
            return 0;
    }

    return 0;
}



//------------------------------------------------------------------------
// CreatePartsFromLineString
//------------------------------------------------------------------------

int CreatePartsFromLineString(OGRLineString* pPathGeom, OGRLayer* const poPkLayer, int nMValField, double dfStep, OGRLayer* const poOutLayer, int bDisplayProgress, int bQuiet, const char* pszOutputSepFieldName = NULL, const char* pszOutputSepFieldValue = NULL)
{
    //check repers type
    OGRwkbGeometryType eGeomType = poPkLayer->GetGeomType();
    if (wkbFlatten(eGeomType) != wkbPoint)
    {
        fprintf(stderr, "Unsupported geometry type %s for path\n", OGRGeometryTypeToName(eGeomType));
        return 1;
    }

    //create sorted list of repers
    std::map<double, OGRPoint*> moRepers;
    poPkLayer->ResetReading();
    OGRFeature* pReperFeature = NULL;
    double dfTestDistance = 0;
    while ((pReperFeature = poPkLayer->GetNextFeature()) != NULL)
    {
        double dfReperPos = pReperFeature->GetFieldAsDouble(nMValField);
        OGRGeometry* pGeom = pReperFeature->GetGeometryRef();
        if (NULL != pGeom)
        {
            OGRPoint* pPt = (OGRPoint*)pGeom->clone();
            if (!bQuiet)
            {
                if (moRepers.find(dfReperPos) != moRepers.end())
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                        "The distance %f is already present in repers file!", dfReperPos);
                }
            }
            //check if reper incide path
            dfTestDistance = pPathGeom->Project(pPt);
            if (dfTestDistance == 0 || dfTestDistance == pPathGeom->get_Length())
            {
                if (!bQuiet)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                        "The distance %f is out of path!", dfReperPos);
                }
            }
            else
            {
                moRepers[dfReperPos] = pPt;
            }           
        }
        OGRFeature::DestroyFeature(pReperFeature);
    }

    if (moRepers.size() < 2)
    {
        fprintf(stderr, "Not enough repers to proceed\n");
        return 1;
    }

    //check direction
    if (!bQuiet)
    {
        fprintf(stdout, "Check path direction\n");
    }

    //get distance along path from pt1 and pt2. If pt1 distance > pt2 distance, reverse path
    OGRPoint *pt1, *pt2;
    std::map<double, OGRPoint*>::const_iterator IT;
    IT = moRepers.begin();
    double dfPosition = IT->first;
    double dfBeginPosition = IT->first;
    pt1 = IT->second;
    ++IT;
    pt2 = IT->second;

    double dfDistance1 = pPathGeom->Project(pt1);
    double dfDistance2 = pPathGeom->Project(pt2);

    if (dfDistance1 > dfDistance2)
    {
        if (!bQuiet)
        {
            fprintf(stderr, "Warning: The path is opposite the repers direction. Let's reverse path\n");
        }
        pPathGeom->reversePoints();

        dfDistance1 = pPathGeom->Project(pt1);
        dfDistance2 = pPathGeom->Project(pt2);
    }

    OGRLineString* pPart = NULL;

    std::vector<CURVE_DATA> astSubLines;

    if (!bQuiet)
    {
        fprintf(stdout, "Create parts\n");
    }

    //get first part 
    //If first point is not at the beginning of the path
    //The first part should be from the beginning of the path to the first point. length == part.getLength
    OGRPoint *pPtBeg(NULL), *pPtEnd(NULL);
    double dfPtBegPosition, dfPtEndPosition;

    if (dfDistance1 > DELTA)
    {
        pPart = pPathGeom->getSubLine(0, dfDistance1, FALSE);
        if (NULL != pPart)
        {
            OGRSpatialReference* pSpaRef = pPathGeom->getSpatialReference();
            double dfLen = pPart->get_Length();
            if (pSpaRef->IsGeographic())
            {
                //convert to UTM/WGS84
                OGRPoint pt;
                pPart->Value(dfLen / 2, &pt);
                int nZoneEnv = 30 + (pt.getX() + 3.0) / 6.0 + 0.5;
                int nEPSG;
                if (pt.getY() > 0)
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
                if (pTransformPart->transformTo(&SpatRef) == OGRERR_NONE)
                {
                    OGRLineString* pTransformPartLS = (OGRLineString*)pTransformPart;
                    dfLen = pTransformPartLS->get_Length();
                }

                CURVE_DATA data = { pPart, dfPosition - dfLen, dfPosition, pPart->get_Length() / dfLen };
                astSubLines.push_back(data);

                pPtBeg = new OGRPoint();
                pPart->getPoint(0, pPtBeg);
                dfPtBegPosition = dfPosition - dfLen;

                //AddFeature(poOutLayer, pPart, dfPosition - dfLen, dfPosition, pPart->get_Length() / dfLen, bQuiet);
                delete pTransformPart;
            }
            else
            {
                CURVE_DATA data = { pPart, dfPosition - dfLen, dfPosition, 1.0 };
                astSubLines.push_back(data);
                //AddFeature(poOutLayer, pPart, dfPosition - dfLen, dfPosition, 1.0, bQuiet);
                pPtBeg = new OGRPoint();
                pPart->getPoint(0, pPtBeg);
                dfPtBegPosition = dfPosition - dfLen;
            }
        }
    }

    pPart = pPathGeom->getSubLine(dfDistance1, dfDistance2, FALSE);
    if (NULL != pPart)
    {
        CURVE_DATA data = { pPart, dfPosition, IT->first, pPart->get_Length() / (IT->first - dfPosition) };
        astSubLines.push_back(data);
//        AddFeature(poOutLayer, pPart, dfPosition, IT->first, pPart->get_Length() / (IT->first - dfPosition), bQuiet);
    }

    GDALProgressFunc pfnProgress = NULL;
    void        *pProgressArg = NULL;

    double dfFactor = 1.0 / moRepers.size();
    if (bDisplayProgress)
    {
        pfnProgress = GDALScaledProgress;
        pProgressArg = GDALCreateScaledProgress(0.0, 1.0, GDALTermProgress, NULL);
    }

    int nCount = 2;
    dfDistance1 = dfDistance2;
    dfPosition = IT->first;
    ++IT;//get third point    

    double dfEndPosition;
    while (IT != moRepers.end())
    {
        if (bDisplayProgress)
        {
            pfnProgress(nCount * dfFactor, "", pProgressArg);
            nCount++;
        }

        dfEndPosition = IT->first;

        dfDistance2 = pPathGeom->Project(IT->second);

        pPart = pPathGeom->getSubLine(dfDistance1, dfDistance2, FALSE);
        if (NULL != pPart)
        {
            CURVE_DATA data = { pPart, dfPosition, IT->first, pPart->get_Length() / (IT->first - dfPosition) };
            astSubLines.push_back(data);
//            AddFeature(poOutLayer, pPart, dfPosition, IT->first, pPart->get_Length() / (IT->first - dfPosition), bQuiet);
            dfDistance1 = dfDistance2;
            dfPosition = IT->first;
        }

        ++IT;
    }

    //get last part
    pPart = pPathGeom->getSubLine(dfDistance1, pPathGeom->get_Length(), FALSE);
    if (NULL != pPart)
    {
        OGRSpatialReference* pSpaRef = pPathGeom->getSpatialReference();
        double dfLen = pPart->get_Length();
        if (pSpaRef->IsGeographic())
        {
            //convert to UTM/WGS84
            OGRPoint pt;
            pPart->Value(dfLen / 2, &pt);
            int nZoneEnv = 30 + (pt.getX() + 3.0) / 6.0 + 0.5;
            int nEPSG;
            if (pt.getY() > 0)
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
            if (pTransformPart->transformTo(&SpatRef) == OGRERR_NONE)
            {
                OGRLineString* pTransformPartLS = (OGRLineString*)pTransformPart;
                dfLen = pTransformPartLS->get_Length();
            }
            CURVE_DATA data = { pPart, dfPosition, dfPosition + dfLen, pPart->get_Length() / dfLen };
            astSubLines.push_back(data);
            //AddFeature(poOutLayer, pPart, dfPosition, dfPosition + dfLen, pPart->get_Length() / dfLen, bQuiet);

            pPtEnd = new OGRPoint();
            pPart->getPoint(pPart->getNumPoints() - 1, pPtEnd);
            dfPtEndPosition = dfPosition + dfLen;

            delete pTransformPart;
        }
        else
        {
            CURVE_DATA data = { pPart, dfPosition, dfPosition + dfLen, 1.0 };
            astSubLines.push_back(data);
            //AddFeature(poOutLayer, pPart, dfPosition - dfLen, dfPosition, 1.0, bQuiet);
            pPtEnd = new OGRPoint();
            pPart->getPoint(pPart->getNumPoints() - 1, pPtEnd);
            dfPtEndPosition = dfPosition + dfLen;
        }
    }

    //create pickets
    if (!bQuiet)
    {
        fprintf(stdout, "\nCreate pickets\n");
    }

    long nBegin = 0;
    
    if (pPtBeg != NULL)
        nBegin = ceil(dfPtBegPosition / dfStep) * dfStep;
    else
        nBegin = ceil(dfBeginPosition / dfStep) * dfStep;

    double dfRoundBeg = nBegin;

    if (pPtEnd != NULL)
        dfEndPosition = dfPtEndPosition;

    dfFactor = dfStep / (dfEndPosition - dfRoundBeg);
    nCount = 0;
    moRepers.clear();

    if (pPtBeg != NULL)
        moRepers[dfPtBegPosition] = pPtBeg;
    if (pPtEnd != NULL)
        moRepers[dfPtEndPosition] = pPtEnd;

    for (double dfDist = dfRoundBeg; dfDist <= dfEndPosition; dfDist += dfStep)
    {
        if (bDisplayProgress)
        {
            pfnProgress(nCount * dfFactor, "", pProgressArg);
            nCount++;
        }

        for (int j = 0; j < (int)astSubLines.size(); j++)
        {
            if (astSubLines[j].IsInside(dfDist))
            {
                double dfRealDist = (dfDist - astSubLines[j].dfBeg) * astSubLines[j].dfFactor;
                OGRPoint *pReperPoint = new OGRPoint();
                astSubLines[j].pPart->Value(dfRealDist, pReperPoint);

                moRepers[dfDist] = pReperPoint;
                break;
            }
        }
    }

    if (!bQuiet)
    {
        fprintf(stdout, "\nCreate sublines\n");
    }

    IT = moRepers.begin();
    dfFactor = 1.0 / moRepers.size();
    nCount = 0;
    dfDistance1 = 0;
    dfPosition = IT->first;

    while (IT != moRepers.end())
    {
        if (bDisplayProgress)
        {
            pfnProgress(nCount * dfFactor, "", pProgressArg);
            nCount++;
        }

        dfDistance2 = pPathGeom->Project(IT->second);

        if (dfDistance1 != dfDistance2)
        {
            pPart = pPathGeom->getSubLine(dfDistance1, dfDistance2, FALSE);
            if (NULL != pPart)
            {
                AddFeature(poOutLayer, pPart, dfPosition, IT->first, pPart->get_Length() / (IT->first - dfPosition), bQuiet, pszOutputSepFieldName, pszOutputSepFieldValue);
                dfDistance1 = dfDistance2;
                dfPosition = IT->first;
            }
        }

        ++IT;
    }

    if (!bQuiet)
    {
        fprintf(stdout, "\nSuccess!\n\n");
    }

    return 0;
}

//------------------------------------------------------------------------
// CreateParts
//------------------------------------------------------------------------
int CreateParts(OGRLayer* const poLnLayer, OGRLayer* const poPkLayer, int nMValField, double dfStep, OGRLayer* const poOutLayer, int bDisplayProgress, int bQuiet, const char* pszOutputSepFieldName = NULL, const char* pszOutputSepFieldValue = NULL)
{


    //check path and get first line
    OGRwkbGeometryType eGeomType = poLnLayer->GetGeomType();
    if (wkbFlatten(eGeomType) != wkbLineString && wkbFlatten(eGeomType) != wkbMultiLineString)
    {
        fprintf(stderr, "Unsupported geometry type %s for path\n", OGRGeometryTypeToName(eGeomType));
        return 1;
    }

    poLnLayer->ResetReading();
    //get first geometry
    //TODO: attruibute filter for path geometry 
    OGRFeature* pPathFeature = poLnLayer->GetNextFeature();
    if (NULL != pPathFeature)
    {
        OGRGeometry* pGeom = pPathFeature->GetGeometryRef();

        if (wkbFlatten(pGeom->getGeometryType()) == wkbMultiLineString)
        {
            if (!bQuiet)
            {
                fprintf(stdout, "\nThe geometry %ld is wkbMultiLineString type\n", pPathFeature->GetFID());
            }

            OGRGeometryCollection* pGeomColl = (OGRGeometryCollection*)pGeom;
            for (int i = 0; i < pGeomColl->getNumGeometries(); ++i)
            {
                OGRLineString* pPath = (OGRLineString*)pGeomColl->getGeometryRef(i)->clone();
                pPath->assignSpatialReference(pGeomColl->getSpatialReference());
                if (CreatePartsFromLineString(pPath, poPkLayer, nMValField, dfStep, poOutLayer, bDisplayProgress, bQuiet, pszOutputSepFieldName, pszOutputSepFieldValue) != 0)
                    return 1;
            }
            return 0;
        }
        else
        {
            if (NULL != pGeom)
            {
                return CreatePartsFromLineString((OGRLineString*)pGeom->clone(), poPkLayer, nMValField, dfStep, poOutLayer, bDisplayProgress, bQuiet, pszOutputSepFieldName, pszOutputSepFieldValue);
            }
        }

        OGRFeature::DestroyFeature(pPathFeature);
    }


    return 1;
}

//------------------------------------------------------------------------
// CreatePartsMultiple
//------------------------------------------------------------------------
int CreatePartsMultiple(OGRLayer* const poLnLayer, const char* pszLineSepFieldName, OGRLayer* const poPkLayer, const char* pszPicketsSepFieldName, int nMValField, double dfStep, OGRLayer* const poOutLayer, const char* pszOutputSepFieldName, int bDisplayProgress, int bQuiet)
{
    //read all sep field values into array
    std::set<CPLString> asIDs;

    OGRFeatureDefn *pDefn = poLnLayer->GetLayerDefn();
    int nLineSepFieldInd = pDefn->GetFieldIndex(pszLineSepFieldName);
    if (nLineSepFieldInd == -1)
    {
        fprintf(stderr, "The field %s not found\n", pszLineSepFieldName);
        return 1;
    }

    poLnLayer->ResetReading();
    OGRFeature* pFeature = NULL;
    while ((pFeature = poLnLayer->GetNextFeature()) != NULL)
    {
        CPLString sID = pFeature->GetFieldAsString(nLineSepFieldInd);
        asIDs.insert(sID);

        OGRFeature::DestroyFeature(pFeature);
    }

    for (std::set<CPLString>::const_iterator it = asIDs.begin(); it != asIDs.end(); ++it)
    {
        //create select clause
        //int ntest1 = poLnLayer->GetFeatureCount();
        CPLString sLineWhere;
        sLineWhere.Printf("%s = \"%s\"", pszLineSepFieldName, it->c_str());
        poLnLayer->SetAttributeFilter(sLineWhere);
        //int ntest2 = poLnLayer->GetFeatureCount();

        //ntest1 = poPkLayer->GetFeatureCount();
        CPLString sPkWhere;
        sPkWhere.Printf("%s = \"%s\"", pszPicketsSepFieldName, it->c_str());
        poPkLayer->SetAttributeFilter(sPkWhere);
        //ntest2 = poPkLayer->GetFeatureCount();

        if (!bQuiet)
        {
            fprintf(stdout, "The %s %s\n", pszPicketsSepFieldName, it->c_str());
        }

        CreateParts(poLnLayer, poPkLayer, nMValField, dfStep, poOutLayer, bDisplayProgress, bQuiet, pszOutputSepFieldName, *it);
    }
}

//------------------------------------------------------------------------
// GetPosition
//------------------------------------------------------------------------
int GetPosition(OGRLayer* const poPkLayer, double dfX, double dfY, int bDisplayProgress, int bQuiet)
{
    //create point
    OGRPoint pt;
    pt.setX(dfX);
    pt.setY(dfY);
    pt.assignSpatialReference(poPkLayer->GetSpatialRef());

    poPkLayer->ResetReading();
    OGRLineString *pCloserPart = NULL;
    double dfBeg, dfScale;
    double dfMinDistance = std::numeric_limits<double>::max();
    OGRFeature* pFeature = NULL;
    while ((pFeature = poPkLayer->GetNextFeature()) != NULL)
    {
        OGRGeometry* pCurrentGeom = pFeature->GetGeometryRef();
        if (pCurrentGeom != NULL)
        {
            double dfCurrentDistance = pCurrentGeom->Distance(&pt);
            if (dfCurrentDistance < dfMinDistance)
            {
                dfMinDistance = dfCurrentDistance;
                if (pCloserPart != NULL)
                    delete pCloserPart;
                pCloserPart = (OGRLineString*)pFeature->StealGeometry();
                dfBeg = pFeature->GetFieldAsDouble(FIELD_START);
                dfScale = pFeature->GetFieldAsDouble(FIELD_SCALE_FACTOR);
            }
        }
        OGRFeature::DestroyFeature(pFeature);
    }

    //now we have closest part
    //get real distance
    double dfRealDist = pCloserPart->Project(&pt);
    //compute reference distance
    double dfRefDist = dfBeg + dfRealDist / dfScale;
    if (bQuiet == TRUE)
    {
        fprintf(stdout, "%f\n", dfRefDist);
    }
    else
    {
        fprintf(stdout, "The position for coordinates lat:%f, long:%f is %f\n", dfY, dfX, dfRefDist);
    }

    return 0;
}

//------------------------------------------------------------------------
// GetCoordinates
//------------------------------------------------------------------------
int GetCoordinates(OGRLayer* const poPkLayer, double dfPos, int bDisplayProgress, int bQuiet)
{
    CPLString szAttributeFilter;
    szAttributeFilter.Printf("%s < %f AND %s > %f", FIELD_START, dfPos, FIELD_FINISH, dfPos);
    poPkLayer->SetAttributeFilter(szAttributeFilter); //TODO: ExecuteSQL should be faster
    poPkLayer->ResetReading();

    bool bHaveCoords = false;
    OGRFeature* pFeature = NULL;
    while ((pFeature = poPkLayer->GetNextFeature()) != NULL)
    {
        bHaveCoords = true;
        double dfStart = pFeature->GetFieldAsDouble(FIELD_START);
        double dfPosCorr = dfPos - dfStart;
        double dfSF = pFeature->GetFieldAsDouble(FIELD_SCALE_FACTOR);
        dfPosCorr *= dfSF;
        OGRLineString *pLine = (OGRLineString*)pFeature->GetGeometryRef();

        OGRPoint pt;
        pLine->Value(dfPosCorr, &pt);

        if (bQuiet == TRUE)
        {
            fprintf(stdout, "%f,%f,%f\n", pt.getX(), pt.getY(), pt.getZ());
        }
        else
        {
            fprintf(stdout, "The position for distance %f is lat:%f, long:%f, height:%f\n", dfPos, pt.getY(), pt.getX(), pt.getZ());
        }
        OGRFeature::DestroyFeature(pFeature);
    }

    if (bHaveCoords)
    {
        return 0;
    }
    else
    {
        fprintf(stderr, "Get coordinates for position %f failed\n", dfPos);
        return 1;
    }
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

#define CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(nExtraArg) \
    do { if (iArg + nExtraArg >= nArgc) \
        Usage(CPLSPrintf("%s option requires %d argument(s)", papszArgv[iArg], nExtraArg)); } while(0)

int main( int nArgc, char ** papszArgv )

{
    int          nRetCode = 0;
    int          bQuiet = FALSE;
    const char  *pszFormat = "ESRI Shapefile";

    const char  *pszOutputDataSource = NULL;
    const char  *pszLineDataSource = NULL;
    const char  *pszPicketsDataSource = NULL;
    const char  *pszPartsDataSource = NULL;
    char  *pszOutputLayerName = NULL;
    const char  *pszLineLayerName = NULL;
    const char  *pszPicketsLayerName = NULL;
    const char  *pszPicketsMField = NULL;
    const char  *pszPartsLayerName = NULL;

    const char  *pszLineSepFieldName = NULL;
    const char  *pszPicketsSepFieldName = NULL;
    const char  *pszOutputSepFieldName = "uniq_uid";
    
    char        **papszDSCO = NULL, **papszLCO = NULL;
    
    operation stOper = op_unknown;
    double dfX(-100000000), dfY(-100000000), dfPos(-100000000);

    int bDisplayProgress = FALSE;
    
    double dfPosBeg(-100000000), dfPosEnd(-100000000);
    double dfStep(-100000000);

    /* Check strict compilation and runtime library version as we use C++ API */
    if (! GDAL_CHECK_VERSION(papszArgv[0]))
        exit(1);

    EarlySetConfigOptions(nArgc, papszArgv);

/* -------------------------------------------------------------------- */
/*      Register format(s).                                             */
/* -------------------------------------------------------------------- */
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
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   papszArgv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            return 0;
        }
        else if( EQUAL(papszArgv[iArg],"--help") )
            Usage();
        else if ( EQUAL(papszArgv[iArg], "--long-usage") )
        {
            Usage(FALSE);
        }

        else if( EQUAL(papszArgv[iArg],"-q") || EQUAL(papszArgv[iArg],"-quiet") )
        {
            bQuiet = TRUE;
        }
        else if( EQUAL(papszArgv[iArg],"-f") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            //bFormatExplicitelySet = TRUE;
            pszFormat = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-dsco") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszDSCO = CSLAddString(papszDSCO, papszArgv[++iArg] );
        }
        else if( EQUAL(papszArgv[iArg],"-lco") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszLCO = CSLAddString(papszLCO, papszArgv[++iArg] );
        }        
        else if( EQUAL(papszArgv[iArg],"-create") )
        {
            stOper = op_create;
        }
        else if( EQUAL(papszArgv[iArg],"-get_pos") )
        {
            stOper = op_get_pos;
        }        
        else if( EQUAL(papszArgv[iArg],"-get_coord") )
        {
            stOper = op_get_coord;
        }        
        else if( EQUAL(papszArgv[iArg],"-get_subline") )
        {
            stOper = op_get_subline;
        }        
        else if( EQUAL(papszArgv[iArg],"-l") )
        {
             CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
           pszLineDataSource = papszArgv[++iArg];
        }        
        else if( EQUAL(papszArgv[iArg],"-ln") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszLineLayerName = papszArgv[++iArg];
        }    
        else if (EQUAL(papszArgv[iArg], "-lf"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszLineSepFieldName = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-p") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszPicketsDataSource = papszArgv[++iArg];
        }        
        else if( EQUAL(papszArgv[iArg],"-pn") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszPicketsLayerName = papszArgv[++iArg];
        }    
        else if( EQUAL(papszArgv[iArg],"-pm") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszPicketsMField = papszArgv[++iArg];
        }
        else if (EQUAL(papszArgv[iArg], "-pf"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszPicketsSepFieldName = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-r") )
        {
             CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
           pszPartsDataSource = papszArgv[++iArg];
        }        
        else if( EQUAL(papszArgv[iArg],"-rn") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszPartsLayerName = papszArgv[++iArg];
        }         
        else if( EQUAL(papszArgv[iArg],"-o") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszOutputDataSource = papszArgv[++iArg];
        }   
        else if( EQUAL(papszArgv[iArg],"-on") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszOutputLayerName = CPLStrdup(papszArgv[++iArg]);
        }        
        else if (EQUAL(papszArgv[iArg], "-of"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszOutputSepFieldName = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-x") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfX = CPLAtofM(papszArgv[++iArg]);
        } 
        else if( EQUAL(papszArgv[iArg],"-y") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfY = CPLAtofM(papszArgv[++iArg]);
        } 
        else if( EQUAL(papszArgv[iArg],"-m") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfPos = CPLAtofM(papszArgv[++iArg]);
        }  
        else if( EQUAL(papszArgv[iArg],"-mb") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfPosBeg = CPLAtofM(papszArgv[++iArg]);
        }  
        else if( EQUAL(papszArgv[iArg],"-me") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfPosEnd = CPLAtofM(papszArgv[++iArg]);
        }  
        else if( EQUAL(papszArgv[iArg],"-s") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfStep = CPLAtofM(papszArgv[++iArg]);
        }  
        else if( EQUAL(papszArgv[iArg],"-progress") )
        {
            bDisplayProgress = TRUE;
        }
        else if( papszArgv[iArg][0] == '-' )
        {
            Usage(CPLSPrintf("Unknown option name '%s'", papszArgv[iArg]));
        }
    }

    
    if(stOper == op_create)
    {
#ifdef HAVE_GEOS_PROJECT
        if( pszOutputDataSource == NULL)
            Usage("no output datasource provided");
        else if(pszLineDataSource == NULL)
            Usage("no path datasource provided");
        else  if(pszPicketsDataSource == NULL)
            Usage("no repers datasource provided");
        else  if(pszPicketsMField == NULL)
            Usage("no position field provided");
        else  if (dfStep == -100000000)
            Usage("no step provided");
            
    /* -------------------------------------------------------------------- */
    /*      Open data source.                                               */
    /* -------------------------------------------------------------------- */
        OGRDataSource       *poLnDS;
        OGRDataSource       *poODS = NULL;
        OGRSFDriver         *poDriver = NULL;
        OGRDataSource *poPkDS = NULL;
        OGRLayer *poPkLayer = NULL;

        poLnDS = OGRSFDriverRegistrar::Open( pszLineDataSource, FALSE );

    /* -------------------------------------------------------------------- */
    /*      Report failure                                                  */
    /* -------------------------------------------------------------------- */
        if( poLnDS == NULL )
        {
            OGRSFDriverRegistrar    *poR = OGRSFDriverRegistrar::GetRegistrar();
            
            fprintf( stderr, "FAILURE:\n"
                    "Unable to open path datasource `%s' with the following drivers.\n",
                    pszLineDataSource);

            for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
            {
                fprintf( stderr, "  -> %s\n", poR->GetDriver(iDriver)->GetName() );
            }

            exit( 1 );
        }
        
        poPkDS = OGRSFDriverRegistrar::Open( pszPicketsDataSource, FALSE );
    /* -------------------------------------------------------------------- */
    /*      Report failure                                                  */
    /* -------------------------------------------------------------------- */
        if( poPkDS == NULL )
        {
            OGRSFDriverRegistrar    *poR = OGRSFDriverRegistrar::GetRegistrar();
            
            fprintf( stderr, "FAILURE:\n"
                    "Unable to open repers datasource `%s' with the following drivers.\n",
                    pszPicketsDataSource);

            for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
            {
                fprintf( stderr, "  -> %s\n", poR->GetDriver(iDriver)->GetName() );
            }

            exit( 1 );
        }
    
    
    /* -------------------------------------------------------------------- */
    /*      Find the output driver.                                         */
    /* -------------------------------------------------------------------- */

        if (!bQuiet)
            CheckDestDataSourceNameConsistency(pszOutputDataSource, pszFormat);

        OGRSFDriverRegistrar *poR = OGRSFDriverRegistrar::GetRegistrar();
        int                  iDriver;

        poDriver = poR->GetDriverByName(pszFormat);
        if( poDriver == NULL )
        {
            fprintf( stderr, "Unable to find driver `%s'.\n", pszFormat );
            fprintf( stderr,  "The following drivers are available:\n" );
        
            for( iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
            {
                fprintf( stderr,  "  -> `%s'\n", poR->GetDriver(iDriver)->GetName() );
            }
            exit( 1 );
        }

        if( !poDriver->TestCapability( ODrCCreateDataSource ) )
        {
            fprintf( stderr,  "%s driver does not support data source creation.\n",
                    pszFormat );
            exit( 1 );
        }

    /* -------------------------------------------------------------------- */
    /*      Create the output data source.                                  */
    /* -------------------------------------------------------------------- */
        poODS = poDriver->CreateDataSource( pszOutputDataSource, papszDSCO );
        if( poODS == NULL )
        {
            fprintf( stderr,  "%s driver failed to create %s\n", 
                    pszFormat, pszOutputDataSource );
            exit( 1 );
        }

        OGRLayer *poLnLayer, *poOutLayer;

        if(pszLineLayerName == NULL)
        {
            poLnLayer = poLnDS->GetLayer(0);
        }
        else
        {
            poLnLayer = poLnDS->GetLayerByName(pszLineLayerName);
        }
        
        if(poLnLayer == NULL)
        {
            fprintf( stderr, "Get path layer failed.\n" );
            exit( 1 );
        }   
        
        if(pszPicketsLayerName == NULL)
        {
            poPkLayer = poPkDS->GetLayer(0);
        }
        else
        {
            poPkLayer = poPkDS->GetLayerByName(pszPicketsLayerName);
        }
        
        if(poPkLayer == NULL)
        {
            fprintf( stderr, "Get repers layer failed.\n" );
            exit( 1 );    
        }
                 
        OGRFeatureDefn *poPkFDefn = poPkLayer->GetLayerDefn();
        int nMValField = poPkFDefn->GetFieldIndex( pszPicketsMField );

        if (pszLineSepFieldName != NULL && pszPicketsSepFieldName != NULL)
        {
            poOutLayer = SetupTargetLayer(poLnLayer, poODS, papszLCO, pszOutputLayerName, pszOutputSepFieldName);
            if(poOutLayer == NULL)
            {
                fprintf( stderr, "Create output layer failed.\n" );
                exit( 1 );    
            }    

            //do the work
            nRetCode = CreatePartsMultiple(poLnLayer, pszLineSepFieldName, poPkLayer, pszPicketsSepFieldName, nMValField, dfStep, poOutLayer, pszOutputSepFieldName, bDisplayProgress, bQuiet);
        }
        else
        {
            poOutLayer = SetupTargetLayer(poLnLayer, poODS, papszLCO, pszOutputLayerName);
            if(poOutLayer == NULL)
            {
                fprintf( stderr, "Create output layer failed.\n" );
                exit( 1 );    
            }     
        
            //do the work
            nRetCode = CreateParts(poLnLayer, poPkLayer, nMValField, dfStep, poOutLayer, bDisplayProgress, bQuiet);
        }
        
        //clean up        
        OGRDataSource::DestroyDataSource(poLnDS);
        OGRDataSource::DestroyDataSource(poPkDS);
        OGRDataSource::DestroyDataSource(poODS);
            
        if (NULL != pszOutputLayerName)
            CPLFree(pszOutputLayerName);
#else //HAVE_GEOS_PROJECT
        fprintf( stderr, "GEOS support not enabled or incompatible version.\n" );
        exit( 1 );       
#endif //HAVE_GEOS_PROJECT            
    }
    else if(stOper == op_get_pos)
    {
#ifdef HAVE_GEOS_PROJECT    
        OGRDataSource *poPartsDS = NULL;
        OGRLayer *poPartsLayer = NULL;

        if (pszPartsDataSource == NULL)
            Usage("no parts datasource provided");
        else if(dfX == -100000000 || dfY == -100000000)
            Usage("no coordinates provided");
            
        poPartsDS = OGRSFDriverRegistrar::Open( pszPartsDataSource, FALSE );
    /* -------------------------------------------------------------------- */
    /*      Report failure                                                  */
    /* -------------------------------------------------------------------- */
        if (poPartsDS == NULL)
        {
            OGRSFDriverRegistrar    *poR = OGRSFDriverRegistrar::GetRegistrar();
            
            fprintf( stderr, "FAILURE:\n"
                    "Unable to open parts datasource `%s' with the following drivers.\n",
                    pszPicketsDataSource);

            for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
            {
                fprintf( stderr, "  -> %s\n", poR->GetDriver(iDriver)->GetName() );
            }

            exit( 1 );
        }
            
        if(pszPartsLayerName == NULL)
        {
            poPartsLayer = poPartsDS->GetLayer(0);
        }
        else
        {
            poPartsLayer = poPartsDS->GetLayerByName(pszPartsLayerName);
        }
        
        if (poPartsLayer == NULL)
        {
            fprintf( stderr, "Get parts layer failed.\n" );
            exit( 1 );    
        }  

        //do the work
        nRetCode = GetPosition(poPartsLayer, dfX, dfY, bDisplayProgress, bQuiet);

        //clean up
        OGRDataSource::DestroyDataSource(poPartsDS);
#else //HAVE_GEOS_PROJECT
        fprintf( stderr, "GEOS support not enabled or incompatible version.\n" );
        exit( 1 );       
#endif //HAVE_GEOS_PROJECT            
    }
    else if(stOper == op_get_coord)
    {
        OGRDataSource *poPartsDS = NULL;
        OGRLayer *poPartsLayer = NULL;

        if (pszPartsDataSource == NULL)
            Usage("no parts datasource provided");
        else if(dfPos == -100000000)
            Usage("no position provided");
            
        poPartsDS = OGRSFDriverRegistrar::Open(pszPartsDataSource, FALSE);
    /* -------------------------------------------------------------------- */
    /*      Report failure                                                  */
    /* -------------------------------------------------------------------- */
        if (poPartsDS == NULL)
        {
            OGRSFDriverRegistrar    *poR = OGRSFDriverRegistrar::GetRegistrar();
            
            fprintf( stderr, "FAILURE:\n"
                    "Unable to open parts datasource `%s' with the following drivers.\n",
                    pszPicketsDataSource);

            for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
            {
                fprintf( stderr, "  -> %s\n", poR->GetDriver(iDriver)->GetName() );
            }

            exit( 1 );
        }
            
        if(pszPartsLayerName == NULL)
        {
            poPartsLayer = poPartsDS->GetLayer(0);
        }
        else
        {
            poPartsLayer = poPartsDS->GetLayerByName(pszPartsLayerName);
        }
        
        if (poPartsLayer == NULL)
        {
            fprintf( stderr, "Get parts layer failed.\n" );
            exit( 1 );    
        }     
        //do the work
        nRetCode = GetCoordinates(poPartsLayer, dfPos, bDisplayProgress, bQuiet);

        //clean up
        OGRDataSource::DestroyDataSource(poPartsDS);
    }
    else if (stOper == op_get_subline)
    {
        if (pszOutputDataSource == NULL)
            Usage("no output datasource provided");
        else if (pszPartsDataSource == NULL)
            Usage("no parts datasource provided");
        else  if (dfPosBeg == -100000000)
            Usage("no begin position provided");
        else  if (dfPosEnd == -100000000)
            Usage("no end position provided");

        /* -------------------------------------------------------------------- */
        /*      Open data source.                                               */
        /* -------------------------------------------------------------------- */
        OGRDataSource       *poPartsDS;
        OGRDataSource       *poODS = NULL;
        OGRSFDriver         *poDriver = NULL;

        poPartsDS = OGRSFDriverRegistrar::Open(pszPartsDataSource, FALSE);

        /* -------------------------------------------------------------------- */
        /*      Report failure                                                  */
        /* -------------------------------------------------------------------- */
        if (poPartsDS == NULL)
        {
            OGRSFDriverRegistrar    *poR = OGRSFDriverRegistrar::GetRegistrar();

            fprintf(stderr, "FAILURE:\n"
                "Unable to open parts datasource `%s' with the following drivers.\n",
                pszLineDataSource);

            for (int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++)
            {
                fprintf(stderr, "  -> %s\n", poR->GetDriver(iDriver)->GetName());
            }

            exit(1);
        }

        /* -------------------------------------------------------------------- */
        /*      Find the output driver.                                         */
        /* -------------------------------------------------------------------- */

        if (!bQuiet)
            CheckDestDataSourceNameConsistency(pszOutputDataSource, pszFormat);

        OGRSFDriverRegistrar *poR = OGRSFDriverRegistrar::GetRegistrar();
        int                  iDriver;

        poDriver = poR->GetDriverByName(pszFormat);
        if (poDriver == NULL)
        {
            fprintf(stderr, "Unable to find driver `%s'.\n", pszFormat);
            fprintf(stderr, "The following drivers are available:\n");

            for (iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++)
            {
                fprintf(stderr, "  -> `%s'\n", poR->GetDriver(iDriver)->GetName());
            }
            exit(1);
        }

        if (!poDriver->TestCapability(ODrCCreateDataSource))
        {
            fprintf(stderr, "%s driver does not support data source creation.\n",
                pszFormat);
            exit(1);
        }

        /* -------------------------------------------------------------------- */
        /*      Create the output data source.                                  */
        /* -------------------------------------------------------------------- */
        poODS = poDriver->CreateDataSource(pszOutputDataSource, papszDSCO);
        if (poODS == NULL)
        {
            fprintf(stderr, "%s driver failed to create %s\n",
                pszFormat, pszOutputDataSource);
            exit(1);
        }

        OGRLayer *poPartsLayer, *poOutLayer;

        if (pszLineLayerName == NULL)
        {
            poPartsLayer = poPartsDS->GetLayer(0);
        }
        else
        {
            poPartsLayer = poPartsDS->GetLayerByName(pszLineLayerName);
        }

        if (poPartsLayer == NULL)
        {
            fprintf(stderr, "Get parts layer failed.\n");
            exit(1);
        }

        poOutLayer = SetupTargetLayer(poPartsLayer, poODS, papszLCO, pszOutputLayerName);
        if (poOutLayer == NULL)
        {
            fprintf(stderr, "Create output layer failed.\n");
            exit(1);
        }

        //do the work
        nRetCode = CreateSubline(poPartsLayer, dfPosBeg, dfPosEnd, poOutLayer, bDisplayProgress, bQuiet);

        //clean up        
        OGRDataSource::DestroyDataSource(poPartsDS);
        OGRDataSource::DestroyDataSource(poODS);

        if (NULL != pszOutputLayerName)
            CPLFree(pszOutputLayerName);
    }
    else
    {
        Usage("no operation provided");
    }  
    
/* -------------------------------------------------------------------- */
/*      Close down.                                                     */
/* -------------------------------------------------------------------- */

    CSLDestroy( papszArgv );
    CSLDestroy( papszDSCO );
    CSLDestroy( papszLCO );

    OGRCleanupAll();

#ifdef DBMALLOC
    malloc_dump(1);
#endif
    
    return nRetCode;
}

