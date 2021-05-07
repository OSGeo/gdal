/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Defines OGRLMutexedDataSource class
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGRMUTEXEDDATASOURCELAYER_H_INCLUDED
#define OGRMUTEXEDDATASOURCELAYER_H_INCLUDED

#ifndef DOXYGEN_SKIP

#include "ogrsf_frmts.h"
#include "cpl_multiproc.h"
#include "ogrmutexedlayer.h"
#include <map>

/** OGRMutexedDataSource class protects all virtual methods of OGRDataSource
 *  with a mutex.
 *  If the passed mutex is NULL, then no locking will be done.
 *
 *  Note that the constructors and destructors are not explicitly protected
 *  by the mutex.
 */
class CPL_DLL OGRMutexedDataSource : public OGRDataSource
{
    CPL_DISALLOW_COPY_ASSIGN(OGRMutexedDataSource)

  protected:
    OGRDataSource *m_poBaseDataSource;
    int            m_bHasOwnership;
    CPLMutex      *m_hGlobalMutex;
    int            m_bWrapLayersInMutexedLayer;
    std::map<OGRLayer*, OGRMutexedLayer* > m_oMapLayers{};
    std::map<OGRMutexedLayer*, OGRLayer* > m_oReverseMapLayers{};

    OGRLayer*           WrapLayerIfNecessary(OGRLayer* poLayer);

  public:

    /* The construction of the object isn't protected by the mutex */
                 OGRMutexedDataSource(OGRDataSource* poBaseDataSource,
                                      int bTakeOwnership,
                                      CPLMutex* hMutexIn,
                                      int bWrapLayersInMutexedLayer);

    /* The destruction of the object isn't protected by the mutex */
    virtual     ~OGRMutexedDataSource();

    OGRDataSource*      GetBaseDataSource() { return m_poBaseDataSource; }

    virtual const char  *GetName() override;

    virtual int         GetLayerCount() override ;
    virtual OGRLayer    *GetLayer(int) override;
    virtual OGRLayer    *GetLayerByName(const char *) override;
    virtual OGRErr      DeleteLayer(int) override;

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

    virtual void        FlushCache() override;

    virtual OGRErr      StartTransaction(int bForce=FALSE) override;
    virtual OGRErr      CommitTransaction() override;
    virtual OGRErr      RollbackTransaction() override;

    virtual char      **GetMetadata( const char * pszDomain = "" ) override;
    virtual CPLErr      SetMetadata( char ** papszMetadata,
                                     const char * pszDomain = "" ) override;
    virtual const char *GetMetadataItem( const char * pszName,
                                         const char * pszDomain = "" ) override;
    virtual CPLErr      SetMetadataItem( const char * pszName,
                                         const char * pszValue,
                                         const char * pszDomain = "" ) override;

    virtual const OGRFieldDomain* GetFieldDomain(const std::string& name) const override;

    virtual bool        AddFieldDomain(std::unique_ptr<OGRFieldDomain>&& domain,
                                       std::string& failureReason) override;

    virtual std::shared_ptr<GDALGroup> GetRootGroup() const override;

};

#endif /* #ifndef DOXYGEN_SKIP */

#endif // OGRMUTEXEDDATASOURCELAYER_H_INCLUDED
