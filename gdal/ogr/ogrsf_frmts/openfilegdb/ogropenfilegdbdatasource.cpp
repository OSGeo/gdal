/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Open FileGDB OGR driver.
 * Author:   Even Rouault, <even dot rouault at mines-dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_openfilegdb.h"
#include "ogr_mem.h"
#include <map>

CPL_CVSID("$Id");

/************************************************************************/
/*                      OGROpenFileGDBDataSource()                      */
/************************************************************************/
OGROpenFileGDBDataSource::OGROpenFileGDBDataSource()
{
    m_pszName = NULL;
    m_papszFiles = NULL;
    bLastSQLUsedOptimizedImplementation = FALSE;
}

/************************************************************************/
/*                     ~OGROpenFileGDBDataSource()                      */
/************************************************************************/
OGROpenFileGDBDataSource::~OGROpenFileGDBDataSource()

{
    size_t i;
    for( i = 0; i < m_apoLayers.size(); i++ )
        delete m_apoLayers[i];
    for( i = 0; i < m_apoHiddenLayers.size(); i++ )
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
    else
    {
        VSIStatBufL sStat;
        return VSIStatExL(pszFilename, &sStat, VSI_STAT_EXISTS_FLAG) == 0;
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGROpenFileGDBDataSource::Open( const char* pszFilename )

{
    FileGDBTable oTable;

    m_pszName = CPLStrdup(pszFilename);

    m_osDirName = pszFilename;
    int nInterestTable = -1;
    const char* pszFilenameWithoutPath = CPLGetFilename(pszFilename);
    if( strlen(pszFilenameWithoutPath) == strlen("a00000000.gdbtable") &&
        pszFilenameWithoutPath[0] == 'a' &&
        sscanf(pszFilenameWithoutPath, "a%08x.gdbtable", &nInterestTable) == 1 )
    {
        m_osDirName = CPLGetPath(m_osDirName);
    }
    else
    {
        nInterestTable = -1;
    }

    if( EQUAL(CPLGetExtension(m_osDirName), "zip") &&
        strncmp(m_osDirName, "/vsizip/", strlen("/vsizip/")) != 0 )
    {
        m_osDirName = "/vsizip/" + m_osDirName;
    }
    else  if( EQUAL(CPLGetExtension(m_osDirName), "tar") &&
        strncmp(m_osDirName, "/vsitar/", strlen("/vsitar/")) != 0 )
    {
        m_osDirName = "/vsitar/" + m_osDirName;
    }

    if( strncmp(m_osDirName, "/vsizip/", strlen("/vsizip/")) == 0 ||
        strncmp(m_osDirName, "/vsitar/", strlen("/vsitar/")) == 0)
    {
        /* Look for one subdirectory ending with .gdb extension */
        char** papszDir = CPLReadDir(m_osDirName);
        int iCandidate = -1;
        for( int i=0; papszDir && papszDir[i] != NULL; i++ )
        {
            VSIStatBufL sStat;
            if( EQUAL(CPLGetExtension(papszDir[i]), "gdb") &&
                VSIStatL( CPLSPrintf("%s/%s", m_osDirName.c_str(), papszDir[i]), &sStat ) == 0 &&
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
    const char* psza00000001 = CPLFormFilename(m_osDirName, "a00000001", "gdbtable");
    if( !FileExists(psza00000001) || !oTable.Open(psza00000001) )
    {
        if( nInterestTable >= 0 && FileExists(m_pszName) )
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
    int i;

    std::vector<std::string> aosTableNames;
    for(i=0;i<oTable.GetTotalRecordCount();i++)
    {
        if( !oTable.SelectRow(i) )
        {
            if( oTable.HasGotError() )
                break;
            aosTableNames.push_back("");
            continue;
        }

        const OGRField* psField = oTable.GetFieldValue(0);
        if( psField != NULL )
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

    if( m_apoLayers.size() == 0 && nInterestTable >= 0 )
    {
        if( FileExists(m_pszName) )
        {
            const char* pszLyrName;
            if( nInterestTable <= (int)aosTableNames.size()  &&
                aosTableNames[nInterestTable-1].size() != 0 )
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

void OGROpenFileGDBDataSource::AddLayer( const CPLString& osName,
                                         int nInterestTable,
                                         int& nCandidateLayers,
                                         int& nLayersSDC,
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
    if( idx > 0 && (nInterestTable < 0 || nInterestTable == idx) )
    {
        const char* pszFilename = CPLFormFilename(
            m_osDirName, CPLSPrintf("a%08x", idx), "gdbtable");
        if( FileExists(pszFilename) )
        {
            nCandidateLayers ++;

            if( m_papszFiles != NULL )
            {
                const char* pszSDC = CPLResetExtension(pszFilename, "gdbtable.sdc");
                if( FileExists(pszSDC) )
                {
                    nLayersSDC ++;
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "%s layer has a %s file whose format is unhandled",
                            osName.c_str(), pszSDC);
                    return;
                }
            }

            m_apoLayers.push_back(
                new OGROpenFileGDBLayer(pszFilename,
                                        osName,
                                        osDefinition,
                                        osDocumentation,
                                        pszGeomName, eGeomType));
        }
    }
}

/***********************************************************************/
/*                         OpenFileGDBv10()                            */
/***********************************************************************/

int OGROpenFileGDBDataSource::OpenFileGDBv10(int iGDBItems,
                                             int nInterestTable)
{
    FileGDBTable oTable;
    int i;

    CPLDebug("OpenFileGDB", "FileGDB v10 or later");

    if( !oTable.Open(CPLFormFilename(m_osDirName,
            CPLSPrintf("a%08x.gdbtable", iGDBItems + 1), NULL)) )
        return FALSE;

    int iName = oTable.GetFieldIdx("Name");
    int iDefinition = oTable.GetFieldIdx("Definition");
    int iDocumentation = oTable.GetFieldIdx("Documentation");
    if( iName < 0 || iDefinition < 0 || iDocumentation < 0 ||
        oTable.GetField(iName)->GetType() != FGFT_STRING ||
        oTable.GetField(iDefinition)->GetType() != FGFT_XML ||
        oTable.GetField(iDocumentation)->GetType() != FGFT_XML )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Wrong structure for GDB_Items table");
        return FALSE;
    }

    int nCandidateLayers = 0, nLayersSDC = 0;
    for(i=0;i<oTable.GetTotalRecordCount();i++)
    {
        if( !oTable.SelectRow(i) )
        {
            if( oTable.HasGotError() )
                break;
            continue;
        }

        const OGRField* psField = oTable.GetFieldValue(iDefinition);
        if( psField != NULL &&
            (strstr(psField->String, "DEFeatureClassInfo") != NULL ||
                strstr(psField->String, "DETableInfo") != NULL) )
        {
            CPLString osDefinition(psField->String);

            psField = oTable.GetFieldValue(iDocumentation);
            CPLString osDocumentation( psField != NULL ? psField->String : "" );

            psField = oTable.GetFieldValue(iName);
            if( psField != NULL )
            {
                AddLayer( psField->String, nInterestTable, nCandidateLayers, nLayersSDC,
                          osDefinition, osDocumentation,
                          NULL, wkbUnknown );
            }
        }
    }

    if( m_apoLayers.size() == 0 && nCandidateLayers > 0 &&
        nCandidateLayers == nLayersSDC )
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
    int i;

    CPLDebug("OpenFileGDB", "FileGDB v9");

    /* Fetch names of layers */
    if( !oTable.Open(CPLFormFilename(m_osDirName,
            CPLSPrintf("a%08x", iGDBObjectClasses + 1), "gdbtable")) )
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
    int nCandidateLayers = 0, nLayersSDC = 0;
    for(i=0;i<oTable.GetTotalRecordCount();i++)
    {
        if( !oTable.SelectRow(i) )
        {
            if( oTable.HasGotError() )
                break;
            aosName.push_back( "" );
            continue;
        }

        const OGRField* psField = oTable.GetFieldValue(iName);
        if( psField != NULL )
        {
            std::string osName(psField->String);
            psField = oTable.GetFieldValue(iCLSID);
            if( psField != NULL )
            {
                /* Is it a non-spatial table ? */
                if( strcmp(psField->String, "{7A566981-C114-11D2-8A28-006097AFF44E}") == 0 )
                {
                    aosName.push_back( "" );
                    AddLayer( osName, nInterestTable, nCandidateLayers, nLayersSDC,
                              "", "", NULL, wkbNone );
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
    if( !oTable.Open(CPLFormFilename(m_osDirName,
            CPLSPrintf("a%08x", iGDBFeatureClasses + 1), "gdbtable")) )
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

    for(i=0;i<oTable.GetTotalRecordCount();i++)
    {
        if( !oTable.SelectRow(i) )
        {
            if( oTable.HasGotError() )
                break;
            continue;
        }

        const OGRField* psField;

        psField = oTable.GetFieldValue(iGeometryType);
        if( psField == NULL )
            continue;
        int nGeomType = psField->Integer;
        OGRwkbGeometryType eGeomType = wkbUnknown;
        switch( nGeomType )
        {
            case FGTGT_NONE: /* doesn't make sense ! */ break;
            case FGTGT_POINT: eGeomType = wkbPoint; break;
            case FGTGT_MULTIPOINT: eGeomType = wkbMultiPoint; break;
            case FGTGT_LINE: eGeomType = wkbMultiLineString; break;
            case FGTGT_POLYGON: eGeomType = wkbMultiPolygon; break;
            case FGTGT_MULTIPATCH: eGeomType = wkbMultiPolygon; break;
        }

        psField = oTable.GetFieldValue(iShapeField);
        if( psField == NULL )
            continue;
        CPLString osGeomFieldName(psField->String);

        psField = oTable.GetFieldValue(iObjectClassID);
        if( psField == NULL )
            continue;

        int idx = psField->Integer;
        if( psField != NULL && idx > 0 && idx <= (int)aosName.size() &&
            aosName[idx-1].size() > 0 )
        {
            const std::string osName(aosName[idx-1]);
            AddLayer( osName, nInterestTable, nCandidateLayers, nLayersSDC,
                      "", "", osGeomFieldName.c_str(), eGeomType);
        }
    }

    if( m_apoLayers.size() == 0 && nCandidateLayers > 0 &&
        nCandidateLayers == nLayersSDC )
        return FALSE;

    return TRUE;
}

/***********************************************************************/
/*                         TestCapability()                            */
/***********************************************************************/

int OGROpenFileGDBDataSource::TestCapability( const char * pszCap )
{
    (void)pszCap;
    return FALSE;
}

/***********************************************************************/
/*                            GetLayer()                               */
/***********************************************************************/

OGRLayer* OGROpenFileGDBDataSource::GetLayer( int iIndex )
{
    if( iIndex < 0 || iIndex >= (int) m_apoLayers.size() )
        return NULL;
    return m_apoLayers[iIndex];
}

/***********************************************************************/
/*                          GetLayerByName()                           */
/***********************************************************************/

OGRLayer* OGROpenFileGDBDataSource::GetLayerByName( const char* pszName )
{
    OGRLayer* poLayer;

    poLayer = OGRDataSource::GetLayerByName(pszName);
    if( poLayer != NULL )
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
        const char* pszFilename = CPLFormFilename(
                            m_osDirName, CPLSPrintf("a%08x", idx), "gdbtable");
        if( FileExists(pszFilename) )
        {
            poLayer = new OGROpenFileGDBLayer(
                                    pszFilename, pszName, "", "");
            m_apoHiddenLayers.push_back(poLayer);
            return poLayer;
        }
    }
    return NULL;
}


/************************************************************************/
/*                   OGROpenFileGDBSingleFeatureLayer                   */
/************************************************************************/

class OGROpenFileGDBSingleFeatureLayer : public OGRLayer
{
  private:
    char               *pszVal;
    OGRFeatureDefn     *poFeatureDefn;
    int                 iNextShapeId;

  public:
                        OGROpenFileGDBSingleFeatureLayer( const char* pszLayerName,
                                                          const char *pszVal );
                        ~OGROpenFileGDBSingleFeatureLayer();

    virtual void        ResetReading() { iNextShapeId = 0; }
    virtual OGRFeature *GetNextFeature();
    virtual OGRFeatureDefn *GetLayerDefn() { return poFeatureDefn; }
    virtual int         TestCapability( const char * ) { return FALSE; }
};

/************************************************************************/
/*                 OGROpenFileGDBSingleFeatureLayer()                   */
/************************************************************************/

OGROpenFileGDBSingleFeatureLayer::OGROpenFileGDBSingleFeatureLayer(const char* pszLayerName,
                                                                   const char *pszVal )
{
    poFeatureDefn = new OGRFeatureDefn( pszLayerName );
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    OGRFieldDefn oField( "FIELD_1", OFTString );
    poFeatureDefn->AddFieldDefn( &oField );

    iNextShapeId = 0;
    this->pszVal = pszVal ? CPLStrdup(pszVal) : NULL;
}

/************************************************************************/
/*                 ~OGROpenFileGDBSingleFeatureLayer()                  */
/************************************************************************/

OGROpenFileGDBSingleFeatureLayer::~OGROpenFileGDBSingleFeatureLayer()
{
    if( poFeatureDefn != NULL )
        poFeatureDefn->Release();
    CPLFree(pszVal);
}


/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature * OGROpenFileGDBSingleFeatureLayer::GetNextFeature()
{
    if (iNextShapeId != 0)
        return NULL;

    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
    if (pszVal)
        poFeature->SetField(0, pszVal);
    poFeature->SetFID(iNextShapeId ++);
    return poFeature;
}

/***********************************************************************/
/*                     OGROpenFileGDBSimpleSQLLayer                    */
/***********************************************************************/

class OGROpenFileGDBSimpleSQLLayer: public OGRLayer
{
        OGRLayer        *poBaseLayer;
        FileGDBIterator *poIter;
        OGRFeatureDefn  *poFeatureDefn;

    public:
        OGROpenFileGDBSimpleSQLLayer(OGRLayer* poBaseLayer,
                                     FileGDBIterator* poIter,
                                     int nColumns,
                                     swq_col_def* pasColDefs);
       ~OGROpenFileGDBSimpleSQLLayer();

       virtual void        ResetReading();
       virtual OGRFeature* GetNextFeature();
       virtual OGRFeature* GetFeature( GIntBig nFeatureId );
       virtual OGRFeatureDefn* GetLayerDefn() { return poFeatureDefn; }
       virtual int         TestCapability( const char * );
       virtual const char* GetFIDColumn() { return poBaseLayer->GetFIDColumn(); }
       virtual OGRErr      GetExtent( OGREnvelope *psExtent, int bForce )
                            { return poBaseLayer->GetExtent(psExtent, bForce); }
       virtual GIntBig     GetFeatureCount(int bForce);
};

/***********************************************************************/
/*                    OGROpenFileGDBSimpleSQLLayer()                   */
/***********************************************************************/

OGROpenFileGDBSimpleSQLLayer::OGROpenFileGDBSimpleSQLLayer(
                                            OGRLayer* poBaseLayer,
                                            FileGDBIterator* poIter,
                                            int nColumns,
                                            swq_col_def* pasColDefs) :
        poBaseLayer(poBaseLayer), poIter(poIter)
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
                CPLAssert(poFieldDefn != NULL ); /* already checked before */
                poFeatureDefn->AddFieldDefn(poFieldDefn);
            }
        }
    }
    SetDescription( poFeatureDefn->GetName() );
    ResetReading();
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
    if( poSrcFeature == NULL )
        return NULL;

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
    while(TRUE)
    {
        int nRow = poIter->GetNextRowSortedByValue();
        if( nRow < 0 )
            return NULL;
        OGRFeature* poFeature = GetFeature(nRow + 1);
        if( poFeature == NULL )
            return NULL;

        if( (m_poFilterGeom == NULL
             || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL ||
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
    if( m_poFilterGeom == NULL && m_poAttrQuery == NULL )
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
        return( m_poFilterGeom == NULL && m_poAttrQuery == NULL );
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
    if (EQUALN(pszSQLCommand, "GetLayerDefinition ", strlen("GetLayerDefinition ")))
    {
        OGROpenFileGDBLayer* poLayer = (OGROpenFileGDBLayer*)
            GetLayerByName(pszSQLCommand + strlen("GetLayerDefinition "));
        if (poLayer)
        {
            OGRLayer* poRet = new OGROpenFileGDBSingleFeatureLayer(
                "LayerDefinition", poLayer->GetXMLDefinition().c_str() );
            return poRet;
        }
        else
            return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Special case GetLayerMetadata                                   */
/* -------------------------------------------------------------------- */
    if (EQUALN(pszSQLCommand, "GetLayerMetadata ", strlen("GetLayerMetadata ")))
    {
        OGROpenFileGDBLayer* poLayer = (OGROpenFileGDBLayer*)
            GetLayerByName(pszSQLCommand + strlen("GetLayerMetadata "));
        if (poLayer)
        {
            OGRLayer* poRet = new OGROpenFileGDBSingleFeatureLayer(
                "LayerMetadata", poLayer->GetXMLDocumentation().c_str() );
            return poRet;
        }
        else
            return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Special case GetLayerAttrIndexUse (only for debugging purposes) */
/* -------------------------------------------------------------------- */
    if (EQUALN(pszSQLCommand, "GetLayerAttrIndexUse ", strlen("GetLayerAttrIndexUse ")))
    {
        OGROpenFileGDBLayer* poLayer = (OGROpenFileGDBLayer*)
            GetLayerByName(pszSQLCommand + strlen("GetLayerAttrIndexUse "));
        if (poLayer)
        {
            OGRLayer* poRet = new OGROpenFileGDBSingleFeatureLayer(
                "LayerAttrIndexUse", CPLSPrintf("%d", poLayer->GetAttrIndexUse()) );
            return poRet;
        }
        else
            return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Special case GetLayerSpatialIndexState (only for debugging purposes) */
/* -------------------------------------------------------------------- */
    if (EQUALN(pszSQLCommand, "GetLayerSpatialIndexState ", strlen("GetLayerSpatialIndexState ")))
    {
        OGROpenFileGDBLayer* poLayer = (OGROpenFileGDBLayer*)
            GetLayerByName(pszSQLCommand + strlen("GetLayerSpatialIndexState "));
        if (poLayer)
        {
            OGRLayer* poRet = new OGROpenFileGDBSingleFeatureLayer(
                "LayerSpatialIndexState", CPLSPrintf("%d", poLayer->GetSpatialIndexState()) );
            return poRet;
        }
        else
            return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Special case GetLastSQLUsedOptimizedImplementation (only for debugging purposes) */
/* -------------------------------------------------------------------- */
    if (EQUAL(pszSQLCommand, "GetLastSQLUsedOptimizedImplementation"))
    {
        OGRLayer* poRet = new OGROpenFileGDBSingleFeatureLayer(
            "GetLastSQLUsedOptimizedImplementation",
                    CPLSPrintf("%d", bLastSQLUsedOptimizedImplementation) );
        return poRet;
    }

    bLastSQLUsedOptimizedImplementation = FALSE;

/* -------------------------------------------------------------------- */
/*      Special cases for SQL optimizations                             */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszSQLCommand, "SELECT ", strlen("SELECT ")) &&
        (pszDialect == NULL || EQUAL(pszDialect, "") || EQUAL(pszDialect, "OGRSQL")) &&
        CSLTestBoolean(CPLGetConfigOption("OPENFILEGDB_USE_INDEX", "YES")) )
    {
        swq_select oSelect;
        if( oSelect.preparse(pszSQLCommand) != OGRERR_NONE )
            return NULL;

/* -------------------------------------------------------------------- */
/*      MIN/MAX/SUM/AVG/COUNT optimization                              */
/* -------------------------------------------------------------------- */
        if( oSelect.join_count == 0 && oSelect.poOtherSelect == NULL &&
            oSelect.table_count == 1 && oSelect.order_specs == 0 )
        {
            OGROpenFileGDBLayer* poLayer = 
                (OGROpenFileGDBLayer*)GetLayerByName( oSelect.table_defs[0].table_name);
            if( poLayer )
            {
                OGRMemLayer* poMemLayer = NULL;

                int i;
                for(i = 0; i < oSelect.result_columns; i ++ )
                {
                    swq_col_func col_func = oSelect.column_defs[i].col_func;
                    if( !(col_func == SWQCF_MIN || col_func == SWQCF_MAX ||
                          col_func == SWQCF_COUNT || col_func == SWQCF_AVG ||
                          col_func == SWQCF_SUM) )
                        break;

                    if( oSelect.column_defs[i].field_name == NULL )
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
                    int nCount = 0;
                    double dfSum = 0.0;
                    const OGRField* psField = NULL;
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
                        double dfMin = 0.0, dfMax = 0.0;
                        if( !poLayer->GetMinMaxSumCount(poFieldDefn, dfMin, dfMax,
                                                        dfSum, nCount) )
                            break;
                        psField = &sField;
                        if( col_func == SWQCF_AVG )
                        {
                            if( nCount == 0 )
                            {
                                eOutOGRType = OFTReal;
                                psField = NULL;
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

                    if( poMemLayer == NULL )
                    {
                        poMemLayer = new OGRMemLayer("SELECT", NULL, wkbNone);
                        OGRFeature* poFeature = new OGRFeature(poMemLayer->GetLayerDefn());
                        poMemLayer->CreateFeature(poFeature);
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
                    if( psField != NULL )
                    {
                        OGRFeature* poFeature = poMemLayer->GetFeature(0);
                        poFeature->SetField(oFieldDefn.GetNameRef(), (OGRField*) psField);
                        poMemLayer->SetFeature(poFeature);
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
                    bLastSQLUsedOptimizedImplementation = TRUE;
                    return poMemLayer;
                }
            }
        }

/* -------------------------------------------------------------------- */
/*      ORDER BY optimization                                           */
/* -------------------------------------------------------------------- */
        if( oSelect.join_count == 0 && oSelect.poOtherSelect == NULL &&
            oSelect.table_count == 1 && oSelect.order_specs == 1 )
        {
            OGROpenFileGDBLayer* poLayer = 
                (OGROpenFileGDBLayer*)GetLayerByName( oSelect.table_defs[0].table_name);
            if( poLayer != NULL &&
                poLayer->HasIndexForField(oSelect.order_defs[0].field_name) )
            {
                OGRErr eErr = OGRERR_NONE;
                if( oSelect.where_expr != NULL )
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
                    int i;
                    for(i = 0; i < oSelect.result_columns; i ++ )
                    {
                        if( oSelect.column_defs[i].col_func != SWQCF_NONE )
                            break;
                        if( oSelect.column_defs[i].field_name == NULL )
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
                    swq_op op = SWQ_UNKNOWN;
                    swq_expr_node* poValue = NULL;
                    if( oSelect.where_expr != NULL )
                    {
                        op = (swq_op)oSelect.where_expr->nOperation;
                        poValue = oSelect.where_expr->papoSubExpr[1];
                    }

                    FileGDBIterator *poIter = poLayer->BuildIndex(
                                    oSelect.order_defs[0].field_name,
                                    oSelect.order_defs[0].ascending_flag,
                                    op, poValue);

                    /* Check that they are no NULL values */
                    if( oSelect.where_expr == NULL &&
                        poIter->GetRowCount() != poLayer->GetFeatureCount(FALSE) )
                    {
                        delete poIter;
                        poIter = NULL;
                    }

                    if( poIter != NULL )
                    {
                        CPLDebug("OpenFileGDB", "Using OGROpenFileGDBSimpleSQLLayer");
                        bLastSQLUsedOptimizedImplementation = TRUE;
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
    if( strlen(pszFilenameWithoutPath) == strlen("a00000000.gdbtable") &&
        pszFilenameWithoutPath[0] == 'a' &&
        sscanf(pszFilenameWithoutPath, "a%08x.gdbtable", &nInterestTable) == 1 )
    {
        osFilenameRadix = CPLSPrintf("a%08x.", nInterestTable);
    }

    char** papszFiles = VSIReadDir(m_osDirName);
    CPLStringList osStringList;
    char** papszIter = papszFiles;
    for( ; papszIter != NULL && *papszIter != NULL ; papszIter ++ )
    {
        if( strcmp(*papszIter, ".") == 0 || strcmp(*papszIter, "..") == 0 )
            continue;
        if( osFilenameRadix.size() == 0 ||
            strncmp(*papszIter, osFilenameRadix, osFilenameRadix.size()) == 0 )
        {
            osStringList.AddString(CPLFormFilename(m_osDirName, *papszIter, NULL));
        }
    }
    CSLDestroy(papszFiles);
    return osStringList.StealList();
}
