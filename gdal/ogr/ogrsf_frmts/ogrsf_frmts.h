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
 ****************************************************************************/

#ifndef _OGRSF_FRMTS_H_INCLUDED
#define _OGRSF_FRMTS_H_INCLUDED

#include "ogr_feature.h"
#include "ogr_featurestyle.h"

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

/* Note: any virtual method added to this class must also be added in the */
/* OGRLayerDecorator class. */

class CPL_DLL OGRLayer
{
  protected:
    int          m_bFilterIsEnvelope;
    OGRGeometry *m_poFilterGeom;
    OGREnvelope  m_sFilterEnvelope;
    
    int          FilterGeometry( OGRGeometry * );
    //int          FilterGeometry( OGRGeometry *, OGREnvelope* psGeometryEnvelope);
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

    virtual const char *GetName();
    virtual OGRwkbGeometryType GetGeomType();
    virtual OGRFeatureDefn *GetLayerDefn() = 0;

    virtual OGRSpatialReference *GetSpatialRef() { return NULL; }

    virtual int         GetFeatureCount( int bForce = TRUE );
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE);

    virtual int         TestCapability( const char * ) = 0;

    virtual const char *GetInfo( const char * );

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
    virtual OGRErr      DeleteField( int iField );
    virtual OGRErr      ReorderFields( int* panMap );
    virtual OGRErr      AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags );

    virtual OGRErr      SyncToDisk();

    virtual OGRStyleTable *GetStyleTable();
    virtual void        SetStyleTableDirectly( OGRStyleTable *poStyleTable );
                            
    virtual void        SetStyleTable(OGRStyleTable *poStyleTable);

    virtual OGRErr      StartTransaction();
    virtual OGRErr      CommitTransaction();
    virtual OGRErr      RollbackTransaction();

    virtual const char *GetFIDColumn();
    virtual const char *GetGeometryColumn();

    virtual OGRErr      SetIgnoredFields( const char **papszFields );

    int                 Reference();
    int                 Dereference();
    int                 GetRefCount() const;

    GIntBig             GetFeaturesRead();

    /* non virtual : conveniency wrapper for ReorderFields() */
    OGRErr              ReorderField( int iOldFieldPos, int iNewFieldPos );

    int                 AttributeFilterEvaluationNeedsGeometry();

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

    void        *m_hMutex;

    OGRLayer*       BuildLayerFromSelectInfo(void* psSelectInfo,
                                             OGRGeometry *poSpatialFilter,
                                             const char *pszDialect);

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

    virtual OGRStyleTable *GetStyleTable();
    virtual void        SetStyleTableDirectly( OGRStyleTable *poStyleTable );
                            
    virtual void        SetStyleTable(OGRStyleTable *poStyleTable);

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
    void                SetDriver( OGRSFDriver *poDriver );

  protected:

    OGRErr              ProcessSQLCreateIndex( const char * );
    OGRErr              ProcessSQLDropIndex( const char * );
    OGRErr              ProcessSQLDropTable( const char * );
    OGRErr              ProcessSQLAlterTableAddColumn( const char * );
    OGRErr              ProcessSQLAlterTableDropColumn( const char * );
    OGRErr              ProcessSQLAlterTableAlterColumn( const char * );
    OGRErr              ProcessSQLAlterTableRenameColumn( const char * );

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
 * Singleton manager for OGRSFDriver instances that will be used to try
 * and open datasources.  Normally the registrar is populated with 
 * standard drivers using the OGRRegisterAll() function and does not need
 * to be directly accessed.  The driver registrar and all registered drivers
 * may be cleaned up on shutdown using OGRCleanupAll().
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
    GIntBig     *panOpenDSPID;

  public:

                ~OGRSFDriverRegistrar();

    static OGRSFDriverRegistrar *GetRegistrar();
    static OGRDataSource *Open( const char *pszName, int bUpdate=FALSE,
                                OGRSFDriver ** ppoDriver = NULL );

    OGRDataSource *OpenShared( const char *pszName, int bUpdate=FALSE,
                               OGRSFDriver ** ppoDriver = NULL );
    OGRErr      ReleaseDataSource( OGRDataSource * );

    void        RegisterDriver( OGRSFDriver * poDriver );
    void        DeregisterDriver( OGRSFDriver * poDriver );

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

void CPL_DLL RegisterOGRFileGDB();
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
void CPL_DLL RegisterOGRMSSQLSpatial();
void CPL_DLL RegisterOGRMySQL();
void CPL_DLL RegisterOGROCI();
void CPL_DLL RegisterOGRDGN();
void CPL_DLL RegisterOGRGML();
void CPL_DLL RegisterOGRLIBKML();
void CPL_DLL RegisterOGRKML();
void CPL_DLL RegisterOGRGeoJSON();
void CPL_DLL RegisterOGRAVCBin();
void CPL_DLL RegisterOGRAVCE00();
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
void CPL_DLL RegisterOGRDXF();
void CPL_DLL RegisterOGRDWG();
void CPL_DLL RegisterOGRSDE();
void CPL_DLL RegisterOGRIDB();
void CPL_DLL RegisterOGRGMT();
void CPL_DLL RegisterOGRBNA();
void CPL_DLL RegisterOGRGPX();
void CPL_DLL RegisterOGRGeoconcept();
void CPL_DLL RegisterOGRIngres();
void CPL_DLL RegisterOGRPCIDSK();
void CPL_DLL RegisterOGRXPlane();
void CPL_DLL RegisterOGRNAS();
void CPL_DLL RegisterOGRGeoRSS();
void CPL_DLL RegisterOGRGTM();
void CPL_DLL RegisterOGRVFK();
void CPL_DLL RegisterOGRPGDump();
void CPL_DLL RegisterOGROSM();
void CPL_DLL RegisterOGRGPSBabel();
void CPL_DLL RegisterOGRSUA();
void CPL_DLL RegisterOGROpenAir();
void CPL_DLL RegisterOGRPDS();
void CPL_DLL RegisterOGRWFS();
void CPL_DLL RegisterOGRSOSI();
void CPL_DLL RegisterOGRHTF();
void CPL_DLL RegisterOGRAeronavFAA();
void CPL_DLL RegisterOGRGeomedia();
void CPL_DLL RegisterOGRMDB();
void CPL_DLL RegisterOGREDIGEO();
void CPL_DLL RegisterOGRGFT();
void CPL_DLL RegisterOGRSVG();
void CPL_DLL RegisterOGRCouchDB();
void CPL_DLL RegisterOGRIdrisi();
void CPL_DLL RegisterOGRARCGEN();
void CPL_DLL RegisterOGRSEGUKOOA();
void CPL_DLL RegisterOGRSEGY();
void CPL_DLL RegisterOGRXLS();
void CPL_DLL RegisterOGRODS();
void CPL_DLL RegisterOGRXLSX();
void CPL_DLL RegisterOGRElastic();
void CPL_DLL RegisterOGRPDF();
CPL_C_END


#endif /* ndef _OGRSF_FRMTS_H_INCLUDED */
