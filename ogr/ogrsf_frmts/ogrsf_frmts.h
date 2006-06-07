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
 * Revision 1.58  2006/06/07 14:39:09  mloskot
 * Added new KML (Beta) driver to OGR. Thanks to Christopher Condit - the author of KML drvier.
 *
 * Revision 1.57  2006/01/27 00:09:07  fwarmerdam
 * added Get{FID,Geometry}Column() support
 *
 * Revision 1.56  2005/11/22 17:01:09  fwarmerdam
 * added SDE support
 *
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

    virtual const char *GetFIDColumn();
    virtual const char *GetGeometryColumn();

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
void CPL_DLL RegisterOGRKML();
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
void CPL_DLL RegisterOGRSDE();

CPL_C_END


#endif /* ndef _OGRSF_FRMTS_H_INCLUDED */
