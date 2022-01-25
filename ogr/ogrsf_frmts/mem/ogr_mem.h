/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions within the OGR Memory driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGRMEM_H_INCLUDED
#define OGRMEM_H_INCLUDED

#include "ogrsf_frmts.h"

#include <map>

/************************************************************************/
/*                             OGRMemLayer                              */
/************************************************************************/
class OGRMemDataSource;

class IOGRMemLayerFeatureIterator;

class CPL_DLL OGRMemLayer CPL_NON_FINAL: public OGRLayer
{
    CPL_DISALLOW_COPY_ASSIGN(OGRMemLayer)

    typedef std::map<GIntBig, OGRFeature*>           FeatureMap;
    typedef std::map<GIntBig, OGRFeature*>::iterator FeatureIterator;

    OGRFeatureDefn     *m_poFeatureDefn;

    GIntBig             m_nFeatureCount;

    GIntBig             m_iNextReadFID;
    GIntBig             m_nMaxFeatureCount;  // Max size of papoFeatures.
    OGRFeature        **m_papoFeatures;
    bool                m_bHasHoles;

    FeatureMap          m_oMapFeatures;
    FeatureIterator     m_oMapFeaturesIter;

    GIntBig             m_iNextCreateFID;

    bool                m_bUpdatable;
    bool                m_bAdvertizeUTF8;

    bool                m_bUpdated;

    // Only use it in the lifetime of a function where the list of features
    // doesn't change.
    IOGRMemLayerFeatureIterator* GetIterator();

  public:
                        OGRMemLayer( const char * pszName,
                                     OGRSpatialReference *poSRS,
                                     OGRwkbGeometryType eGeomType );
    virtual            ~OGRMemLayer();

    void                ResetReading() override;
    OGRFeature *        GetNextFeature() override;
    virtual OGRErr      SetNextByIndex( GIntBig nIndex ) override;

    OGRFeature         *GetFeature( GIntBig nFeatureId ) override;
    OGRErr              ISetFeature( OGRFeature *poFeature ) override;
    OGRErr              ICreateFeature( OGRFeature *poFeature ) override;
    virtual OGRErr      DeleteFeature( GIntBig nFID ) override;

    OGRFeatureDefn *    GetLayerDefn() override { return m_poFeatureDefn; }

    GIntBig             GetFeatureCount( int ) override;

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE ) override;
    virtual OGRErr      DeleteField( int iField ) override;
    virtual OGRErr      ReorderFields( int* panMap ) override;
    virtual OGRErr      AlterFieldDefn( int iField,
                                        OGRFieldDefn* poNewFieldDefn,
                                        int nFlags ) override;
    virtual OGRErr      CreateGeomField( OGRGeomFieldDefn *poGeomField,
                                         int bApproxOK = TRUE ) override;

    int                 TestCapability( const char * ) override;

    bool                IsUpdatable() const { return m_bUpdatable; }
    void                SetUpdatable( bool bUpdatableIn )
        { m_bUpdatable = bUpdatableIn; }
    void                SetAdvertizeUTF8( bool bAdvertizeUTF8In )
        { m_bAdvertizeUTF8 = bAdvertizeUTF8In; }

    bool                HasBeenUpdated() const { return m_bUpdated; }
    void                SetUpdated(bool bUpdated) { m_bUpdated = bUpdated; }

    GIntBig             GetNextReadFID() { return m_iNextReadFID; }
};

/************************************************************************/
/*                           OGRMemDataSource                           */
/************************************************************************/

class OGRMemDataSource CPL_NON_FINAL: public OGRDataSource
{
    CPL_DISALLOW_COPY_ASSIGN(OGRMemDataSource)

    OGRMemLayer       **papoLayers;
    int                 nLayers;

    char                *pszName;

  public:
                        OGRMemDataSource( const char *, char ** );
                        virtual ~OGRMemDataSource();

    const char          *GetName() override { return pszName; }
    int                 GetLayerCount() override { return nLayers; }
    OGRLayer            *GetLayer( int ) override;

    virtual OGRLayer    *ICreateLayer( const char *,
                                       OGRSpatialReference * = nullptr,
                                       OGRwkbGeometryType = wkbUnknown,
                                       char ** = nullptr ) override;
    OGRErr              DeleteLayer( int iLayer ) override;

    int                 TestCapability( const char * ) override;

    bool                AddFieldDomain(std::unique_ptr<OGRFieldDomain>&& domain,
                                       std::string& failureReason) override;

    bool                DeleteFieldDomain(const std::string& name,
                                          std::string& failureReason) override;

    bool                UpdateFieldDomain(std::unique_ptr<OGRFieldDomain>&& domain,
                                          std::string& failureReason) override;

};

/************************************************************************/
/*                             OGRMemDriver                             */
/************************************************************************/

class OGRMemDriver final: public OGRSFDriver
{
  public:
    virtual ~OGRMemDriver();

    const char *GetName() override;
    OGRDataSource *Open( const char *, int ) override;

    virtual OGRDataSource *CreateDataSource( const char *pszName,
                                             char ** = nullptr ) override;

    int TestCapability( const char * ) override;
};

#endif  // ndef OGRMEM_H_INCLUDED
