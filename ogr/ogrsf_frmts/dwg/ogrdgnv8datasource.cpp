/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGR Driver for DGNv8
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
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

#include "ogr_dgnv8.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                        OGRDGNV8DataSource()                          */
/************************************************************************/

OGRDGNV8DataSource::OGRDGNV8DataSource(OGRDGNV8Services* poServices) :
    m_poServices(poServices),
    m_papoLayers(nullptr),
    m_nLayers(0),
    m_papszOptions(nullptr),
    m_poDb(static_cast<const OdRxObject*>(nullptr)),
    m_bUpdate(false),
    m_bModified(false)
{}

/************************************************************************/
/*                       ~OGRDGNV8DataSource()                          */
/************************************************************************/

OGRDGNV8DataSource::~OGRDGNV8DataSource()

{
    OGRDGNV8DataSource::FlushCache();

    for( int i = 0; i < m_nLayers; i++ )
        delete m_papoLayers[i];

    CPLFree( m_papoLayers );
    CSLDestroy( m_papszOptions );
}


/************************************************************************/
/*                              FlushCache()                            */
/************************************************************************/

void OGRDGNV8DataSource::FlushCache()
{
    if( m_poDb.isNull() || !m_bModified )
        return;
    m_bModified = false;

    for( int i = 0; i < m_nLayers; i++ )
    {
       m_papoLayers[i]->m_pModel->fitToView();
    }

    OdString oFilename( FromUTF8(GetDescription()) );
    try
    {
        m_poDb->writeFile( oFilename );
    }
    catch (const OdError& e)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Teigha DGN error occurred: %s",
                 ToUTF8(e.description()).c_str());
    }
    catch (const std::exception &exc)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "std::exception occurred: %s", exc.what());
    }
    catch (...)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unknown exception occurred");
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRDGNV8DataSource::Open( const char * pszFilename, bool bUpdate )

{
    SetDescription(pszFilename);
    
    OdString oFilename( FromUTF8(pszFilename) );
    try
    {
        m_poDb = m_poServices->readFile( oFilename );
    }
    catch (const OdError& e)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Teigha DGN error occurred: %s",
                 ToUTF8(e.description()).c_str());
        return FALSE;
    }
    catch (const std::exception &exc)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "std::exception occurred: %s", exc.what());
        return FALSE;
    }
    catch (...)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unknown exception occurred");
        return FALSE;
    }

    OdDgModelTablePtr pModelTable = m_poDb->getModelTable();
    if (pModelTable.isNull())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "No model table found");
        return FALSE;
    }

    // Loop over models
    OdDgElementIteratorPtr pIter = pModelTable->createIterator();
    for ( ; !pIter.isNull() && !pIter->done(); pIter->step() )
    {
        OdDgModelPtr pModel = OdDgModel::cast(
                pIter->item().openObject( 
                    bUpdate ? OdDg::kForWrite : OdDg::kForRead ) );
        if ( !pModel.isNull() )
        {
            OGRDGNV8Layer* poLayer = new OGRDGNV8Layer(this, pModel);
            m_papoLayers = static_cast<OGRDGNV8Layer**>(
                    CPLRealloc(m_papoLayers,
                               sizeof(OGRDGNV8Layer*) * (m_nLayers + 1)));
            m_papoLayers[ m_nLayers++ ] = poLayer;
        }
    }

    m_bUpdate = bUpdate;

    return m_bUpdate || m_nLayers > 0;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDGNV8DataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return m_bUpdate;
    else if( EQUAL(pszCap,ODsCCurveGeometries) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRDGNV8DataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= m_nLayers )
        return nullptr;

    return m_papoLayers[iLayer];
}

/************************************************************************/
/*                         EraseSubElements()                           */
/************************************************************************/

template<class T> static void EraseSubElements(T container)
{
    if( !container.isNull() )
    {
        OdDgElementIteratorPtr pIter = container->createIterator();
        for(; !pIter.isNull() && !pIter->done(); pIter->step() )
        {
            OdDgElementPtr pElement =
                pIter->item().openObject(OdDg::kForWrite);
            if( !pElement.isNull() )
            {
                pElement->erase(true);
            }
        }
    }
}

/************************************************************************/
/*                          InitWithSeed()                              */
/************************************************************************/

void OGRDGNV8DataSource::InitWithSeed()
{
#if 0
    EraseSubElements(m_poDb->getLevelTable(OdDg::kForWrite));
#endif

    if( !CPLTestBool(CSLFetchNameValueDef(
            m_papszOptions, "COPY_SEED_FILE_COLOR_TABLE", "NO")) )
    {
        OdDgColorTablePtr colorTable = m_poDb->getColorTable(OdDg::kForWrite);
        if( !colorTable.isNull() )
        {
            const ODCOLORREF* defColors = OdDgColorTable::defaultPalette();
            OdArray<ODCOLORREF> palette;
            palette.insert(palette.begin(), defColors, defColors + 256);
            colorTable->setPalette(palette);
        }
    }
    
    OdDgModelTablePtr pModelTable = m_poDb->getModelTable();

    if( CPLTestBool(CSLFetchNameValueDef(
            m_papszOptions, "COPY_SEED_FILE_MODEL", "YES")) )
    {
        if( !pModelTable.isNull() )
        {
            OdDgElementIteratorPtr pIter = pModelTable->createIterator();
            for(; !pIter.isNull() && !pIter->done(); pIter->step() )
            {
                OdDgModelPtr pModel = OdDgModel::cast(
                    pIter->item().openObject( OdDg::kForWrite ) );
                if( !pModel.isNull() )
                {
                    OdDgElementIteratorPtr pIter2 =
                        pModel->createGraphicsElementsIterator();
                    for(; !pIter2.isNull() && !pIter2->done(); pIter2->step() )
                    {
                        OdDgElementPtr pElement =
                            pIter2->item().openObject(OdDg::kForWrite);
                        if( !pElement.isNull() )
                        {
                            pElement->erase(true);
                        }
                    }

                    if( !CPLTestBool(CSLFetchNameValueDef(
                            m_papszOptions,
                            "COPY_SEED_FILE_MODEL_CONTROL_ELEMENTS", "YES")) )
                    {
                        pIter2 =
                            pModel->createControlElementsIterator();
                        for(; !pIter2.isNull() && !pIter2->done();
                            pIter2->step() )
                        {
                            OdDgElementPtr pElement =
                                pIter2->item().openObject(OdDg::kForWrite);
                            if( !pElement.isNull() )
                            {
                                pElement->erase(true);
                            }
                        }
                    }
                }
            }
        }
    }
    else
    {
        // Erase existing models
        EraseSubElements(pModelTable);

        // Recreate a new model and bind it as default
        OdDgModelPtr model = OdDgModel::createObject();
        pModelTable->add( model );
        
        m_poDb->setActiveModelId( model->elementId() );
        m_poDb->setDefaultModelId( model->elementId() );

        // Erase existing views
        EraseSubElements(m_poDb->getNamedViewTable());
        OdDgViewGroupTablePtr pViewGroupTable = m_poDb->getViewGroupTable();
        EraseSubElements(pViewGroupTable);

        // Recreate a new view group and bind it as default
        model->createViewGroup();

        OdDgElementIteratorPtr pIter = pViewGroupTable->createIterator();
        m_poDb->setActiveViewGroupId(pIter->item());
    }

#if 0
    CPLString osTmpFile(CPLString(GetDescription()) + ".tmp");
    OdString odTmpFile( FromUTF8( osTmpFile ) );
    m_poDb->writeFile( odTmpFile );
    m_poDb = m_poServices->readFile( odTmpFile );
    VSIUnlink(osTmpFile);
#endif
}

/************************************************************************/
/*                            FillMD()                                  */
/************************************************************************/

static void FillMD( CPLStringList& osDGNMD, const char* pszKey, OdString str )
{
    CPLString osVal( OGRDGNV8DataSource::ToUTF8(str) );
    if( !osVal.empty() )
        osDGNMD.SetNameValue(pszKey, osVal);
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **OGRDGNV8DataSource::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "DGN", nullptr);
}

/************************************************************************/
/*                          GetMetadata()                              */
/************************************************************************/

char** OGRDGNV8DataSource::GetMetadata(const char* pszDomain)
{
    if( pszDomain != nullptr && EQUAL(pszDomain, "DGN") )
    {
        m_osDGNMD.Clear();
        OdDgSummaryInformationPtr summary = oddgGetSummaryInformation(m_poDb);
        FillMD( m_osDGNMD, "APPLICATION", summary->getApplicationName() );
        FillMD( m_osDGNMD, "TITLE", summary->getTitle() );
        FillMD( m_osDGNMD, "SUBJECT", summary->getSubject() );
        FillMD( m_osDGNMD, "AUTHOR", summary->getAuthor() );
        FillMD( m_osDGNMD, "KEYWORDS", summary->getKeywords() );
        FillMD( m_osDGNMD, "TEMPLATE", summary->getTemplate() );
        FillMD( m_osDGNMD, "COMMENTS", summary->getComments() );
        FillMD( m_osDGNMD, "LAST_SAVED_BY", summary->getLastSavedBy() );
        FillMD( m_osDGNMD, "REVISION_NUMBER", summary->getRevisionNumber() );
        OdDgDocumentSummaryInformationPtr docSummaryInfo =
                                oddgGetDocumentSummaryInformation(m_poDb);
        FillMD( m_osDGNMD, "CATEGORY", docSummaryInfo->getCategory() );
        FillMD( m_osDGNMD, "MANAGER", docSummaryInfo->getManager() );
        FillMD( m_osDGNMD, "COMPANY", docSummaryInfo->getCompany() );
        return m_osDGNMD.List();
    }
    return GDALDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                        GetMetadataItem()                             */
/************************************************************************/

const char* OGRDGNV8DataSource::GetMetadataItem(const char* pszName,
                                                const char* pszDomain)
{
    return CSLFetchNameValue( GetMetadata(pszDomain), pszName );
}

/************************************************************************/
/*                             PreCreate()                              */
/*                                                                      */
/*      Called by OGRDGNV8DriverCreate() method to setup a stub         */
/*      OGRDataSource object without the associated file created        */
/*      yet.                                                            */
/************************************************************************/

bool OGRDGNV8DataSource::PreCreate( const char *pszFilename,
                                    char **papszOptionsIn )

{
    m_bUpdate = true;
    m_bModified = true;
    m_papszOptions = CSLDuplicate( papszOptionsIn );
    SetDescription( pszFilename );
    
    VSILFILE* fp = VSIFOpenL(pszFilename, "wb");
    if( fp == nullptr )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Cannot write %s", pszFilename);
        return false;
    }
    VSIFCloseL(fp);
    
    const char* pszSeed = CSLFetchNameValue(m_papszOptions, "SEED");
    
    try
    {
        if( pszSeed )
            m_poDb = m_poServices->readFile( FromUTF8(pszSeed) );
        else
            m_poDb = m_poServices->createDatabase();
        
        if( pszSeed )
        {
            InitWithSeed();
        }
    }
    catch (const OdError& e)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Teigha DGN error occurred: %s",
                 ToUTF8(e.description()).c_str());
        return false;
    }
    catch (const std::exception &exc)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "std::exception occurred: %s", exc.what());
        return false;
    }
    catch (...)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unknown exception occurred");
        return false;
    }

    OdDgSummaryInformationPtr summary = oddgGetSummaryInformation(m_poDb);
    CPLString osDefaultAppName("GDAL ");
    osDefaultAppName += GDALVersionInfo("RELEASE_NAME");
    osDefaultAppName += " with " + ToUTF8(summary->getApplicationName());
    const char* pszAppName = CSLFetchNameValue(m_papszOptions,
                                                  "APPLICATION");
    if( pszSeed == nullptr && pszAppName == nullptr )
        pszAppName = osDefaultAppName;
    if( pszAppName )
        summary->setApplicationName(FromUTF8(pszAppName));

    const char* pszTitle = CSLFetchNameValue(m_papszOptions, "TITLE");
    if( pszTitle )
        summary->setTitle(FromUTF8(pszTitle));

    const char* pszSubject = CSLFetchNameValue(m_papszOptions, "SUBJECT");
    if( pszSubject )
        summary->setSubject(FromUTF8(pszSubject));

    const char* pszAuthor = CSLFetchNameValue(m_papszOptions, "AUTHOR");
    if( pszAuthor )
        summary->setAuthor(FromUTF8(pszAuthor));

    const char* pszKeywords = CSLFetchNameValue(m_papszOptions, "KEYWORDS");
    if( pszKeywords )
        summary->setKeywords(FromUTF8(pszKeywords));

    const char* pszTemplate = CSLFetchNameValue(m_papszOptions, "TEMPLATE");
    if( pszTemplate )
        summary->setTemplate(FromUTF8(pszTemplate));

    const char* pszComments = CSLFetchNameValue(m_papszOptions, "COMMENTS");
    if( pszComments )
        summary->setComments(FromUTF8(pszComments));

    const char* pszLastSavedBy = CSLFetchNameValue(m_papszOptions,
                                                   "LAST_SAVED_BY");
    if( pszLastSavedBy )
        summary->setLastSavedBy(FromUTF8(pszLastSavedBy));

    const char* pszRevisionNumber = CSLFetchNameValue(m_papszOptions,
                                                      "REVISION_NUMBER");
    if( pszRevisionNumber )
        summary->setRevisionNumber(FromUTF8(pszRevisionNumber));

    OdDgDocumentSummaryInformationPtr docSummaryInfo =
        oddgGetDocumentSummaryInformation(m_poDb);

    const char* pszCategory = CSLFetchNameValue(m_papszOptions, "CATEGORY");
    if( pszCategory )
        docSummaryInfo->setCategory(FromUTF8(pszCategory));

    const char* pszManager = CSLFetchNameValue(m_papszOptions, "MANAGER");
    if( pszManager )
        docSummaryInfo->setManager(FromUTF8(pszManager));

    const char* pszCompany = CSLFetchNameValue(m_papszOptions, "COMPANY");
    if( pszCompany )
        docSummaryInfo->setCompany(FromUTF8(pszCompany));

    return true;
}

/************************************************************************/
/*                             ToUTF8()                                 */
/************************************************************************/

CPLString OGRDGNV8DataSource::ToUTF8(const OdString& str)
{
    CPL_STATIC_ASSERT( sizeof(OdChar) == sizeof(wchar_t) );
    char* pszUTF8 = CPLRecodeFromWChar(
        reinterpret_cast<const wchar_t*>(str.c_str()),
        "WCHAR_T",
        CPL_ENC_UTF8);
    CPLString osRet(pszUTF8);
    CPLFree(pszUTF8);
    return osRet;    
}

/************************************************************************/
/*                            FromUTF8()                                */
/************************************************************************/

OdString OGRDGNV8DataSource::FromUTF8(const CPLString& str)
{
    CPL_STATIC_ASSERT( sizeof(OdChar) == sizeof(wchar_t) );
    OdChar* pwszWide = reinterpret_cast<OdChar*>(CPLRecodeToWChar(
        str.c_str(),
        CPL_ENC_UTF8,
        "WCHAR_T"));
    OdString osRet(pwszWide);
    CPLFree(pwszWide);
    return osRet;    
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *OGRDGNV8DataSource::ICreateLayer( const char *pszLayerName,
                                            OGRSpatialReference * /*poSRS*/,
                                            OGRwkbGeometryType /*eGeomType*/,
                                            char **papszOptions )

{
    if( !m_bUpdate )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "CreateLayer() only supported on update mode." );
        return nullptr;
    }

    OdDgModelPtr model;
    try
    {
        OdDgModelTablePtr pModelTable = m_poDb->getModelTable(OdDg::kForWrite);
        // First try to find a model that matches the layer name (case of a seed
        // file)
        OdDgElementIteratorPtr pIter = pModelTable->createIterator();
        for ( ; !pIter->done(); pIter->step() )
        {
            OdDgModelPtr pModel = OdDgModel::cast(
                    pIter->item().openObject( OdDg::kForWrite ) );
            if ( !pModel.isNull() )
            {
                if( ToUTF8(pModel->getName()) == CPLString(pszLayerName) )
                {
                    model = pModel;
                    break;
                }
            }
        }
        // If we don't find a match, but there's at least one model, pick
        // the default one
        if( model.isNull() && m_nLayers == 0 )
            model = m_poDb->getActiveModelId().openObject( OdDg::kForWrite );
        if( model.isNull() )
        {
            model = OdDgModel::createObject();
            pModelTable->add( model );
        }
        
        const char* pszDim = CSLFetchNameValue(papszOptions, "DIM");
        if( pszDim != nullptr )
        {
            model->setModelIs3dFlag( EQUAL(pszDim, "3") );
        }

        model->setWorkingUnit( OdDgModel::kWuMasterUnit );
            
        model->setName( FromUTF8(pszLayerName) );
        
        const char* pszDescription = CSLFetchNameValue(papszOptions,
                                                       "DESCRIPTION");
        if( pszDescription )
            model->setDescription( FromUTF8(pszDescription) );
    }
    catch (const OdError& e)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Teigha DGN error occurred: %s",
                 ToUTF8(e.description()).c_str());
        return nullptr;
    }
    catch (const std::exception &exc)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "std::exception occurred: %s", exc.what());
        return nullptr;
    }
    catch (...)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unknown exception occurred");
        return nullptr;
    }
    
    m_bModified = true;

    OGRDGNV8Layer* poLayer = new OGRDGNV8Layer(this, model);
    m_papoLayers = static_cast<OGRDGNV8Layer**>(
                    CPLRealloc(m_papoLayers,
                               sizeof(OGRDGNV8Layer*) * (m_nLayers + 1)));
    m_papoLayers[ m_nLayers++ ] = poLayer;
    return poLayer;
}
