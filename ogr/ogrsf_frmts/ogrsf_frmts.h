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

#define OLCRandomRead          "RandomRead"
#define OLCSequentialWrite     "SequentialWrite"
#define OLCRandomWrite         "RandomWrite"
#define OLCFastSpatialFilter   "FastSpatialFilter"
#define OLCFastFeatureCount    "FastFeatureCount"
#define OLCCreateField         "CreateField"

#define ODsCCreateLayer        "CreateLayer"

#define ODrCCreateDataSource   "CreateDataSource"

/************************************************************************/
/*                               OGRLayer                               */
/************************************************************************/

/**
 * This class represents a layer of simple features, with access methods.
 *
 */

class OGRLayer
{
  public:
    virtual     ~OGRLayer(){}
                 OGRLayer(){m_poStyleTable = NULL;}
    virtual OGRGeometry *GetSpatialFilter() = 0;
    virtual void        SetSpatialFilter( OGRGeometry * ) = 0;

    virtual void        ResetReading() = 0;
    virtual OGRFeature *GetNextFeature() = 0;
    virtual OGRFeature *GetFeature( long nFID );
    virtual OGRErr      SetFeature( OGRFeature *poFeature );
    virtual OGRErr      CreateFeature( OGRFeature *poFeature );

    virtual OGRFeatureDefn *GetLayerDefn() = 0;

    virtual OGRSpatialReference *GetSpatialRef() { return NULL; }

    virtual int         GetFeatureCount( int bForce = TRUE );

    virtual int         TestCapability( const char * ) = 0;

    virtual const char *GetInfo( const char * );

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );

    OGRStyleTable       *GetStyleTable(){return m_poStyleTable;}
    void                 SetStyleTable(OGRStyleTable *poStyleTable){m_poStyleTable = poStyleTable;}

 protected:
    OGRStyleTable *m_poStyleTable;
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

class OGRDataSource
{
  public:

    OGRDataSource();
    virtual     ~OGRDataSource();

    virtual const char  *GetName() = 0;

    virtual int         GetLayerCount() = 0;
    virtual OGRLayer    *GetLayer(int) = 0;

    virtual int         TestCapability( const char * ) = 0;

    virtual OGRLayer    *CreateLayer( const char *, 
                                      OGRSpatialReference * = NULL,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = NULL );
    OGRStyleTable       *GetStyleTable(){return m_poStyleTable;}
    
  protected:
    OGRStyleTable *m_poStyleTable;
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

class OGRSFDriver
{
  public:
    virtual     ~OGRSFDriver();

    virtual const char  *GetName() = 0;

    virtual OGRDataSource *Open( const char *pszName, int bUpdate=FALSE ) = 0;

    virtual int         TestCapability( const char * ) = 0;

    virtual OGRDataSource *CreateDataSource( const char *pszName,
                                             char ** = NULL );
};


/************************************************************************/
/*                         OGRSFDriverRegistrar                         */
/************************************************************************/

/**
 * Singleton manager for drivers.
 *
 */

class OGRSFDriverRegistrar
{
    int         nDrivers;
    OGRSFDriver **papoDrivers;

                OGRSFDriverRegistrar();

  public:

                ~OGRSFDriverRegistrar();

    static OGRSFDriverRegistrar *GetRegistrar();
    static OGRDataSource *Open( const char *pszName, int bUpdate=FALSE,
                                OGRSFDriver ** ppoDriver = NULL );

    void        RegisterDriver( OGRSFDriver * );

    int         GetDriverCount( void );
    OGRSFDriver *GetDriver( int );

};

/* -------------------------------------------------------------------- */
/*      Various available registration methods.                         */
/* -------------------------------------------------------------------- */
CPL_C_START
void    OGRRegisterAll();

void    RegisterOGRShape();
void    RegisterOGRNTF();
void    RegisterOGRFME();
void    RegisterOGRSDTS();
void    RegisterOGRTiger();
void    RegisterOGRS57();
void    RegisterOGRTAB();
void    RegisterOGRMIF();
void    RegisterOGROGDI();
void    RegisterOGRPG();
void    RegisterOGRDGN();
CPL_C_END


#endif /* ndef _OGRSF_FRMTS_H_INCLUDED */
