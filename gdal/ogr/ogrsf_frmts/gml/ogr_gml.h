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
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef OGR_GML_H_INCLUDED
#define OGR_GML_H_INCLUDED

#include "ogrsf_frmts.h"
#include "gmlreader.h"
#include "gmlutils.h"

#include <memory>

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

class OGRGMLLayer final: public OGRLayer
{
    OGRFeatureDefn     *poFeatureDefn;

    GIntBig             iNextGMLId;
    bool                bInvalidFIDFound;
    char                *pszFIDPrefix;

    bool                bWriter;
    bool                bSameSRS;

    OGRGMLDataSource    *poDS;

    GMLFeatureClass     *poFClass;

    void                *hCacheSRS;

    bool                bUseOldFIDFormat;

    bool                bFaceHoleNegative;

  public:
                        OGRGMLLayer( const char * pszName,
                                     bool bWriter,
                                     OGRGMLDataSource *poDS );

                        virtual ~OGRGMLLayer();

    void                ResetReading() override;
    OGRFeature *        GetNextFeature() override;

    GIntBig             GetFeatureCount( int bForce = TRUE ) override;
    OGRErr              GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

    OGRErr              ICreateFeature( OGRFeature *poFeature ) override;

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE ) override;
    virtual OGRErr      CreateGeomField( OGRGeomFieldDefn *poField,
                                     int bApproxOK = TRUE ) override;

    int                 TestCapability( const char * ) override;
};

/************************************************************************/
/*                           OGRGMLDataSource                           */
/************************************************************************/

class OGRGMLDataSource final: public OGRDataSource
{
    OGRLayer          **papoLayers;
    int                 nLayers;

    char                *pszName;

    OGRGMLLayer         *TranslateGMLSchema( GMLFeatureClass * );

    char               **papszCreateOptions;

    // output related parameters
    VSILFILE           *fpOutput;
    bool                bFpOutputIsNonSeekable;
    bool                bFpOutputSingleFile;
    OGREnvelope3D       sBoundingRect;
    bool                bBBOX3D;
    int                 nBoundedByLocation;

    int                 nSchemaInsertLocation;
    bool                bIsOutputGML3;
    bool                bIsOutputGML3Deegree; /* if TRUE, then bIsOutputGML3 is also TRUE */
    bool                bIsOutputGML32; /* if TRUE, then bIsOutputGML3 is also TRUE */
    OGRGMLSRSNameFormat eSRSNameFormat;
    bool                bWriteSpaceIndentation;

    OGRSpatialReference* poWriteGlobalSRS;
    bool                bWriteGlobalSRS;

    // input related parameters.
    CPLString           osFilename;
    CPLString           osXSDFilename;

    IGMLReader          *poReader;
    bool                bOutIsTempFile;

    void                InsertHeader();

    bool                bExposeGMLId;
    bool                bExposeFid;
    bool                bIsWFS;

    bool                bUseGlobalSRSName;

    bool                m_bInvertAxisOrderIfLatLong;
    bool                m_bConsiderEPSGAsURN;
    GMLSwapCoordinatesEnum m_eSwapCoordinates;
    bool                m_bGetSecondaryGeometryOption;

    ReadMode            eReadMode;
    GMLFeature         *poStoredGMLFeature;
    OGRGMLLayer        *poLastReadLayer;

    bool                bEmptyAsNull;

    OGRSpatialReference m_oStandaloneGeomSRS{};
    std::unique_ptr<OGRGeometry> m_poStandaloneGeom{};

    void                FindAndParseTopElements(VSILFILE* fp);
    void                SetExtents(double dfMinX, double dfMinY, double dfMaxX, double dfMaxY);

    void                BuildJointClassFromXSD();
    void                BuildJointClassFromScannedSchema();

    void                WriteTopElements();

  public:
                        OGRGMLDataSource();
                        virtual ~OGRGMLDataSource();

    bool                Open( GDALOpenInfo* poOpenInfo );
    bool                Create( const char *pszFile, char **papszOptions );

    const char          *GetName() override { return pszName; }
    int                 GetLayerCount() override { return nLayers; }
    OGRLayer            *GetLayer( int ) override;

    virtual OGRLayer    *ICreateLayer( const char *,
                                      OGRSpatialReference * = nullptr,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = nullptr ) override;

    int                 TestCapability( const char * ) override;

    VSILFILE            *GetOutputFP() const { return fpOutput; }
    IGMLReader          *GetReader() const { return poReader; }

    void                GrowExtents( OGREnvelope3D *psGeomBounds, int nCoordDimension );

    int                 ExposeId() const { return bExposeGMLId || bExposeFid; }

    static void         PrintLine(VSILFILE* fp, const char *fmt, ...) CPL_PRINT_FUNC_FORMAT (2, 3);

    bool                IsGML3Output() const { return bIsOutputGML3; }
    bool                IsGML3DeegreeOutput() const { return bIsOutputGML3Deegree; }
    bool                IsGML32Output() const { return bIsOutputGML32; }
    OGRGMLSRSNameFormat GetSRSNameFormat() const { return eSRSNameFormat; }
    bool                WriteSpaceIndentation() const { return bWriteSpaceIndentation; }
    const char         *GetGlobalSRSName();

    bool                GetInvertAxisOrderIfLatLong() const { return m_bInvertAxisOrderIfLatLong; }
    bool                GetConsiderEPSGAsURN() const { return m_bConsiderEPSGAsURN; }
    GMLSwapCoordinatesEnum GetSwapCoordinates() const { return m_eSwapCoordinates; }
    bool                GetSecondaryGeometryOption() const { return m_bGetSecondaryGeometryOption; }

    ReadMode            GetReadMode() const { return eReadMode; }
    void                SetStoredGMLFeature(GMLFeature* poStoredGMLFeatureIn) { poStoredGMLFeature = poStoredGMLFeatureIn; }
    GMLFeature*         PeekStoredGMLFeature() const { return  poStoredGMLFeature; }

    OGRGMLLayer*        GetLastReadLayer() const { return poLastReadLayer; }
    void                SetLastReadLayer(OGRGMLLayer* poLayer) { poLastReadLayer = poLayer; }

    const char         *GetAppPrefix() const;
    bool                RemoveAppPrefix() const;
    bool                WriteFeatureBoundedBy() const;
    const char         *GetSRSDimensionLoc() const;
    bool                GMLFeatureCollection() const;

    virtual OGRLayer *          ExecuteSQL( const char *pszSQLCommand,
                                            OGRGeometry *poSpatialFilter,
                                            const char *pszDialect ) override;
    virtual void                ReleaseResultSet( OGRLayer * poResultsSet ) override;

    static bool          CheckHeader(const char* pszStr);
};

#endif /* OGR_GML_H_INCLUDED */
