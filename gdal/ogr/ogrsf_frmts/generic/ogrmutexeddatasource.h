/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Defines OGRLMutexedDataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef _OGRMUTEXEDDATASOURCELAYER_H_INCLUDED
#define _OGRMUTEXEDDATASOURCELAYER_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_multiproc.h"

/** OGRMutexedDataSource class protects all virtual methods of OGRDataSource
 *  with a mutex.
 *  If the passed mutex is NULL, then no locking will be done.
 *
 *  Note that the constructors and destructors are not explictely protected
 *  by the mutex*
 */
class CPL_DLL OGRMutexedDataSource : public OGRDataSource
{
  protected:
    OGRDataSource *m_poBaseDataSource;
    int            m_bHasOwnership;
    CPLMutex      *m_hGlobalMutex;

  public:

    /* The construction of the object isn't protected by the mutex */
                 OGRMutexedDataSource(OGRDataSource* poBaseDataSource,
                                      int bTakeOwnership,
                                      CPLMutex* hMutexIn);

    /* The destruction of the object isn't protected by the mutex */
    virtual     ~OGRMutexedDataSource();

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
};

#endif // _OGRMUTEXEDDATASOURCELAYER_H_INCLUDED
