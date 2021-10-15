/******************************************************************************
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

CPL_CVSID("$Id$")

class OGRDataSourceWithTransaction;

class OGRLayerWithTransaction final: public OGRLayerDecorator
{
        CPL_DISALLOW_COPY_ASSIGN(OGRLayerWithTransaction)

    protected:
        friend class OGRDataSourceWithTransaction;

        OGRDataSourceWithTransaction* m_poDS;
        OGRFeatureDefn* m_poFeatureDefn;

    public:

        OGRLayerWithTransaction(OGRDataSourceWithTransaction* poDS,
                                OGRLayer* poBaseLayer);
    virtual ~OGRLayerWithTransaction() override;

    virtual const char *GetName() override { return GetDescription(); }
    virtual OGRFeatureDefn *GetLayerDefn() override;

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE ) override;
    virtual OGRErr      DeleteField( int iField ) override;
    virtual OGRErr      ReorderFields( int* panMap ) override;
    virtual OGRErr      AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags ) override;

    virtual OGRErr      CreateGeomField( OGRGeomFieldDefn *poField,
                                     int bApproxOK = TRUE ) override;

    virtual OGRFeature *GetNextFeature() override;
    virtual OGRFeature *GetFeature( GIntBig nFID ) override;
    virtual OGRErr      ISetFeature( OGRFeature *poFeature ) override;
    virtual OGRErr      ICreateFeature( OGRFeature *poFeature ) override;
};

class OGRDataSourceWithTransaction final: public OGRDataSource
{
        CPL_DISALLOW_COPY_ASSIGN(OGRDataSourceWithTransaction)

  protected:
    OGRDataSource *m_poBaseDataSource;
    IOGRTransactionBehaviour* m_poTransactionBehavior;
    int            m_bHasOwnershipDataSource;
    int            m_bHasOwnershipTransactionBehavior;
    int            m_bInTransaction;

    std::map<CPLString, OGRLayerWithTransaction* > m_oMapLayers{};
    std::set<OGRLayerWithTransaction*> m_oSetLayers{};
    std::set<OGRLayer*> m_oSetExecuteSQLLayers{};

    OGRLayer*     WrapLayer(OGRLayer* poLayer);
    void          RemapLayers();

  public:

                 OGRDataSourceWithTransaction(OGRDataSource* poBaseDataSource,
                                          IOGRTransactionBehaviour* poTransactionBehaviour,
                                          int bTakeOwnershipDataSource,
                                          int bTakeOwnershipTransactionBehavior);

    virtual     ~OGRDataSourceWithTransaction() override;

    int                 IsInTransaction() const { return m_bInTransaction; }

    virtual const char  *GetName() override;

    virtual int         GetLayerCount() override ;
    virtual OGRLayer    *GetLayer(int) override;
    virtual OGRLayer    *GetLayerByName(const char *) override;
    virtual OGRErr      DeleteLayer(int) override;
    virtual bool        IsLayerPrivate(int iLayer) const override;

    virtual int         TestCapability( const char * ) override;

    virtual OGRLayer   *ICreateLayer( const char *pszName,
                                     OGRSpatialReference *poSpatialRef = nullptr,
                                     OGRwkbGeometryType eGType = wkbUnknown,
                                     char ** papszOptions = nullptr ) override;
    virtual OGRLayer   *CopyLayer( OGRLayer *poSrcLayer,
                                   const char *pszNewName,
                                   char **papszOptions = nullptr ) override;

    virtual OGRStyleTable *GetStyleTable() override;
    virtual void        SetStyleTableDirectly( OGRStyleTable *poStyleTable ) override;

    virtual void        SetStyleTable(OGRStyleTable *poStyleTable) override;

    virtual OGRLayer *  ExecuteSQL( const char *pszStatement,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect ) override;
    virtual void        ReleaseResultSet( OGRLayer * poResultsSet ) override;

    virtual void        FlushCache(bool bAtClosing) override;

    virtual OGRErr      StartTransaction(int bForce=FALSE) override;
    virtual OGRErr      CommitTransaction() override;
    virtual OGRErr      RollbackTransaction() override;

    virtual const OGRFieldDomain* GetFieldDomain(const std::string& name) const override;
    virtual bool        AddFieldDomain(std::unique_ptr<OGRFieldDomain>&& domain,
                                       std::string& failureReason) override;

    virtual std::shared_ptr<GDALGroup> GetRootGroup() const override;

    virtual char      **GetMetadata( const char * pszDomain = "" ) override;
    virtual CPLErr      SetMetadata( char ** papszMetadata,
                                     const char * pszDomain = "" ) override;
    virtual const char *GetMetadataItem( const char * pszName,
                                         const char * pszDomain = "" ) override;
    virtual CPLErr      SetMetadataItem( const char * pszName,
                                         const char * pszValue,
                                         const char * pszDomain = "" ) override;
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
                                int bTakeOwnershipTransactionBehavior)
{
    return new OGRDataSourceWithTransaction(poBaseDataSource,
                                            poTransactionBehaviour,
                                            bTakeOwnershipDataSource,
                                            bTakeOwnershipTransactionBehavior);
}

/************************************************************************/
/*                      OGRDataSourceWithTransaction                    */
/************************************************************************/

OGRDataSourceWithTransaction::OGRDataSourceWithTransaction(
    OGRDataSource* poBaseDataSource,
    IOGRTransactionBehaviour* poTransactionBehaviour,
    int bTakeOwnershipDataSource,
    int bTakeOwnershipTransactionBehavior) :
    m_poBaseDataSource(poBaseDataSource),
    m_poTransactionBehavior(poTransactionBehaviour),
    m_bHasOwnershipDataSource(bTakeOwnershipDataSource),
    m_bHasOwnershipTransactionBehavior(bTakeOwnershipTransactionBehavior),
    m_bInTransaction(FALSE)
{}

OGRDataSourceWithTransaction::~OGRDataSourceWithTransaction()
{
    std::set<OGRLayerWithTransaction*>::iterator oIter = m_oSetLayers.begin();
    for(; oIter != m_oSetLayers.end(); ++oIter )
        delete *oIter;

    if( m_bHasOwnershipDataSource )
        delete m_poBaseDataSource;
    if( m_bHasOwnershipTransactionBehavior )
        delete m_poTransactionBehavior;
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
            OGRLayerWithTransaction* poMutexedLayer = new OGRLayerWithTransaction(this,poLayer);
            m_oMapLayers[poLayer->GetName()] = poMutexedLayer;
            m_oSetLayers.insert(poMutexedLayer);
            poLayer = poMutexedLayer;
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
        if( m_poBaseDataSource == nullptr )
            poWrappedLayer->m_poDecoratedLayer = nullptr;
        else
        {
            poWrappedLayer->m_poDecoratedLayer =
                m_poBaseDataSource->GetLayerByName(poWrappedLayer->GetName());
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
    if( !m_poBaseDataSource ) return nullptr;
    return WrapLayer(m_poBaseDataSource->GetLayer(iIndex));
}

OGRLayer    *OGRDataSourceWithTransaction::GetLayerByName(const char *pszName)
{
    if( !m_poBaseDataSource ) return nullptr;
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
    if( eErr == OGRERR_NONE && !osName.empty() )
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

bool OGRDataSourceWithTransaction::IsLayerPrivate(int iLayer) const
{
    if( !m_poBaseDataSource ) return false;
    return m_poBaseDataSource->IsLayerPrivate(iLayer);
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
    if( !m_poBaseDataSource ) return nullptr;
    return WrapLayer(m_poBaseDataSource->CreateLayer(pszName, poSpatialRef, eGType, papszOptions));
}

OGRLayer   *OGRDataSourceWithTransaction::CopyLayer( OGRLayer *poSrcLayer,
                                   const char *pszNewName,
                                   char **papszOptions )
{
    if( !m_poBaseDataSource ) return nullptr;
    return WrapLayer(m_poBaseDataSource->CopyLayer(poSrcLayer, pszNewName, papszOptions ));
}

OGRStyleTable *OGRDataSourceWithTransaction::GetStyleTable()
{
    if( !m_poBaseDataSource ) return nullptr;
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
    if( !m_poBaseDataSource ) return nullptr;
    OGRLayer* poLayer = m_poBaseDataSource->ExecuteSQL(pszStatement, poSpatialFilter,
                                                       pszDialect);
    if( poLayer != nullptr )
        m_oSetExecuteSQLLayers.insert(poLayer);
    return poLayer;
}

void        OGRDataSourceWithTransaction::ReleaseResultSet( OGRLayer * poResultsSet )
{
    if( !m_poBaseDataSource ) return;
    m_oSetExecuteSQLLayers.erase(poResultsSet);
    m_poBaseDataSource->ReleaseResultSet(poResultsSet);
}

void      OGRDataSourceWithTransaction::FlushCache(bool bAtClosing)
{
    if( !m_poBaseDataSource ) return;
    return m_poBaseDataSource->FlushCache(bAtClosing);
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
    if( !m_oSetExecuteSQLLayers.empty() )
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
        m_poTransactionBehavior->StartTransaction(m_poBaseDataSource, bHasReopenedDS);
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
    if( !m_oSetExecuteSQLLayers.empty() )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot interrupt transaction while a layer returned by "
                 "ExecuteSQL() hasn't been released.");
        return OGRERR_FAILURE;
    }
    m_bInTransaction = FALSE;
    int bHasReopenedDS = FALSE;
    OGRErr eErr =
        m_poTransactionBehavior->CommitTransaction(m_poBaseDataSource, bHasReopenedDS);
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
    if( !m_oSetExecuteSQLLayers.empty() )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot interrupt transaction while a layer returned by "
                 "ExecuteSQL() hasn't been released.");
        return OGRERR_FAILURE;
    }
    m_bInTransaction = FALSE;
    int bHasReopenedDS = FALSE;
    OGRErr eErr =
        m_poTransactionBehavior->RollbackTransaction(m_poBaseDataSource, bHasReopenedDS);
    if( bHasReopenedDS )
        RemapLayers();
    return eErr;
}

const OGRFieldDomain* OGRDataSourceWithTransaction::GetFieldDomain(const std::string& name) const
{
    if( !m_poBaseDataSource ) return nullptr;
    return m_poBaseDataSource->GetFieldDomain(name);
}

bool OGRDataSourceWithTransaction::AddFieldDomain(std::unique_ptr<OGRFieldDomain>&& domain, std::string& failureReason)
{
    if( !m_poBaseDataSource ) return false;
    return m_poBaseDataSource->AddFieldDomain(std::move(domain), failureReason);
}

std::shared_ptr<GDALGroup> OGRDataSourceWithTransaction::GetRootGroup() const
{
    if( !m_poBaseDataSource ) return nullptr;
    return m_poBaseDataSource->GetRootGroup();
}

char      **OGRDataSourceWithTransaction::GetMetadata( const char * pszDomain )
{
    if( !m_poBaseDataSource ) return nullptr;
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
    if( !m_poBaseDataSource ) return nullptr;
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
    OGRDataSourceWithTransaction* poDS, OGRLayer* poBaseLayer) :
    OGRLayerDecorator(poBaseLayer, FALSE),
    m_poDS(poDS),
    m_poFeatureDefn(nullptr)
{}

OGRLayerWithTransaction::~OGRLayerWithTransaction()
{
    if( m_poFeatureDefn )
        m_poFeatureDefn->Release();
}

OGRFeatureDefn *OGRLayerWithTransaction::GetLayerDefn()
{
    if( !m_poDecoratedLayer )
    {
        if( m_poFeatureDefn == nullptr )
        {
            m_poFeatureDefn = new OGRFeatureDefn(GetDescription());
            m_poFeatureDefn->Reference();
        }
        return m_poFeatureDefn;
    }
    else if( m_poFeatureDefn == nullptr )
    {
        OGRFeatureDefn* poSrcFeatureDefn = m_poDecoratedLayer->GetLayerDefn();
        m_poFeatureDefn = poSrcFeatureDefn->Clone();
        m_poFeatureDefn->Reference();
    }
    return m_poFeatureDefn;
}

OGRErr      OGRLayerWithTransaction::CreateField( OGRFieldDefn *poField,
                                            int bApproxOK )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    int nFields = m_poDecoratedLayer->GetLayerDefn()->GetFieldCount();
    OGRErr eErr = m_poDecoratedLayer->CreateField(poField, bApproxOK);
    if( m_poFeatureDefn && eErr == OGRERR_NONE && m_poDecoratedLayer->GetLayerDefn()->GetFieldCount() == nFields + 1 )
    {
        m_poFeatureDefn->AddFieldDefn( m_poDecoratedLayer->GetLayerDefn()->GetFieldDefn(nFields) );
    }
    return eErr;
}

OGRErr      OGRLayerWithTransaction::CreateGeomField( OGRGeomFieldDefn *poField,
                                            int bApproxOK )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    int nFields = m_poDecoratedLayer->GetLayerDefn()->GetGeomFieldCount();
    OGRErr eErr = m_poDecoratedLayer->CreateGeomField(poField, bApproxOK);
    if( m_poFeatureDefn && eErr == OGRERR_NONE && m_poDecoratedLayer->GetLayerDefn()->GetGeomFieldCount() == nFields + 1 )
    {
        m_poFeatureDefn->AddGeomFieldDefn( m_poDecoratedLayer->GetLayerDefn()->GetGeomFieldDefn(nFields) );
    }
    return eErr;
}

OGRErr      OGRLayerWithTransaction::DeleteField( int iField )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    OGRErr eErr = m_poDecoratedLayer->DeleteField(iField);
    if( m_poFeatureDefn && eErr == OGRERR_NONE )
        m_poFeatureDefn->DeleteFieldDefn(iField);
    return eErr;
}

OGRErr      OGRLayerWithTransaction::ReorderFields( int* panMap )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    OGRErr eErr = m_poDecoratedLayer->ReorderFields(panMap);
    if( m_poFeatureDefn && eErr == OGRERR_NONE )
        m_poFeatureDefn->ReorderFieldDefns(panMap);
    return eErr;
}

OGRErr      OGRLayerWithTransaction::AlterFieldDefn( int iField,
                                                     OGRFieldDefn* poNewFieldDefn,
                                                     int nFlagsIn )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    OGRErr eErr = m_poDecoratedLayer->AlterFieldDefn(iField, poNewFieldDefn, nFlagsIn);
    if( m_poFeatureDefn && eErr == OGRERR_NONE )
    {
        OGRFieldDefn* poSrcFieldDefn = m_poDecoratedLayer->GetLayerDefn()->GetFieldDefn(iField);
        OGRFieldDefn* poDstFieldDefn = m_poFeatureDefn->GetFieldDefn(iField);
        poDstFieldDefn->SetName(poSrcFieldDefn->GetNameRef());
        poDstFieldDefn->SetType(poSrcFieldDefn->GetType());
        poDstFieldDefn->SetSubType(poSrcFieldDefn->GetSubType());
        poDstFieldDefn->SetWidth(poSrcFieldDefn->GetWidth());
        poDstFieldDefn->SetPrecision(poSrcFieldDefn->GetPrecision());
        poDstFieldDefn->SetDefault(poSrcFieldDefn->GetDefault());
        poDstFieldDefn->SetNullable(poSrcFieldDefn->IsNullable());
    }
    return eErr;
}

OGRFeature * OGRLayerWithTransaction::GetNextFeature()
{
    if( !m_poDecoratedLayer ) return nullptr;
    OGRFeature* poSrcFeature = m_poDecoratedLayer->GetNextFeature();
    if( !poSrcFeature )
        return nullptr;
    OGRFeature* poFeature = new OGRFeature(GetLayerDefn());
    poFeature->SetFrom(poSrcFeature);
    poFeature->SetFID(poSrcFeature->GetFID());
    delete poSrcFeature;
    return poFeature;
}

OGRFeature * OGRLayerWithTransaction::GetFeature( GIntBig nFID )
{
    if( !m_poDecoratedLayer ) return nullptr;
    OGRFeature* poSrcFeature = m_poDecoratedLayer->GetFeature(nFID);
    if( !poSrcFeature )
        return nullptr;
    OGRFeature* poFeature = new OGRFeature(GetLayerDefn());
    poFeature->SetFrom(poSrcFeature);
    poFeature->SetFID(poSrcFeature->GetFID());
    delete poSrcFeature;
    return poFeature;
}

OGRErr       OGRLayerWithTransaction::ISetFeature( OGRFeature *poFeature )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    OGRFeature* poSrcFeature = new OGRFeature(m_poDecoratedLayer->GetLayerDefn());
    poSrcFeature->SetFrom(poFeature);
    poSrcFeature->SetFID(poFeature->GetFID());
    OGRErr eErr = m_poDecoratedLayer->SetFeature(poSrcFeature);
    delete poSrcFeature;
    return eErr;
}

OGRErr       OGRLayerWithTransaction::ICreateFeature( OGRFeature *poFeature )
{
    if( !m_poDecoratedLayer ) return OGRERR_FAILURE;
    OGRFeature* poSrcFeature = new OGRFeature(m_poDecoratedLayer->GetLayerDefn());
    poSrcFeature->SetFrom(poFeature);
    poSrcFeature->SetFID(poFeature->GetFID());
    OGRErr eErr = m_poDecoratedLayer->CreateFeature(poSrcFeature);
    poFeature->SetFID(poSrcFeature->GetFID());
    delete poSrcFeature;
    return eErr;
}
