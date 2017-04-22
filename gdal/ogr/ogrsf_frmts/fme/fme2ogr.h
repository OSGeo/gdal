/******************************************************************************
 * $Id$
 *
 * Project:  FMEObjects Translator
 * Purpose:  Declarations for translating IFMEFeatures to OGRFeatures.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, 2001 Safe Software Inc.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef FME2OGR_H_INCLUDED
#define FME2OGR_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_minixml.h"

#include <isession.h>
#include <ireader.h>
#include <ifeature.h>
#include <ifeatvec.h>
#include <ipipeline.h>
#include <ispatialindex.h>

class OGRFMEDataSource;

void CPLFMEError( IFMESession *, const char *, ... );

/************************************************************************/
/*                             OGRFMELayer                              */
/************************************************************************/

class OGRFMELayer : public OGRLayer
{
  protected:
    OGRFeatureDefn     *poFeatureDefn;
    OGRSpatialReference *poSpatialRef;

    OGRFMEDataSource   *poDS;

    char               *pszAttributeFilter;

    IFMEFeature         *poFMEFeature;

  public:
    explicit            OGRFMELayer( OGRFMEDataSource * );
    virtual             ~OGRFMELayer();

    virtual int         Initialize( IFMEFeature *,
                                    OGRSpatialReference * );

    virtual OGRErr      SetAttributeFilter( const char * ) override;

    OGRSpatialReference *GetSpatialRef() override;

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }
};

/************************************************************************/
/*                          OGRFMELayerCached                           */
/************************************************************************/

class OGRFMELayerCached : public OGRFMELayer
{
  private:
    int                 nPreviousFeature;

    char               *pszIndexBase;
    IFMESpatialIndex   *poIndex;

    OGRFeature *        ReadNextIndexFeature();

    OGREnvelope         sExtents;

    int                 bQueryActive;

  public:
    explicit            OGRFMELayerCached( OGRFMEDataSource * );
    virtual            ~OGRFMELayerCached();

    virtual void        ResetReading() override;
    virtual OGRFeature *GetNextFeature() override;
    virtual GIntBig     GetFeatureCount( int bForce ) override;

    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;

    virtual int         TestCapability( const char * ) override;

    int                 AssignIndex( const char *pszBase, const OGREnvelope*,
                                     OGRSpatialReference *poSRS );
    CPLXMLNode         *SerializeToXML();
    int                InitializeFromXML( CPLXMLNode * );
};

/************************************************************************/
/*                            OGRFMELayerDB                             */
/************************************************************************/

class OGRFMELayerDB : public OGRFMELayer
{
  private:
    int                 nPreviousFeature;

    IFMEUniversalReader *poReader;

    char                *pszReaderName;
    char                *pszDataset;
    IFMEStringArray     *poUserDirectives;

    int                 CreateReader();

  public:
                       OGRFMELayerDB( OGRFMEDataSource * poDSIn,
                                      const char *pszReaderName,
                                      const char *pszDataset,
                                      IFMEStringArray *poUserDirectives );
    virtual            ~OGRFMELayerDB();

    virtual OGRErr      SetAttributeFilter( const char * ) override;

    virtual void        ResetReading() override;
    virtual OGRFeature *GetNextFeature() override;
    virtual GIntBig     GetFeatureCount( int bForce ) override;

    virtual int         TestCapability( const char * ) override;

    void                AssignIndex( const char *pszBase );
};

/************************************************************************/
/*                           OGRFMEDataSource                           */
/************************************************************************/

class OGRFMEDataSource : public OGRDataSource
{
    char                *pszName;          // full name, i.e. "SHAPE:D:\DATA"
    char                *pszReaderName;    // reader/driver name, i.e. "SHAPE"
    char                *pszDataset;       // FME dataset name, i.e. "D:\DATA"

    IFMEStringArray   *poUserDirectives;

    IFMESession         *poSession;
    IFMEUniversalReader *poReader;

    int                 nLayers;
    OGRFMELayer         **papoLayers;

    IFMEFeature         *poFMEFeature;

    int                 ReadFMEFeature();

    IFMEString          *poFMEString;

    char                *PromptForSource();
    char                *ReadFileSource(const char *);
    OGRSpatialReference *ExtractSRS();
    int                 IsPartOfConnectionCache( IFMEUniversalReader * );
    void                OfferForConnectionCaching( IFMEUniversalReader *,
                                                   const char *,
                                                   const char *);

    int                 bUseCaching;
    int                 bCoordSysOverride;

    void                ClarifyGeometryClass( IFMEFeature *poFeature,
                                          OGRwkbGeometryType &eBestGeomType );

  public:
                        OGRFMEDataSource();
                        ~OGRFMEDataSource();

    IFMESession *       AcquireSession();
    void                ReleaseSession();

    int                 TestCapability( const char * ) override;

    OGRGeometry        *ProcessGeometry( OGRFMELayer *, IFMEFeature *,
                                         OGRwkbGeometryType );
    OGRFeature         *ProcessFeature( OGRFMELayer *, IFMEFeature * );
    void                ResetReading() override;

    int                 Open( const char * );

    const char          *GetName() override { return pszName; }
    int                 GetLayerCount() override { return nLayers; }
    OGRLayer            *GetLayer( int ) override;

    IFMESession         *GetFMESession() { return poSession; }
    IFMEUniversalReader *GetFMEReader() { return poReader; }

    void                BuildSpatialIndexes();

    OGRSpatialReference *FME2OGRSpatialRef( const char *pszFMECoordsys );

    // Stuff related to persistent feature caches.
    CPLXMLNode          *SerializeToXML();
    int                 InitializeFromXML( CPLXMLNode * );
};

/************************************************************************/
/*                             OGRFMEDriver                             */
/************************************************************************/

class OGRFMEDriver : public OGRSFDriver
{
  public:
                ~OGRFMEDriver();

    int                 TestCapability( const char * ) override;

    const char *GetName() override;
    OGRDataSource *Open( const char *, int ) override;
};

/************************************************************************/
/*                           OGRFMECacheIndex                           */
/************************************************************************/

class OGRFMECacheIndex
{
    CPLXMLNode  *psTree;        // Implicitly locked if this is non-NULL.

    char        *pszPath;
    void        *hLock;

  public:
    explicit     OGRFMECacheIndex( const char *pszPath );
                ~OGRFMECacheIndex();

    const char *GetPath() { return pszPath; }

    int         Load();
    int         Save();

    CPLXMLNode  *FindMatch( const char *pszDriver,
                            const char *pszDataset,
                            IFMEStringArray &oUserDirectives );

    int         Lock();
    int         Unlock();

    static void        Touch( CPLXMLNode * );
    void        Add( CPLXMLNode * );
    static void        Reference( CPLXMLNode * );
    static void        Dereference( CPLXMLNode * );

    int         ExpireOldCaches( IFMESession * );
};

// The number of seconds an unreferenced spatial cache should be retained in
// the cache index before cleaning up if unused.
// Default: 15 minutes
#ifndef FMECACHE_RETENTION
#  define FMECACHE_RETENTION    900
#endif

// The number of seconds before a "referenced" data source in the cache
// index is considered to be orphaned due to a process dying or something.
#ifndef FMECACHE_REF_TIMEOUT
#  define FMECACHE_REF_TIMEOUT  FMECACHE_RETENTION*3
#endif

// The number of seconds from creation a spatial cache should be retained in
// the cache index before cleaning it up.
// Default: 1hour
#ifndef FMECACHE_MAX_RETENTION
#  define FMECACHE_MAX_RETENTION    3600
#endif

CPL_C_START
void RegisterOGRFME();
CPL_C_END

#endif /* ndef FME2OGR_H_INCLUDED */
