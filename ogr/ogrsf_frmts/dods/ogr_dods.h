/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR/DODS driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.2  2004/01/22 21:15:36  warmerda
 * parse url into components
 *
 * Revision 1.1  2004/01/21 20:08:29  warmerda
 * New
 *
 */

#ifndef _OGR_DODS_H_INCLUDED
#define _OGR_DODS_H_INLLUDED

#include "ogrsf_frmts.h"
#include "cpl_error.h"

// Lots of DODS related definitions

#include <string>
#include <sstream>
#include <algorithm>
#include <exception>

// #define DODS_DEBUG 1
#include <debug.h>

#include <BaseType.h>		// DODS
#include <Byte.h>
#include <Int16.h>
#include <UInt16.h>
#include <Int32.h>
#include <UInt32.h>
#include <Float32.h>
#include <Float64.h>
#include <Str.h>
#include <Url.h>
#include <Array.h>
#include <Structure.h>
#include <Sequence.h>
#include <Grid.h>

#include <AISConnect.h>		
#include <DDS.h>
#include <DAS.h>
#include <Error.h>
#include <escaping.h>

/************************************************************************/
/*                           OGRDODSFieldDefn                           */
/************************************************************************/
class OGRDODSFieldDefn {
public:
    OGRDODSFieldDefn();
    ~OGRDODSFieldDefn();
    
    int Initialize( AttrTable * );
    int Initialize( const char *, const char * = "das" );

    int  bValid;
    char *pszFieldName;
    char *pszFieldScope;
    int  iFieldIndex;
    char *pszFieldValue;
};

/************************************************************************/
/*                             OGRDODSLayer                             */
/************************************************************************/

class OGRDODSDataSource;
    
class OGRDODSLayer : public OGRLayer
{
  protected:
    OGRFeatureDefn     *poFeatureDefn;

    OGRSpatialReference *poSRS;

    OGRGeometry         *poFilterGeom;

    int                 iNextShapeId;

    OGRDODSDataSource  *poDS;

    char               *pszQuery;

    char               *pszFIDColumn;

    char               *pszTarget;

    int                *panFieldMapping;

    int                 BuildFields( BaseType *, const char * );

    int                 ProvideDataDDS();
    int                 bDataLoaded;

    AISConnect         *poConnection;
    DataDDS             oDataDDS;

    BaseType           *poTargetVar;
    
    AttrTable          *poOGRLayerInfo;

    int                 bKnowExtent;
    OGREnvelope         sExtent;

  public:
                        OGRDODSLayer( OGRDODSDataSource *poDS, 
                                      const char *pszTarget,
                                      AttrTable *poAttrInfo );
    virtual             ~OGRDODSLayer();

    virtual void        ResetReading();

    virtual OGRFeature *GetNextFeature();

    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    virtual OGRSpatialReference *GetSpatialRef();

    virtual OGRGeometry *GetSpatialFilter() { return poFilterGeom; }
    virtual void        SetSpatialFilter( OGRGeometry * );

    virtual int         TestCapability( const char * );

    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE);
};

/************************************************************************/
/*                         OGRDODSSequenceLayer                         */
/************************************************************************/

class OGRDODSSequenceLayer : public OGRDODSLayer
{
private:
    OGRDODSFieldDefn    oXField;
    OGRDODSFieldDefn    oYField;
    OGRDODSFieldDefn    oZField;

    double              GetFieldValueAsDouble( OGRDODSFieldDefn *, int );
    BaseType           *GetFieldValue( OGRDODSFieldDefn *, int );
    
public:
                        OGRDODSSequenceLayer( OGRDODSDataSource *poDS, 
                                              const char *pszTarget,
                                              AttrTable *poAttrInfo );
    virtual             ~OGRDODSSequenceLayer();

    virtual OGRFeature *GetFeature( long nFeatureId );
    
    virtual int         GetFeatureCount( int );
};

/************************************************************************/
/*                          OGRDODSDataSource                           */
/************************************************************************/

class OGRDODSDataSource : public OGRDataSource
{
    OGRDODSLayer        **papoLayers;
    int                 nLayers;
    
    char               *pszName;

    void                AddLayer( OGRDODSLayer * );

  public: // Just intended for read access by layer classes.
    AISConnect         *poConnection;

    DAS                 oDAS;
    DDS                 oDDS;

    string              oBaseURL;
    string              oProjection;
    string              oConstraints;

  public:

                        OGRDODSDataSource();
                        ~OGRDODSDataSource();

    int                 Open( const char * );

    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );

    int                 TestCapability( const char * );
};

/************************************************************************/
/*                             OGRDODSDriver                            */
/************************************************************************/

class OGRDODSDriver : public OGRSFDriver
{
  public:
                ~OGRDODSDriver();
    const char *GetName();
    OGRDataSource *Open( const char *, int );
    int                 TestCapability( const char * );
};


#endif /* ndef _OGR_DODS_H_INCLUDED */


