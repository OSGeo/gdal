/******************************************************************************
 * $Id$
 *
 * Project:  FMEObjects Translator
 * Purpose:  Declarations for translating IFMEFeatures to OGRFeatures.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, 2001 Safe Software Inc.
 * All Rights Reserved
 *
 * This software may not be copied or reproduced, in all or in part, 
 * without the prior written consent of Safe Software Inc.
 *
 * The entire risk as to the results and performance of the software,
 * supporting text and other information contained in this file
 * (collectively called the "Software") is with the user.  Although
 * Safe Software Incorporated has used considerable efforts in preparing 
 * the Software, Safe Software Incorporated does not warrant the
 * accuracy or completeness of the Software. In no event will Safe Software 
 * Incorporated be liable for damages, including loss of profits or 
 * consequential damages, arising out of the use of the Software.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.5  2005/02/22 12:57:19  fwarmerdam
 * use OGRLayer base spatial filter support
 *
 * Revision 1.4  2002/11/08 21:20:58  warmerda
 * ensure a query is issued if resetreading never called
 *
 * Revision 1.3  2002/09/04 18:44:48  warmerda
 * try to make aggregates into multipolygons
 *
 * Revision 1.2  2002/07/11 16:08:11  warmerda
 * added FMECACHE_MAX_RETENTION
 *
 * Revision 1.1  2002/05/24 06:23:57  warmerda
 * New
 *
 * Revision 1.16  2002/05/24 06:17:01  warmerda
 * clean up dependencies on strimp.h, and fme2ogrspatialref func
 *
 * Revision 1.15  2002/05/24 03:59:41  warmerda
 * added support for cache index and related stuff
 *
 * Revision 1.14  2002/05/06 14:06:37  warmerda
 * override coordsys from cached features if needed
 *
 * Revision 1.13  2002/05/03 14:14:25  warmerda
 * Added support for clarifying geometry types, based in part on scanning all
 * the features.  Also - convert polygons to multipolygons if the layer type
 * is multipolygon.
 *
 * Revision 1.12  2002/04/10 20:10:38  warmerda
 * Added support for getting geometry extents
 *
 * Revision 1.11  2001/11/26 23:36:24  warmerda
 * override SetAttributeFilter on OGRFMELayerDB
 *
 * Revision 1.10  2001/11/22 21:18:42  warmerda
 * added SDE connection caching
 *
 * Revision 1.9  2001/11/21 15:45:03  warmerda
 * added SRS support
 *
 * Revision 1.8  2001/11/15 21:44:21  warmerda
 * preserve reader name
 *
 * Revision 1.7  2001/11/01 21:52:39  warmerda
 * added mutex to protect access to session
 *
 * Revision 1.6  2001/10/25 14:30:10  warmerda
 * added ReadFileSource
 *
 * Revision 1.5  2001/09/07 15:52:42  warmerda
 * flesh out OGRFMELayerDB
 *
 * Revision 1.4  2001/08/17 20:08:50  warmerda
 * capture user directives in prompt logic
 *
 * Revision 1.3  2001/07/27 17:24:45  warmerda
 * First phase rewrite for MapGuide
 *
 * Revision 1.2  1999/11/23 15:39:51  warmerda
 * tab expantion
 *
 * Revision 1.1  1999/11/23 15:22:58  warmerda
 * New
 *
 * Revision 1.4  1999/11/22 20:07:47  warmerda
 * added ProcessGeometry, and GetSpatialRef
 *
 * Revision 1.3  1999/11/10 14:04:44  warmerda
 * updated to new fmeobjects kit
 *
 * Revision 1.2  1999/09/09 21:05:34  warmerda
 * further fleshed out
 *
 * Revision 1.1  1999/09/09 20:40:56  warmerda
 * New
 *
 */

#ifndef _FME2OGR_H_INCLUDED
#define _FME2OGR_H_INCLUDED

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
                        OGRFMELayer( OGRFMEDataSource * );
    virtual             ~OGRFMELayer();

    virtual int         Initialize( IFMEFeature *,
                                    OGRSpatialReference * );

    virtual OGRErr      SetAttributeFilter( const char * );
    
    OGRSpatialReference *GetSpatialRef();

    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }
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
                       OGRFMELayerCached( OGRFMEDataSource * );
    virtual            ~OGRFMELayerCached();
                       
    virtual void        ResetReading();
    virtual OGRFeature *GetNextFeature();
    virtual int         GetFeatureCount( int bForce );

    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE);
    
    virtual int         TestCapability( const char * );

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
                       
    virtual OGRErr      SetAttributeFilter( const char * );
    
    virtual void        ResetReading();
    virtual OGRFeature *GetNextFeature();
    virtual int         GetFeatureCount( int bForce );

    virtual int         TestCapability( const char * );

    void                AssignIndex( const char *pszBase );
};

/************************************************************************/
/*                           OGRFMEDataSource                           */
/************************************************************************/

class OGRFMEDataSource : public OGRDataSource
{
    char                *pszName;          // full name, ie. "SHAPE:D:\DATA"
    char                *pszReaderName;    // reader/driver name, ie. "SHAPE"
    char                *pszDataset;       // FME dataset name, ie. "D:\DATA"

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
    
    int                 TestCapability( const char * );

    OGRGeometry        *ProcessGeometry( OGRFMELayer *, IFMEFeature *,
                                         OGRwkbGeometryType );
    OGRFeature         *ProcessFeature( OGRFMELayer *, IFMEFeature * );
    void                ResetReading();

    int                 Open( const char * );

    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );

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

    int                 TestCapability( const char * );

    const char *GetName();
    OGRDataSource *Open( const char *, int );
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
                OGRFMECacheIndex( const char *pszPath );
                ~OGRFMECacheIndex();

    const char *GetPath() { return pszPath; }

    int         Load();
    int         Save();

    CPLXMLNode  *FindMatch( const char *pszDriver,
                            const char *pszDataset,
                            IFMEStringArray &oUserDirectives );

    int         Lock();
    int         Unlock();
    
    void        Touch( CPLXMLNode * );
    void        Add( CPLXMLNode * );
    void        Reference( CPLXMLNode * );
    void        Dereference( CPLXMLNode * );
    
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

#endif /* ndef _FME2OGR_H_INCLUDED */
