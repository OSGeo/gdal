/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Open FileGDB OGR driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
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
#include "ogr_openfilegdb.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "filegdbtable.h"
#include "gdal.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_mem.h"
#include "ogrsf_frmts.h"
#include "ogr_swq.h"

#include "filegdb_fielddomain.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                      OGROpenFileGDBDataSource()                      */
/************************************************************************/
OGROpenFileGDBDataSource::OGROpenFileGDBDataSource() :
    m_pszName(nullptr),
    m_papszFiles(nullptr),
    bLastSQLUsedOptimizedImplementation(false)
{}

/************************************************************************/
/*                     ~OGROpenFileGDBDataSource()                      */
/************************************************************************/
OGROpenFileGDBDataSource::~OGROpenFileGDBDataSource()

{
    for( size_t i = 0; i < m_apoLayers.size(); i++ )
        delete m_apoLayers[i];
    for( size_t i = 0; i < m_apoHiddenLayers.size(); i++ )
        delete m_apoHiddenLayers[i];
    CPLFree(m_pszName);
    CSLDestroy(m_papszFiles);
}

/************************************************************************/
/*                             FileExists()                             */
/************************************************************************/

int OGROpenFileGDBDataSource::FileExists(const char* pszFilename)
{
    if( m_papszFiles )
        return CSLFindString(m_papszFiles, CPLGetFilename(pszFilename)) >= 0;

    VSIStatBufL sStat;
    CPLString osFilename(pszFilename);
    return VSIStatExL(osFilename, &sStat, VSI_STAT_EXISTS_FLAG) == 0;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGROpenFileGDBDataSource::Open( const char* pszFilename )

{
    FileGDBTable oTable;

    m_pszName = CPLStrdup(pszFilename);

    m_osDirName = pszFilename;
    int nInterestTable = 0;
    unsigned int unInterestTable = 0;
    const char* pszFilenameWithoutPath = CPLGetFilename(pszFilename);
    if( strlen(pszFilenameWithoutPath) == strlen("a00000000.gdbtable") &&
        pszFilenameWithoutPath[0] == 'a' &&
        sscanf(pszFilenameWithoutPath, "a%08x.gdbtable", &unInterestTable) == 1 )
    {
        nInterestTable = static_cast<int>(unInterestTable);
        m_osDirName = CPLGetPath(m_osDirName);
    }

    if( EQUAL(CPLGetExtension(m_osDirName), "zip") &&
        !STARTS_WITH(m_osDirName, "/vsizip/") )
    {
        m_osDirName = "/vsizip/" + m_osDirName;
    }
    else  if( EQUAL(CPLGetExtension(m_osDirName), "tar") &&
        !STARTS_WITH(m_osDirName, "/vsitar/") )
    {
        m_osDirName = "/vsitar/" + m_osDirName;
    }

    if( STARTS_WITH(m_osDirName, "/vsizip/") ||
        STARTS_WITH(m_osDirName, "/vsitar/"))
    {
        /* Look for one subdirectory ending with .gdb extension */
        char** papszDir = VSIReadDir(m_osDirName);
        int iCandidate = -1;
        for( int i=0; papszDir && papszDir[i] != nullptr; i++ )
        {
            VSIStatBufL sStat;
            if( EQUAL(CPLGetExtension(papszDir[i]), "gdb") &&
                VSIStatL( CPLSPrintf("%s/%s", m_osDirName.c_str(), papszDir[i]),
                          &sStat ) == 0 &&
                VSI_ISDIR(sStat.st_mode) )
            {
                if( iCandidate < 0 )
                    iCandidate = i;
                else
                {
                    iCandidate = -1;
                    break;
                }
            }
        }
        if( iCandidate >= 0 )
        {
            m_osDirName += "/";
            m_osDirName += papszDir[iCandidate];
        }
        CSLDestroy(papszDir);
    }

    m_papszFiles = VSIReadDir(m_osDirName);

    /* Explore catalog table */
    CPLString osa00000001 =
        CPLFormFilename(m_osDirName, "a00000001", "gdbtable");
    if( !FileExists(osa00000001) || !oTable.Open(osa00000001) )
    {
        if( nInterestTable > 0 && FileExists(m_pszName) )
        {
            const char* pszLyrName = CPLSPrintf("a%08x", nInterestTable);
            OGROpenFileGDBLayer* poLayer = new OGROpenFileGDBLayer(
                                m_pszName, pszLyrName, "", "");
            const char* pszTablX = CPLResetExtension(m_pszName, "gdbtablx");
            if( (!FileExists(pszTablX) &&
                 poLayer->GetLayerDefn()->GetFieldCount() == 0 &&
                 poLayer->GetFeatureCount() == 0) ||
                !poLayer->IsValidLayerDefn() )
            {
                delete poLayer;
                return FALSE;
            }
            m_apoLayers.push_back(poLayer);
            return TRUE;
        }
        return FALSE;
    }

    if( !(oTable.GetFieldCount() >= 2 &&
          oTable.GetTotalRecordCount() < 100000 &&
          oTable.GetField(0)->GetName() == "Name" &&
          oTable.GetField(0)->GetType() == FGFT_STRING &&
          oTable.GetField(1)->GetName() == "FileFormat" &&
          (oTable.GetField(1)->GetType() == FGFT_INT16 ||
           oTable.GetField(1)->GetType() == FGFT_INT32) ) )
    {
        return FALSE;
    }

    int iGDBItems = -1; /* V10 */
    int iGDBFeatureClasses = -1; /* V9.X */
    int iGDBObjectClasses = -1; /* V9.X */

    std::vector<std::string> aosTableNames;
    try
    {
        for( int i=0;i<oTable.GetTotalRecordCount();i++)
        {
            if( !oTable.SelectRow(i) )
            {
                if( oTable.HasGotError() )
                    break;
                aosTableNames.push_back("");
                continue;
            }

            const OGRField* psField = oTable.GetFieldValue(0);
            if( psField != nullptr )
            {
                aosTableNames.push_back(psField->String);

                if( strcmp(psField->String, "GDB_Items") == 0 )
                {
                    iGDBItems = i;
                }
                else if( strcmp(psField->String, "GDB_FeatureClasses") == 0 )
                {
                    iGDBFeatureClasses = i;
                }
                else if( strcmp(psField->String, "GDB_ObjectClasses") == 0 )
                {
                    iGDBObjectClasses = i;
                }
                m_osMapNameToIdx[psField->String] = 1 + i;
            }
            else
            {
                aosTableNames.push_back("");
            }
        }
    }
    catch( const std::exception& )
    {
        return FALSE;
    }

    oTable.Close();

    if( iGDBItems >= 0 )
    {
        int bRet = OpenFileGDBv10(iGDBItems,
                                  nInterestTable);
        if( !bRet )
            return FALSE;
    }
    else if( iGDBFeatureClasses >= 0 && iGDBObjectClasses >= 0 )
    {
        int bRet = OpenFileGDBv9(iGDBFeatureClasses,
                                 iGDBObjectClasses,
                                 nInterestTable);
        if( !bRet )
            return FALSE;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "No GDB_Items nor GDB_FeatureClasses table");
        return FALSE;
    }

    if( m_apoLayers.empty() && nInterestTable > 0 )
    {
        if( FileExists(m_pszName) )
        {
            const char* pszLyrName = nullptr;
            if( nInterestTable <= (int)aosTableNames.size()  &&
                !aosTableNames[nInterestTable-1].empty() )
                pszLyrName = aosTableNames[nInterestTable-1].c_str();
            else
                pszLyrName = CPLSPrintf("a%08x", nInterestTable);
            m_apoLayers.push_back(new OGROpenFileGDBLayer(
                m_pszName, pszLyrName, "", ""));
        }
        else
        {
            return FALSE;
        }
    }

    return TRUE;
}

/***********************************************************************/
/*                             AddLayer()                              */
/***********************************************************************/

OGRLayer* OGROpenFileGDBDataSource::AddLayer( const CPLString& osName,
                                         int nInterestTable,
                                         int& nCandidateLayers,
                                         int& nLayersSDCOrCDF,
                                         const CPLString& osDefinition,
                                         const CPLString& osDocumentation,
                                         const char* pszGeomName,
                                         OGRwkbGeometryType eGeomType )
{
    std::map<std::string, int>::const_iterator oIter =
                                    m_osMapNameToIdx.find(osName);
    int idx = 0;
    if( oIter != m_osMapNameToIdx.end() )
        idx = oIter->second;
    if( idx > 0 && (nInterestTable <= 0 || nInterestTable == idx) )
    {
        m_osMapNameToIdx.erase(osName);

        CPLString osFilename = CPLFormFilename(
            m_osDirName, CPLSPrintf("a%08x", idx), "gdbtable");
        if( FileExists(osFilename) )
        {
            nCandidateLayers ++;

            if( m_papszFiles != nullptr )
            {
                CPLString osSDC = CPLResetExtension(osFilename, "gdbtable.sdc");
                CPLString osCDF = CPLResetExtension(osFilename, "gdbtable.cdf");
                if( FileExists(osSDC) || FileExists(osCDF) )
                {
                    nLayersSDCOrCDF ++;
                    if( GDALGetDriverByName("FileGDB") == nullptr )
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                "%s layer has a %s file whose format is unhandled",
                                osName.c_str(), FileExists(osSDC) ? osSDC.c_str() : osCDF.c_str());
                    }
                    else
                    {
                        CPLDebug("OpenFileGDB",
                                 "%s layer has a %s file whose format is unhandled",
                                  osName.c_str(), FileExists(osSDC) ? osSDC.c_str() : osCDF.c_str());
                    }
                    return nullptr;
                }
            }

            m_apoLayers.push_back(
                new OGROpenFileGDBLayer(osFilename,
                                        osName,
                                        osDefinition,
                                        osDocumentation,
                                        pszGeomName, eGeomType));
            return m_apoLayers.back();
        }
    }
    return nullptr;
}

/***********************************************************************/
/*                      OGROpenFileGDBGroup                            */
/***********************************************************************/

class OGROpenFileGDBGroup final: public GDALGroup
{
protected:
    friend class OGROpenFileGDBDataSource;
    std::vector<std::shared_ptr<GDALGroup>> m_apoSubGroups{};
    std::vector<OGRLayer*> m_apoLayers{};

public:
    OGROpenFileGDBGroup(const std::string& osParentName, const char* pszName):
        GDALGroup(osParentName, pszName) {}

    std::vector<std::string> GetGroupNames(CSLConstList papszOptions) const override;
    std::shared_ptr<GDALGroup> OpenGroup(const std::string& osName,
                                         CSLConstList papszOptions) const override;

    std::vector<std::string> GetVectorLayerNames(CSLConstList papszOptions) const override;
    OGRLayer* OpenVectorLayer(const std::string& osName,
                              CSLConstList papszOptions) const override;
};

std::vector<std::string> OGROpenFileGDBGroup::GetGroupNames(CSLConstList) const
{
    std::vector<std::string> ret;
    for( const auto& poSubGroup: m_apoSubGroups )
        ret.emplace_back(poSubGroup->GetName());
    return ret;
}

std::shared_ptr<GDALGroup> OGROpenFileGDBGroup::OpenGroup(const std::string& osName,
                                                          CSLConstList) const
{
    for( const auto& poSubGroup: m_apoSubGroups )
    {
        if( poSubGroup->GetName() == osName )
            return poSubGroup;
    }
    return nullptr;
}

std::vector<std::string> OGROpenFileGDBGroup::GetVectorLayerNames(CSLConstList) const
{
    std::vector<std::string> ret;
    for( const auto& poLayer: m_apoLayers )
        ret.emplace_back(poLayer->GetName());
    return ret;
}

OGRLayer* OGROpenFileGDBGroup::OpenVectorLayer(const std::string& osName,
                                               CSLConstList) const
{
    for( const auto& poLayer: m_apoLayers )
    {
        if( poLayer->GetName() == osName )
            return poLayer;
    }
    return nullptr;
}

/***********************************************************************/
/*                         OpenFileGDBv10()                            */
/***********************************************************************/

int OGROpenFileGDBDataSource::OpenFileGDBv10(int iGDBItems,
                                             int nInterestTable)
{

    CPLDebug("OpenFileGDB", "FileGDB v10 or later");

    FileGDBTable oTable;

    CPLString osFilename(CPLFormFilename(m_osDirName,
            CPLSPrintf("a%08x.gdbtable", iGDBItems + 1), nullptr));
    if( !oTable.Open(osFilename) )
        return FALSE;

    int iName = oTable.GetFieldIdx("Name");
    int iPath = oTable.GetFieldIdx("Path");
    int iDefinition = oTable.GetFieldIdx("Definition");
    int iDocumentation = oTable.GetFieldIdx("Documentation");
    if( iName < 0 || iPath < 0 || iDefinition < 0 || iDocumentation < 0 ||
        oTable.GetField(iName)->GetType() != FGFT_STRING ||
        oTable.GetField(iPath)->GetType() != FGFT_STRING ||
        oTable.GetField(iDefinition)->GetType() != FGFT_XML ||
        oTable.GetField(iDocumentation)->GetType() != FGFT_XML )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Wrong structure for GDB_Items table");
        return FALSE;
    }

    auto poRootGroup = std::make_shared<OGROpenFileGDBGroup>(std::string(), "");
    m_poRootGroup = poRootGroup;
    std::map<std::string, std::shared_ptr<OGROpenFileGDBGroup>> oMapPathToFeatureDataset;

    // First pass to collect FeatureDatasets
    for( int i=0;i<oTable.GetTotalRecordCount();i++)
    {
        if( !oTable.SelectRow(i) )
        {
            if( oTable.HasGotError() )
                break;
            continue;
        }

        const OGRField* psField = oTable.GetFieldValue(iDefinition);
        if( psField != nullptr &&
            strstr(psField->String, "DEFeatureDataset") != nullptr )
        {
            std::string osName;
            psField = oTable.GetFieldValue(iName);
            if( psField != nullptr )
            {
                const char* pszName = psField->String;
                if( pszName )
                    osName = pszName;
            }

            std::string osPath;
            psField = oTable.GetFieldValue(iPath);
            if( psField != nullptr )
            {
                const char* pszPath = psField->String;
                if( pszPath )
                    osPath = pszPath;
            }

            if( !osName.empty() && !osPath.empty() )
            {
                auto poSubGroup = std::make_shared<OGROpenFileGDBGroup>(
                    poRootGroup->GetName(), osName.c_str());
                oMapPathToFeatureDataset[osPath] = poSubGroup;
                poRootGroup->m_apoSubGroups.emplace_back(poSubGroup);
            }
        }
    }

    // Now collect layers
    int nCandidateLayers = 0;
    int nLayersSDCOrCDF = 0;
    for( int i=0;i<oTable.GetTotalRecordCount();i++)
    {
        if( !oTable.SelectRow(i) )
        {
            if( oTable.HasGotError() )
                break;
            continue;
        }

        const OGRField* psField = oTable.GetFieldValue(iDefinition);
        if( psField != nullptr &&
            (strstr(psField->String, "DEFeatureClassInfo") != nullptr ||
                strstr(psField->String, "DETableInfo") != nullptr) )
        {
            CPLString osDefinition(psField->String);

            psField = oTable.GetFieldValue(iDocumentation);
            CPLString osDocumentation( psField != nullptr ? psField->String : "" );

            psField = oTable.GetFieldValue(iName);
            if( psField != nullptr )
            {
                OGRLayer* poLayer =
                    AddLayer( psField->String, nInterestTable, nCandidateLayers, nLayersSDCOrCDF,
                          osDefinition, osDocumentation,
                          nullptr, wkbUnknown );
                if( poLayer )
                {
                    bool bAttachedToFeatureDataset = false;

                    psField = oTable.GetFieldValue(iPath);
                    if( psField != nullptr )
                    {
                        const char* pszPath = psField->String;
                        if( pszPath )
                        {
                            std::string osPath(pszPath);
                            const auto nPos = osPath.rfind('\\');
                            if( nPos != 0 && nPos != std::string::npos )
                            {
                                std::string osPathParent = osPath.substr(0, nPos);
                                const auto oIter = oMapPathToFeatureDataset.find(osPathParent);
                                if( oIter == oMapPathToFeatureDataset.end() )
                                {
                                    CPLError(CE_Warning, CPLE_AppDefined,
                                             "Cannot find feature dataset of "
                                             "path %s referenced by table %s",
                                             osPathParent.c_str(),
                                             pszPath);
                                }
                                else
                                {
                                    // Add the layer to the group of the corresponding
                                    // feature dataset
                                    oIter->second->m_apoLayers.emplace_back(poLayer);
                                    bAttachedToFeatureDataset = true;
                                }
                            }
                        }
                    }

                    if( !bAttachedToFeatureDataset )
                    {
                        poRootGroup->m_apoLayers.emplace_back(poLayer);
                    }
                }
            }
        }
        else if( psField != nullptr &&
            (strstr(psField->String, "GPCodedValueDomain2") != nullptr ||
                strstr(psField->String, "GPRangeDomain2") != nullptr) )
        {
            auto poDomain = ParseXMLFieldDomainDef(psField->String);
            if( poDomain )
            {
                const auto domainName = poDomain->GetName();
                m_oMapFieldDomains[domainName] = std::move(poDomain);
            }
        }
    }

    if( m_apoLayers.empty() && nCandidateLayers > 0 &&
        nCandidateLayers == nLayersSDCOrCDF )
        return FALSE;

    return TRUE;
}

/***********************************************************************/
/*                         OpenFileGDBv9()                             */
/***********************************************************************/

int OGROpenFileGDBDataSource::OpenFileGDBv9(int iGDBFeatureClasses,
                                            int iGDBObjectClasses,
                                            int nInterestTable)
{
    FileGDBTable oTable;

    CPLDebug("OpenFileGDB", "FileGDB v9");

    /* Fetch names of layers */
    CPLString osFilename(CPLFormFilename(m_osDirName,
            CPLSPrintf("a%08x", iGDBObjectClasses + 1), "gdbtable"));
    if( !oTable.Open(osFilename) )
        return FALSE;

    int iName = oTable.GetFieldIdx("Name");
    int iCLSID = oTable.GetFieldIdx("CLSID");
    if( iName < 0 || oTable.GetField(iName)->GetType() != FGFT_STRING ||
        iCLSID < 0 || oTable.GetField(iCLSID)->GetType() != FGFT_STRING )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Wrong structure for GDB_ObjectClasses table");
        return FALSE;
    }

    std::vector< std::string > aosName;
    int nCandidateLayers = 0, nLayersSDCOrCDF = 0;
    for( int i = 0; i < oTable.GetTotalRecordCount(); i++ )
    {
        if( !oTable.SelectRow(i) )
        {
            if( oTable.HasGotError() )
                break;
            aosName.push_back( "" );
            continue;
        }

        const OGRField* psField = oTable.GetFieldValue(iName);
        if( psField != nullptr )
        {
            std::string osName(psField->String);
            psField = oTable.GetFieldValue(iCLSID);
            if( psField != nullptr )
            {
                /* Is it a non-spatial table ? */
                if( strcmp(psField->String, "{7A566981-C114-11D2-8A28-006097AFF44E}") == 0 )
                {
                    aosName.push_back( "" );
                    AddLayer( osName, nInterestTable, nCandidateLayers, nLayersSDCOrCDF,
                              "", "", nullptr, wkbNone );
                }
                else
                {
                    /* We should perhaps also check that the CLSID is the one of a spatial table */
                    aosName.push_back( osName );
                }
            }
        }
    }
    oTable.Close();

    /* Find tables that are spatial layers */
    osFilename = CPLFormFilename(m_osDirName,
            CPLSPrintf("a%08x", iGDBFeatureClasses + 1), "gdbtable");
    if( !oTable.Open(osFilename) )
        return FALSE;

    int iObjectClassID = oTable.GetFieldIdx("ObjectClassID");
    int iGeometryType = oTable.GetFieldIdx("GeometryType");
    int iShapeField = oTable.GetFieldIdx("ShapeField");
    if( iObjectClassID < 0 || iGeometryType < 0 || iShapeField < 0 ||
        oTable.GetField(iObjectClassID)->GetType() != FGFT_INT32 ||
        oTable.GetField(iGeometryType)->GetType() != FGFT_INT32 ||
        oTable.GetField(iShapeField)->GetType() != FGFT_STRING )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Wrong structure for GDB_FeatureClasses table");
        return FALSE;
    }

    for( int i = 0; i < oTable.GetTotalRecordCount(); i++ )
    {
        if( !oTable.SelectRow(i) )
        {
            if( oTable.HasGotError() )
                break;
            continue;
        }

        const OGRField* psField = oTable.GetFieldValue(iGeometryType);
        if( psField == nullptr )
            continue;
        const int nGeomType = psField->Integer;
        OGRwkbGeometryType eGeomType = wkbUnknown;
        switch( nGeomType )
        {
            case FGTGT_NONE: /* doesn't make sense ! */ break;
            case FGTGT_POINT: eGeomType = wkbPoint; break;
            case FGTGT_MULTIPOINT: eGeomType = wkbMultiPoint; break;
            case FGTGT_LINE: eGeomType = wkbMultiLineString; break;
            case FGTGT_POLYGON: eGeomType = wkbMultiPolygon; break;
            case FGTGT_MULTIPATCH: eGeomType = wkbUnknown; break;
        }

        psField = oTable.GetFieldValue(iShapeField);
        if( psField == nullptr )
            continue;
        CPLString osGeomFieldName(psField->String);

        psField = oTable.GetFieldValue(iObjectClassID);
        if( psField == nullptr )
            continue;

        int idx = psField->Integer;
        if( idx > 0 && idx <= static_cast<int>(aosName.size()) &&
            !aosName[idx-1].empty() )
        {
            const std::string osName(aosName[idx-1]);
            AddLayer( osName, nInterestTable, nCandidateLayers, nLayersSDCOrCDF,
                      "", "", osGeomFieldName.c_str(), eGeomType);
        }
    }

    if( m_apoLayers.empty() && nCandidateLayers > 0 &&
        nCandidateLayers == nLayersSDCOrCDF )
        return FALSE;

    return TRUE;
}

/***********************************************************************/
/*                         TestCapability()                            */
/***********************************************************************/

int OGROpenFileGDBDataSource::TestCapability( const char * /* pszCap */ )
{
    return FALSE;
}

/***********************************************************************/
/*                            GetLayer()                               */
/***********************************************************************/

OGRLayer* OGROpenFileGDBDataSource::GetLayer( int iIndex )
{
    if( iIndex < 0 || iIndex >= (int) m_apoLayers.size() )
        return nullptr;
    return m_apoLayers[iIndex];
}

/***********************************************************************/
/*                          GetLayerByName()                           */
/***********************************************************************/

OGRLayer* OGROpenFileGDBDataSource::GetLayerByName( const char* pszName )
{
    OGRLayer* poLayer = OGRDataSource::GetLayerByName(pszName);
    if( poLayer != nullptr )
        return poLayer;

    for(size_t i=0;i<m_apoHiddenLayers.size();i++)
    {
        if( EQUAL(m_apoHiddenLayers[i]->GetName(), pszName) )
            return m_apoHiddenLayers[i];
    }

    std::map<std::string, int>::const_iterator oIter = m_osMapNameToIdx.find(pszName);
    if( oIter != m_osMapNameToIdx.end() )
    {
        int idx = oIter->second;
        CPLString osFilename(CPLFormFilename(
                        m_osDirName, CPLSPrintf("a%08x", idx), "gdbtable"));
        if( FileExists(osFilename) )
        {
            poLayer = new OGROpenFileGDBLayer(
                                    osFilename, pszName, "", "");
            m_apoHiddenLayers.push_back(poLayer);
            return poLayer;
        }
    }
    return nullptr;
}

/************************************************************************/
/*                   OGROpenFileGDBSingleFeatureLayer                   */
/************************************************************************/

class OGROpenFileGDBSingleFeatureLayer final: public OGRLayer
{
  private:
    char               *pszVal;
    OGRFeatureDefn     *poFeatureDefn;
    int                 iNextShapeId;

  public:
                        OGROpenFileGDBSingleFeatureLayer( const char* pszLayerName,
                                                          const char *pszVal );
               virtual ~OGROpenFileGDBSingleFeatureLayer();

    virtual void        ResetReading() override { iNextShapeId = 0; }
    virtual OGRFeature *GetNextFeature() override;
    virtual OGRFeatureDefn *GetLayerDefn() override { return poFeatureDefn; }
    virtual int         TestCapability( const char * ) override { return FALSE; }
};

/************************************************************************/
/*                 OGROpenFileGDBSingleFeatureLayer()                   */
/************************************************************************/

OGROpenFileGDBSingleFeatureLayer::OGROpenFileGDBSingleFeatureLayer(
    const char* pszLayerName,
    const char *pszValIn ) :
    pszVal(pszValIn ? CPLStrdup(pszValIn) : nullptr),
    poFeatureDefn(new OGRFeatureDefn( pszLayerName )),
    iNextShapeId(0)
{
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    OGRFieldDefn oField( "FIELD_1", OFTString );
    poFeatureDefn->AddFieldDefn( &oField );
}

/************************************************************************/
/*                 ~OGROpenFileGDBSingleFeatureLayer()                  */
/************************************************************************/

OGROpenFileGDBSingleFeatureLayer::~OGROpenFileGDBSingleFeatureLayer()
{
    if( poFeatureDefn != nullptr )
        poFeatureDefn->Release();
    CPLFree(pszVal);
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature * OGROpenFileGDBSingleFeatureLayer::GetNextFeature()
{
    if (iNextShapeId != 0)
        return nullptr;

    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    if (pszVal)
        poFeature->SetField(0, pszVal);
    poFeature->SetFID(iNextShapeId ++);
    return poFeature;
}

/***********************************************************************/
/*                     OGROpenFileGDBSimpleSQLLayer                    */
/***********************************************************************/

class OGROpenFileGDBSimpleSQLLayer final: public OGRLayer
{
        OGRLayer        *poBaseLayer;
        FileGDBIterator *poIter;
        OGRFeatureDefn  *poFeatureDefn;

    public:
        OGROpenFileGDBSimpleSQLLayer(OGRLayer* poBaseLayer,
                                     FileGDBIterator* poIter,
                                     int nColumns,
                                     swq_col_def* pasColDefs);
       virtual ~OGROpenFileGDBSimpleSQLLayer();

       virtual void        ResetReading() override;
       virtual OGRFeature* GetNextFeature() override;
       virtual OGRFeature* GetFeature( GIntBig nFeatureId ) override;
       virtual OGRFeatureDefn* GetLayerDefn() override { return poFeatureDefn; }
       virtual int         TestCapability( const char * ) override;
       virtual const char* GetFIDColumn() override { return poBaseLayer->GetFIDColumn(); }
       virtual OGRErr      GetExtent( OGREnvelope *psExtent, int bForce ) override
                            { return poBaseLayer->GetExtent(psExtent, bForce); }
       virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }
       virtual GIntBig     GetFeatureCount(int bForce) override;
};

/***********************************************************************/
/*                    OGROpenFileGDBSimpleSQLLayer()                   */
/***********************************************************************/

OGROpenFileGDBSimpleSQLLayer::OGROpenFileGDBSimpleSQLLayer(
    OGRLayer* poBaseLayerIn,
    FileGDBIterator* poIterIn,
    int nColumns,
    swq_col_def* pasColDefs) :
    poBaseLayer(poBaseLayerIn),
    poIter(poIterIn),
    poFeatureDefn(nullptr)
{
    if( nColumns == 1 && strcmp(pasColDefs[0].field_name, "*") == 0 )
    {
        poFeatureDefn = poBaseLayer->GetLayerDefn();
        poFeatureDefn->Reference();
    }
    else
    {
        poFeatureDefn = new OGRFeatureDefn(poBaseLayer->GetName());
        poFeatureDefn->SetGeomType(poBaseLayer->GetGeomType());
        poFeatureDefn->Reference();
        if( poBaseLayer->GetGeomType() != wkbNone )
        {
            poFeatureDefn->GetGeomFieldDefn(0)->SetName(poBaseLayer->GetGeometryColumn());
            poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poBaseLayer->GetSpatialRef());
        }
        for(int i=0;i<nColumns;i++)
        {
            if( strcmp(pasColDefs[i].field_name, "*") == 0 )
            {
                for(int j=0;j<poBaseLayer->GetLayerDefn()->GetFieldCount();j++)
                    poFeatureDefn->AddFieldDefn(poBaseLayer->GetLayerDefn()->GetFieldDefn(j));
            }
            else
            {
                OGRFieldDefn* poFieldDefn = poBaseLayer->GetLayerDefn()->GetFieldDefn(
                    poBaseLayer->GetLayerDefn()->GetFieldIndex(pasColDefs[i].field_name));
                CPLAssert(poFieldDefn != nullptr ); /* already checked before */
                poFeatureDefn->AddFieldDefn(poFieldDefn);
            }
        }
    }
    SetDescription( poFeatureDefn->GetName() );
    OGROpenFileGDBSimpleSQLLayer::ResetReading();
}

/***********************************************************************/
/*                   ~OGROpenFileGDBSimpleSQLLayer()                   */
/***********************************************************************/

OGROpenFileGDBSimpleSQLLayer::~OGROpenFileGDBSimpleSQLLayer()
{
    if( poFeatureDefn )
    {
        poFeatureDefn->Release();
    }
    delete poIter;
}

/***********************************************************************/
/*                          ResetReading()                             */
/***********************************************************************/

void OGROpenFileGDBSimpleSQLLayer::ResetReading()
{
    poIter->Reset();
}

/***********************************************************************/
/*                          GetFeature()                               */
/***********************************************************************/

OGRFeature* OGROpenFileGDBSimpleSQLLayer::GetFeature( GIntBig nFeatureId )
{
    OGRFeature* poSrcFeature = poBaseLayer->GetFeature(nFeatureId);
    if( poSrcFeature == nullptr )
        return nullptr;

    if( poFeatureDefn == poBaseLayer->GetLayerDefn() )
        return poSrcFeature;
    else
    {
        OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
        poFeature->SetFrom(poSrcFeature);
        poFeature->SetFID(poSrcFeature->GetFID());
        delete poSrcFeature;
        return poFeature;
    }
}

/***********************************************************************/
/*                         GetNextFeature()                            */
/***********************************************************************/

OGRFeature* OGROpenFileGDBSimpleSQLLayer::GetNextFeature()
{
    while( true )
    {
        int nRow = poIter->GetNextRowSortedByValue();
        if( nRow < 0 )
            return nullptr;
        OGRFeature* poFeature = GetFeature(nRow + 1);
        if( poFeature == nullptr )
            return nullptr;

        if( (m_poFilterGeom == nullptr
             || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == nullptr ||
                m_poAttrQuery->Evaluate( poFeature ) ) )
        {
            return poFeature;
        }

        delete poFeature;
    }
}

/***********************************************************************/
/*                         GetFeatureCount()                           */
/***********************************************************************/

GIntBig OGROpenFileGDBSimpleSQLLayer::GetFeatureCount( int bForce )
{

    /* No filter */
    if( m_poFilterGeom == nullptr && m_poAttrQuery == nullptr )
    {
        return poIter->GetRowCount();
    }

    return OGRLayer::GetFeatureCount(bForce);
}

/***********************************************************************/
/*                         TestCapability()                            */
/***********************************************************************/

int OGROpenFileGDBSimpleSQLLayer::TestCapability( const char * pszCap )
{

    if( EQUAL(pszCap,OLCFastFeatureCount) )
    {
        return m_poFilterGeom == nullptr && m_poAttrQuery == nullptr;
    }
    else if( EQUAL(pszCap,OLCFastGetExtent) )
    {
        return TRUE;
    }
    else if( EQUAL(pszCap,OLCRandomRead) )
    {
        return TRUE;
    }
    else if( EQUAL(pszCap,OLCStringsAsUTF8) )
    {
        return TRUE; /* ? */
    }

    return FALSE;
}

/***********************************************************************/
/*                            ExecuteSQL()                             */
/***********************************************************************/

OGRLayer* OGROpenFileGDBDataSource::ExecuteSQL( const char *pszSQLCommand,
                                                OGRGeometry *poSpatialFilter,
                                                const char *pszDialect )
{

/* -------------------------------------------------------------------- */
/*      Special case GetLayerDefinition                                 */
/* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszSQLCommand, "GetLayerDefinition "))
    {
        OGROpenFileGDBLayer* poLayer = reinterpret_cast<OGROpenFileGDBLayer *>(
            GetLayerByName(pszSQLCommand + strlen("GetLayerDefinition ")) );
        if (poLayer)
        {
            OGRLayer* poRet = new OGROpenFileGDBSingleFeatureLayer(
                "LayerDefinition", poLayer->GetXMLDefinition().c_str() );
            return poRet;
        }

        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Special case GetLayerMetadata                                   */
/* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszSQLCommand, "GetLayerMetadata "))
    {
        OGROpenFileGDBLayer* poLayer = reinterpret_cast<OGROpenFileGDBLayer *>(
            GetLayerByName(pszSQLCommand + strlen("GetLayerMetadata ")) );
        if (poLayer)
        {
            OGRLayer* poRet = new OGROpenFileGDBSingleFeatureLayer(
                "LayerMetadata", poLayer->GetXMLDocumentation().c_str() );
            return poRet;
        }

        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Special case GetLayerAttrIndexUse (only for debugging purposes) */
/* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszSQLCommand, "GetLayerAttrIndexUse "))
    {
        OGROpenFileGDBLayer* poLayer = reinterpret_cast<OGROpenFileGDBLayer *>(
            GetLayerByName(pszSQLCommand + strlen("GetLayerAttrIndexUse ")) );
        if (poLayer)
        {
            OGRLayer* poRet = new OGROpenFileGDBSingleFeatureLayer(
                "LayerAttrIndexUse",
                CPLSPrintf("%d", poLayer->GetAttrIndexUse()) );
            return poRet;
        }

        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Special case GetLayerSpatialIndexState (only for debugging purposes) */
/* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszSQLCommand, "GetLayerSpatialIndexState "))
    {
        OGROpenFileGDBLayer* poLayer = reinterpret_cast<OGROpenFileGDBLayer*>(
            GetLayerByName(pszSQLCommand +
                           strlen("GetLayerSpatialIndexState ")) );
        if (poLayer)
        {
            OGRLayer* poRet = new OGROpenFileGDBSingleFeatureLayer(
                "LayerSpatialIndexState",
                CPLSPrintf("%d", poLayer->GetSpatialIndexState()) );
            return poRet;
        }

        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Special case GetLastSQLUsedOptimizedImplementation (only for debugging purposes) */
/* -------------------------------------------------------------------- */
    if (EQUAL(pszSQLCommand, "GetLastSQLUsedOptimizedImplementation"))
    {
        OGRLayer* poRet = new OGROpenFileGDBSingleFeatureLayer(
            "GetLastSQLUsedOptimizedImplementation",
            CPLSPrintf(
                "%d", static_cast<int>(bLastSQLUsedOptimizedImplementation))) ;
        return poRet;
    }

    bLastSQLUsedOptimizedImplementation = false;

/* -------------------------------------------------------------------- */
/*      Special cases for SQL optimizations                             */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszSQLCommand, "SELECT ") &&
        (pszDialect == nullptr || EQUAL(pszDialect, "") ||
         EQUAL(pszDialect, "OGRSQL")) &&
        CPLTestBool(CPLGetConfigOption("OPENFILEGDB_USE_INDEX", "YES")) )
    {
        swq_select oSelect;
        if( oSelect.preparse(pszSQLCommand) != CE_None )
            return nullptr;

/* -------------------------------------------------------------------- */
/*      MIN/MAX/SUM/AVG/COUNT optimization                              */
/* -------------------------------------------------------------------- */
        if( oSelect.join_count == 0 && oSelect.poOtherSelect == nullptr &&
            oSelect.table_count == 1 && oSelect.order_specs == 0 &&
            oSelect.query_mode != SWQM_DISTINCT_LIST )
        {
            OGROpenFileGDBLayer* poLayer =
                reinterpret_cast<OGROpenFileGDBLayer *>(
                    GetLayerByName( oSelect.table_defs[0].table_name));
            if( poLayer )
            {
                OGRMemLayer* poMemLayer = nullptr;

                int i = 0;  // Used after for.
                for( ; i < oSelect.result_columns; i ++ )
                {
                    swq_col_func col_func = oSelect.column_defs[i].col_func;
                    if( !(col_func == SWQCF_MIN || col_func == SWQCF_MAX ||
                          col_func == SWQCF_COUNT || col_func == SWQCF_AVG ||
                          col_func == SWQCF_SUM) )
                        break;

                    if( oSelect.column_defs[i].field_name == nullptr )
                        break;
                    if( oSelect.column_defs[i].distinct_flag )
                        break;
                    if( oSelect.column_defs[i].target_type != SWQ_OTHER )
                        break;

                    int idx = poLayer->GetLayerDefn()->GetFieldIndex(
                                            oSelect.column_defs[i].field_name);
                    if( idx < 0 )
                        break;

                    OGRFieldDefn* poFieldDefn = poLayer->GetLayerDefn()->GetFieldDefn(idx);

                    if( col_func == SWQCF_SUM && poFieldDefn->GetType() == OFTDateTime )
                        break;

                    int eOutOGRType = -1;
                    const OGRField* psField = nullptr;
                    OGRField sField;
                    if( col_func == SWQCF_MIN || col_func == SWQCF_MAX )
                    {
                        psField = poLayer->GetMinMaxValue(
                                 poFieldDefn, col_func == SWQCF_MIN, eOutOGRType);
                        if( eOutOGRType < 0 )
                            break;
                    }
                    else
                    {
                        double dfMin = 0.0;
                        double dfMax = 0.0;
                        int nCount = 0;
                        double dfSum = 0.0;

                        if( !poLayer->GetMinMaxSumCount(poFieldDefn,
                                                        dfMin, dfMax,
                                                        dfSum, nCount) )
                            break;
                        psField = &sField;
                        if( col_func == SWQCF_AVG )
                        {
                            if( nCount == 0 )
                            {
                                eOutOGRType = OFTReal;
                                psField = nullptr;
                            }
                            else
                            {
                                if( poFieldDefn->GetType() == OFTDateTime )
                                {
                                    eOutOGRType = OFTDateTime;
                                    FileGDBDoubleDateToOGRDate(dfSum / nCount, &sField);
                                }
                                else
                                {
                                    eOutOGRType = OFTReal;
                                    sField.Real = dfSum / nCount;
                                }
                            }
                        }
                        else if( col_func == SWQCF_COUNT )
                        {
                            sField.Integer = nCount;
                            eOutOGRType = OFTInteger;
                        }
                        else
                        {
                            sField.Real = dfSum;
                            eOutOGRType = OFTReal;
                        }
                    }

                    if( poMemLayer == nullptr )
                    {
                        poMemLayer = new OGRMemLayer("SELECT", nullptr, wkbNone);
                        OGRFeature* poFeature = new OGRFeature(poMemLayer->GetLayerDefn());
                        CPL_IGNORE_RET_VAL(poMemLayer->CreateFeature(poFeature));
                        delete poFeature;
                    }

                    const char* pszMinMaxFieldName =
                        CPLSPrintf( "%s_%s", (col_func == SWQCF_MIN) ? "MIN" :
                                             (col_func == SWQCF_MAX) ? "MAX" :
                                             (col_func == SWQCF_AVG) ? "AVG" :
                                             (col_func == SWQCF_SUM) ? "SUM" :
                                                                       "COUNT",
                                            oSelect.column_defs[i].field_name);
                    OGRFieldDefn oFieldDefn(pszMinMaxFieldName,
                                            (OGRFieldType) eOutOGRType);
                    poMemLayer->CreateField(&oFieldDefn);
                    if( psField != nullptr )
                    {
                        OGRFeature* poFeature = poMemLayer->GetFeature(0);
                        poFeature->SetField(oFieldDefn.GetNameRef(), (OGRField*) psField);
                        CPL_IGNORE_RET_VAL(poMemLayer->SetFeature(poFeature));
                        delete poFeature;
                    }
                }
                if( i != oSelect.result_columns )
                {
                    delete poMemLayer;
                }
                else
                {
                    CPLDebug("OpenFileGDB",
                        "Using optimized MIN/MAX/SUM/AVG/COUNT implementation");
                    bLastSQLUsedOptimizedImplementation = true;
                    return poMemLayer;
                }
            }
        }

/* -------------------------------------------------------------------- */
/*      ORDER BY optimization                                           */
/* -------------------------------------------------------------------- */
        if( oSelect.join_count == 0 && oSelect.poOtherSelect == nullptr &&
            oSelect.table_count == 1 && oSelect.order_specs == 1 &&
            oSelect.query_mode != SWQM_DISTINCT_LIST )
        {
            OGROpenFileGDBLayer* poLayer =
                reinterpret_cast<OGROpenFileGDBLayer *>(
                    GetLayerByName( oSelect.table_defs[0].table_name) );
            if( poLayer != nullptr &&
                poLayer->HasIndexForField(oSelect.order_defs[0].field_name) )
            {
                OGRErr eErr = OGRERR_NONE;
                if( oSelect.where_expr != nullptr )
                {
                    /* The where must be a simple comparison on the column */
                    /* that is used for ordering */
                    if( oSelect.where_expr->eNodeType == SNT_OPERATION &&
                        OGROpenFileGDBIsComparisonOp(oSelect.where_expr->nOperation) &&
                        oSelect.where_expr->nOperation != SWQ_NE &&
                        oSelect.where_expr->nSubExprCount == 2 &&
                        (oSelect.where_expr->papoSubExpr[0]->eNodeType == SNT_COLUMN ||
                         oSelect.where_expr->papoSubExpr[0]->eNodeType == SNT_CONSTANT) &&
                        oSelect.where_expr->papoSubExpr[0]->field_type == SWQ_STRING &&
                        EQUAL(oSelect.where_expr->papoSubExpr[0]->string_value,
                              oSelect.order_defs[0].field_name) &&
                        oSelect.where_expr->papoSubExpr[1]->eNodeType == SNT_CONSTANT )
                    {
                        /* ok */
                    }
                    else
                        eErr = OGRERR_FAILURE;
                }
                if( eErr == OGRERR_NONE )
                {
                    int i = 0;  // Used after for.
                    for( ; i < oSelect.result_columns; i++ )
                    {
                        if( oSelect.column_defs[i].col_func != SWQCF_NONE )
                            break;
                        if( oSelect.column_defs[i].field_name == nullptr )
                            break;
                        if( oSelect.column_defs[i].distinct_flag )
                            break;
                        if( oSelect.column_defs[i].target_type != SWQ_OTHER )
                            break;
                        if( strcmp(oSelect.column_defs[i].field_name, "*") != 0 &&
                            poLayer->GetLayerDefn()->GetFieldIndex(
                                        oSelect.column_defs[i].field_name) < 0 )
                            break;
                    }
                    if( i != oSelect.result_columns )
                        eErr = OGRERR_FAILURE;
                }
                if( eErr == OGRERR_NONE )
                {
                    int op = -1;
                    swq_expr_node* poValue = nullptr;
                    if( oSelect.where_expr != nullptr )
                    {
                        op = oSelect.where_expr->nOperation;
                        poValue = oSelect.where_expr->papoSubExpr[1];
                    }

                    FileGDBIterator *poIter = poLayer->BuildIndex(
                                    oSelect.order_defs[0].field_name,
                                    oSelect.order_defs[0].ascending_flag,
                                    op, poValue);

                    /* Check that they are no NULL values */
                    if( oSelect.where_expr == nullptr &&
                        poIter->GetRowCount() != poLayer->GetFeatureCount(FALSE) )
                    {
                        delete poIter;
                        poIter = nullptr;
                    }

                    if( poIter != nullptr )
                    {
                        CPLDebug("OpenFileGDB", "Using OGROpenFileGDBSimpleSQLLayer");
                        bLastSQLUsedOptimizedImplementation = true;
                        return new OGROpenFileGDBSimpleSQLLayer(poLayer,
                                                                poIter,
                                                                oSelect.result_columns,
                                                                oSelect.column_defs);
                    }
                }
            }
        }
    }

    return OGRDataSource::ExecuteSQL(pszSQLCommand, poSpatialFilter, pszDialect);
}

/***********************************************************************/
/*                           ReleaseResultSet()                        */
/***********************************************************************/

void OGROpenFileGDBDataSource::ReleaseResultSet( OGRLayer * poResultsSet )
{
    delete poResultsSet;
}

/***********************************************************************/
/*                           GetFileList()                             */
/***********************************************************************/

char** OGROpenFileGDBDataSource::GetFileList()
{
    int nInterestTable = -1;
    const char* pszFilenameWithoutPath = CPLGetFilename(m_pszName);
    CPLString osFilenameRadix;
    unsigned int unInterestTable = 0;
    if( strlen(pszFilenameWithoutPath) == strlen("a00000000.gdbtable") &&
        pszFilenameWithoutPath[0] == 'a' &&
        sscanf(pszFilenameWithoutPath, "a%08x.gdbtable", &unInterestTable) == 1 )
    {
        nInterestTable = static_cast<int>(unInterestTable);
        osFilenameRadix = CPLSPrintf("a%08x.", nInterestTable);
    }

    char** papszFiles = VSIReadDir(m_osDirName);
    CPLStringList osStringList;
    char** papszIter = papszFiles;
    for( ; papszIter != nullptr && *papszIter != nullptr ; papszIter ++ )
    {
        if( strcmp(*papszIter, ".") == 0 || strcmp(*papszIter, "..") == 0 )
            continue;
        if( osFilenameRadix.empty() ||
            strncmp(*papszIter, osFilenameRadix, osFilenameRadix.size()) == 0 )
        {
            osStringList.AddString(CPLFormFilename(m_osDirName, *papszIter, nullptr));
        }
    }
    CSLDestroy(papszFiles);
    return osStringList.StealList();
}
