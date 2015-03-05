/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implement OGRDataSourceWithTransaction class
 * Author:   Even Rouault, even dot rouault at spatialys dot com
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
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

#include "ogremulatedtransaction.h"
#include "ogrlayerdecorator.h"
#include <map>
#include <set>

CPL_CVSID("$Id$");

class OGRDataSourceWithTransaction;

class OGRLayerWithTransaction: public OGRLayerDecorator
{
    protected:
        friend class OGRDataSourceWithTransaction;

        OGRDataSourceWithTransaction* m_poDS;
        OGRFeatureDefn* m_poFeatureDefn;
    
    public:
        
        OGRLayerWithTransaction(OGRDataSourceWithTransaction* poDS,
                                OGRLayer* poBaseLayer);
       ~OGRLayerWithTransaction();

    virtual const char *GetName() { return GetDescription(); }
    virtual OGRFeatureDefn *GetLayerDefn();

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
    virtual OGRErr      DeleteField( int iField );
    virtual OGRErr      ReorderFields( int* panMap );
    virtual OGRErr      AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags );
};


class OGRDataSourceWithTransaction : public OGRDataSource
{
  protected:
    OGRDataSource *m_poBaseDataSource;
    IOGRTransactionBehaviour* m_poTransactionBehaviour;
    int            m_bHasOwnershipDataSource;
    int            m_bHasOwnershipTransactionBehaviour;
    int            m_bInTransaction;

    std::map<CPLString, OGRLayerWithTransaction* > m_oMapLayers;
    std::set<OGRLayerWithTransaction*> m_oSetLayers;
    std::set<OGRLayer*> m_oSetExecuteSQLLayers;

    OGRLayer*     WrapLayer(OGRLayer* poLayer);
    void          RemapLayers();

  public:

                 OGRDataSourceWithTransaction(OGRDataSource* poBaseDataSource,
                                          IOGRTransactionBehaviour* poTransactionBehaviour,
                                          int bTakeOwnershipDataSource,
                                          int bTakeOwnershipTransactionBehaviour);

    virtual     ~OGRDataSourceWithTransaction();
    
    int                 IsInTransaction() const { return m_bInTransaction; }

    virtual const char  *GetName();

    virtual int         GetLayerCount() ;
    virtual OGRLayer    *GetLayer(int);
    virtual OGRLayer    *GetLayerByName(const char *);
    virtual OGRErr      DeleteLayer(int);

    virtual int         TestCapability( const char * );

    virtual OGRLayer   *ICreateLayer( const char *pszName, 
                                     OGRSpatialReference *poSpatialRef = NULL,
                                     OGRwkbGeometryType eGType = wkbUnknown,
                                     char ** papszOptions = NULL );
    virtual OGRLayer   *CopyLayer( OGRLayer *poSrcLayer, 
                                   const char *pszNewName, 
                                   char **papszOptions = NULL );

    virtual OGRStyleTable *GetStyleTable();
    virtual void        SetStyleTableDirectly( OGRStyleTable *poStyleTable );
                            
    virtual void        SetStyleTable(OGRStyleTable *poStyleTable);

    virtual OGRLayer *  ExecuteSQL( const char *pszStatement,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect );
    virtual void        ReleaseResultSet( OGRLayer * poResultsSet );
    
    virtual void        FlushCache();

    virtual OGRErr      StartTransaction(int bForce=FALSE);
    virtual OGRErr      CommitTransaction();
    virtual OGRErr      RollbackTransaction();

    virtual char      **GetMetadata( const char * pszDomain = "" );
    virtual CPLErr      SetMetadata( char ** papszMetadata,
                                     const char * pszDomain = "" );
    virtual const char *GetMetadataItem( const char * pszName,
                                         const char * pszDomain = "" );
    virtual CPLErr      SetMetadataItem( const char * pszName,
                                         const char * pszValue,
                                         const char * pszDomain = "" );
};

/************************************************************************/
/*                         ~IOGRTransactionBehaviour                    */
/************************************************************************/

IOGRTransactionBehaviour::~IOGRTransactionBehaviour()
{
}

/************************************************************************/
/*              OGRCreateEmulatedTransactionDataSourceWrapper()         */
/************************************************************************/

OGRDataSource* OGRCreateEmulatedTransactionDataSourceWrapper(
                                OGRDataSource* poBaseDataSource,
                                IOGRTransactionBehaviour* poTransactionBehaviour,
                                int bTakeOwnershipDataSource,
                                int bTakeOwnershipTransactionBehaviour)
{
    return new OGRDataSourceWithTransaction(poBaseDataSource,
                                            poTransactionBehaviour,
                                            bTakeOwnershipDataSource,
                                            bTakeOwnershipTransactionBehaviour);
}


/************************************************************************/
/*                      OGRDataSourceWithTransaction                    */
/************************************************************************/

OGRDataSourceWithTransaction::OGRDataSourceWithTransaction(
                                OGRDataSource* poBaseDataSource,
                                IOGRTransactionBehaviour* poTransactionBehaviour,
                                int bTakeOwnershipDataSource,
                                int bTakeOwnershipTransactionBehaviour) :
            m_poBaseDataSource(poBaseDataSource),
            m_poTransactionBehaviour(poTransactionBehaviour),
            m_bHasOwnershipDataSource(bTakeOwnershipDataSource),
            m_bHasOwnershipTransactionBehaviour(bTakeOwnershipTransactionBehaviour),
            m_bInTransaction(FALSE)
{
}

OGRDataSourceWithTransaction::~OGRDataSourceWithTransaction()
{
    std::set<OGRLayerWithTransaction*>::iterator oIter = m_oSetLayers.begin();
    for(; oIter != m_oSetLayers.end(); ++oIter )
        delete *oIter;

    if( m_bHasOwnershipDataSource )
        delete m_poBaseDataSource;
    if( m_bHasOwnershipTransactionBehaviour )
        delete m_poTransactionBehaviour;
}


OGRLayer* OGRDataSourceWithTransaction::WrapLayer(OGRLayer* poLayer)
{
    if( poLayer )
    {
        OGRLayer* poWrappedLayer = m_oMapLayers[poLayer->GetName()];
        if( poWrappedLayer )
            poLayer = poWrappedLayer;
        else
        {
            OGRLayerWithTransaction* poWrappedLayer = new OGRLayerWithTransaction(this,poLayer);
            m_oMapLayers[poLayer->GetName()] = poWrappedLayer;
            m_oSetLayers.insert(poWrappedLayer);
            poLayer = poWrappedLayer;
        }
    }
    return poLayer;
}

void OGRDataSourceWithTransaction::RemapLayers()
{
    std::set<OGRLayerWithTransaction*>::iterator oIter = m_oSetLayers.begin();
    for(; oIter != m_oSetLayers.end(); ++oIter )
    {
        OGRLayerWithTransaction* poWrappedLayer = *oIter;
        if( m_poBaseDataSource == NULL )
            poWrappedLayer->m_poDecoratedLayer = NULL;
        else
        {
            OGRFeatureDefn* poOldFeatureDefn = poWrappedLayer->m_poFeatureDefn;
            poWrappedLayer->m_poDecoratedLayer =
                m_poBaseDataSource->GetLayerByName(poWrappedLayer->GetName());
            if( poOldFeatureDefn != NULL )
            {
                if( poWrappedLayer->m_poDecoratedLayer != NULL )
                {
#ifdef DEBUG
                    int nRefCount = poOldFeatureDefn->GetReferenceCount();
#endif
                    m_poTransactionBehaviour->ReadoptOldFeatureDefn(m_poBaseDataSource,
                                                                    poWrappedLayer->m_poDecoratedLayer,
                                                                    poOldFeatureDefn);
#ifdef DEBUG
                    CPLAssert(poWrappedLayer->m_poDecoratedLayer->GetLayerDefn() == poOldFeatureDefn);
                    CPLAssert(poOldFeatureDefn->GetReferenceCount() == nRefCount + 1 );
#endif
                }
            }
        }
    }
    m_oMapLayers.clear();
}

const char  *OGRDataSourceWithTransaction::GetName()
{
    if( !m_poBaseDataSource ) return "";
    return m_poBaseDataSource->GetName();
}

int         OGRDataSourceWithTransaction::GetLayerCount()
{
    if( !m_poBaseDataSource ) return 0;
    return m_poBaseDataSource->GetLayerCount();
}

OGRLayer    *OGRDataSourceWithTransaction::GetLayer(int iIndex)
{
    if( !m_poBaseDataSource ) return NULL;
    return WrapLayer(m_poBaseDataSource->GetLayer(iIndex));
    
}

OGRLayer    *OGRDataSourceWithTransaction::GetLayerByName(const char *pszName)
{
    if( !m_poBaseDataSource ) return NULL;
    return WrapLayer(m_poBaseDataSource->GetLayerByName(pszName));
}

OGRErr      OGRDataSourceWithTransaction::DeleteLayer(int iIndex)
{
    if( !m_poBaseDataSource ) return OGRERR_FAILURE;
    OGRLayer* poLayer = GetLayer(iIndex);
    CPLString osName;
    if( poLayer )
        osName = poLayer->GetName();
    OGRErr eErr = m_poBaseDataSource->DeleteLayer(iIndex);
    if( eErr == OGRERR_NONE && osName.size())
    {
        std::map<CPLString, OGRLayerWithTransaction*>::iterator oIter = m_oMapLayers.find(osName);
        if(oIter != m_oMapLayers.end())
        {
            delete oIter->second;
            m_oSetLayers.erase(oIter->second);
            m_oMapLayers.erase(oIter);
        }
    }
    return eErr;
}

int         OGRDataSourceWithTransaction::TestCapability( const char * pszCap )
{
    if( !m_poBaseDataSource ) return FALSE;

    if( EQUAL(pszCap,ODsCEmulatedTransactions) )
        return TRUE;

    return m_poBaseDataSource->TestCapability(pszCap);
}

OGRLayer   *OGRDataSourceWithTransaction::ICreateLayer( const char *pszName, 
                                     OGRSpatialReference *poSpatialRef,
                                     OGRwkbGeometryType eGType,
                                     char ** papszOptions)
{
    if( !m_poBaseDataSource ) return NULL;
    return WrapLayer(m_poBaseDataSource->CreateLayer(pszName, poSpatialRef, eGType, papszOptions));
}

OGRLayer   *OGRDataSourceWithTransaction::CopyLayer( OGRLayer *poSrcLayer, 
                                   const char *pszNewName, 
                                   char **papszOptions )
{
    if( !m_poBaseDataSource ) return NULL;
    return WrapLayer(m_poBaseDataSource->CopyLayer(poSrcLayer, pszNewName, papszOptions ));
}

OGRStyleTable *OGRDataSourceWithTransaction::GetStyleTable()
{
    if( !m_poBaseDataSource ) return NULL;
    return m_poBaseDataSource->GetStyleTable();
}

void        OGRDataSourceWithTransaction::SetStyleTableDirectly( OGRStyleTable *poStyleTable )
{
    if( !m_poBaseDataSource ) return;
    m_poBaseDataSource->SetStyleTableDirectly(poStyleTable);
}

void        OGRDataSourceWithTransaction::SetStyleTable(OGRStyleTable *poStyleTable)
{
    if( !m_poBaseDataSource ) return;
    m_poBaseDataSource->SetStyleTable(poStyleTable);
}

OGRLayer *  OGRDataSourceWithTransaction::ExecuteSQL( const char *pszStatement,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect )
{
    if( !m_poBaseDataSource ) return NULL;
    OGRLayer* poLayer = m_poBaseDataSource->ExecuteSQL(pszStatement, poSpatialFilter,
                                                       pszDialect);
    if( poLayer != NULL )
        m_oSetExecuteSQLLayers.insert(poLayer);
    return poLayer;
}

void        OGRDataSourceWithTransaction::ReleaseResultSet( OGRLayer * poResultsSet )
{
    if( !m_poBaseDataSource ) return;
    m_oSetExecuteSQLLayers.erase(poResultsSet);
    m_poBaseDataSource->ReleaseResultSet(poResultsSet);
}

void      OGRDataSourceWithTransaction::FlushCache()
{
    if( !m_poBaseDataSource ) return;
    return m_poBaseDataSource->FlushCache();
}

OGRErr OGRDataSourceWithTransaction::StartTransaction(int bForce)
{
    if( !m_poBaseDataSource ) return OGRERR_FAILURE;
    if( !bForce )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Transactions only supported in forced mode");
        return OGRERR_UNSUPPORTED_OPERATION;
    }
    if( m_oSetExecuteSQLLayers.size() != 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot start transaction while a layer returned by "
                 "ExecuteSQL() hasn't been released.");
        return OGRERR_FAILURE;
    }
    if( m_bInTransaction )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Transaction is already in progress");
        return OGRERR_FAILURE;
    }
    int bHasReopenedDS = FALSE;
    OGRErr eErr =
        m_poTransactionBehaviour->StartTransaction(m_poBaseDataSource, bHasReopenedDS);
    if( bHasReopenedDS )
        RemapLayers();
    if( eErr == OGRERR_NONE )
        m_bInTransaction = TRUE;
    return eErr;
}

OGRErr OGRDataSourceWithTransaction::CommitTransaction()
{
    if( !m_poBaseDataSource ) return OGRERR_FAILURE;
    if( !m_bInTransaction )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "No transaction in progress");
        return OGRERR_FAILURE;
    }
    if( m_oSetExecuteSQLLayers.size() != 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot interrupt transaction while a layer returned by "
                 "ExecuteSQL() hasn't been released.");
        return OGRERR_FAILURE;
    }
    m_bInTransaction = FALSE;
    int bHasReopenedDS = FALSE;
    OGRErr eErr =
        m_poTransactionBehaviour->CommitTransaction(m_poBaseDataSource, bHasReopenedDS);
    if( bHasReopenedDS )
        RemapLayers();
    return eErr;
}

OGRErr OGRDataSourceWithTransaction::RollbackTransaction()
{
    if( !m_poBaseDataSource ) return OGRERR_FAILURE;
    if( !m_bInTransaction )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "No transaction in progress");
        return OGRERR_FAILURE;
    }
    if( m_oSetExecuteSQLLayers.size() != 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot interrupt transaction while a layer returned by "
                 "ExecuteSQL() hasn't been released.");
        return OGRERR_FAILURE;
    }
    m_bInTransaction = FALSE;
    int bHasReopenedDS = FALSE;
    OGRErr eErr =
        m_poTransactionBehaviour->RollbackTransaction(m_poBaseDataSource, bHasReopenedDS);
    if( bHasReopenedDS )
        RemapLayers();
    return eErr;
}

char      **OGRDataSourceWithTransaction::GetMetadata( const char * pszDomain )
{
    if( !m_poBaseDataSource ) return NULL;
    return m_poBaseDataSource->GetMetadata(pszDomain);
}

CPLErr      OGRDataSourceWithTransaction::SetMetadata( char ** papszMetadata,
                                          const char * pszDomain )
{
    if( !m_poBaseDataSource ) return CE_Failure;
    return m_poBaseDataSource->SetMetadata(papszMetadata, pszDomain);
}

const char *OGRDataSourceWithTransaction::GetMetadataItem( const char * pszName,
                                              const char * pszDomain )
{
    if( !m_poBaseDataSource ) return NULL;
    return m_poBaseDataSource->GetMetadataItem(pszName, pszDomain);
}

CPLErr      OGRDataSourceWithTransaction::SetMetadataItem( const char * pszName,
                                              const char * pszValue,
                                              const char * pszDomain )
{
    if( !m_poBaseDataSource ) return CE_Failure;
    return m_poBaseDataSource->SetMetadataItem(pszName, pszValue, pszDomain);
}


/************************************************************************/
/*                       OGRLayerWithTransaction                        */
/************************************************************************/

OGRLayerWithTransaction::OGRLayerWithTransaction(
                    OGRDataSourceWithTransaction* poDS, OGRLayer* poBaseLayer):
    OGRLayerDecorator(poBaseLayer, FALSE),
    m_poDS(poDS),
    m_poFeatureDefn(NULL)
{
}

OGRLayerWithTransaction::~OGRLayerWithTransaction()
{
    if( m_poFeatureDefn )
        m_poFeatureDefn->Release();
}

OGRFeatureDefn *OGRLayerWithTransaction::GetLayerDefn()
{
    if( !m_poDecoratedLayer )
    {
        if( m_poFeatureDefn == NULL )
        {
            m_poFeatureDefn = new OGRFeatureDefn(GetDescription());
            m_poFeatureDefn->Reference();
        }
        return m_poFeatureDefn;
    }
    if( m_poFeatureDefn == NULL )
    {
        m_poFeatureDefn = m_poDecoratedLayer->GetLayerDefn();
        if( m_poFeatureDefn )
            m_poFeatureDefn->Reference();
    }
    return m_poFeatureDefn;
}

OGRErr      OGRLayerWithTransaction::CreateField( OGRFieldDefn *poField,
                                            int bApproxOK )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    if( m_poFeatureDefn != NULL && m_poDS->IsInTransaction() )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Layer structural changes not allowed within emulated transaction");
        return OGRERR_FAILURE;
    }
    return m_poDecoratedLayer->CreateField(poField, bApproxOK);
}

OGRErr      OGRLayerWithTransaction::DeleteField( int iField )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    if( m_poFeatureDefn != NULL && m_poDS->IsInTransaction() )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Layer structural changes not allowed within emulated transaction");
        return OGRERR_FAILURE;
    }
    return m_poDecoratedLayer->DeleteField(iField);
}

OGRErr      OGRLayerWithTransaction::ReorderFields( int* panMap )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    if( m_poFeatureDefn != NULL && m_poDS->IsInTransaction() )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Layer structural changes not allowed within emulated transaction");
        return OGRERR_FAILURE;
    }
    return m_poDecoratedLayer->ReorderFields(panMap);
}

OGRErr      OGRLayerWithTransaction::AlterFieldDefn( int iField,
                                                     OGRFieldDefn* poNewFieldDefn,
                                                     int nFlags )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    if( m_poFeatureDefn != NULL && m_poDS->IsInTransaction() )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Layer structural changes not allowed within emulated transaction");
        return OGRERR_FAILURE;
    }
    return m_poDecoratedLayer->AlterFieldDefn(iField, poNewFieldDefn, nFlags);
}

