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
 ****************************************************************************/

#ifndef OGR_DODS_H_INCLUDED
#define OGR_DODS_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_error.h"
#include "cpl_conv.h"

// Lots of DODS related definitions

#include <string>
#include <sstream>
#include <algorithm>
#include <exception>

#include "libdap_headers.h"

using namespace libdap;

/************************************************************************/
/*                           OGRDODSFieldDefn                           */
/************************************************************************/
class OGRDODSFieldDefn {
public:
    OGRDODSFieldDefn();
    ~OGRDODSFieldDefn();

    bool Initialize( AttrTable *,
                     BaseType *poTarget = nullptr, BaseType *poSuperSeq = nullptr );
    bool Initialize( const char *, const char * = "das",
                     BaseType *poTarget = nullptr, BaseType *poSuperSeq = nullptr );

    bool bValid;
    char *pszFieldName;
    char *pszFieldScope;
    int  iFieldIndex;
    char *pszFieldValue;
    char *pszPathToSequence;

    bool bRelativeToSuperSequence;
    bool bRelativeToSequence;
};

/************************************************************************/
/*                             OGRDODSLayer                             */
/************************************************************************/

class OGRDODSDataSource;

class OGRDODSLayer CPL_NON_FINAL: public OGRLayer
{
  protected:
    OGRFeatureDefn     *poFeatureDefn;

    OGRSpatialReference *poSRS;

    int                 iNextShapeId;

    OGRDODSDataSource  *poDS;

    char               *pszQuery;

    char               *pszFIDColumn;

    char               *pszTarget;

    OGRDODSFieldDefn  **papoFields;

    virtual bool        ProvideDataDDS();
    bool                bDataLoaded;

    AISConnect         *poConnection;
    DataDDS            *poDataDDS;

    BaseType           *poTargetVar;

    AttrTable          *poOGRLayerInfo;

    bool                bKnowExtent;
    OGREnvelope         sExtent;

  public:
                        OGRDODSLayer( OGRDODSDataSource *poDS,
                                      const char *pszTarget,
                                      AttrTable *poAttrInfo );
    virtual             ~OGRDODSLayer();

    virtual void        ResetReading() override;

    virtual OGRFeature *GetNextFeature() override;

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    virtual OGRSpatialReference *GetSpatialRef() override;

    virtual int         TestCapability( const char * ) override;

    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }
};

/************************************************************************/
/*                         OGRDODSSequenceLayer                         */
/************************************************************************/

class OGRDODSSequenceLayer final: public OGRDODSLayer
{
private:
    OGRDODSFieldDefn    oXField;
    OGRDODSFieldDefn    oYField;
    OGRDODSFieldDefn    oZField;

    const char         *pszSubSeqPath;

    Sequence           *poSuperSeq;

    int                 iLastSuperSeq;

    int                 nRecordCount; /* -1 if not yet known */
    int                 nSuperSeqCount;
    int                *panSubSeqSize;

    double              GetFieldValueAsDouble( OGRDODSFieldDefn *, int );
    BaseType           *GetFieldValue( OGRDODSFieldDefn *, int,
                                       Sequence * );

    static double              BaseTypeToDouble( BaseType * );

    bool                BuildFields( BaseType *, const char *,
                                     const char * );

    Sequence           *FindSuperSequence( BaseType * );

protected:
    virtual bool        ProvideDataDDS() override;

public:
                        OGRDODSSequenceLayer( OGRDODSDataSource *poDS,
                                              const char *pszTarget,
                                              AttrTable *poAttrInfo );
    virtual             ~OGRDODSSequenceLayer();

    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    virtual GIntBig     GetFeatureCount( int ) override;
};

/************************************************************************/
/*                           OGRDODSGridLayer                           */
/************************************************************************/

class OGRDODSDim
{
public:
    OGRDODSDim() {
        pszDimName = nullptr;
        nDimStart = 0;
        nDimEnd = 0;
        nDimStride = 0;
        nDimEntries = 0;
        poMap = nullptr;
        pRawData = nullptr;
        iLastValue = 0;
    }
    ~OGRDODSDim() {
        CPLFree( pszDimName );
        CPLFree( pRawData );
    }

    char *pszDimName;
    int  nDimStart;
    int  nDimEnd;
    int  nDimStride;
    int  nDimEntries;
    Array *poMap;
    void *pRawData;
    int  iLastValue;
};

class OGRDODSArrayRef
{
public:
    OGRDODSArrayRef() {
        pszName = nullptr;
        iFieldIndex = -1;
        poArray = nullptr;
        pRawData = nullptr;
            }
    ~OGRDODSArrayRef() {
        CPLFree( pszName );
        CPLFree( pRawData );
    }

    char *pszName;
    int   iFieldIndex;
    Array *poArray;
    void  *pRawData;
};

class OGRDODSGridLayer final: public OGRDODSLayer
{
    Grid               *poTargetGrid; // NULL if simple array used.
    Array              *poTargetArray;

    int                 nArrayRefCount;
    OGRDODSArrayRef    *paoArrayRefs;  // includes poTargetArray.

    OGRDODSFieldDefn    oXField;
    OGRDODSFieldDefn    oYField;
    OGRDODSFieldDefn    oZField;

    int                 nDimCount;
    OGRDODSDim         *paoDimensions;
    int                 nMaxRawIndex;

    static bool                ArrayEntryToField( Array *poArray, void *pRawData,
                                           int iArrayIndex,
                                           OGRFeature *poFeature, int iField );

    CPL_DISALLOW_COPY_ASSIGN(OGRDODSGridLayer)

protected:
    virtual bool        ProvideDataDDS() override;

public:
                        OGRDODSGridLayer( OGRDODSDataSource *poDS,
                                         const char *pszTarget,
                                         AttrTable *poAttrInfo );
    virtual             ~OGRDODSGridLayer();

    virtual OGRFeature *GetFeature( GIntBig nFeatureId ) override;

    virtual GIntBig     GetFeatureCount( int ) override;
};

/************************************************************************/
/*                          OGRDODSDataSource                           */
/************************************************************************/

class OGRDODSDataSource final: public OGRDataSource
{
    OGRDODSLayer        **papoLayers;
    int                 nLayers;

    char               *pszName;

    void                AddLayer( OGRDODSLayer * );

    CPL_DISALLOW_COPY_ASSIGN(OGRDODSDataSource)

  public: // Just intended for read access by layer classes.
    AISConnect         *poConnection;

    DAS                 oDAS;
    DDS                 *poDDS;
    BaseTypeFactory     *poBTF;

    string              oBaseURL;
    string              oProjection;
    string              oConstraints;

  public:

                        OGRDODSDataSource();
                        ~OGRDODSDataSource();

    int                 Open( const char * );

    const char          *GetName() override { return pszName; }
    int                 GetLayerCount() override { return nLayers; }
    OGRLayer            *GetLayer( int ) override;

    int                 TestCapability( const char * ) override;
};

/************************************************************************/
/*                             OGRDODSDriver                            */
/************************************************************************/

class OGRDODSDriver final: public OGRSFDriver
{
  public:
                ~OGRDODSDriver();
    const char *GetName() override;
    OGRDataSource *Open( const char *, int ) override;
    int                 TestCapability( const char * ) override;
};

string OGRDODSGetVarPath( BaseType * );
int  OGRDODSGetVarIndex( Sequence *poParent, string oVarName );

bool OGRDODSIsFloatInvalid( const float * );
bool OGRDODSIsDoubleInvalid( const double * );

#endif /* ndef OGR_DODS_H_INCLUDED */
