/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Classes related to format registration, and file opening.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.55  2005/11/10 21:37:28  fwarmerdam
 * added DXF/DWG support
 *
 * Revision 1.54  2005/10/25 19:58:53  fwarmerdam
 * added driver tracking on datasource
 *
 * Revision 1.53  2005/09/05 19:31:45  fwarmerdam
 * Added PGeo driver.
 *
 * Revision 1.52  2005/08/05 15:34:34  fwarmerdam
 * added grass
 *
 * Revision 1.51  2005/07/08 22:10:56  pka
 * Initial import of OGR Interlis driver
 *
 * Revision 1.50  2005/02/22 12:40:37  fwarmerdam
 * added base OGRLayer spatial filter support
 *
 * Revision 1.49  2005/02/02 20:00:01  fwarmerdam
 * added SetNextByIndex support
 *
 * Revision 1.48  2005/01/19 20:29:10  fwarmerdam
 * added autoloaddrivers on ogrsfdriverregistrar
 *
 * Revision 1.47  2005/01/03 22:16:44  fwarmerdam
 * added OGRLayer::SetSpatialFilterRect()
 *
 * Revision 1.46  2004/11/21 22:08:49  fwarmerdam
 * added Release() and DestroyDataSource() methods on OGRDataSource
 *
 * Revision 1.45  2004/10/06 19:49:14  fwarmerdam
 * Added Mysql registration function.
 *
 * Revision 1.44  2004/07/20 19:18:44  warmerda
 * added CSV
 *
 * Revision 1.43  2004/07/10 05:03:24  warmerda
 * added SQLite
 *
 * Revision 1.42  2004/02/11 18:03:15  warmerda
 * added RegisterOGRDODS()
 *
 * Revision 1.41  2003/11/06 18:27:35  warmerda
 * Added VRT registration point
 *
 * Revision 1.40  2003/10/09 15:28:07  warmerda
 * added OGRLayer::DeleteFeature() support
 *
 * Revision 1.39  2003/10/06 19:15:40  warmerda
 * added ODBC support
 *
 * Revision 1.38  2003/05/28 19:17:31  warmerda
 * fixup stuff for docs
 *
 * Revision 1.37  2003/04/22 19:36:04  warmerda
 * Added SyncToDisk
 *
 * Revision 1.36  2003/04/08 21:21:29  warmerda
 * added OGRGetDriverByName
 *
 * Revision 1.35  2003/04/08 19:31:32  warmerda
 * added CopyLayer and CopyDataSource entry points
 *
 * Revision 1.34  2003/03/20 20:21:48  warmerda
 * added drop index
 *
 * Revision 1.33  2003/03/19 20:29:06  warmerda
 * added shared access and reference counting
 *
 * Revision 1.32  2003/03/05 05:09:11  warmerda
 * added GetLayerByName() method on OGRDataSource
 *
 * Revision 1.31  2003/03/04 05:47:23  warmerda
 * added indexing support
 *
 * Revision 1.30  2003/03/03 05:06:08  warmerda
 * added support for DeleteDataSource and DeleteLayer
 *
 * Revision 1.29  2003/02/03 21:16:49  warmerda
 * added .rec driver
 *
 * Revision 1.28  2002/12/28 04:09:18  warmerda
 * added Oracle support
 *
 * Revision 1.27  2002/09/26 18:15:31  warmerda
 * moved capabilities macros to ogr_core.h for ogr_api.h
 *
 * Revision 1.26  2002/06/25 14:45:50  warmerda
 * added RegisterOGRFME()
 *
 * Revision 1.25  2002/04/25 03:42:04  warmerda
 * fixed spatial filter support on SQL results
 *
 * Revision 1.24  2002/04/25 02:24:13  warmerda
 * added ExecuteSWQ() method
 *
 * Revision 1.23  2002/02/18 20:56:24  warmerda
 * added AVC registration
 *
 * Revision 1.22  2002/01/25 20:47:58  warmerda
 * added GML registration
 *
 * Revision 1.21  2001/11/15 21:19:21  warmerda
 * added transaction semantics
 *
 * Revision 1.20  2001/06/19 15:50:23  warmerda
 * added feature attribute query support
 *
 * Revision 1.19  2001/03/15 04:01:43  danmo
 * Added OGRLayer::GetExtent()
 *
 * Revision 1.18  2001/02/06 17:10:28  warmerda
 * export entry points from DLL
 *
 * Revision 1.17  2001/01/19 21:13:50  warmerda
 * expanded tabs
 *
 * Revision 1.16  2000/11/28 19:00:32  warmerda
 * added RegisterOGRDGN
 *
 * Revision 1.15  2000/10/17 17:54:53  warmerda
 * added postgresql support
 *
 * Revision 1.14  2000/08/24 04:44:05  danmo
 * Added optional OGDI driver in OGR
 *
 * Revision 1.13  2000/08/18 21:52:53  svillene
 * Add OGR Representation
 *
 * Revision 1.12  1999/11/14 18:13:08  svillene
 * add RegisterOGRTAB RegisterOGRMIF
 *
 * Revision 1.11  1999/11/04 21:09:40  warmerda
 * Made a bunch of changes related to supporting creation of new
 * layers and data sources.
 *
 * Revision 1.10  1999/10/06 19:02:43  warmerda
 * Added tiger registration.
 *
 * Revision 1.9  1999/09/22 03:05:08  warmerda
 * added SDTS
 *
 * Revision 1.8  1999/09/09 21:04:55  warmerda
 * added fme support
 *
 * Revision 1.7  1999/08/28 03:12:43  warmerda
 * Added NTF.
 *
 * Revision 1.6  1999/07/27 00:50:39  warmerda
 * added a number of OGRLayer methods
 *
 * Revision 1.5  1999/07/26 13:59:05  warmerda
 * added feature writing api
 *
 * Revision 1.4  1999/07/21 13:23:27  warmerda
 * Fixed multiple inclusion protection.
 *
 * Revision 1.3  1999/07/08 20:04:58  warmerda
 * added GetFeatureCount
 *
 * Revision 1.2  1999/07/06 20:25:09  warmerda
 * added some documentation
 *
 * Revision 1.1  1999/07/05 18:59:00  warmerda
 * new
 *
 */

#ifndef _OGRSF_FRMTS_H_INCLUDED
#define _OGRSF_FRMTS_H_INCLUDED

#include "ogr_feature.h"

/**
 * \file ogrsf_frmts.h
 *
 * Classes related to registration of format support, and opening datasets.
 */

class OGRLayerAttrIndex;
class OGRSFDriver;

/************************************************************************/
/*                               OGRLayer                               */
/************************************************************************/

/**
 * This class represents a layer of simple features, with access methods.
 *
 */

class CPL_DLL OGRLayer
{
  protected:
    int          m_bFilterIsEnvelope;
    OGRGeometry  *m_poFilterGeom;
    OGREnvelope  m_sFilterEnvelope;

    int          FilterGeometry( OGRGeometry * );
    int          InstallFilter( OGRGeometry * );

  public:
    OGRLayer();
    virtual     ~OGRLayer();

    virtual OGRGeometry *GetSpatialFilter();
    virtual void        SetSpatialFilter( OGRGeometry * );
    virtual void        SetSpatialFilterRect( double dfMinX, double dfMinY,
                                              double dfMaxX, double dfMaxY );

    virtual OGRErr      SetAttributeFilter( const char * );

    virtual void        ResetReading() = 0;
    virtual OGRFeature *GetNextFeature() = 0;
    virtual OGRErr      SetNextByIndex( long nIndex );
    virtual OGRFeature *GetFeature( long nFID );
    virtual OGRErr      SetFeature( OGRFeature *poFeature );
    virtual OGRErr      CreateFeature( OGRFeature *poFeature );
    virtual OGRErr      DeleteFeature( long nFID );

    virtual OGRFeatureDefn *GetLayerDefn() = 0;

    virtual OGRSpatialReference *GetSpatialRef() { return NULL; }

    virtual int         GetFeatureCount( int bForce = TRUE );
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE);

    virtual int         TestCapability( const char * ) = 0;

    virtual const char *GetInfo( const char * );

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );

    virtual OGRErr      SyncToDisk();

    OGRStyleTable       *GetStyleTable(){return m_poStyleTable;}
    void                 SetStyleTable(OGRStyleTable *poStyleTable){m_poStyleTable = poStyleTable;}

    virtual OGRErr       StartTransaction();
    virtual OGRErr       CommitTransaction();
    virtual OGRErr       RollbackTransaction();

    int                 Reference();
    int                 Dereference();
    int                 GetRefCount() const;

    GIntBig             GetFeaturesRead();
    
    /* consider these private */
    OGRErr               InitializeIndexSupport( const char * );
    OGRLayerAttrIndex   *GetIndex() { return m_poAttrIndex; }

 protected:
    OGRStyleTable       *m_poStyleTable;
    OGRFeatureQuery     *m_poAttrQuery;
    OGRLayerAttrIndex   *m_poAttrIndex;

    int                  m_nRefCount;

    GIntBig              m_nFeaturesRead;
};


/************************************************************************/
/*                            OGRDataSource                             */
/************************************************************************/

/**
 * This class represents a data source.  A data source potentially
 * consists of many layers (OGRLayer).  A data source normally consists
 * of one, or a related set of files, though the name doesn't have to be
 * a real item in the file system.
 *
 * When an OGRDataSource is destroyed, all it's associated OGRLayers objects
 * are also destroyed.
 */ 

class CPL_DLL OGRDataSource
{
    friend class OGRSFDriverRegistrar;

  public:

    OGRDataSource();
    virtual     ~OGRDataSource();
    static void         DestroyDataSource( OGRDataSource * );

    virtual const char  *GetName() = 0;

    virtual int         GetLayerCount() = 0;
    virtual OGRLayer    *GetLayer(int) = 0;
    virtual OGRLayer    *GetLayerByName(const char *);
    virtual OGRErr      DeleteLayer(int);

    virtual int         TestCapability( const char * ) = 0;

    virtual OGRLayer   *CreateLayer( const char *pszName, 
                                     OGRSpatialReference *poSpatialRef = NULL,
                                     OGRwkbGeometryType eGType = wkbUnknown,
                                     char ** papszOptions = NULL );
    virtual OGRLayer   *CopyLayer( OGRLayer *poSrcLayer, 
                                   const char *pszNewName, 
                                   char **papszOptions = NULL );
    OGRStyleTable       *GetStyleTable(){return m_poStyleTable;}

    virtual OGRLayer *  ExecuteSQL( const char *pszStatement,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect );
    virtual void        ReleaseResultSet( OGRLayer * poResultsSet );

    virtual OGRErr      SyncToDisk();

    int                 Reference();
    int                 Dereference();
    int                 GetRefCount() const;
    int                 GetSummaryRefCount() const;
    OGRErr              Release();

    OGRSFDriver        *GetDriver() const;

  protected:

    OGRErr              ProcessSQLCreateIndex( const char * );
    OGRErr              ProcessSQLDropIndex( const char * );

    OGRStyleTable      *m_poStyleTable;
    int                 m_nRefCount;
    OGRSFDriver        *m_poDriver;
};

/************************************************************************/
/*                             OGRSFDriver                              */
/************************************************************************/

/**
 * Represents an operational format driver.
 *
 * One OGRSFDriver derived class will normally exist for each file format
 * registered for use, regardless of whether a file has or will be opened.
 * The list of available drivers is normally managed by the
 * OGRSFDriverRegistrar.
 */

class CPL_DLL OGRSFDriver
{
  public:
    virtual     ~OGRSFDriver();

    virtual const char  *GetName() = 0;

    virtual OGRDataSource *Open( const char *pszName, int bUpdate=FALSE ) = 0;

    virtual int         TestCapability( const char * ) = 0;

    virtual OGRDataSource *CreateDataSource( const char *pszName,
                                             char ** = NULL );
    virtual OGRErr      DeleteDataSource( const char *pszName );

    virtual OGRDataSource *CopyDataSource( OGRDataSource *poSrcDS, 
                                           const char *pszNewName, 
                                           char **papszOptions = NULL );
};


/************************************************************************/
/*                         OGRSFDriverRegistrar                         */
/************************************************************************/

/**
 * Singleton manager for drivers.
 *
 */

class CPL_DLL OGRSFDriverRegistrar
{
    int         nDrivers;
    OGRSFDriver **papoDrivers;

                OGRSFDriverRegistrar();

    int         nOpenDSCount;
    char        **papszOpenDSRawName;
    OGRDataSource **papoOpenDS;
    OGRSFDriver **papoOpenDSDriver;

  public:

                ~OGRSFDriverRegistrar();

    static OGRSFDriverRegistrar *GetRegistrar();
    static OGRDataSource *Open( const char *pszName, int bUpdate=FALSE,
                                OGRSFDriver ** ppoDriver = NULL );

    OGRDataSource *OpenShared( const char *pszName, int bUpdate=FALSE,
                               OGRSFDriver ** ppoDriver = NULL );
    OGRErr      ReleaseDataSource( OGRDataSource * );

    void        RegisterDriver( OGRSFDriver * poDriver );

    int         GetDriverCount( void );
    OGRSFDriver *GetDriver( int iDriver );
    OGRSFDriver *GetDriverByName( const char * );

    int         GetOpenDSCount() { return nOpenDSCount; } 
    OGRDataSource *GetOpenDS( int );

    void        AutoLoadDrivers();
};

/* -------------------------------------------------------------------- */
/*      Various available registration methods.                         */
/* -------------------------------------------------------------------- */
CPL_C_START
void CPL_DLL OGRRegisterAll();

void CPL_DLL RegisterOGRShape();
void CPL_DLL RegisterOGRNTF();
void CPL_DLL RegisterOGRFME();
void CPL_DLL RegisterOGRSDTS();
void CPL_DLL RegisterOGRTiger();
void CPL_DLL RegisterOGRS57();
void CPL_DLL RegisterOGRTAB();
void CPL_DLL RegisterOGRMIF();
void CPL_DLL RegisterOGROGDI();
void CPL_DLL RegisterOGRODBC();
void CPL_DLL RegisterOGRPG();
void CPL_DLL RegisterOGRMySQL();
void CPL_DLL RegisterOGROCI();
void CPL_DLL RegisterOGRDGN();
void CPL_DLL RegisterOGRGML();
void CPL_DLL RegisterOGRAVCBin();
void CPL_DLL RegisterOGRAVCE00();
void CPL_DLL RegisterOGRFME();
void CPL_DLL RegisterOGRREC();
void CPL_DLL RegisterOGRMEM();
void CPL_DLL RegisterOGRVRT();
void CPL_DLL RegisterOGRDODS();
void CPL_DLL RegisterOGRSQLite();
void CPL_DLL RegisterOGRCSV();
void CPL_DLL RegisterOGRILI1();
void CPL_DLL RegisterOGRILI2();
void CPL_DLL RegisterOGRGRASS();
void CPL_DLL RegisterOGRPGeo();
void CPL_DLL RegisterOGRDXFDWG();

CPL_C_END


#endif /* ndef _OGRSF_FRMTS_H_INCLUDED */
