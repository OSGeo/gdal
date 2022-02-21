/******************************************************************************
 *
 * Project:  CSV Translator
 * Purpose:  Implements OGRCSVDataSource class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include "ogr_csv.h"

#include <cerrno>
#include <cstring>
#include <string>

#include "cpl_conv.h"
#include "cpl_csv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "cpl_vsi_virtual.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_spatialref.h"
#include "ogreditablelayer.h"
#include "ogrsf_frmts.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                     OGRCSVEditableLayerSynchronizer                  */
/************************************************************************/

class OGRCSVEditableLayerSynchronizer final: public IOGREditableLayerSynchronizer
{
    OGRCSVLayer *m_poCSVLayer;
    char        **m_papszOpenOptions;

  public:
    OGRCSVEditableLayerSynchronizer(OGRCSVLayer *poCSVLayer,
                                    char **papszOpenOptions) :
        m_poCSVLayer(poCSVLayer),
        m_papszOpenOptions(CSLDuplicate(papszOpenOptions)) {}
    virtual ~OGRCSVEditableLayerSynchronizer() GDAL_OVERRIDE;

    virtual OGRErr EditableSyncToDisk(OGRLayer *poEditableLayer,
                                      OGRLayer **ppoDecoratedLayer) override;
};

/************************************************************************/
/*                     ~OGRCSVEditableLayerSynchronizer()               */
/************************************************************************/

OGRCSVEditableLayerSynchronizer::~OGRCSVEditableLayerSynchronizer()
{
    CSLDestroy(m_papszOpenOptions);
}

/************************************************************************/
/*                       EditableSyncToDisk()                           */
/************************************************************************/

OGRErr OGRCSVEditableLayerSynchronizer::EditableSyncToDisk(
    OGRLayer *poEditableLayer, OGRLayer **ppoDecoratedLayer)
{
    CPLAssert(m_poCSVLayer == *ppoDecoratedLayer);

    const CPLString osLayerName(m_poCSVLayer->GetName());
    const CPLString osFilename(m_poCSVLayer->GetFilename());
    const bool bCreateCSVT = m_poCSVLayer->GetCreateCSVT();
    const CPLString osCSVTFilename(CPLResetExtension(osFilename, "csvt"));
    VSIStatBufL sStatBuf;
    const bool bHasCSVT = VSIStatL(osCSVTFilename, &sStatBuf) == 0;
    CPLString osTmpFilename(osFilename);
    CPLString osTmpCSVTFilename(osFilename);
    if( VSIStatL(osFilename, &sStatBuf) == 0 )
    {
        osTmpFilename += "_ogr_tmp.csv";
        osTmpCSVTFilename += "_ogr_tmp.csvt";
    }
    const char chDelimiter = m_poCSVLayer->GetDelimiter();
    OGRCSVLayer *poCSVTmpLayer = new OGRCSVLayer(
        osLayerName, nullptr, osTmpFilename, true, true, chDelimiter);
    poCSVTmpLayer->BuildFeatureDefn(nullptr, nullptr, m_papszOpenOptions);
    poCSVTmpLayer->SetCRLF(m_poCSVLayer->GetCRLF());
    poCSVTmpLayer->SetCreateCSVT(bCreateCSVT || bHasCSVT);
    poCSVTmpLayer->SetWriteBOM(m_poCSVLayer->GetWriteBOM());
    poCSVTmpLayer->SetStringQuoting(m_poCSVLayer->GetStringQuoting());

    if( m_poCSVLayer->GetGeometryFormat() == OGR_CSV_GEOM_AS_WKT )
        poCSVTmpLayer->SetWriteGeometry(wkbNone, OGR_CSV_GEOM_AS_WKT, nullptr);

    const bool bKeepGeomColmuns = CPLFetchBool(m_papszOpenOptions, "KEEP_GEOM_COLUMNS", true);

    OGRErr eErr = OGRERR_NONE;
    OGRFeatureDefn *poEditableFDefn = poEditableLayer->GetLayerDefn();
    for( int i = 0; eErr == OGRERR_NONE && i < poEditableFDefn->GetFieldCount();
         i++ )
    {
        OGRFieldDefn oFieldDefn(poEditableFDefn->GetFieldDefn(i));
        int iGeomFieldIdx = 0;
        if( (EQUAL(oFieldDefn.GetNameRef(), "WKT") &&
             (iGeomFieldIdx = poEditableFDefn->GetGeomFieldIndex("")) >= 0) ||
            (bKeepGeomColmuns &&
             (iGeomFieldIdx = poEditableFDefn->GetGeomFieldIndex(
                 (std::string("geom_") + oFieldDefn.GetNameRef()).c_str())) >= 0) )
        {
            OGRGeomFieldDefn oGeomFieldDefn(
                poEditableFDefn->GetGeomFieldDefn(iGeomFieldIdx));
            eErr = poCSVTmpLayer->CreateGeomField(&oGeomFieldDefn);
        }
        else
        {
            eErr = poCSVTmpLayer->CreateField(&oFieldDefn);
        }
    }

    const bool bHasXY = !m_poCSVLayer->GetXField().empty() &&
                        !m_poCSVLayer->GetYField().empty();
    const bool bHasZ = !m_poCSVLayer->GetZField().empty();
    if( bHasXY && !bKeepGeomColmuns )
    {
        if( poCSVTmpLayer->GetLayerDefn()->GetFieldIndex(
                m_poCSVLayer->GetXField()) < 0 )
        {
            OGRFieldDefn oFieldDefn(m_poCSVLayer->GetXField(), OFTReal);
            if( eErr == OGRERR_NONE )
                eErr = poCSVTmpLayer->CreateField(&oFieldDefn);
        }
        if( poCSVTmpLayer->GetLayerDefn()->GetFieldIndex(
                m_poCSVLayer->GetYField()) < 0 )
        {
            OGRFieldDefn oFieldDefn(m_poCSVLayer->GetYField(), OFTReal);
            if( eErr == OGRERR_NONE )
                eErr = poCSVTmpLayer->CreateField(&oFieldDefn);
        }
        if( bHasZ && poCSVTmpLayer->GetLayerDefn()->GetFieldIndex(
                m_poCSVLayer->GetZField()) < 0 )
        {
            OGRFieldDefn oFieldDefn(m_poCSVLayer->GetZField(), OFTReal);
            if( eErr == OGRERR_NONE )
                eErr = poCSVTmpLayer->CreateField(&oFieldDefn);
        }
    }

    int nFirstGeomColIdx = 0;
    if( m_poCSVLayer->HasHiddenWKTColumn() )
    {
        poCSVTmpLayer->SetWriteGeometry(
            poEditableFDefn->GetGeomFieldDefn(0)->GetType(),
            OGR_CSV_GEOM_AS_WKT,
            poEditableFDefn->GetGeomFieldDefn(0)->GetNameRef());
        nFirstGeomColIdx = 1;
    }

    if( !(poEditableFDefn->GetGeomFieldCount() == 1 && bHasXY) )
    {
        for( int i = nFirstGeomColIdx;
             eErr == OGRERR_NONE && i < poEditableFDefn->GetGeomFieldCount();
             i++ )
        {
            OGRGeomFieldDefn oGeomFieldDefn(
                poEditableFDefn->GetGeomFieldDefn(i));
            if( poCSVTmpLayer->GetLayerDefn()->GetGeomFieldIndex(
                    oGeomFieldDefn.GetNameRef()) >= 0 )
                continue;
            eErr = poCSVTmpLayer->CreateGeomField(&oGeomFieldDefn);
        }
    }

    poEditableLayer->ResetReading();

    // Disable all filters.
    const char* pszQueryStringConst = poEditableLayer->GetAttrQueryString();
    char* pszQueryStringBak = pszQueryStringConst ? CPLStrdup(pszQueryStringConst) : nullptr;
    poEditableLayer->SetAttributeFilter(nullptr);

    const int iFilterGeomIndexBak = poEditableLayer->GetGeomFieldFilter();
    OGRGeometry* poFilterGeomBak = poEditableLayer->GetSpatialFilter();
    if( poFilterGeomBak )
        poFilterGeomBak = poFilterGeomBak->clone();
    poEditableLayer->SetSpatialFilter(nullptr);

    auto aoMapSrcToTargetIdx = poCSVTmpLayer->GetLayerDefn()->
        ComputeMapForSetFrom(poEditableLayer->GetLayerDefn(), true);
    aoMapSrcToTargetIdx.push_back(-1); // add dummy entry to be sure that .data() is valid

    for( auto&& poFeature: poEditableLayer )
    {
        if( eErr != OGRERR_NONE )
            break;
        OGRFeature *poNewFeature =
            new OGRFeature(poCSVTmpLayer->GetLayerDefn());
        poNewFeature->SetFrom(poFeature.get(), aoMapSrcToTargetIdx.data(), true);
        if( bHasXY )
        {
            OGRGeometry *poGeom = poFeature->GetGeometryRef();
            if( poGeom != nullptr &&
                wkbFlatten(poGeom->getGeometryType()) == wkbPoint )
            {
                auto poPoint = poGeom->toPoint();
                poNewFeature->SetField(m_poCSVLayer->GetXField(),
                                       poPoint->getX());
                poNewFeature->SetField(m_poCSVLayer->GetYField(),
                                       poPoint->getY());
                if( bHasZ )
                {
                    poNewFeature->SetField(
                        m_poCSVLayer->GetZField(), poPoint->getZ());
                }
            }
        }
        eErr = poCSVTmpLayer->CreateFeature(poNewFeature);
        delete poNewFeature;
    }
    delete poCSVTmpLayer;

    // Restore filters.
    poEditableLayer->SetAttributeFilter(pszQueryStringBak);
    CPLFree(pszQueryStringBak);
    poEditableLayer->SetSpatialFilter(iFilterGeomIndexBak, poFilterGeomBak);
    delete poFilterGeomBak;


    if( eErr != OGRERR_NONE )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error while creating %s",
                 osTmpFilename.c_str());
        VSIUnlink(osTmpFilename);
        VSIUnlink(CPLResetExtension(osTmpFilename, "csvt"));
        return eErr;
    }

    delete m_poCSVLayer;

    if( osFilename != osTmpFilename )
    {
        const CPLString osTmpOriFilename(osFilename + ".ogr_bak");
        const CPLString osTmpOriCSVTFilename(osCSVTFilename + ".ogr_bak");
        if( VSIRename(osFilename, osTmpOriFilename) != 0 ||
            (bHasCSVT &&
             VSIRename(osCSVTFilename, osTmpOriCSVTFilename) != 0) ||
            VSIRename(osTmpFilename, osFilename) != 0 ||
            (bHasCSVT && VSIRename(osTmpCSVTFilename, osCSVTFilename) != 0) )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot rename files");
            *ppoDecoratedLayer = nullptr;
            m_poCSVLayer = nullptr;
            return OGRERR_FAILURE;
        }
        VSIUnlink(osTmpOriFilename);
        if( bHasCSVT )
            VSIUnlink(osTmpOriCSVTFilename);
    }

    VSILFILE *fp = VSIFOpenL(osFilename, "rb+");
    if( fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot reopen updated %s",
                 osFilename.c_str());
        *ppoDecoratedLayer = nullptr;
        m_poCSVLayer = nullptr;
        return OGRERR_FAILURE;
    }

    m_poCSVLayer = new OGRCSVLayer(osLayerName, fp,
                                   osFilename,
                                   false, /* new */
                                   true, /* update */
                                   chDelimiter);
    m_poCSVLayer->BuildFeatureDefn(nullptr, nullptr, m_papszOpenOptions);
    *ppoDecoratedLayer = m_poCSVLayer;

    return OGRERR_NONE;
}

/************************************************************************/
/*                        OGRCSVEditableLayer                           */
/************************************************************************/

class OGRCSVEditableLayer final: public OGREditableLayer
{
    std::set<CPLString> m_oSetFields;

  public:
    OGRCSVEditableLayer(OGRCSVLayer *poCSVLayer, char **papszOpenOptions);

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE ) override;
    virtual OGRErr      DeleteField( int iField ) override;
    virtual OGRErr      AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlagsIn ) override;
    virtual GIntBig     GetFeatureCount( int bForce = TRUE ) override;
};

/************************************************************************/
/*                       GRCSVEditableLayer()                           */
/************************************************************************/

OGRCSVEditableLayer::OGRCSVEditableLayer(OGRCSVLayer *poCSVLayer,
                                         char **papszOpenOptions) :
    OGREditableLayer(poCSVLayer, true,
                     new OGRCSVEditableLayerSynchronizer(
                         poCSVLayer, papszOpenOptions),
                     true)
{
    SetSupportsCreateGeomField(true);
    SetSupportsCurveGeometries(true);
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRCSVEditableLayer::CreateField( OGRFieldDefn *poNewField,
                                         int bApproxOK )

{
    if( m_poEditableFeatureDefn->GetFieldCount() >= 10000 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Limiting to 10000 fields");
        return OGRERR_FAILURE;
    }

    if( m_oSetFields.empty() )
    {
        for( int i = 0; i < m_poEditableFeatureDefn->GetFieldCount(); i++ )
        {
            m_oSetFields.insert(CPLString(
                m_poEditableFeatureDefn->GetFieldDefn(i)->GetNameRef()).toupper());
        }
    }

    const OGRCSVCreateFieldAction eAction = OGRCSVLayer::PreCreateField(
        m_poEditableFeatureDefn, m_oSetFields, poNewField, bApproxOK);
    if( eAction == CREATE_FIELD_DO_NOTHING )
        return OGRERR_NONE;
    if( eAction == CREATE_FIELD_ERROR )
        return OGRERR_FAILURE;
    OGRErr eErr = OGREditableLayer::CreateField(poNewField, bApproxOK);
    if( eErr == OGRERR_NONE )
    {
        m_oSetFields.insert(CPLString(poNewField->GetNameRef()).toupper());
    }
    return eErr;
}

OGRErr OGRCSVEditableLayer::DeleteField( int iField )
{
    m_oSetFields.clear();
    return OGREditableLayer::DeleteField(iField);
}

OGRErr OGRCSVEditableLayer::AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlagsIn )
{
    m_oSetFields.clear();
    return OGREditableLayer::AlterFieldDefn(iField, poNewFieldDefn, nFlagsIn);
}

/************************************************************************/
/*                        GetFeatureCount()                             */
/************************************************************************/

GIntBig OGRCSVEditableLayer::GetFeatureCount( int bForce )
{
    const GIntBig nRet = OGREditableLayer::GetFeatureCount(bForce);
    if( m_poDecoratedLayer != nullptr && m_nNextFID <= 0 )
    {
        const GIntBig nTotalFeatureCount =
            static_cast<OGRCSVLayer *>(m_poDecoratedLayer)
                ->GetTotalFeatureCount();
        if( nTotalFeatureCount >= 0 )
            SetNextFID(nTotalFeatureCount + 1);
    }
    return nRet;
}

/************************************************************************/
/*                          OGRCSVDataSource()                          */
/************************************************************************/

OGRCSVDataSource::OGRCSVDataSource() :
    pszName(nullptr),
    papoLayers(nullptr),
    nLayers(0),
    bUpdate(false),
    bEnableGeometryFields(false)
{}

/************************************************************************/
/*                         ~OGRCSVDataSource()                          */
/************************************************************************/

OGRCSVDataSource::~OGRCSVDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree(papoLayers);

    if( bUpdate )
        OGRCSVDriverRemoveFromMap(pszName, this);

    CPLFree(pszName);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRCSVDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap, ODsCCreateLayer) )
        return bUpdate;
    else if( EQUAL(pszCap, ODsCDeleteLayer) )
        return bUpdate;
    else if( EQUAL(pszCap, ODsCCreateGeomFieldAfterCreateLayer) )
        return bUpdate && bEnableGeometryFields;
    else if( EQUAL(pszCap, ODsCCurveGeometries) )
        return TRUE;
    else if( EQUAL(pszCap, ODsCMeasuredGeometries) )
        return TRUE;
    else if( EQUAL(pszCap, ODsCRandomLayerWrite) )
        return bUpdate;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRCSVDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return nullptr;

    return papoLayers[iLayer];
}

/************************************************************************/
/*                          GetRealExtension()                          */
/************************************************************************/

CPLString OGRCSVDataSource::GetRealExtension(CPLString osFilename)
{
    const CPLString osExt = CPLGetExtension(osFilename);
    if( STARTS_WITH(osFilename, "/vsigzip/") && EQUAL(osExt, "gz") )
    {
        if( osFilename.size() > 7 &&
            EQUAL(osFilename + osFilename.size() - 7, ".csv.gz") )
            return "csv";
        else if( osFilename.size() > 7 &&
                 EQUAL(osFilename + osFilename.size() - 7, ".tsv.gz") )
            return "tsv";
    }
    return osExt;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRCSVDataSource::Open( const char *pszFilename, int bUpdateIn,
                            int bForceOpen, char **papszOpenOptionsIn )

{
    pszName = CPLStrdup(pszFilename);
    bUpdate = CPL_TO_BOOL(bUpdateIn);

    if( bUpdate && bForceOpen && EQUAL(pszFilename, "/vsistdout/") )
        return TRUE;

    // For writable /vsizip/, do nothing more.
    if( bUpdate && bForceOpen && STARTS_WITH(pszFilename, "/vsizip/") )
        return TRUE;

    CPLString osFilename(pszFilename);
    const CPLString osBaseFilename = CPLGetFilename(pszFilename);
    const CPLString osExt = GetRealExtension(osFilename);

    bool bIgnoreExtension = STARTS_WITH_CI(osFilename, "CSV:");
    bool bUSGeonamesFile = false;
    if( bIgnoreExtension )
    {
        osFilename = osFilename + 4;
    }

    // Those are *not* real .XLS files, but text file with tab as column
    // separator.
    if (EQUAL(osBaseFilename, "NfdcFacilities.xls") ||
        EQUAL(osBaseFilename, "NfdcRunways.xls") ||
        EQUAL(osBaseFilename, "NfdcRemarks.xls") ||
        EQUAL(osBaseFilename, "NfdcSchedules.xls"))
    {
        if( bUpdate )
            return FALSE;
        bIgnoreExtension = true;
    }
    else if( (STARTS_WITH_CI(osBaseFilename, "NationalFile_") ||
              STARTS_WITH_CI(osBaseFilename, "POP_PLACES_") ||
              STARTS_WITH_CI(osBaseFilename, "HIST_FEATURES_") ||
              STARTS_WITH_CI(osBaseFilename, "US_CONCISE_") ||
              STARTS_WITH_CI(osBaseFilename, "AllNames_") ||
              STARTS_WITH_CI(osBaseFilename, "Feature_Description_History_") ||
              STARTS_WITH_CI(osBaseFilename, "ANTARCTICA_") ||
              STARTS_WITH_CI(osBaseFilename, "GOVT_UNITS_") ||
              STARTS_WITH_CI(osBaseFilename, "NationalFedCodes_") ||
              STARTS_WITH_CI(osBaseFilename, "AllStates_") ||
              STARTS_WITH_CI(osBaseFilename, "AllStatesFedCodes_") ||
              (osBaseFilename.size() > 2 &&
               STARTS_WITH_CI(osBaseFilename + 2, "_Features_")) ||
              (osBaseFilename.size() > 2 &&
               STARTS_WITH_CI(osBaseFilename + 2, "_FedCodes_"))) &&
             (EQUAL(osExt, "txt") || EQUAL(osExt, "zip")) )
    {
        if( bUpdate )
            return FALSE;
        bIgnoreExtension = true;
        bUSGeonamesFile = true;

        if( EQUAL(osExt, "zip") && strstr(osFilename, "/vsizip/") == nullptr )
        {
            osFilename = "/vsizip/" + osFilename;
        }
    }
    else if (EQUAL(osBaseFilename, "allCountries.txt") ||
             EQUAL(osBaseFilename, "allCountries.zip"))
    {
        if( bUpdate )
            return FALSE;
        bIgnoreExtension = true;

        if( EQUAL(osExt, "zip") && strstr(osFilename, "/vsizip/") == nullptr )
        {
            osFilename = "/vsizip/" + osFilename;
        }
    }

    // Determine what sort of object this is.
    VSIStatBufL sStatBuf;

    if( VSIStatExL(osFilename, &sStatBuf, VSI_STAT_NATURE_FLAG) != 0 )
        return FALSE;

    // Is this a single CSV file?
    if( VSI_ISREG(sStatBuf.st_mode)
        && (bIgnoreExtension || EQUAL(osExt, "csv") || EQUAL(osExt, "tsv")) )
    {
        if (EQUAL(CPLGetFilename(osFilename), "NfdcFacilities.xls"))
        {
            return OpenTable(osFilename, papszOpenOptionsIn, "ARP");
        }
        else if (EQUAL(CPLGetFilename(osFilename), "NfdcRunways.xls"))
        {
            OpenTable(osFilename, papszOpenOptionsIn, "BaseEndPhysical");
            OpenTable(osFilename, papszOpenOptionsIn, "BaseEndDisplaced");
            OpenTable(osFilename, papszOpenOptionsIn, "ReciprocalEndPhysical");
            OpenTable(osFilename, papszOpenOptionsIn,
                      "ReciprocalEndDisplaced");
            return nLayers != 0;
        }
        else if (bUSGeonamesFile)
        {
            // GNIS specific.
            if( STARTS_WITH_CI(osBaseFilename, "NationalFedCodes_") ||
                STARTS_WITH_CI(osBaseFilename, "AllStatesFedCodes_") ||
                STARTS_WITH_CI(osBaseFilename, "ANTARCTICA_") ||
                (osBaseFilename.size() > 2 &&
                 STARTS_WITH_CI(osBaseFilename+2, "_FedCodes_")) )
            {
                OpenTable(osFilename, papszOpenOptionsIn, nullptr, "PRIMARY");
            }
            else if (STARTS_WITH_CI(osBaseFilename, "GOVT_UNITS_") ||
                     STARTS_WITH_CI(osBaseFilename,
                                    "Feature_Description_History_"))
            {
                OpenTable(osFilename, papszOpenOptionsIn, nullptr, "");
            }
            else
            {
                OpenTable(osFilename, papszOpenOptionsIn, nullptr, "PRIM");
                OpenTable(osFilename, papszOpenOptionsIn, nullptr, "SOURCE");
            }
            return nLayers != 0;
        }

        return OpenTable(osFilename, papszOpenOptionsIn);
    }

    // Is this a single a ZIP file with only a CSV file inside?
    if( STARTS_WITH(osFilename, "/vsizip/") &&
        EQUAL(osExt, "zip") &&
        VSI_ISREG(sStatBuf.st_mode) )
    {
        char** papszFiles = VSIReadDir(osFilename);
        if (CSLCount(papszFiles) != 1 ||
            !EQUAL(CPLGetExtension(papszFiles[0]), "CSV"))
        {
            CSLDestroy(papszFiles);
            return FALSE;
        }
        osFilename = CPLFormFilename(osFilename, papszFiles[0], nullptr);
        CSLDestroy(papszFiles);
        return OpenTable(osFilename, papszOpenOptionsIn);
    }

    // Otherwise it has to be a directory.
    if( !VSI_ISDIR(sStatBuf.st_mode) )
        return FALSE;

    // Scan through for entries ending in .csv.
    int nNotCSVCount = 0;
    char **papszNames = VSIReadDir(osFilename);

    for( int i = 0; papszNames != nullptr && papszNames[i] != nullptr; i++ )
    {
        const CPLString oSubFilename =
            CPLFormFilename(osFilename, papszNames[i], nullptr);

        if( EQUAL(papszNames[i], ".") || EQUAL(papszNames[i], "..") )
            continue;

        if (EQUAL(CPLGetExtension(oSubFilename), "csvt"))
            continue;

        if( VSIStatL(oSubFilename, &sStatBuf) != 0 ||
            !VSI_ISREG(sStatBuf.st_mode) )
        {
            nNotCSVCount++;
            continue;
        }

        if (EQUAL(CPLGetExtension(oSubFilename), "csv"))
        {
            if( !OpenTable(oSubFilename, papszOpenOptionsIn) )
            {
                CPLDebug("CSV", "Cannot open %s", oSubFilename.c_str());
                nNotCSVCount++;
                continue;
            }
        }
        // GNIS specific.
        else if( strlen(papszNames[i]) > 2 &&
                 STARTS_WITH_CI(papszNames[i] + 2, "_Features_") &&
                 EQUAL(CPLGetExtension(papszNames[i]), "txt") )
        {
            bool bRet =
                OpenTable(oSubFilename, papszOpenOptionsIn, nullptr, "PRIM");
            bRet |= OpenTable(oSubFilename, papszOpenOptionsIn, nullptr, "SOURCE");
            if( !bRet )
            {
                CPLDebug("CSV", "Cannot open %s", oSubFilename.c_str());
                nNotCSVCount++;
                continue;
            }
        }
        // GNIS specific.
        else if( strlen(papszNames[i]) > 2 &&
                 STARTS_WITH_CI(papszNames[i] + 2, "_FedCodes_") &&
                 EQUAL(CPLGetExtension(papszNames[i]), "txt") )
        {
            if( !OpenTable(oSubFilename, papszOpenOptionsIn, nullptr, "PRIMARY") )
            {
                CPLDebug("CSV", "Cannot open %s", oSubFilename.c_str());
                nNotCSVCount++;
                continue;
            }
        }
        else
        {
            nNotCSVCount++;
            continue;
        }
    }

    CSLDestroy(papszNames);

    // We presume that this is indeed intended to be a CSV
    // datasource if over half the files were .csv files.
    return bForceOpen || nNotCSVCount < nLayers;
}

/************************************************************************/
/*                              OpenTable()                             */
/************************************************************************/

bool OGRCSVDataSource::OpenTable( const char *pszFilename,
                                  char **papszOpenOptionsIn,
                                  const char *pszNfdcRunwaysGeomField,
                                  const char *pszGeonamesGeomFieldPrefix )

{
    // Open the file.
    VSILFILE *fp = nullptr;

    if( bUpdate )
        fp = VSIFOpenExL(pszFilename, "rb+", true);
    else
        fp = VSIFOpenExL(pszFilename, "rb", true);
    if( fp == nullptr )
    {
        CPLError(CE_Warning, CPLE_OpenFailed, "Failed to open %s.",
                 VSIGetLastErrorMsg());
        return false;
    }

    if( !bUpdate && strstr(pszFilename, "/vsigzip/") == nullptr &&
        strstr(pszFilename, "/vsizip/") == nullptr )
        fp = reinterpret_cast<VSILFILE *>(VSICreateBufferedReaderHandle(
            reinterpret_cast<VSIVirtualHandle *>(fp)));

    CPLString osLayerName = CPLGetBasename(pszFilename);
    CPLString osExt = CPLGetExtension(pszFilename);
    if( STARTS_WITH(pszFilename, "/vsigzip/") && EQUAL(osExt, "gz") )
    {
        if( strlen(pszFilename) > 7 &&
            EQUAL(pszFilename + strlen(pszFilename) - 7, ".csv.gz") )
        {
            osLayerName = osLayerName.substr(0, osLayerName.size() - 4);
            osExt = "csv";
        }
        else if( strlen(pszFilename) > 7 &&
                 EQUAL(pszFilename + strlen(pszFilename) - 7, ".tsv.gz") )
        {
            osLayerName = osLayerName.substr(0, osLayerName.size() - 4);
            osExt = "tsv";
        }
    }

    // Read and parse a line.  Did we get multiple fields?

    const char *pszLine = CPLReadLineL(fp);
    if( pszLine == nullptr )
    {
        VSIFCloseL(fp);
        return false;
    }
    char chDelimiter = CSVDetectSeperator(pszLine);
    if( chDelimiter != '\t' && strchr(pszLine, '\t') != nullptr )
    {
        // Force the delimiter to be TAB for a .tsv file that has a tabulation
        // in its first line */
        if( EQUAL(osExt, "tsv") )
        {
            chDelimiter = '\t';
        }
        else
        {
            for( int nDontHonourStrings = 0;
                 nDontHonourStrings <= 1;
                 nDontHonourStrings++ )
            {
                const bool bHonourStrings = !CPL_TO_BOOL(nDontHonourStrings);
                // Read the first 2 lines to see if they have the same number
                // of fields, if using tabulation.
                VSIRewindL(fp);
                char **papszTokens =
                    CSVReadParseLine3L(fp, OGR_CSV_MAX_LINE_SIZE, "\t",
                                       bHonourStrings,
                                       false, // bKeepLeadingAndClosingQuotes
                                       false, // bMergeDelimiter
                                       true // bSkipBOM
                                      );
                const int nTokens1 = CSLCount(papszTokens);
                CSLDestroy(papszTokens);
                papszTokens =
                    CSVReadParseLine3L(fp, OGR_CSV_MAX_LINE_SIZE, "\t",
                                       bHonourStrings,
                                       false, // bKeepLeadingAndClosingQuotes
                                       false, // bMergeDelimiter
                                       true // bSkipBOM
                                      );
                const int nTokens2 = CSLCount(papszTokens);
                CSLDestroy(papszTokens);
                if( nTokens1 >= 2 && nTokens1 == nTokens2 )
                {
                    chDelimiter = '\t';
                    break;
                }
            }
        }
    }

    VSIRewindL(fp);

#if 0
    const char *pszDelimiter = CSLFetchNameValueDef(papszOpenOptionsIn,
                                                    "SEPARATOR", "AUTO");
    if( !EQUAL(pszDelimiter, "AUTO") )
    {
        if (EQUAL(pszDelimiter, "COMMA"))
            chDelimiter = ',';
        else if (EQUAL(pszDelimiter, "SEMICOLON"))
            chDelimiter = ';';
        else if (EQUAL(pszDelimiter, "TAB"))
            chDelimiter = '\t';
        else if (EQUAL(pszDelimiter, "SPACE"))
            chDelimiter = ' ';
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "SEPARATOR=%s not understood, use one of COMMA, "
                     "SEMICOLON, SPACE or TAB.",
                     pszDelimiter);
        }
    }
#endif

    // GNIS specific.
    if( pszGeonamesGeomFieldPrefix != nullptr && strchr(pszLine, '|') != nullptr )
        chDelimiter = '|';

    char szDelimiter[2];
    szDelimiter[0] = chDelimiter;
    szDelimiter[1] = 0;
    char **papszFields = CSVReadParseLine3L(fp, OGR_CSV_MAX_LINE_SIZE,
                                            szDelimiter,
                                            true, // bHonourStrings,
                                            false, // bKeepLeadingAndClosingQuotes
                                            false, // bMergeDelimiter
                                            true // bSkipBOM
                                           );

    if( CSLCount(papszFields) < 2 )
    {
        VSIFCloseL(fp);
        CSLDestroy(papszFields);
        return false;
    }

    VSIRewindL(fp);
    CSLDestroy(papszFields);

    // Create a layer.
    nLayers++;
    papoLayers = static_cast<OGRLayer **>(
        CPLRealloc(papoLayers, sizeof(void *) * nLayers));

    if (pszNfdcRunwaysGeomField != nullptr)
    {
        osLayerName += "_";
        osLayerName += pszNfdcRunwaysGeomField;
    }
    else if (pszGeonamesGeomFieldPrefix != nullptr &&
             !EQUAL(pszGeonamesGeomFieldPrefix, ""))
    {
        osLayerName += "_";
        osLayerName += pszGeonamesGeomFieldPrefix;
    }
    if (EQUAL(pszFilename, "/vsistdin/"))
        osLayerName = "layer";

    OGRCSVLayer *poCSVLayer = new OGRCSVLayer(osLayerName, fp, pszFilename,
                                              FALSE, bUpdate, chDelimiter);
    poCSVLayer->BuildFeatureDefn(pszNfdcRunwaysGeomField,
                                 pszGeonamesGeomFieldPrefix,
                                 papszOpenOptionsIn);
    OGRLayer *poLayer = poCSVLayer;
    if( bUpdate )
    {
        poLayer = new OGRCSVEditableLayer(poCSVLayer, papszOpenOptionsIn);
    }
    papoLayers[nLayers - 1] = poLayer;

    return true;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRCSVDataSource::ICreateLayer( const char *pszLayerName,
                                OGRSpatialReference *poSpatialRef,
                                OGRwkbGeometryType eGType,
                                char **papszOptions )
{
    // Verify we are in update mode.
    if( !bUpdate )
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Data source %s opened read-only.\n"
                 "New layer %s cannot be created.",
                 pszName, pszLayerName);

        return nullptr;
    }

    // Verify that the datasource is a directory.
    VSIStatBufL sStatBuf;

    if( STARTS_WITH(pszName, "/vsizip/"))
    {
        // Do nothing.
    }
    else if( !EQUAL(pszName, "/vsistdout/") &&
             (VSIStatL(pszName, &sStatBuf) != 0 ||
              !VSI_ISDIR(sStatBuf.st_mode)) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attempt to create csv layer (file) against a "
                 "non-directory datasource.");
        return nullptr;
    }

    // What filename would we use?
    CPLString osFilename;

    if( osDefaultCSVName != "" )
    {
        osFilename = CPLFormFilename(pszName, osDefaultCSVName, nullptr);
        osDefaultCSVName = "";
    }
    else
    {
        osFilename = CPLFormFilename(pszName, pszLayerName, "csv");
    }

    // Does this directory/file already exist?
    if( VSIStatL(osFilename, &sStatBuf) == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attempt to create layer %s, but %s already exists.",
                 pszLayerName, osFilename.c_str());
        return nullptr;
    }

    // Create the empty file.

    const char *pszDelimiter = CSLFetchNameValue(papszOptions, "SEPARATOR");
    char chDelimiter = ',';
    if (pszDelimiter != nullptr)
    {
        if (EQUAL(pszDelimiter, "COMMA"))
            chDelimiter = ',';
        else if (EQUAL(pszDelimiter, "SEMICOLON"))
            chDelimiter = ';';
        else if (EQUAL(pszDelimiter, "TAB"))
            chDelimiter = '\t';
        else if (EQUAL(pszDelimiter, "SPACE"))
            chDelimiter = ' ';
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "SEPARATOR=%s not understood, use one of "
                     "COMMA, SEMICOLON, SPACE or TAB.",
                     pszDelimiter);
        }
    }

    // Create a layer.

    OGRCSVLayer *poCSVLayer = new OGRCSVLayer(pszLayerName, nullptr, osFilename,
                                              true, true, chDelimiter);

    poCSVLayer->BuildFeatureDefn();

    // Was a particular CRLF order requested?
    const char *pszCRLFFormat = CSLFetchNameValue(papszOptions, "LINEFORMAT");
    bool bUseCRLF = false;

    if( pszCRLFFormat == nullptr )
    {
#ifdef WIN32
        bUseCRLF = true;
#endif
    }
    else if( EQUAL(pszCRLFFormat, "CRLF") )
    {
        bUseCRLF = true;
    }
    else if( !EQUAL(pszCRLFFormat, "LF") )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "LINEFORMAT=%s not understood, use one of CRLF or LF.",
                 pszCRLFFormat);
#ifdef WIN32
        bUseCRLF = true;
#endif
    }

    poCSVLayer->SetCRLF(bUseCRLF);

    const char* pszStringQuoting =
        CSLFetchNameValueDef(papszOptions, "STRING_QUOTING", "IF_AMBIGUOUS");
    poCSVLayer->SetStringQuoting(
        EQUAL(pszStringQuoting, "IF_NEEDED") ? OGRCSVLayer::StringQuoting::IF_NEEDED:
        EQUAL(pszStringQuoting, "ALWAYS") ?    OGRCSVLayer::StringQuoting::ALWAYS:
                                               OGRCSVLayer::StringQuoting::IF_AMBIGUOUS
    );

    // Should we write the geometry?
    const char *pszGeometry = CSLFetchNameValue(papszOptions, "GEOMETRY");
    if( bEnableGeometryFields )
    {
        poCSVLayer->SetWriteGeometry(
            eGType, OGR_CSV_GEOM_AS_WKT,
            CSLFetchNameValueDef(papszOptions, "GEOMETRY_NAME", "WKT"));
    }
    else if( pszGeometry != nullptr )
    {
        if( EQUAL(pszGeometry, "AS_WKT") )
        {
            poCSVLayer->SetWriteGeometry(
                eGType, OGR_CSV_GEOM_AS_WKT,
                CSLFetchNameValueDef(papszOptions, "GEOMETRY_NAME", "WKT"));
        }
        else if( EQUAL(pszGeometry, "AS_XYZ") ||
                 EQUAL(pszGeometry, "AS_XY") ||
                 EQUAL(pszGeometry, "AS_YX") )
        {
            if( eGType == wkbUnknown || wkbFlatten(eGType) == wkbPoint )
            {
                poCSVLayer->SetWriteGeometry(
                    eGType,
                    EQUAL(pszGeometry, "AS_XYZ") ? OGR_CSV_GEOM_AS_XYZ :
                    EQUAL(pszGeometry, "AS_XY") ?  OGR_CSV_GEOM_AS_XY :
                    OGR_CSV_GEOM_AS_YX);
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Geometry type %s is not compatible with "
                         "GEOMETRY=AS_XYZ.",
                         OGRGeometryTypeToName(eGType));
            }
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Unsupported value %s for creation option GEOMETRY",
                     pszGeometry);
        }
    }

    // Should we create a CSVT file?
    const char *pszCreateCSVT = CSLFetchNameValue(papszOptions, "CREATE_CSVT");
    if( pszCreateCSVT && CPLTestBool(pszCreateCSVT) )
    {
        poCSVLayer->SetCreateCSVT(true);

        // Create .prj file.
        if( poSpatialRef != nullptr && osFilename != "/vsistdout/" )
        {
            char *pszWKT = nullptr;
            poSpatialRef->exportToWkt(&pszWKT);
            if( pszWKT )
            {
                VSILFILE *fpPRJ =
                    VSIFOpenL(CPLResetExtension(osFilename, "prj"), "wb");
                if( fpPRJ )
                {
                    CPL_IGNORE_RET_VAL(VSIFPrintfL(fpPRJ, "%s\n", pszWKT));
                    VSIFCloseL(fpPRJ);
                }
                CPLFree(pszWKT);
            }
        }
    }

    // Should we write a UTF8 BOM?
    const char *pszWriteBOM = CSLFetchNameValue(papszOptions, "WRITE_BOM");
    if( pszWriteBOM )
        poCSVLayer->SetWriteBOM(CPLTestBool(pszWriteBOM));

    nLayers++;
    papoLayers = static_cast<OGRLayer **>(
        CPLRealloc(papoLayers, sizeof(void *) * nLayers));
    OGRLayer *poLayer = poCSVLayer;
    if( osFilename != "/vsistdout/" )
        poLayer = new OGRCSVEditableLayer(poCSVLayer, nullptr);
    papoLayers[nLayers - 1] = poLayer;
    return poLayer;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGRCSVDataSource::DeleteLayer( int iLayer )

{
    // Verify we are in update mode.
    if( !bUpdate )
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Data source %s opened read-only.\n"
                 "Layer %d cannot be deleted.",
                 pszName, iLayer);

        return OGRERR_FAILURE;
    }

    if( iLayer < 0 || iLayer >= nLayers )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Layer %d not in legal range of 0 to %d.",
                 iLayer, nLayers - 1);
        return OGRERR_FAILURE;
    }

    char *pszFilename = CPLStrdup(CPLFormFilename(
        pszName, papoLayers[iLayer]->GetLayerDefn()->GetName(), "csv"));
    char *pszFilenameCSVT = CPLStrdup(CPLFormFilename(
        pszName, papoLayers[iLayer]->GetLayerDefn()->GetName(), "csvt"));

    delete papoLayers[iLayer];

    while( iLayer < nLayers - 1 )
    {
        papoLayers[iLayer] = papoLayers[iLayer + 1];
        iLayer++;
    }

    nLayers--;

    VSIUnlink(pszFilename);
    CPLFree(pszFilename);
    VSIUnlink(pszFilenameCSVT);
    CPLFree(pszFilenameCSVT);

    return OGRERR_NONE;
}

/************************************************************************/
/*                       CreateForSingleFile()                          */
/************************************************************************/

void OGRCSVDataSource::CreateForSingleFile( const char *pszDirname,
                                            const char *pszFilename )
{
    pszName = CPLStrdup(pszDirname);
    bUpdate = true;
    osDefaultCSVName = CPLGetFilename(pszFilename);
}
