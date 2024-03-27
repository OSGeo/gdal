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

/************************************************************************/
/*                     OGRCSVEditableLayerSynchronizer                  */
/************************************************************************/

class OGRCSVEditableLayerSynchronizer final
    : public IOGREditableLayerSynchronizer
{
    OGRCSVLayer *m_poCSVLayer;
    char **m_papszOpenOptions;

  public:
    OGRCSVEditableLayerSynchronizer(OGRCSVLayer *poCSVLayer,
                                    char **papszOpenOptions)
        : m_poCSVLayer(poCSVLayer),
          m_papszOpenOptions(CSLDuplicate(papszOpenOptions))
    {
    }

    virtual ~OGRCSVEditableLayerSynchronizer() override;

    virtual OGRErr EditableSyncToDisk(OGRLayer *poEditableLayer,
                                      OGRLayer **ppoDecoratedLayer) override;

    std::vector<std::string> GetFileList()
    {
        return m_poCSVLayer->GetFileList();
    }
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

    GDALDataset *poDS = m_poCSVLayer->GetDataset();
    const CPLString osLayerName(m_poCSVLayer->GetName());
    const CPLString osFilename(m_poCSVLayer->GetFilename());
    const bool bCreateCSVT = m_poCSVLayer->GetCreateCSVT();
    const CPLString osCSVTFilename(CPLResetExtension(osFilename, "csvt"));
    VSIStatBufL sStatBuf;
    const bool bHasCSVT = VSIStatL(osCSVTFilename, &sStatBuf) == 0;
    CPLString osTmpFilename(osFilename);
    CPLString osTmpCSVTFilename(osFilename);
    if (VSIStatL(osFilename, &sStatBuf) == 0)
    {
        osTmpFilename += "_ogr_tmp.csv";
        osTmpCSVTFilename += "_ogr_tmp.csvt";
    }
    const char chDelimiter = m_poCSVLayer->GetDelimiter();
    OGRCSVLayer *poCSVTmpLayer = new OGRCSVLayer(
        poDS, osLayerName, nullptr, -1, osTmpFilename, true, true, chDelimiter);
    poCSVTmpLayer->BuildFeatureDefn(nullptr, nullptr, m_papszOpenOptions);
    poCSVTmpLayer->SetCRLF(m_poCSVLayer->GetCRLF());
    poCSVTmpLayer->SetCreateCSVT(bCreateCSVT || bHasCSVT);
    poCSVTmpLayer->SetWriteBOM(m_poCSVLayer->GetWriteBOM());
    poCSVTmpLayer->SetStringQuoting(m_poCSVLayer->GetStringQuoting());

    if (m_poCSVLayer->GetGeometryFormat() == OGR_CSV_GEOM_AS_WKT)
        poCSVTmpLayer->SetWriteGeometry(wkbNone, OGR_CSV_GEOM_AS_WKT, nullptr);

    const bool bKeepGeomColmuns =
        CPLFetchBool(m_papszOpenOptions, "KEEP_GEOM_COLUMNS", true);

    OGRErr eErr = OGRERR_NONE;
    OGRFeatureDefn *poEditableFDefn = poEditableLayer->GetLayerDefn();
    for (int i = 0; eErr == OGRERR_NONE && i < poEditableFDefn->GetFieldCount();
         i++)
    {
        OGRFieldDefn oFieldDefn(poEditableFDefn->GetFieldDefn(i));
        int iGeomFieldIdx = 0;
        if ((EQUAL(oFieldDefn.GetNameRef(), "WKT") &&
             (iGeomFieldIdx = poEditableFDefn->GetGeomFieldIndex("")) >= 0) ||
            (bKeepGeomColmuns &&
             (iGeomFieldIdx = poEditableFDefn->GetGeomFieldIndex(
                  (std::string("geom_") + oFieldDefn.GetNameRef()).c_str())) >=
                 0))
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
    if (bHasXY && !bKeepGeomColmuns)
    {
        if (poCSVTmpLayer->GetLayerDefn()->GetFieldIndex(
                m_poCSVLayer->GetXField()) < 0)
        {
            OGRFieldDefn oFieldDefn(m_poCSVLayer->GetXField(), OFTReal);
            if (eErr == OGRERR_NONE)
                eErr = poCSVTmpLayer->CreateField(&oFieldDefn);
        }
        if (poCSVTmpLayer->GetLayerDefn()->GetFieldIndex(
                m_poCSVLayer->GetYField()) < 0)
        {
            OGRFieldDefn oFieldDefn(m_poCSVLayer->GetYField(), OFTReal);
            if (eErr == OGRERR_NONE)
                eErr = poCSVTmpLayer->CreateField(&oFieldDefn);
        }
        if (bHasZ && poCSVTmpLayer->GetLayerDefn()->GetFieldIndex(
                         m_poCSVLayer->GetZField()) < 0)
        {
            OGRFieldDefn oFieldDefn(m_poCSVLayer->GetZField(), OFTReal);
            if (eErr == OGRERR_NONE)
                eErr = poCSVTmpLayer->CreateField(&oFieldDefn);
        }
    }

    int nFirstGeomColIdx = 0;
    if (m_poCSVLayer->HasHiddenWKTColumn())
    {
        poCSVTmpLayer->SetWriteGeometry(
            poEditableFDefn->GetGeomFieldDefn(0)->GetType(),
            OGR_CSV_GEOM_AS_WKT,
            poEditableFDefn->GetGeomFieldDefn(0)->GetNameRef());
        nFirstGeomColIdx = 1;
    }

    if (!(poEditableFDefn->GetGeomFieldCount() == 1 && bHasXY))
    {
        for (int i = nFirstGeomColIdx;
             eErr == OGRERR_NONE && i < poEditableFDefn->GetGeomFieldCount();
             i++)
        {
            OGRGeomFieldDefn oGeomFieldDefn(
                poEditableFDefn->GetGeomFieldDefn(i));
            if (poCSVTmpLayer->GetLayerDefn()->GetGeomFieldIndex(
                    oGeomFieldDefn.GetNameRef()) >= 0)
                continue;
            eErr = poCSVTmpLayer->CreateGeomField(&oGeomFieldDefn);
        }
    }

    poEditableLayer->ResetReading();

    // Disable all filters.
    const char *pszQueryStringConst = poEditableLayer->GetAttrQueryString();
    char *pszQueryStringBak =
        pszQueryStringConst ? CPLStrdup(pszQueryStringConst) : nullptr;
    poEditableLayer->SetAttributeFilter(nullptr);

    const int iFilterGeomIndexBak = poEditableLayer->GetGeomFieldFilter();
    OGRGeometry *poFilterGeomBak = poEditableLayer->GetSpatialFilter();
    if (poFilterGeomBak)
        poFilterGeomBak = poFilterGeomBak->clone();
    poEditableLayer->SetSpatialFilter(nullptr);

    auto aoMapSrcToTargetIdx =
        poCSVTmpLayer->GetLayerDefn()->ComputeMapForSetFrom(
            poEditableLayer->GetLayerDefn(), true);
    aoMapSrcToTargetIdx.push_back(
        -1);  // add dummy entry to be sure that .data() is valid

    for (auto &&poFeature : poEditableLayer)
    {
        if (eErr != OGRERR_NONE)
            break;
        OGRFeature *poNewFeature =
            new OGRFeature(poCSVTmpLayer->GetLayerDefn());
        poNewFeature->SetFrom(poFeature.get(), aoMapSrcToTargetIdx.data(),
                              true);
        if (bHasXY)
        {
            OGRGeometry *poGeom = poFeature->GetGeometryRef();
            if (poGeom != nullptr &&
                wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
            {
                auto poPoint = poGeom->toPoint();
                poNewFeature->SetField(m_poCSVLayer->GetXField(),
                                       poPoint->getX());
                poNewFeature->SetField(m_poCSVLayer->GetYField(),
                                       poPoint->getY());
                if (bHasZ)
                {
                    poNewFeature->SetField(m_poCSVLayer->GetZField(),
                                           poPoint->getZ());
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

    if (eErr != OGRERR_NONE)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error while creating %s",
                 osTmpFilename.c_str());
        VSIUnlink(osTmpFilename);
        VSIUnlink(CPLResetExtension(osTmpFilename, "csvt"));
        return eErr;
    }

    delete m_poCSVLayer;

    if (osFilename != osTmpFilename)
    {
        const CPLString osTmpOriFilename(osFilename + ".ogr_bak");
        const CPLString osTmpOriCSVTFilename(osCSVTFilename + ".ogr_bak");
        if (VSIRename(osFilename, osTmpOriFilename) != 0 ||
            (bHasCSVT &&
             VSIRename(osCSVTFilename, osTmpOriCSVTFilename) != 0) ||
            VSIRename(osTmpFilename, osFilename) != 0 ||
            (bHasCSVT && VSIRename(osTmpCSVTFilename, osCSVTFilename) != 0))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot rename files");
            *ppoDecoratedLayer = nullptr;
            m_poCSVLayer = nullptr;
            return OGRERR_FAILURE;
        }
        VSIUnlink(osTmpOriFilename);
        if (bHasCSVT)
            VSIUnlink(osTmpOriCSVTFilename);
    }

    VSILFILE *fp = VSIFOpenL(osFilename, "rb+");
    if (fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot reopen updated %s",
                 osFilename.c_str());
        *ppoDecoratedLayer = nullptr;
        m_poCSVLayer = nullptr;
        return OGRERR_FAILURE;
    }

    m_poCSVLayer =
        new OGRCSVLayer(poDS, osLayerName, fp, -1, osFilename, false, /* new */
                        true, /* update */
                        chDelimiter);
    m_poCSVLayer->BuildFeatureDefn(nullptr, nullptr, m_papszOpenOptions);
    *ppoDecoratedLayer = m_poCSVLayer;

    return OGRERR_NONE;
}

/************************************************************************/
/*                        OGRCSVEditableLayer                           */
/************************************************************************/

class OGRCSVEditableLayer final : public IOGRCSVLayer, public OGREditableLayer
{
    std::set<CPLString> m_oSetFields;

  public:
    OGRCSVEditableLayer(OGRCSVLayer *poCSVLayer, char **papszOpenOptions);

    OGRLayer *GetLayer() override
    {
        return this;
    }

    std::vector<std::string> GetFileList() override
    {
        return cpl::down_cast<OGRCSVEditableLayerSynchronizer *>(
                   m_poSynchronizer)
            ->GetFileList();
    }

    virtual OGRErr CreateField(const OGRFieldDefn *poField,
                               int bApproxOK = TRUE) override;
    virtual OGRErr DeleteField(int iField) override;
    virtual OGRErr AlterFieldDefn(int iField, OGRFieldDefn *poNewFieldDefn,
                                  int nFlagsIn) override;
    virtual GIntBig GetFeatureCount(int bForce = TRUE) override;
};

/************************************************************************/
/*                       GRCSVEditableLayer()                           */
/************************************************************************/

OGRCSVEditableLayer::OGRCSVEditableLayer(OGRCSVLayer *poCSVLayer,
                                         char **papszOpenOptions)
    : OGREditableLayer(
          poCSVLayer, true,
          new OGRCSVEditableLayerSynchronizer(poCSVLayer, papszOpenOptions),
          true)
{
    SetSupportsCreateGeomField(true);
    SetSupportsCurveGeometries(true);
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRCSVEditableLayer::CreateField(const OGRFieldDefn *poNewField,
                                        int bApproxOK)

{
    if (m_poEditableFeatureDefn->GetFieldCount() >= 10000)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Limiting to 10000 fields");
        return OGRERR_FAILURE;
    }

    if (m_oSetFields.empty())
    {
        for (int i = 0; i < m_poEditableFeatureDefn->GetFieldCount(); i++)
        {
            m_oSetFields.insert(
                CPLString(
                    m_poEditableFeatureDefn->GetFieldDefn(i)->GetNameRef())
                    .toupper());
        }
    }

    const OGRCSVCreateFieldAction eAction = OGRCSVLayer::PreCreateField(
        m_poEditableFeatureDefn, m_oSetFields, poNewField, bApproxOK);
    if (eAction == CREATE_FIELD_DO_NOTHING)
        return OGRERR_NONE;
    if (eAction == CREATE_FIELD_ERROR)
        return OGRERR_FAILURE;
    OGRErr eErr = OGREditableLayer::CreateField(poNewField, bApproxOK);
    if (eErr == OGRERR_NONE)
    {
        m_oSetFields.insert(CPLString(poNewField->GetNameRef()).toupper());
    }
    return eErr;
}

OGRErr OGRCSVEditableLayer::DeleteField(int iField)
{
    m_oSetFields.clear();
    return OGREditableLayer::DeleteField(iField);
}

OGRErr OGRCSVEditableLayer::AlterFieldDefn(int iField,
                                           OGRFieldDefn *poNewFieldDefn,
                                           int nFlagsIn)
{
    m_oSetFields.clear();
    return OGREditableLayer::AlterFieldDefn(iField, poNewFieldDefn, nFlagsIn);
}

/************************************************************************/
/*                        GetFeatureCount()                             */
/************************************************************************/

GIntBig OGRCSVEditableLayer::GetFeatureCount(int bForce)
{
    const GIntBig nRet = OGREditableLayer::GetFeatureCount(bForce);
    if (m_poDecoratedLayer != nullptr && m_nNextFID <= 0)
    {
        const GIntBig nTotalFeatureCount =
            static_cast<OGRCSVLayer *>(m_poDecoratedLayer)
                ->GetTotalFeatureCount();
        if (nTotalFeatureCount >= 0)
            SetNextFID(nTotalFeatureCount + 1);
    }
    return nRet;
}

/************************************************************************/
/*                          OGRCSVDataSource()                          */
/************************************************************************/

OGRCSVDataSource::OGRCSVDataSource() = default;

/************************************************************************/
/*                         ~OGRCSVDataSource()                          */
/************************************************************************/

OGRCSVDataSource::~OGRCSVDataSource()

{
    m_apoLayers.clear();

    if (bUpdate)
        OGRCSVDriverRemoveFromMap(pszName, this);

    CPLFree(pszName);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRCSVDataSource::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, ODsCCreateLayer))
        return bUpdate;
    else if (EQUAL(pszCap, ODsCDeleteLayer))
        return bUpdate;
    else if (EQUAL(pszCap, ODsCCreateGeomFieldAfterCreateLayer))
        return bUpdate && bEnableGeometryFields;
    else if (EQUAL(pszCap, ODsCCurveGeometries))
        return TRUE;
    else if (EQUAL(pszCap, ODsCMeasuredGeometries))
        return TRUE;
    else if (EQUAL(pszCap, ODsCZGeometries))
        return TRUE;
    else if (EQUAL(pszCap, ODsCRandomLayerWrite))
        return bUpdate;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRCSVDataSource::GetLayer(int iLayer)

{
    if (iLayer < 0 || iLayer >= static_cast<int>(m_apoLayers.size()))
        return nullptr;

    return m_apoLayers[iLayer]->GetLayer();
}

/************************************************************************/
/*                          GetRealExtension()                          */
/************************************************************************/

CPLString OGRCSVDataSource::GetRealExtension(CPLString osFilename)
{
    const CPLString osExt = CPLGetExtension(osFilename);
    if (STARTS_WITH(osFilename, "/vsigzip/") && EQUAL(osExt, "gz"))
    {
        if (osFilename.size() > 7 &&
            EQUAL(osFilename + osFilename.size() - 7, ".csv.gz"))
            return "csv";
        else if (osFilename.size() > 7 &&
                 EQUAL(osFilename + osFilename.size() - 7, ".tsv.gz"))
            return "tsv";
        else if (osFilename.size() > 7 &&
                 EQUAL(osFilename + osFilename.size() - 7, ".psv.gz"))
            return "psv";
    }
    return osExt;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRCSVDataSource::Open(const char *pszFilename, int bUpdateIn,
                           int bForceOpen, char **papszOpenOptionsIn)

{
    pszName = CPLStrdup(pszFilename);
    bUpdate = CPL_TO_BOOL(bUpdateIn);

    if (bUpdate && bForceOpen && EQUAL(pszFilename, "/vsistdout/"))
        return TRUE;

    // For writable /vsizip/, do nothing more.
    if (bUpdate && bForceOpen && STARTS_WITH(pszFilename, "/vsizip/"))
        return TRUE;

    CPLString osFilename(pszFilename);
    const CPLString osBaseFilename = CPLGetFilename(pszFilename);
    const CPLString osExt = GetRealExtension(osFilename);

    bool bIgnoreExtension = STARTS_WITH_CI(osFilename, "CSV:");
    bool bUSGeonamesFile = false;
    if (bIgnoreExtension)
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
        if (bUpdate)
            return FALSE;
        bIgnoreExtension = true;
    }
    else if ((STARTS_WITH_CI(osBaseFilename, "NationalFile_") ||
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
             (EQUAL(osExt, "txt") || EQUAL(osExt, "zip")))
    {
        if (bUpdate)
            return FALSE;
        bIgnoreExtension = true;
        bUSGeonamesFile = true;

        if (EQUAL(osExt, "zip") && strstr(osFilename, "/vsizip/") == nullptr)
        {
            osFilename = "/vsizip/" + osFilename;
        }
    }
    else if (EQUAL(osBaseFilename, "allCountries.txt") ||
             EQUAL(osBaseFilename, "allCountries.zip"))
    {
        if (bUpdate)
            return FALSE;
        bIgnoreExtension = true;

        if (EQUAL(osExt, "zip") && strstr(osFilename, "/vsizip/") == nullptr)
        {
            osFilename = "/vsizip/" + osFilename;
        }
    }

    // Determine what sort of object this is.
    VSIStatBufL sStatBuf;

    if (VSIStatExL(osFilename, &sStatBuf, VSI_STAT_NATURE_FLAG) != 0)
        return FALSE;

    // Is this a single CSV file?
    if (VSI_ISREG(sStatBuf.st_mode) &&
        (bIgnoreExtension || EQUAL(osExt, "csv") || EQUAL(osExt, "tsv") ||
         EQUAL(osExt, "psv")))
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
            OpenTable(osFilename, papszOpenOptionsIn, "ReciprocalEndDisplaced");
            return !m_apoLayers.empty();
        }
        else if (bUSGeonamesFile)
        {
            // GNIS specific.
            if (STARTS_WITH_CI(osBaseFilename, "NationalFedCodes_") ||
                STARTS_WITH_CI(osBaseFilename, "AllStatesFedCodes_") ||
                STARTS_WITH_CI(osBaseFilename, "ANTARCTICA_") ||
                (osBaseFilename.size() > 2 &&
                 STARTS_WITH_CI(osBaseFilename + 2, "_FedCodes_")))
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
            return !m_apoLayers.empty();
        }

        return OpenTable(osFilename, papszOpenOptionsIn);
    }

    // Is this a single a ZIP file with only a CSV file inside?
    if (STARTS_WITH(osFilename, "/vsizip/") && EQUAL(osExt, "zip") &&
        VSI_ISREG(sStatBuf.st_mode))
    {
        char **papszFiles = VSIReadDir(osFilename);
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
    if (!VSI_ISDIR(sStatBuf.st_mode))
        return FALSE;

    // Scan through for entries ending in .csv.
    int nNotCSVCount = 0;
    char **papszNames = VSIReadDir(osFilename);

    for (int i = 0; papszNames != nullptr && papszNames[i] != nullptr; i++)
    {
        const CPLString oSubFilename =
            CPLFormFilename(osFilename, papszNames[i], nullptr);

        if (EQUAL(papszNames[i], ".") || EQUAL(papszNames[i], ".."))
            continue;

        if (EQUAL(CPLGetExtension(oSubFilename), "csvt"))
            continue;

        if (VSIStatL(oSubFilename, &sStatBuf) != 0 ||
            !VSI_ISREG(sStatBuf.st_mode))
        {
            nNotCSVCount++;
            continue;
        }

        if (EQUAL(CPLGetExtension(oSubFilename), "csv"))
        {
            if (!OpenTable(oSubFilename, papszOpenOptionsIn))
            {
                CPLDebug("CSV", "Cannot open %s", oSubFilename.c_str());
                nNotCSVCount++;
                continue;
            }
        }
        // GNIS specific.
        else if (strlen(papszNames[i]) > 2 &&
                 STARTS_WITH_CI(papszNames[i] + 2, "_Features_") &&
                 EQUAL(CPLGetExtension(papszNames[i]), "txt"))
        {
            bool bRet =
                OpenTable(oSubFilename, papszOpenOptionsIn, nullptr, "PRIM");
            bRet |=
                OpenTable(oSubFilename, papszOpenOptionsIn, nullptr, "SOURCE");
            if (!bRet)
            {
                CPLDebug("CSV", "Cannot open %s", oSubFilename.c_str());
                nNotCSVCount++;
                continue;
            }
        }
        // GNIS specific.
        else if (strlen(papszNames[i]) > 2 &&
                 STARTS_WITH_CI(papszNames[i] + 2, "_FedCodes_") &&
                 EQUAL(CPLGetExtension(papszNames[i]), "txt"))
        {
            if (!OpenTable(oSubFilename, papszOpenOptionsIn, nullptr,
                           "PRIMARY"))
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
    return bForceOpen || nNotCSVCount < GetLayerCount();
}

/************************************************************************/
/*                              OpenTable()                             */
/************************************************************************/

bool OGRCSVDataSource::OpenTable(const char *pszFilename,
                                 char **papszOpenOptionsIn,
                                 const char *pszNfdcRunwaysGeomField,
                                 const char *pszGeonamesGeomFieldPrefix)

{
    // Open the file.
    VSILFILE *fp = nullptr;

    if (bUpdate)
        fp = VSIFOpenExL(pszFilename, "rb+", true);
    else
        fp = VSIFOpenExL(pszFilename, "rb", true);
    if (fp == nullptr)
    {
        CPLError(CE_Warning, CPLE_OpenFailed, "Failed to open %s.",
                 VSIGetLastErrorMsg());
        return false;
    }

    if (!bUpdate && strstr(pszFilename, "/vsigzip/") == nullptr &&
        strstr(pszFilename, "/vsizip/") == nullptr)
        fp = VSICreateBufferedReaderHandle(fp);

    CPLString osLayerName = CPLGetBasename(pszFilename);
    CPLString osExt = CPLGetExtension(pszFilename);
    if (STARTS_WITH(pszFilename, "/vsigzip/") && EQUAL(osExt, "gz"))
    {
        if (strlen(pszFilename) > 7 &&
            EQUAL(pszFilename + strlen(pszFilename) - 7, ".csv.gz"))
        {
            osLayerName = osLayerName.substr(0, osLayerName.size() - 4);
            osExt = "csv";
        }
        else if (strlen(pszFilename) > 7 &&
                 EQUAL(pszFilename + strlen(pszFilename) - 7, ".tsv.gz"))
        {
            osLayerName = osLayerName.substr(0, osLayerName.size() - 4);
            osExt = "tsv";
        }
        else if (strlen(pszFilename) > 7 &&
                 EQUAL(pszFilename + strlen(pszFilename) - 7, ".psv.gz"))
        {
            osLayerName = osLayerName.substr(0, osLayerName.size() - 4);
            osExt = "psv";
        }
    }

    int nMaxLineSize = atoi(CPLGetConfigOption(
        "OGR_CSV_MAX_LINE_SIZE",
        CSLFetchNameValueDef(papszOpenOptionsIn, "MAX_LINE_SIZE",
                             CPLSPrintf("%d", OGR_CSV_DEFAULT_MAX_LINE_SIZE))));
    size_t nMaxLineSizeAsSize_t = static_cast<size_t>(nMaxLineSize);
    if (nMaxLineSize == 0)
    {
        nMaxLineSize = -1;
        nMaxLineSizeAsSize_t = static_cast<size_t>(-1);
    }

    // Read and parse a line to detect separator.

    std::string osLine;
    {
        const char *pszLine = CPLReadLine2L(fp, nMaxLineSize, nullptr);
        if (pszLine == nullptr)
        {
            VSIFCloseL(fp);
            return false;
        }
        osLine = pszLine;
    }

    char chDelimiter = ',';
    const char *pszDelimiter =
        CSLFetchNameValueDef(papszOpenOptionsIn, "SEPARATOR", "AUTO");
    if (EQUAL(pszDelimiter, "AUTO"))
    {
        chDelimiter = CSVDetectSeperator(osLine.c_str());
        if (chDelimiter != '\t' && osLine.find('\t') != std::string::npos)
        {
            // Force the delimiter to be TAB for a .tsv file that has a tabulation
            // in its first line */
            if (EQUAL(osExt, "tsv"))
            {
                chDelimiter = '\t';
            }
            else
            {
                for (int nDontHonourStrings = 0; nDontHonourStrings <= 1;
                     nDontHonourStrings++)
                {
                    const bool bHonourStrings =
                        !CPL_TO_BOOL(nDontHonourStrings);
                    // Read the first 2 lines to see if they have the same number
                    // of fields, if using tabulation.
                    VSIRewindL(fp);
                    char **papszTokens = CSVReadParseLine3L(
                        fp, nMaxLineSizeAsSize_t, "\t", bHonourStrings,
                        false,  // bKeepLeadingAndClosingQuotes
                        false,  // bMergeDelimiter
                        true    // bSkipBOM
                    );
                    const int nTokens1 = CSLCount(papszTokens);
                    CSLDestroy(papszTokens);
                    papszTokens = CSVReadParseLine3L(
                        fp, nMaxLineSizeAsSize_t, "\t", bHonourStrings,
                        false,  // bKeepLeadingAndClosingQuotes
                        false,  // bMergeDelimiter
                        true    // bSkipBOM
                    );
                    const int nTokens2 = CSLCount(papszTokens);
                    CSLDestroy(papszTokens);
                    if (nTokens1 >= 2 && nTokens1 == nTokens2)
                    {
                        chDelimiter = '\t';
                        break;
                    }
                }
            }
        }

        // GNIS specific.
        if (pszGeonamesGeomFieldPrefix != nullptr &&
            osLine.find('|') != std::string::npos)
            chDelimiter = '|';
    }
    else if (EQUAL(pszDelimiter, "COMMA"))
        chDelimiter = ',';
    else if (EQUAL(pszDelimiter, "SEMICOLON"))
        chDelimiter = ';';
    else if (EQUAL(pszDelimiter, "TAB"))
        chDelimiter = '\t';
    else if (EQUAL(pszDelimiter, "SPACE"))
        chDelimiter = ' ';
    else if (EQUAL(pszDelimiter, "PIPE"))
        chDelimiter = '|';
    else
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "SEPARATOR=%s not understood, use one of COMMA, "
                 "SEMICOLON, TAB, SPACE or PIPE",
                 pszDelimiter);
    }

    VSIRewindL(fp);

    // Create a layer.
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

    auto poCSVLayer =
        std::make_unique<OGRCSVLayer>(this, osLayerName, fp, nMaxLineSize,
                                      pszFilename, FALSE, bUpdate, chDelimiter);
    poCSVLayer->BuildFeatureDefn(pszNfdcRunwaysGeomField,
                                 pszGeonamesGeomFieldPrefix,
                                 papszOpenOptionsIn);
    if (bUpdate)
    {
        m_apoLayers.emplace_back(std::make_unique<OGRCSVEditableLayer>(
            poCSVLayer.release(), papszOpenOptionsIn));
    }
    else
    {
        m_apoLayers.emplace_back(std::move(poCSVLayer));
    }

    return true;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRCSVDataSource::ICreateLayer(const char *pszLayerName,
                               const OGRGeomFieldDefn *poSrcGeomFieldDefn,
                               CSLConstList papszOptions)
{
    // Verify we are in update mode.
    if (!bUpdate)
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Data source %s opened read-only.\n"
                 "New layer %s cannot be created.",
                 pszName, pszLayerName);

        return nullptr;
    }

    const auto eGType =
        poSrcGeomFieldDefn ? poSrcGeomFieldDefn->GetType() : wkbNone;
    const auto poSpatialRef =
        poSrcGeomFieldDefn ? poSrcGeomFieldDefn->GetSpatialRef() : nullptr;

    // Verify that the datasource is a directory.
    VSIStatBufL sStatBuf;

    if (STARTS_WITH(pszName, "/vsizip/"))
    {
        // Do nothing.
    }
    else if (!EQUAL(pszName, "/vsistdout/") &&
             (VSIStatL(pszName, &sStatBuf) != 0 ||
              !VSI_ISDIR(sStatBuf.st_mode)))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attempt to create csv layer (file) against a "
                 "non-directory datasource.");
        return nullptr;
    }

    const bool bCreateCSVT =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "CREATE_CSVT", "NO"));

    // What filename would we use?
    CPLString osFilename;

    if (strcmp(pszName, "/vsistdout/") == 0)
    {
        osFilename = pszName;
        if (bCreateCSVT)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "CREATE_CSVT is not compatible with /vsistdout/ output");
            return nullptr;
        }
    }
    else if (osDefaultCSVName != "")
    {
        osFilename = CPLFormFilename(pszName, osDefaultCSVName, nullptr);
        osDefaultCSVName = "";
    }
    else
    {
        osFilename = CPLFormFilename(pszName, pszLayerName, "csv");
    }

    // Does this directory/file already exist?
    if (VSIStatL(osFilename, &sStatBuf) == 0)
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

    auto poCSVLayer = std::make_unique<OGRCSVLayer>(
        this, pszLayerName, nullptr, -1, osFilename, true, true, chDelimiter);

    poCSVLayer->BuildFeatureDefn();

    // Was a particular CRLF order requested?
    const char *pszCRLFFormat = CSLFetchNameValue(papszOptions, "LINEFORMAT");
    bool bUseCRLF = false;

    if (pszCRLFFormat == nullptr)
    {
#ifdef _WIN32
        bUseCRLF = true;
#endif
    }
    else if (EQUAL(pszCRLFFormat, "CRLF"))
    {
        bUseCRLF = true;
    }
    else if (!EQUAL(pszCRLFFormat, "LF"))
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "LINEFORMAT=%s not understood, use one of CRLF or LF.",
                 pszCRLFFormat);
#ifdef _WIN32
        bUseCRLF = true;
#endif
    }

    poCSVLayer->SetCRLF(bUseCRLF);

    const char *pszStringQuoting =
        CSLFetchNameValueDef(papszOptions, "STRING_QUOTING", "IF_AMBIGUOUS");
    poCSVLayer->SetStringQuoting(
        EQUAL(pszStringQuoting, "IF_NEEDED")
            ? OGRCSVLayer::StringQuoting::IF_NEEDED
        : EQUAL(pszStringQuoting, "ALWAYS")
            ? OGRCSVLayer::StringQuoting::ALWAYS
            : OGRCSVLayer::StringQuoting::IF_AMBIGUOUS);

    // Should we write the geometry?
    const char *pszGeometry = CSLFetchNameValue(papszOptions, "GEOMETRY");
    if (bEnableGeometryFields)
    {
        poCSVLayer->SetWriteGeometry(
            eGType, OGR_CSV_GEOM_AS_WKT,
            CSLFetchNameValueDef(papszOptions, "GEOMETRY_NAME", "WKT"));
    }
    else if (pszGeometry != nullptr)
    {
        if (EQUAL(pszGeometry, "AS_WKT"))
        {
            poCSVLayer->SetWriteGeometry(
                eGType, OGR_CSV_GEOM_AS_WKT,
                CSLFetchNameValueDef(papszOptions, "GEOMETRY_NAME", "WKT"));
        }
        else if (EQUAL(pszGeometry, "AS_XYZ") || EQUAL(pszGeometry, "AS_XY") ||
                 EQUAL(pszGeometry, "AS_YX"))
        {
            if (eGType == wkbUnknown || wkbFlatten(eGType) == wkbPoint)
            {
                poCSVLayer->SetWriteGeometry(
                    eGType, EQUAL(pszGeometry, "AS_XYZ")  ? OGR_CSV_GEOM_AS_XYZ
                            : EQUAL(pszGeometry, "AS_XY") ? OGR_CSV_GEOM_AS_XY
                                                          : OGR_CSV_GEOM_AS_YX);
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
    if (bCreateCSVT)
    {
        poCSVLayer->SetCreateCSVT(true);

        // Create .prj file.
        if (poSpatialRef != nullptr)
        {
            char *pszWKT = nullptr;
            poSpatialRef->exportToWkt(&pszWKT);
            if (pszWKT)
            {
                VSILFILE *fpPRJ =
                    VSIFOpenL(CPLResetExtension(osFilename, "prj"), "wb");
                if (fpPRJ)
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
    if (pszWriteBOM)
        poCSVLayer->SetWriteBOM(CPLTestBool(pszWriteBOM));

    if (poCSVLayer->GetLayerDefn()->GetGeomFieldCount() > 0 &&
        poSrcGeomFieldDefn)
    {
        auto poGeomFieldDefn = poCSVLayer->GetLayerDefn()->GetGeomFieldDefn(0);
        poGeomFieldDefn->SetCoordinatePrecision(
            poSrcGeomFieldDefn->GetCoordinatePrecision());
    }

    if (osFilename != "/vsistdout/")
        m_apoLayers.emplace_back(std::make_unique<OGRCSVEditableLayer>(
            poCSVLayer.release(), nullptr));
    else
        m_apoLayers.emplace_back(std::move(poCSVLayer));

    return m_apoLayers.back()->GetLayer();
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGRCSVDataSource::DeleteLayer(int iLayer)

{
    // Verify we are in update mode.
    if (!bUpdate)
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Data source %s opened read-only.\n"
                 "Layer %d cannot be deleted.",
                 pszName, iLayer);

        return OGRERR_FAILURE;
    }

    if (iLayer < 0 || iLayer >= GetLayerCount())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Layer %d not in legal range of 0 to %d.", iLayer,
                 GetLayerCount() - 1);
        return OGRERR_FAILURE;
    }

    for (const auto &osFilename : m_apoLayers[iLayer]->GetFileList())
    {
        VSIUnlink(osFilename.c_str());
    }

    m_apoLayers.erase(m_apoLayers.begin() + iLayer);

    return OGRERR_NONE;
}

/************************************************************************/
/*                       CreateForSingleFile()                          */
/************************************************************************/

void OGRCSVDataSource::CreateForSingleFile(const char *pszDirname,
                                           const char *pszFilename)
{
    pszName = CPLStrdup(pszDirname);
    bUpdate = true;
    osDefaultCSVName = CPLGetFilename(pszFilename);
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **OGRCSVDataSource::GetFileList()
{
    CPLStringList oFileList;
    for (auto &poLayer : m_apoLayers)
    {
        for (const auto &osFilename : poLayer->GetFileList())
        {
            oFileList.AddString(osFilename.c_str());
        }
    }
    return oFileList.StealList();
}
