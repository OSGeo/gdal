/******************************************************************************
 * $Id$
 *
 * Project:  GML Reader
 * Purpose:  Declarations for OGR wrapper classes for GML, and GML<->OGR
 *           translation of geometry.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef _OGR_GML_H_INCLUDED
#define _OGR_GML_H_INCLUDED

#include "ogrsf_frmts.h"
#include "gmlreader.h"

class OGRGMLDataSource;

typedef enum
{
    STANDARD,
    SEQUENTIAL_LAYERS,
    INTERLEAVED_LAYERS
} ReadMode;

/************************************************************************/
/*                            OGRGMLLayer                               */
/************************************************************************/

class OGRGMLLayer : public OGRLayer
{
    OGRFeatureDefn     *poFeatureDefn;

    GIntBig             iNextGMLId;
    int                 nTotalGMLCount;
    int                 bInvalidFIDFound;
    char                *pszFIDPrefix;

    int                 bWriter;
    int                 bSameSRS;

    OGRGMLDataSource    *poDS;

    GMLFeatureClass     *poFClass;

    void                *hCacheSRS;

    int                 bUseOldFIDFormat;

    int                 bFaceHoleNegative;

  public:
                        OGRGMLLayer( const char * pszName, 
                                     int bWriter,
                                     OGRGMLDataSource *poDS );

                        ~OGRGMLLayer();

    void                ResetReading();
    OGRFeature *        GetNextFeature();

    GIntBig             GetFeatureCount( int bForce = TRUE );
    OGRErr              GetExtent(OGREnvelope *psExtent, int bForce = TRUE);

    OGRErr              ICreateFeature( OGRFeature *poFeature );
    
    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );
    virtual OGRErr      CreateGeomField( OGRGeomFieldDefn *poField,
                                     int bApproxOK = TRUE );

    int                 TestCapability( const char * );
};

/************************************************************************/
/*                           OGRGMLDataSource                           */
/************************************************************************/

class OGRGMLDataSource : public OGRDataSource
{
    OGRGMLLayer     **papoLayers;
    int                 nLayers;
    
    char                *pszName;
    
    OGRGMLLayer         *TranslateGMLSchema( GMLFeatureClass * );

    char               **papszCreateOptions;

    // output related parameters 
    VSILFILE           *fpOutput;
    int                 bFpOutputIsNonSeekable;
    int                 bFpOutputSingleFile;
    OGREnvelope3D       sBoundingRect;
    int                 bBBOX3D;
    int                 nBoundedByLocation;
    
    int                 nSchemaInsertLocation;
    int                 bIsOutputGML3;
    int                 bIsOutputGML3Deegree; /* if TRUE, then bIsOutputGML3 is also TRUE */
    int                 bIsOutputGML32; /* if TRUE, then bIsOutputGML3 is also TRUE */
    int                 bIsLongSRSRequired;
    int                 bWriteSpaceIndentation;

    OGRSpatialReference* poWriteGlobalSRS;
    int                 bWriteGlobalSRS;

    // input related parameters.
    CPLString           osFilename;
    CPLString           osXSDFilename;

    IGMLReader          *poReader;
    int                 bOutIsTempFile;

    void                InsertHeader();

    int                 bExposeGMLId;
    int                 bExposeFid;
    int                 bIsWFS;

    int                 bUseGlobalSRSName;

    int                 m_bInvertAxisOrderIfLatLong;
    int                 m_bConsiderEPSGAsURN;
    int                 m_bGetSecondaryGeometryOption;

    ReadMode            eReadMode;
    GMLFeature         *poStoredGMLFeature;
    OGRGMLLayer        *poLastReadLayer;
    
    int                 bEmptyAsNull;

    void                FindAndParseTopElements(VSILFILE* fp);
    void                SetExtents(double dfMinX, double dfMinY, double dfMaxX, double dfMaxY);
    
    void                BuildJointClassFromXSD();
    void                BuildJointClassFromScannedSchema();
    
    void                WriteTopElements();

  public:
                        OGRGMLDataSource();
                        ~OGRGMLDataSource();

    int                 Open( GDALOpenInfo* poOpenInfo );
    int                 Create( const char *pszFile, char **papszOptions );

    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );

    virtual OGRLayer    *ICreateLayer( const char *, 
                                      OGRSpatialReference * = NULL,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = NULL );

    int                 TestCapability( const char * );

    VSILFILE            *GetOutputFP() const { return fpOutput; }
    IGMLReader          *GetReader() const { return poReader; }

    void                GrowExtents( OGREnvelope3D *psGeomBounds, int nCoordDimension );

    int                 ExposeId() const { return bExposeGMLId || bExposeFid; }

    static void         PrintLine(VSILFILE* fp, const char *fmt, ...) CPL_PRINT_FUNC_FORMAT (2, 3);

    int                 IsGML3Output() const { return bIsOutputGML3; }
    int                 IsGML3DeegreeOutput() const { return bIsOutputGML3Deegree; }
    int                 IsGML32Output() const { return bIsOutputGML32; }
    int                 IsLongSRSRequired() const { return bIsLongSRSRequired; }
    int                 WriteSpaceIndentation() const { return bWriteSpaceIndentation; }
    const char         *GetGlobalSRSName();

    int                 GetInvertAxisOrderIfLatLong() const { return m_bInvertAxisOrderIfLatLong; }
    int                 GetConsiderEPSGAsURN() const { return m_bConsiderEPSGAsURN; }
    int                 GetSecondaryGeometryOption() const { return m_bGetSecondaryGeometryOption; }

    ReadMode            GetReadMode() const { return eReadMode; }
    void                SetStoredGMLFeature(GMLFeature* poStoredGMLFeatureIn) { poStoredGMLFeature = poStoredGMLFeatureIn; }
    GMLFeature*         PeekStoredGMLFeature() const { return  poStoredGMLFeature; }

    OGRGMLLayer*        GetLastReadLayer() const { return poLastReadLayer; }
    void                SetLastReadLayer(OGRGMLLayer* poLayer) { poLastReadLayer = poLayer; }

    const char         *GetAppPrefix();
    int                 RemoveAppPrefix();
    int                 WriteFeatureBoundedBy();
    const char         *GetSRSDimensionLoc();

    virtual OGRLayer *          ExecuteSQL( const char *pszSQLCommand,
                                            OGRGeometry *poSpatialFilter,
                                            const char *pszDialect );
    virtual void                ReleaseResultSet( OGRLayer * poResultsSet );
    
    static int          CheckHeader(const char* pszStr);
};

#endif /* _OGR_GML_H_INCLUDED */
