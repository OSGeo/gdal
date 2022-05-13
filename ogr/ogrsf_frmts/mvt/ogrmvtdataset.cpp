/******************************************************************************
 *
 * Project:  MVT Translator
 * Purpose:  Mapbox Vector Tile decoder
 * Author:   Even Rouault, Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2018, Even Rouault <even dot rouault at spatialys dot com>
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

#if defined(HAVE_SQLITE) && defined(HAVE_GEOS)
// Needed by mvtutils.h
#define HAVE_MVT_WRITE_SUPPORT
#endif

#include "ogrsf_frmts.h"
#include "cpl_conv.h"
#include "cpl_json.h"
#include "cpl_http.h"
#include "ogr_p.h"

#include "mvt_tile.h"
#include "mvtutils.h"

#include "ogr_geos.h"

#include "gpb.h"

#include <algorithm>
#include <memory>
#include <vector>
#include <set>

CPL_CVSID("$Id$")

#if GEOS_VERSION_MAJOR > 3 || \
    (GEOS_VERSION_MAJOR == 3 && GEOS_VERSION_MINOR >= 8)
#define HAVE_MAKE_VALID
#endif

const char* SRS_EPSG_3857 = "PROJCS[\"WGS 84 / Pseudo-Mercator\",GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],AUTHORITY[\"EPSG\",\"4326\"]],PROJECTION[\"Mercator_1SP\"],PARAMETER[\"central_meridian\",0],PARAMETER[\"scale_factor\",1],PARAMETER[\"false_easting\",0],PARAMETER[\"false_northing\",0],UNIT[\"metre\",1,AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"X\",EAST],AXIS[\"Y\",NORTH],EXTENSION[\"PROJ4\",\"+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext  +no_defs\"],AUTHORITY[\"EPSG\",\"3857\"]]";

// WebMercator related constants
constexpr double kmSPHERICAL_RADIUS = 6378137.0;

constexpr int knMAX_FILES_PER_DIR = 10000;

#ifdef HAVE_MVT_WRITE_SUPPORT

#include <sqlite3.h>
#include "../sqlite/ogrsqliteutility.h"

#include "../sqlite/ogrsqlitevfs.h"

#include "cpl_worker_thread_pool.h"

#include <mutex>

// Limitations from https://github.com/mapbox/mapbox-geostats
constexpr size_t knMAX_COUNT_LAYERS = 1000;
constexpr size_t knMAX_REPORT_LAYERS = 100;
constexpr size_t knMAX_COUNT_FIELDS = 1000;
constexpr size_t knMAX_REPORT_FIELDS = 100;
constexpr size_t knMAX_COUNT_VALUES = 1000;
constexpr size_t knMAX_REPORT_VALUES = 100;
constexpr size_t knMAX_STRING_VALUE_LENGTH = 256;
constexpr size_t knMAX_LAYER_NAME_LENGTH = 256;
constexpr size_t knMAX_FIELD_NAME_LENGTH = 256;

#undef SQLITE_STATIC
#define SQLITE_STATIC      ((sqlite3_destructor_type)nullptr)

#endif

/************************************************************************/
/*                    InitWebMercatorTilingScheme()                     */
/************************************************************************/

static void InitWebMercatorTilingScheme(OGRSpatialReference* poSRS,
                                        double& dfTopX,
                                        double& dfTopY,
                                        double& dfTileDim0)
{
    constexpr double kmMAX_GM =  kmSPHERICAL_RADIUS * M_PI;  // 20037508.342789244
    poSRS->SetFromUserInput(SRS_EPSG_3857);
    dfTopX = -kmMAX_GM;
    dfTopY = kmMAX_GM;
    dfTileDim0 = 2 * kmMAX_GM;
}

/************************************************************************/
/*                           GetCmdId()                                 */
/************************************************************************/

/* For a drawing instruction combining a command id and a command count,
 * return the command id */
static unsigned GetCmdId(unsigned int nCmdCountCombined)
{
    return nCmdCountCombined & 0x7;
}

/************************************************************************/
/*                           GetCmdCount()                              */
/************************************************************************/

/* For a drawing instruction combining a command id and a command count,
 * return the command count */
static unsigned GetCmdCount(unsigned int nCmdCountCombined)
{
    return nCmdCountCombined >> 3;
}

/************************************************************************/
/*                          OGRMVTLayerBase                             */
/************************************************************************/

class OGRMVTLayerBase CPL_NON_FINAL: public OGRLayer, public OGRGetNextFeatureThroughRaw<OGRMVTLayerBase>
{
        virtual OGRFeature         *GetNextRawFeature() = 0;

    protected:
        OGRFeatureDefn             *m_poFeatureDefn = nullptr;

        void                InitFields(const CPLJSONObject& oFields);

    public:
        virtual ~OGRMVTLayerBase();

        virtual OGRFeatureDefn *    GetLayerDefn() override
                                            { return m_poFeatureDefn; }

        DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRMVTLayerBase)

        virtual int                 TestCapability( const char * ) override;
};

/************************************************************************/
/*                             OGRMVTLayer                              */
/************************************************************************/

class OGRMVTDataset;

class OGRMVTLayer final: public OGRMVTLayerBase
{
    OGRMVTDataset       *m_poDS;
    const GByte               *m_pabyDataStart;
    const GByte               *m_pabyDataEnd;
    const GByte               *m_pabyDataCur = nullptr;
    const GByte               *m_pabyDataFeatureStart = nullptr;
    bool                 m_bError = false;
    unsigned int         m_nExtent = knDEFAULT_EXTENT;
    std::vector<CPLString> m_aosKeys;
    typedef struct
    {
        OGRFieldType     eType;
        OGRFieldSubType  eSubType;
        OGRField         sValue;
    } Value;
    std::vector<Value>   m_asValues;
    GIntBig              m_nFID = 0;
    GIntBig              m_nFeatureCount = -1;
    OGRPolygon           m_oClipPoly;
    double               m_dfTileMinX = 0;
    double               m_dfTileMinY = 0;
    double               m_dfTileMaxX = 0;
    double               m_dfTileMaxY = 0;

    void                Init(const CPLJSONObject& oFields);
    bool                QuickScanFeature(const GByte* pabyData,
                                         const GByte* pabyDataFeatureEnd,
                                         bool bScanFields,
                                         bool bScanGeometries,
                                         bool& bGeomTypeSet);
    void                GetXY(int nX, int nY, double& dfX, double& dfY);
    OGRGeometry        *ParseGeometry(unsigned int nGeomType,
                                      const GByte* pabyDataGeometryEnd);
    void                SanitizeClippedGeometry(OGRGeometry*& poGeom);

    virtual OGRFeature         *GetNextRawFeature() override;

  public:
                        OGRMVTLayer(OGRMVTDataset* poDS,
                                    const char* pszLayerName,
                                    const GByte* pabyData,
                                    int nLayerSize,
                                    const CPLJSONObject& oFields,
                                    OGRwkbGeometryType eGeomType);
               virtual ~OGRMVTLayer();

    virtual void                ResetReading() override;

    virtual GIntBig             GetFeatureCount( int bForce ) override;
};

/************************************************************************/
/*                        OGRMVTDirectoryLayer                          */
/************************************************************************/

class OGRMVTDirectoryLayer final: public OGRMVTLayerBase
{
    OGRMVTDataset              *m_poDS;
    int                         m_nZ = 0;
    bool                        m_bUseReadDir = true;
    CPLString                   m_osDirName;
    CPLStringList               m_aosDirContent;
    CPLString                   m_aosSubDirName;
    CPLStringList               m_aosSubDirContent;
    bool                        m_bEOF = false;
    int                         m_nXIndex = 0;
    int                         m_nYIndex = 0;
    GDALDataset                *m_poCurrentTile = nullptr;
    bool                        m_bJsonField = false;
    GIntBig                     m_nFIDBase = 0;
    OGREnvelope                 m_sExtent;
    int                         m_nFilterMinX = 0;
    int                         m_nFilterMinY = 0;
    int                         m_nFilterMaxX = 0;
    int                         m_nFilterMaxY = 0;

    virtual OGRFeature         *GetNextRawFeature() override;
    OGRFeature*                 CreateFeatureFrom(OGRFeature* poSrcFeature);
    void                        ReadNewSubDir();
    void                        OpenTile();
    void                        OpenTileIfNeeded();

  public:
                        OGRMVTDirectoryLayer(
                                    OGRMVTDataset* poDS,
                                    const char* pszLayerName,
                                    const char* pszDirectoryName,
                                    const CPLJSONObject& oFields,
                                    bool bJsonField,
                                    OGRwkbGeometryType eGeomType,
                                    const OGREnvelope* psExtent);
               virtual ~OGRMVTDirectoryLayer();

    virtual void                ResetReading() override;

    virtual GIntBig             GetFeatureCount( int bForce ) override;
    OGRErr              GetExtent( OGREnvelope *psExtent, int bForce ) override;
    virtual OGRErr      GetExtent( int iGeomField, OGREnvelope *psExtent,
                                   int bForce ) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

    virtual void        SetSpatialFilter( OGRGeometry * ) override;
    virtual void        SetSpatialFilter( int iGeomField,
                                          OGRGeometry *poGeom ) override
                { OGRLayer::SetSpatialFilter(iGeomField, poGeom); }

    virtual OGRFeature         *GetFeature(GIntBig nFID) override;

    virtual int                 TestCapability( const char * ) override;
};

/************************************************************************/
/*                           OGRMVTDataset                              */
/************************************************************************/

class OGRMVTDataset final: public GDALDataset
{
    friend class OGRMVTLayer;
    friend class OGRMVTDirectoryLayer;

    GByte                                      *m_pabyData;
    std::vector<std::unique_ptr<OGRLayer>>      m_apoLayers;
    bool                                        m_bGeoreferenced = false;
    double                                      m_dfTileDimX = 0.0;
    double                                      m_dfTileDimY = 0.0;
    double                                      m_dfTopX = 0.0;
    double                                      m_dfTopY = 0.0;
    CPLString                                   m_osMetadataMemFilename;
    bool                                        m_bClip = true;
    CPLString                                   m_osTileExtension{"pbf"};
    OGRSpatialReference*                        m_poSRS = nullptr;
    double                                      m_dfTileDim0 = 0.0;
    double                                      m_dfTopXOrigin = 0.0;
    double                                      m_dfTopYOrigin = 0.0;

    static GDALDataset* OpenDirectory(GDALOpenInfo*);

  public:
               explicit OGRMVTDataset(GByte* pabyData);
               virtual ~OGRMVTDataset();

    virtual int                 GetLayerCount() override
                        { return static_cast<int>(m_apoLayers.size()); }
    virtual OGRLayer*           GetLayer( int ) override;

    virtual int                 TestCapability( const char * ) override
                        { return FALSE; }

    static GDALDataset* Open(GDALOpenInfo*);

    OGRSpatialReference* GetSRS() { return m_poSRS; }
    double GetTileDim0() const { return m_dfTileDim0; }
    double GetTopXOrigin() const { return m_dfTopXOrigin; }
    double GetTopYOrigin() const { return m_dfTopYOrigin; }
};

/************************************************************************/
/*                        ~OGRMVTLayerBase()                            */
/************************************************************************/

OGRMVTLayerBase::~OGRMVTLayerBase()
{
    m_poFeatureDefn->Release();
}

/************************************************************************/
/*                           InitFields()                               */
/************************************************************************/

void OGRMVTLayerBase::InitFields(const CPLJSONObject& oFields)
{
    OGRMVTInitFields(m_poFeatureDefn, oFields);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMVTLayerBase::TestCapability(const char* pszCap)
{
    if( EQUAL(pszCap, OLCStringsAsUTF8) ||
        EQUAL(pszCap, OLCFastSpatialFilter))
    {
        return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                           OGRMVTLayer()                              */
/************************************************************************/

OGRMVTLayer::OGRMVTLayer(OGRMVTDataset* poDS,
                         const char* pszLayerName,
                         const GByte* pabyData,
                         int nLayerSize,
                         const CPLJSONObject& oFields,
                         OGRwkbGeometryType eGeomType):
    m_poDS(poDS),
    m_pabyDataStart(pabyData),
    m_pabyDataEnd(pabyData + nLayerSize)
{
    m_poFeatureDefn = new OGRFeatureDefn(pszLayerName);
    SetDescription(m_poFeatureDefn->GetName());
    m_poFeatureDefn->SetGeomType(eGeomType);
    m_poFeatureDefn->Reference();

    if( m_poDS->m_bGeoreferenced )
    {
        m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(m_poDS->GetSRS());
    }

    Init(oFields);

    GetXY( 0, 0, m_dfTileMinX, m_dfTileMaxY );
    GetXY( m_nExtent, m_nExtent, m_dfTileMaxX, m_dfTileMinY );
    OGRLinearRing* poLR = new OGRLinearRing();
    poLR->addPoint(m_dfTileMinX, m_dfTileMinY);
    poLR->addPoint(m_dfTileMinX, m_dfTileMaxY);
    poLR->addPoint(m_dfTileMaxX, m_dfTileMaxY);
    poLR->addPoint(m_dfTileMaxX, m_dfTileMinY);
    poLR->addPoint(m_dfTileMinX, m_dfTileMinY);
    m_oClipPoly.addRingDirectly(poLR);
}

/************************************************************************/
/*                          ~OGRMVTLayer()                              */
/************************************************************************/

OGRMVTLayer::~OGRMVTLayer()
{
    for( auto& sValue: m_asValues )
    {
        if( sValue.eType == OFTString )
        {
            CPLFree(sValue.sValue.String);
        }
    }
}

/************************************************************************/
/*                               Init()                                 */
/************************************************************************/

void OGRMVTLayer::Init(const CPLJSONObject& oFields)
{
    // First pass to collect keys and values
    const GByte* pabyData = m_pabyDataStart;
    const GByte* pabyDataLimit = m_pabyDataEnd;
    unsigned int nKey = 0;
    bool bGeomTypeSet = false;
    const bool bScanFields = !oFields.IsValid();
    const bool bScanGeometries = m_poFeatureDefn->GetGeomType() == wkbUnknown;
    const bool bQuickScanFeature = bScanFields || bScanGeometries;

    try
    {
        while( pabyData < pabyDataLimit )
        {
            READ_FIELD_KEY(nKey);
            if( nKey == MAKE_KEY(knLAYER_KEYS, WT_DATA) )
            {
                char* pszKey = nullptr;
                READ_TEXT(pabyData, pabyDataLimit, pszKey);
                m_aosKeys.push_back(pszKey);
                CPLFree(pszKey);
            }
            else if( nKey == MAKE_KEY(knLAYER_VALUES, WT_DATA) )
            {
                unsigned int nValueLength = 0;
                READ_SIZE(pabyData, pabyDataLimit, nValueLength);
                const GByte* pabyDataValueEnd = pabyData + nValueLength;
                READ_VARUINT32(pabyData, pabyDataLimit, nKey);
                if( nKey == MAKE_KEY(knVALUE_STRING, WT_DATA) )
                {
                    char* pszValue = nullptr;
                    READ_TEXT(pabyData, pabyDataLimit, pszValue);
                    Value sValue;
                    sValue.eType = OFTString;
                    sValue.eSubType = OFSTNone;
                    sValue.sValue.String = pszValue;
                    m_asValues.push_back(sValue);
                }
                else if( nKey == MAKE_KEY(knVALUE_FLOAT, WT_32BIT) )
                {
                    Value sValue;
                    sValue.eType = OFTReal;
                    sValue.eSubType = OFSTFloat32;
                    sValue.sValue.Real = ReadFloat32(&pabyData, pabyDataLimit);
                    m_asValues.push_back(sValue);
                }
                else if( nKey == MAKE_KEY(knVALUE_DOUBLE, WT_64BIT) )
                {
                    Value sValue;
                    sValue.eType = OFTReal;
                    sValue.eSubType = OFSTNone;
                    sValue.sValue.Real = ReadFloat64(&pabyData, pabyDataLimit);
                    m_asValues.push_back(sValue);
                }
                else if( nKey == MAKE_KEY(knVALUE_INT, WT_VARINT) )
                {
                    GIntBig nVal = 0;
                    READ_VARINT64(pabyData, pabyDataLimit, nVal);
                    Value sValue;
                    sValue.eType = (nVal >= INT_MIN && nVal <= INT_MAX) ?
                                                    OFTInteger : OFTInteger64;
                    sValue.eSubType = OFSTNone;
                    if( sValue.eType == OFTInteger )
                        sValue.sValue.Integer = static_cast<int>(nVal);
                    else
                        sValue.sValue.Integer64 = nVal;
                    m_asValues.push_back(sValue);
                }
                else if( nKey == MAKE_KEY(knVALUE_UINT, WT_VARINT) )
                {
                    GUIntBig nVal = 0;
                    READ_VARUINT64(pabyData, pabyDataLimit, nVal);
                    Value sValue;
                    sValue.eType = (nVal <= INT_MAX) ? OFTInteger : OFTInteger64;
                    sValue.eSubType = OFSTNone;
                    if( sValue.eType == OFTInteger )
                        sValue.sValue.Integer = static_cast<int>(nVal);
                    else
                        sValue.sValue.Integer64 = static_cast<GIntBig>(nVal);
                    m_asValues.push_back(sValue);
                }
                else if( nKey == MAKE_KEY(knVALUE_SINT, WT_VARINT) )
                {
                    GIntBig nVal = 0;
                    READ_VARSINT64(pabyData, pabyDataLimit, nVal);
                    Value sValue;
                    sValue.eType = (nVal >= INT_MIN && nVal <= INT_MAX) ?
                                                        OFTInteger : OFTInteger64;
                    sValue.eSubType = OFSTNone;
                    if( sValue.eType == OFTInteger )
                        sValue.sValue.Integer = static_cast<int>(nVal);
                    else
                        sValue.sValue.Integer64 = nVal;
                    m_asValues.push_back(sValue);
                }
                else if( nKey == MAKE_KEY(knVALUE_BOOL, WT_VARINT) )
                {
                    unsigned nVal = 0;
                    READ_VARUINT32(pabyData, pabyDataLimit, nVal);
                    Value sValue;
                    sValue.eType = OFTInteger;
                    sValue.eSubType = OFSTBoolean;
                    sValue.sValue.Integer = static_cast<int>(nVal);
                    m_asValues.push_back(sValue);
                }

                pabyData = pabyDataValueEnd;
            }
            else if( nKey == MAKE_KEY(knLAYER_EXTENT, WT_VARINT) )
            {
                GUInt32 nExtent = 0;
                READ_VARUINT32(pabyData, pabyDataLimit, nExtent);
                m_nExtent = std::max(1U, nExtent); // to avoid divide by zero
            }
            else
            {
                SKIP_UNKNOWN_FIELD(pabyData, pabyDataLimit, FALSE);
            }
        }

        InitFields(oFields);

        m_nFeatureCount = 0;
        pabyData = m_pabyDataStart;
        // Second pass to iterate over features to figure out the geometry type
        // and attribute schema
        while( pabyData < pabyDataLimit )
        {
            const GByte* pabyDataBefore = pabyData;
            READ_FIELD_KEY(nKey);
            if( nKey == MAKE_KEY(knLAYER_FEATURES, WT_DATA) )
            {
                if( m_pabyDataFeatureStart == nullptr )
                {
                    m_pabyDataFeatureStart = pabyDataBefore;
                    m_pabyDataCur = pabyDataBefore;
                }

                unsigned int nFeatureLength = 0;
                READ_SIZE(pabyData, pabyDataLimit, nFeatureLength);
                const GByte* pabyDataFeatureEnd = pabyData + nFeatureLength;
                if( bQuickScanFeature )
                {
                    if( !QuickScanFeature(pabyData, pabyDataFeatureEnd,
                                        bScanFields,
                                        bScanGeometries,
                                        bGeomTypeSet) )
                    {
                        return;
                    }
                }
                pabyData = pabyDataFeatureEnd;

                m_nFeatureCount ++;
            }
            else
            {
                SKIP_UNKNOWN_FIELD(pabyData, pabyDataLimit, FALSE);
            }
        }
    }
    catch( const GPBException& e )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
    }
}

/************************************************************************/
/*                          MergeFieldDefn()                            */
/************************************************************************/

static void MergeFieldDefn(OGRFieldDefn* poFieldDefn,
                           OGRFieldType eSrcType,
                           OGRFieldSubType eSrcSubType)
{
    if( eSrcType == OFTString )
    {
        poFieldDefn->SetSubType(OFSTNone);
        poFieldDefn->SetType(OFTString);
    }
    else if( poFieldDefn->GetType() == OFTInteger &&
                eSrcType == OFTInteger64 )
    {
        poFieldDefn->SetSubType(OFSTNone);
        poFieldDefn->SetType(OFTInteger64);
    }
    else if(
        (poFieldDefn->GetType() == OFTInteger ||
            poFieldDefn->GetType() == OFTInteger64 ) &&
        eSrcType == OFTReal )
    {
        poFieldDefn->SetSubType(OFSTNone);
        poFieldDefn->SetType(OFTReal);
        poFieldDefn->SetSubType(eSrcSubType);
    }
    else if( poFieldDefn->GetType() == OFTReal &&
                eSrcType == OFTReal &&
                eSrcSubType == OFSTNone )
    {
        poFieldDefn->SetSubType(OFSTNone);
    }
    else if( poFieldDefn->GetType() == OFTInteger &&
                eSrcType == OFTInteger &&
                eSrcSubType == OFSTNone )
    {
        poFieldDefn->SetSubType(OFSTNone);
    }
}

/************************************************************************/
/*                         QuickScanFeature()                           */
/************************************************************************/

bool OGRMVTLayer::QuickScanFeature(const GByte* pabyData,
                                   const GByte* pabyDataFeatureEnd,
                                   bool bScanFields,
                                   bool bScanGeometries,
                                   bool& bGeomTypeSet)
{
    unsigned int nKey = 0;
    unsigned int nGeomType = 0;
    try
    {
        while( pabyData < pabyDataFeatureEnd )
        {
            READ_VARUINT32(pabyData, pabyDataFeatureEnd, nKey);
            if( nKey == MAKE_KEY(knFEATURE_TYPE, WT_VARINT) )
            {
                READ_VARUINT32(pabyData, pabyDataFeatureEnd, nGeomType);
            }
            else if( nKey == MAKE_KEY(knFEATURE_TAGS, WT_DATA) &&
                    bScanFields )
            {
                unsigned int nTagsSize = 0;
                READ_SIZE(pabyData, pabyDataFeatureEnd, nTagsSize);
                const GByte* pabyDataTagsEnd = pabyData + nTagsSize;
                while( pabyData < pabyDataTagsEnd )
                {
                    unsigned int nKeyIdx = 0;
                    unsigned int nValIdx = 0;
                    READ_VARUINT32(pabyData, pabyDataTagsEnd, nKeyIdx);
                    READ_VARUINT32(pabyData, pabyDataTagsEnd, nValIdx);
                    if( nKeyIdx >= m_aosKeys.size() )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                    "Invalid tag key index: %u", nKeyIdx);
                        m_bError = true;
                        return false;
                    }
                    if( nValIdx >= m_asValues.size() )
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                    "Invalid tag value index: %u", nValIdx);
                        m_bError = true;
                        return false;
                    }
                    const int nFieldIdx =
                        m_poFeatureDefn->GetFieldIndex(m_aosKeys[nKeyIdx]);
                    if( nFieldIdx < 0 )
                    {
                        OGRFieldDefn oFieldDefn(m_aosKeys[nKeyIdx],
                                                m_asValues[nValIdx].eType);
                        oFieldDefn.SetSubType(m_asValues[nValIdx].eSubType);
                        m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
                    }
                    else if( m_poFeatureDefn->GetFieldDefn(nFieldIdx)->
                                GetType() != m_asValues[nValIdx].eType ||
                                m_poFeatureDefn->GetFieldDefn(nFieldIdx)->
                                GetSubType() != m_asValues[nValIdx].eSubType )
                    {
                        OGRFieldDefn* poFieldDefn =
                            m_poFeatureDefn->GetFieldDefn(nFieldIdx);
                        OGRFieldType eSrcType(m_asValues[nValIdx].eType);
                        OGRFieldSubType eSrcSubType(m_asValues[nValIdx].eSubType);
                        MergeFieldDefn(poFieldDefn, eSrcType, eSrcSubType);
                    }
                }
            }
            else if( nKey == MAKE_KEY(knFEATURE_GEOMETRY, WT_DATA) &&
                    bScanGeometries &&
                    nGeomType >= knGEOM_TYPE_POINT && nGeomType <= knGEOM_TYPE_POLYGON )
            {
                unsigned int nGeometrySize = 0;
                READ_SIZE(pabyData, pabyDataFeatureEnd, nGeometrySize);
                const GByte* pabyDataGeometryEnd = pabyData + nGeometrySize;
                OGRwkbGeometryType eType = wkbUnknown;

                if( nGeomType == knGEOM_TYPE_POINT )
                {
                    eType = wkbPoint;
                    unsigned int nCmdCountCombined = 0;
                    READ_VARUINT32(pabyData, pabyDataGeometryEnd,
                                    nCmdCountCombined);
                    if( GetCmdId(nCmdCountCombined) == knCMD_MOVETO &&
                        GetCmdCount(nCmdCountCombined) > 1 )
                    {
                        eType = wkbMultiPoint;
                    }
                }
                else if( nGeomType == knGEOM_TYPE_LINESTRING )
                {
                    eType = wkbLineString;
                    for( int iIter = 0;
                            pabyData < pabyDataGeometryEnd; iIter++ )
                    {
                        if( iIter == 1 )
                        {
                            eType = wkbMultiLineString;
                            break;
                        }
                        unsigned int nCmdCountCombined = 0;
                        unsigned int nLineToCount;
                        // Should be a moveto
                        SKIP_VARINT(pabyData, pabyDataGeometryEnd);
                        SKIP_VARINT(pabyData, pabyDataGeometryEnd);
                        SKIP_VARINT(pabyData, pabyDataGeometryEnd);
                        READ_VARUINT32(pabyData, pabyDataGeometryEnd,
                                    nCmdCountCombined);
                        nLineToCount = GetCmdCount(nCmdCountCombined);
                        for( unsigned i = 0; i < 2 * nLineToCount; i++ )
                        {
                            SKIP_VARINT(pabyData, pabyDataGeometryEnd);
                        }
                    }
                }
                else /* if( nGeomType == knGEOM_TYPE_POLYGON ) */
                {
                    eType = wkbPolygon;
                    for( int iIter = 0;
                            pabyData < pabyDataGeometryEnd; iIter++ )
                    {
                        if( iIter == 1 )
                        {
                            eType = wkbMultiPolygon;
                            break;
                        }
                        unsigned int nCmdCountCombined = 0;
                        unsigned int nLineToCount;
                        // Should be a moveto
                        SKIP_VARINT(pabyData, pabyDataGeometryEnd);
                        SKIP_VARINT(pabyData, pabyDataGeometryEnd);
                        SKIP_VARINT(pabyData, pabyDataGeometryEnd);
                        READ_VARUINT32(pabyData, pabyDataGeometryEnd,
                                    nCmdCountCombined);
                        nLineToCount = GetCmdCount(nCmdCountCombined);
                        for( unsigned i = 0; i < 2 * nLineToCount; i++ )
                        {
                            SKIP_VARINT(pabyData, pabyDataGeometryEnd);
                        }
                        // Should be a closepath
                        SKIP_VARINT(pabyData, pabyDataGeometryEnd);
                    }
                }

                if( bGeomTypeSet &&
                    m_poFeatureDefn->GetGeomType() ==
                                            OGR_GT_GetCollection(eType) )
                {
                    // do nothing
                }
                else if( bGeomTypeSet &&
                            eType == OGR_GT_GetCollection(
                                        m_poFeatureDefn->GetGeomType()) )
                {
                    m_poFeatureDefn->SetGeomType(eType);
                }
                else if( bGeomTypeSet &&
                            m_poFeatureDefn->GetGeomType() != eType )
                {
                    m_poFeatureDefn->SetGeomType(wkbUnknown);
                }
                else
                {
                    m_poFeatureDefn->SetGeomType(eType);
                }
                bGeomTypeSet = true;

                pabyData = pabyDataGeometryEnd;
            }
            else
            {
                SKIP_UNKNOWN_FIELD(pabyData, pabyDataFeatureEnd, FALSE);
            }
        }
        return true;
    }
    catch( const GPBException& e )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        return false;
    }
}

/************************************************************************/
/*                         GetFeatureCount()                            */
/************************************************************************/

GIntBig OGRMVTLayer::GetFeatureCount( int bForce )
{
    if( m_poFilterGeom == nullptr && m_poAttrQuery == nullptr &&
        m_nFeatureCount >= 0 )
    {
        return m_nFeatureCount;
    }
    return OGRLayer::GetFeatureCount( bForce );
}

/************************************************************************/
/*                          ResetReading()                              */
/************************************************************************/

void OGRMVTLayer::ResetReading()
{
    m_nFID = 0;
    m_pabyDataCur = m_pabyDataFeatureStart;
}

/************************************************************************/
/*                              GetXY()                                 */
/************************************************************************/

void OGRMVTLayer::GetXY(int nX, int nY, double& dfX, double& dfY)
{
    if( m_poDS->m_bGeoreferenced )
    {
        dfX = m_poDS->m_dfTopX + nX * m_poDS->m_dfTileDimX / m_nExtent;
        dfY = m_poDS->m_dfTopY - nY * m_poDS->m_dfTileDimY / m_nExtent;
    }
    else
    {
        dfX = nX;
        dfY = static_cast<double>(m_nExtent) - nY;
    }
}


/************************************************************************/
/*                     AddWithOverflowAccepted()                        */
/************************************************************************/

CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW
static int AddWithOverflowAccepted(int a, int b)
{
    // In fact in normal situations a+b should not overflow. That can only
    // happen with corrupted datasets. But we don't really want to add code
    // to detect that situation, so basically this is just a trick to perform
    // the addition without the various sanitizers to yell about the overflow.
    //
    // Assumes complement-to-two signed integer representation and that
    // the compiler will safely cast a big unsigned to negative integer.
    return static_cast<int>(
                        static_cast<unsigned>(a) + static_cast<unsigned>(b));
}

/************************************************************************/
/*                           ParseGeometry()                            */
/************************************************************************/

OGRGeometry* OGRMVTLayer::ParseGeometry(unsigned int nGeomType,
                                        const GByte* pabyDataGeometryEnd)
{
    OGRMultiPoint* poMultiPoint = nullptr;
    OGRMultiLineString* poMultiLS = nullptr;
    OGRLineString* poLine = nullptr;
    OGRMultiPolygon* poMultiPoly = nullptr;
    OGRPolygon* poPoly = nullptr;
    OGRLinearRing* poRing = nullptr;

    try
    {
        if( nGeomType == knGEOM_TYPE_POINT )
        {
            unsigned int nCmdCountCombined = 0;
            unsigned int nCount;
            READ_VARUINT32(m_pabyDataCur, pabyDataGeometryEnd,
                            nCmdCountCombined);
            nCount = GetCmdCount(nCmdCountCombined);
            if( GetCmdId(nCmdCountCombined) == knCMD_MOVETO &&
                nCount == 1 )
            {
                int nX = 0;
                int nY = 0;
                READ_VARSINT32(m_pabyDataCur, pabyDataGeometryEnd,
                                nX);
                READ_VARSINT32(m_pabyDataCur, pabyDataGeometryEnd,
                                nY);
                double dfX;
                double dfY;
                GetXY(nX, nY, dfX, dfY);
                OGRPoint* poPoint = new OGRPoint(dfX, dfY);
                if( m_poFeatureDefn->GetGeomType() == wkbMultiPoint )
                {
                    poMultiPoint = new OGRMultiPoint();
                    poMultiPoint->addGeometryDirectly(poPoint);
                    return poMultiPoint;
                }
                else
                {
                    return poPoint;
                }
            }
            else if( GetCmdId(nCmdCountCombined) == knCMD_MOVETO &&
                        nCount > 1 )
            {
                int nX = 0;
                int nY = 0;
                poMultiPoint = new OGRMultiPoint();
                for( unsigned i = 0; i < nCount; i++ )
                {
                    int nDX = 0;
                    int nDY = 0;
                    READ_VARSINT32(m_pabyDataCur,
                                    pabyDataGeometryEnd, nDX);
                    READ_VARSINT32(m_pabyDataCur,
                                    pabyDataGeometryEnd, nDY);
                    //if( nDX != 0 || nDY != 0 )
                    {
                        nX = AddWithOverflowAccepted(nX, nDX);
                        nY = AddWithOverflowAccepted(nY, nDY);
                        double dfX;
                        double dfY;
                        GetXY(nX, nY, dfX, dfY);
                        OGRPoint* poPoint = new OGRPoint(dfX, dfY);
                        if( i == 0 && nCount == 2 &&
                            m_pabyDataCur == pabyDataGeometryEnd )
                        {
                            // Current versions of Mapserver at time of writing
                            // wrongly encode a point with nCount = 2
                            static bool bWarned = false;
                            if( !bWarned )
                            {
                                CPLDebug("MVT",
                                         "Reading likely a broken point as "
                                         "produced by some versions of Mapserver");
                                bWarned = true;
                            }
                            delete poMultiPoint;
                            return poPoint;
                        }
                        poMultiPoint->addGeometryDirectly(poPoint);
                    }
                }
                return poMultiPoint;
            }
        }
        else if( nGeomType == knGEOM_TYPE_LINESTRING )
        {
            int nX = 0;
            int nY = 0;
            while( m_pabyDataCur < pabyDataGeometryEnd )
            {
                unsigned int nCmdCountCombined = 0;
                unsigned int nLineToCount;
                // Should be a moveto
                SKIP_VARINT(m_pabyDataCur, pabyDataGeometryEnd);
                int nDX = 0;
                int nDY = 0;
                READ_VARSINT32(m_pabyDataCur,
                                pabyDataGeometryEnd, nDX);
                READ_VARSINT32(m_pabyDataCur,
                                pabyDataGeometryEnd, nDY);
                nX = AddWithOverflowAccepted(nX, nDX);
                nY = AddWithOverflowAccepted(nY, nDY);
                double dfX;
                double dfY;
                GetXY(nX, nY, dfX, dfY);
                if( poLine != nullptr )
                {
                    if( poMultiLS == nullptr )
                    {
                        poMultiLS = new OGRMultiLineString();
                        poMultiLS->addGeometryDirectly(poLine);
                    }
                    poLine = new OGRLineString();
                    poMultiLS->addGeometryDirectly(poLine);
                }
                else
                {
                    poLine = new OGRLineString();
                }
                poLine->addPoint(dfX, dfY);
                READ_VARUINT32(m_pabyDataCur, pabyDataGeometryEnd,
                            nCmdCountCombined);
                nLineToCount = GetCmdCount(nCmdCountCombined);
                for( unsigned i = 0; i < nLineToCount; i++ )
                {
                    READ_VARSINT32(m_pabyDataCur,
                                    pabyDataGeometryEnd, nDX);
                    READ_VARSINT32(m_pabyDataCur,
                                    pabyDataGeometryEnd, nDY);
                    //if( nDX != 0 || nDY != 0 )
                    {
                        nX = AddWithOverflowAccepted(nX, nDX);
                        nY = AddWithOverflowAccepted(nY, nDY);
                        GetXY(nX, nY, dfX, dfY);
                        poLine->addPoint(dfX, dfY);
                    }
                }
            }
            if( poMultiLS == nullptr && poLine != nullptr &&
                m_poFeatureDefn->GetGeomType() == wkbMultiLineString )
            {
                poMultiLS = new OGRMultiLineString();
                poMultiLS->addGeometryDirectly(poLine);
            }
            if( poMultiLS )
            {
                return poMultiLS;
            }
            else
            {
                return poLine;
            }
        }
        else if( nGeomType == knGEOM_TYPE_POLYGON )
        {
            int externalIsClockwise = 0;
            int nX = 0;
            int nY = 0;
            while( m_pabyDataCur < pabyDataGeometryEnd )
            {
                unsigned int nCmdCountCombined = 0;
                unsigned int nLineToCount;
                // Should be a moveto
                SKIP_VARINT(m_pabyDataCur, pabyDataGeometryEnd);
                int nDX = 0;
                int nDY = 0;
                READ_VARSINT32(m_pabyDataCur,
                                pabyDataGeometryEnd, nDX);
                READ_VARSINT32(m_pabyDataCur,
                                pabyDataGeometryEnd, nDY);
                nX = AddWithOverflowAccepted(nX, nDX);
                nY = AddWithOverflowAccepted(nY, nDY);
                double dfX;
                double dfY;
                GetXY(nX, nY, dfX, dfY);
                poRing = new OGRLinearRing();
                poRing->addPoint(dfX, dfY);
                READ_VARUINT32(m_pabyDataCur, pabyDataGeometryEnd,
                            nCmdCountCombined);
                nLineToCount = GetCmdCount(nCmdCountCombined);
                for( unsigned i = 0; i < nLineToCount; i++ )
                {
                    READ_VARSINT32(m_pabyDataCur,
                                    pabyDataGeometryEnd, nDX);
                    READ_VARSINT32(m_pabyDataCur,
                                    pabyDataGeometryEnd, nDY);
                    //if( nDX != 0 || nDY != 0 )
                    {
                        nX = AddWithOverflowAccepted(nX, nDX);
                        nY = AddWithOverflowAccepted(nY, nDY);
                        GetXY(nX, nY, dfX, dfY);
                        poRing->addPoint(dfX, dfY);
                    }
                }
                // Should be a closepath
                SKIP_VARINT(m_pabyDataCur, pabyDataGeometryEnd);
                poRing->closeRings();
                if( poPoly == nullptr )
                {
                    poPoly = new OGRPolygon();
                    poPoly->addRingDirectly(poRing);
                    externalIsClockwise = poRing->isClockwise();
                }
                else
                {
                    // Detect change of winding order to figure out if this is
                    // an interior or exterior ring
                    if( externalIsClockwise != poRing->isClockwise() )
                    {
                        poPoly->addRingDirectly(poRing);
                    }
                    else
                    {
                        if( poMultiPoly == nullptr )
                        {
                            poMultiPoly = new OGRMultiPolygon();
                            poMultiPoly->addGeometryDirectly(poPoly);
                        }

                        poPoly = new OGRPolygon();
                        poMultiPoly->addGeometryDirectly(poPoly);
                        poPoly->addRingDirectly(poRing);
                    }
                }
                poRing = nullptr;
            }
            if( poMultiPoly == nullptr && poPoly != nullptr &&
                m_poFeatureDefn->GetGeomType() == wkbMultiPolygon )
            {
                poMultiPoly = new OGRMultiPolygon();
                poMultiPoly->addGeometryDirectly(poPoly);
            }
            if( poMultiPoly )
            {
                return poMultiPoly;
            }
            else
            {
                return poPoly;
            }
        }
    }
    catch( const GPBException& e )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        delete poMultiPoint;
        if( poMultiPoly )
            delete poMultiPoly;
        else if( poPoly )
            delete poPoly;
        if( poMultiLS )
            delete poMultiLS;
        else if( poLine )
            delete poLine;
        delete poRing;
    }
    return nullptr;
}

/************************************************************************/
/*                      SanitizeClippedGeometry()                       */
/************************************************************************/

void OGRMVTLayer::SanitizeClippedGeometry(OGRGeometry*& poGeom)
{
    OGRwkbGeometryType eInGeomType = wkbFlatten(poGeom->getGeometryType());
    const OGRwkbGeometryType eLayerGeomType = GetGeomType();
    if( eLayerGeomType == wkbUnknown )
    {
        return;
    }

    // GEOS intersection may return a mix of polygon and linestrings when
    // intersection a multipolygon and a polygon
    if( eInGeomType == wkbGeometryCollection )
    {
        OGRGeometryCollection* poGC = poGeom->toGeometryCollection();
        OGRGeometry* poTargetSingleGeom = nullptr;
        OGRGeometryCollection* poTargetGC = nullptr;
        OGRwkbGeometryType ePartGeom;
        if( eLayerGeomType == wkbPoint || eLayerGeomType == wkbMultiPoint )
        {
            ePartGeom = wkbPoint;
        }
        else if( eLayerGeomType == wkbLineString ||
                 eLayerGeomType == wkbMultiLineString )
        {
            ePartGeom = wkbLineString;
        }
        else
        {
            ePartGeom = wkbPolygon;
        }
        for( auto&& poSubGeom: poGC )
        {
            if( wkbFlatten(poSubGeom->getGeometryType()) == ePartGeom )
            {
                if( poTargetSingleGeom != nullptr )
                {
                    if( poTargetGC == nullptr )
                    {
                        poTargetGC = OGRGeometryFactory::createGeometry(
                                OGR_GT_GetCollection(ePartGeom))->toGeometryCollection();
                        poGeom = poTargetGC;
                        poTargetGC->addGeometryDirectly(poTargetSingleGeom);
                    }

                    poTargetGC->addGeometry(poSubGeom);
                }
                else
                {
                    poTargetSingleGeom = poSubGeom->clone();
                    poGeom = poTargetSingleGeom;
                }
            }
        }
        if( poGeom != poGC )
        {
            delete poGC;
        }
        eInGeomType = wkbFlatten(poGeom->getGeometryType());
    }

    // Wrap single into multi if requested by the layer geometry type
    if( OGR_GT_GetCollection(eInGeomType) == eLayerGeomType )
    {
        OGRGeometryCollection* poGC =
            OGRGeometryFactory::createGeometry(eLayerGeomType)->
                toGeometryCollection();
        poGC->addGeometryDirectly(poGeom);
        poGeom = poGC;
        return;
    }

}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature* OGRMVTLayer::GetNextRawFeature()
{
    if( m_pabyDataCur == nullptr ||
        m_pabyDataCur >= m_pabyDataEnd ||
        m_bError )
    {
        return nullptr;
    }

    unsigned int nKey = 0;
    const GByte* pabyDataLimit = m_pabyDataEnd;
    OGRFeature* poFeature = nullptr;
    unsigned int nFeatureLength = 0;
    unsigned int nGeomType = 0;

    try
    {
        while(true )
        {
            bool bOK = true;

            while( m_pabyDataCur < pabyDataLimit )
            {
                READ_VARUINT32(m_pabyDataCur, pabyDataLimit, nKey);
                if( nKey == MAKE_KEY(knLAYER_FEATURES, WT_DATA) )
                {
                    poFeature = new OGRFeature(m_poFeatureDefn);
                    break;
                }
                else
                {
                    SKIP_UNKNOWN_FIELD(m_pabyDataCur, pabyDataLimit, FALSE);
                }
            }

            if( poFeature == nullptr )
                return nullptr;

            READ_SIZE(m_pabyDataCur, pabyDataLimit, nFeatureLength);
            const GByte* pabyDataFeatureEnd = m_pabyDataCur + nFeatureLength;
            while( m_pabyDataCur < pabyDataFeatureEnd )
            {
                READ_VARUINT32(m_pabyDataCur, pabyDataFeatureEnd, nKey);
                if( nKey == MAKE_KEY(knFEATURE_ID, WT_VARINT) )
                {
                    GUIntBig nID = 0;
                    READ_VARUINT64(m_pabyDataCur, pabyDataFeatureEnd, nID);
                    poFeature->SetField("mvt_id", static_cast<GIntBig>(nID));
                }
                else if( nKey == MAKE_KEY(knFEATURE_TYPE, WT_VARINT) )
                {
                    READ_VARUINT32(m_pabyDataCur, pabyDataFeatureEnd,
                                    nGeomType);
                }
                else if( nKey == MAKE_KEY(knFEATURE_TAGS, WT_DATA) )
                {
                    unsigned int nTagsSize = 0;
                    READ_SIZE(m_pabyDataCur, pabyDataFeatureEnd, nTagsSize);
                    const GByte* pabyDataTagsEnd = m_pabyDataCur + nTagsSize;
                    while( m_pabyDataCur < pabyDataTagsEnd )
                    {
                        unsigned int nKeyIdx = 0;
                        unsigned int nValIdx = 0;
                        READ_VARUINT32(m_pabyDataCur, pabyDataTagsEnd, nKeyIdx);
                        READ_VARUINT32(m_pabyDataCur, pabyDataTagsEnd, nValIdx);
                        if( nKeyIdx < m_aosKeys.size() &&
                            nValIdx < m_asValues.size() )
                        {
                            const int nFieldIdx =
                                m_poFeatureDefn->GetFieldIndex(
                                    m_aosKeys[nKeyIdx]);
                            if( nFieldIdx >= 0 )
                            {
                                if( m_asValues[nValIdx].eType == OFTString )
                                {
                                    poFeature->SetField(nFieldIdx,
                                            m_asValues[nValIdx].sValue.String);
                                }
                                else if( m_asValues[nValIdx].eType ==
                                                                OFTInteger )
                                {
                                    poFeature->SetField(nFieldIdx,
                                            m_asValues[nValIdx].sValue.Integer);
                                }
                                else if( m_asValues[nValIdx].eType ==
                                                                OFTInteger64 )
                                {
                                    poFeature->SetField(nFieldIdx,
                                        m_asValues[nValIdx].sValue.Integer64);
                                }
                                else if( m_asValues[nValIdx].eType == OFTReal )
                                {
                                    poFeature->SetField(nFieldIdx,
                                            m_asValues[nValIdx].sValue.Real);
                                }
                            }
                        }
                    }
                }
                else if( nKey == MAKE_KEY(knFEATURE_GEOMETRY, WT_DATA) &&
                            nGeomType >= 1 && nGeomType <= 3 )
                {
                    unsigned int nGeometrySize = 0;
                    READ_SIZE(m_pabyDataCur, pabyDataFeatureEnd, nGeometrySize);
                    const GByte* pabyDataGeometryEnd = m_pabyDataCur + nGeometrySize;
                    OGRGeometry* poGeom =
                        ParseGeometry(nGeomType, pabyDataGeometryEnd);
                    if( poGeom )
                    {
                        // Clip geometry to tile extent if requested
                        if( m_poDS->m_bClip && OGRGeometryFactory::haveGEOS() )
                        {
                            OGREnvelope sEnvelope;
                            poGeom->getEnvelope(&sEnvelope);
                            if( sEnvelope.MinX >= m_dfTileMinX &&
                                sEnvelope.MinY >= m_dfTileMinY &&
                                sEnvelope.MaxX <= m_dfTileMaxX &&
                                sEnvelope.MaxY <= m_dfTileMaxY )
                            {
                                // do nothing
                            }
                            else if( sEnvelope.MinX < m_dfTileMaxX &&
                                    sEnvelope.MinY < m_dfTileMaxY &&
                                    sEnvelope.MaxX > m_dfTileMinX &&
                                    sEnvelope.MaxY > m_dfTileMinY )
                            {
                                OGRGeometry* poClipped =
                                    poGeom->Intersection(&m_oClipPoly);
                                if( poClipped )
                                {
                                    SanitizeClippedGeometry(poClipped);
                                    if( poClipped->IsEmpty() )
                                    {
                                        delete poClipped;
                                        bOK = false;
                                    }
                                    else
                                    {
                                        poClipped->assignSpatialReference(
                                            GetSpatialRef());
                                        poFeature->SetGeometryDirectly(poClipped);
                                        delete poGeom;
                                        poGeom = nullptr;
                                    }
                                }
                            }
                            else
                            {
                                bOK = false;
                            }
                        }

                        if( poGeom )
                        {
                            poGeom->assignSpatialReference(GetSpatialRef());
                            poFeature->SetGeometryDirectly(poGeom);
                        }
                    }

                    m_pabyDataCur = pabyDataGeometryEnd;
                }
                else
                {
                    SKIP_UNKNOWN_FIELD(m_pabyDataCur, pabyDataFeatureEnd, FALSE);
                }
            }
            m_pabyDataCur = pabyDataFeatureEnd;

            if( bOK )
            {
                poFeature->SetFID(m_nFID);
                m_nFID ++;
                return poFeature;
            }
            else
            {
                delete poFeature;
                poFeature = nullptr;
            }
        }
    }
    catch( const GPBException& e )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        delete poFeature;
        return nullptr;
    }
}

/************************************************************************/
/*                         StripDummyEntries()                           */
/************************************************************************/

static CPLStringList StripDummyEntries(const CPLStringList& aosInput)
{
    CPLStringList aosOutput;
    for( int i = 0; i < aosInput.Count(); i++ )
    {
        if( aosInput[i] != CPLString(".") &&
            aosInput[i] != CPLString("..") &&
            CPLString(aosInput[i]).find(".properties") == std::string::npos )
        {
            aosOutput.AddString( aosInput[i] );
        }
    }
    return aosOutput.Sort();
}

/************************************************************************/
/*                       OGRMVTDirectoryLayer()                         */
/************************************************************************/

OGRMVTDirectoryLayer::OGRMVTDirectoryLayer(
                                    OGRMVTDataset* poDS,
                                    const char* pszLayerName,
                                    const char* pszDirectoryName,
                                    const CPLJSONObject& oFields,
                                    bool bJsonField,
                                    OGRwkbGeometryType eGeomType,
                                    const OGREnvelope* psExtent):
    m_poDS(poDS),
    m_osDirName(pszDirectoryName),
    m_bJsonField(bJsonField)
{
    m_poFeatureDefn = new OGRFeatureDefn(pszLayerName);
    SetDescription(m_poFeatureDefn->GetName());
    m_poFeatureDefn->SetGeomType(eGeomType);
    m_poFeatureDefn->Reference();

    m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poDS->GetSRS());

    if( m_bJsonField )
    {
        OGRFieldDefn oFieldDefnId("mvt_id", OFTInteger64);
        m_poFeatureDefn->AddFieldDefn(&oFieldDefnId);
    }
    else
    {
        InitFields(oFields);
    }

    m_nZ = atoi(CPLGetFilename(m_osDirName));
    SetMetadataItem("ZOOM_LEVEL", CPLSPrintf("%d", m_nZ));
    m_bUseReadDir = CPLTestBool(
        CPLGetConfigOption("MVT_USE_READDIR",
                    (!STARTS_WITH(m_osDirName, "/vsicurl") &&
                     !STARTS_WITH(m_osDirName, "http://") &&
                     !STARTS_WITH(m_osDirName, "https://")) ? "YES": "NO"));
    if( m_bUseReadDir )
    {
        m_aosDirContent = VSIReadDirEx(m_osDirName, knMAX_FILES_PER_DIR);
        if( m_aosDirContent.Count() >= knMAX_FILES_PER_DIR )
        {
            CPLDebug("MVT", "Disabling readdir");
            m_aosDirContent.Clear();
            m_bUseReadDir = false;
        }
        m_aosDirContent = StripDummyEntries(m_aosDirContent);
    }
    OGRMVTDirectoryLayer::ResetReading();

    if( psExtent )
    {
        m_sExtent = *psExtent;
    }

    OGRMVTDirectoryLayer::SetSpatialFilter(nullptr);

    // If the metadata contains an empty fields object, this may be a sign
    // that it doesn't know the schema. In that case check if a tile has
    // attributes, and in that case create a json field.
    if( !m_bJsonField && oFields.IsValid() && oFields.GetChildren().empty() )
    {
        m_bJsonField = true;
        OpenTileIfNeeded();
        m_bJsonField = false;

        if( m_poCurrentTile )
        {
            OGRLayer* poUnderlyingLayer =
                m_poCurrentTile->GetLayerByName(GetName());
            // There is at least the mvt_id field
            if( poUnderlyingLayer->GetLayerDefn()->GetFieldCount() > 1 )
            {
                m_bJsonField = true;
            }
        }
        OGRMVTDirectoryLayer::ResetReading();
    }

    if( m_bJsonField )
    {
        OGRFieldDefn oFieldDefn("json", OFTString);
        m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
}

/************************************************************************/
/*                      ~OGRMVTDirectoryLayer()                         */
/************************************************************************/

OGRMVTDirectoryLayer::~OGRMVTDirectoryLayer()
{
    delete m_poCurrentTile;
}

/************************************************************************/
/*                          ResetReading()                              */
/************************************************************************/

void OGRMVTDirectoryLayer::ResetReading()
{
    m_bEOF = false;
    m_nXIndex = -1;
    m_nYIndex = -1;
    delete m_poCurrentTile;
    m_poCurrentTile = nullptr;
}

/************************************************************************/
/*                            IsBetween()                               */
/************************************************************************/

static bool IsBetween(int nVal, int nMin, int nMax)
{
    return nVal >= nMin && nVal <= nMax;
}

/************************************************************************/
/*                          ReadNewSubDir()                             */
/************************************************************************/

void OGRMVTDirectoryLayer::ReadNewSubDir()
{
    delete m_poCurrentTile;
    m_poCurrentTile = nullptr;
    if( m_bUseReadDir || !m_aosDirContent.empty() )
    {
        while( m_nXIndex < m_aosDirContent.Count() &&
                (CPLGetValueType(m_aosDirContent[m_nXIndex])
                        != CPL_VALUE_INTEGER ||
                 !IsBetween(atoi(m_aosDirContent[m_nXIndex]),
                            m_nFilterMinX, m_nFilterMaxX)) )
        {
            m_nXIndex++;
        }
    }
    else
    {
        if( m_nXIndex < m_nFilterMinX )
            m_nXIndex = m_nFilterMinX;
        else if( m_nXIndex > m_nFilterMaxX )
            m_nXIndex = (1 << m_nZ);
    }
    if( m_nXIndex < ((m_bUseReadDir || !m_aosDirContent.empty()) ?
                                m_aosDirContent.Count() : (1 << m_nZ)) )
    {
        m_aosSubDirName = CPLFormFilename(
            m_osDirName,
            (m_bUseReadDir || !m_aosDirContent.empty()) ?
                            m_aosDirContent[m_nXIndex] :
                            CPLSPrintf("%d", m_nXIndex),
            nullptr);
        if( m_bUseReadDir )
        {
            m_aosSubDirContent = VSIReadDirEx(m_aosSubDirName,
                                              knMAX_FILES_PER_DIR);
            if( m_aosSubDirContent.Count() >= knMAX_FILES_PER_DIR )
            {
                CPLDebug("MVT", "Disabling readdir");
                m_aosSubDirContent.Clear();
                m_bUseReadDir = false;
            }
            m_aosSubDirContent = StripDummyEntries(m_aosSubDirContent);
        }
        m_nYIndex = -1;
        OpenTileIfNeeded();
    }
    else
    {
        m_bEOF = true;
    }
}

/************************************************************************/
/*                            OpenTile()                                */
/************************************************************************/

void OGRMVTDirectoryLayer::OpenTile()
{
    delete m_poCurrentTile;
    m_poCurrentTile = nullptr;
    if( m_nYIndex <
            (m_bUseReadDir ? m_aosSubDirContent.Count() : (1 << m_nZ)) )
    {
        CPLString osFilename = CPLFormFilename(
            m_aosSubDirName,
            m_bUseReadDir ? m_aosSubDirContent[m_nYIndex] :
                CPLSPrintf("%d.%s",
                           m_nYIndex, m_poDS->m_osTileExtension.c_str()),
            nullptr);
        GDALOpenInfo oOpenInfo(("MVT:" + osFilename).c_str(),
                               GA_ReadOnly);
        oOpenInfo.papszOpenOptions = CSLSetNameValue(nullptr,
                "METADATA_FILE",
                m_bJsonField ? "" : m_poDS->m_osMetadataMemFilename.c_str());
        oOpenInfo.papszOpenOptions = CSLSetNameValue(
            oOpenInfo.papszOpenOptions,
            "DO_NOT_ERROR_ON_MISSING_TILE", "YES");
        m_poCurrentTile = OGRMVTDataset::Open(&oOpenInfo);
        CSLDestroy(oOpenInfo.papszOpenOptions);

        int nX = (m_bUseReadDir || !m_aosDirContent.empty()) ?
                        atoi(m_aosDirContent[m_nXIndex]) : m_nXIndex;
        int nY = m_bUseReadDir ? atoi(m_aosSubDirContent[m_nYIndex]) : m_nYIndex;
        m_nFIDBase = (static_cast<GIntBig>(nX) << m_nZ) | nY;
    }
}

/************************************************************************/
/*                         OpenTileIfNeeded()                           */
/************************************************************************/

void OGRMVTDirectoryLayer::OpenTileIfNeeded()
{
    if( m_nXIndex < 0 )
    {
        m_nXIndex = 0;
        ReadNewSubDir();
    }
    while( (m_poCurrentTile == nullptr && !m_bEOF) ||
            (m_poCurrentTile != nullptr &&
                m_poCurrentTile->GetLayerByName(GetName()) == nullptr) )
    {
        m_nYIndex ++;
        if( m_bUseReadDir )
        {
            while( m_nYIndex < m_aosSubDirContent.Count() &&
                   (CPLGetValueType(CPLGetBasename(
                       m_aosSubDirContent[m_nYIndex])) != CPL_VALUE_INTEGER ||
                    !IsBetween(atoi(m_aosSubDirContent[m_nYIndex]),
                               m_nFilterMinY, m_nFilterMaxY)) )
            {
                m_nYIndex++;
            }
        }
        else
        {
            if( m_nYIndex < m_nFilterMinY )
                m_nYIndex = m_nFilterMinY;
            else if( m_nYIndex > m_nFilterMaxY )
                m_nYIndex = (1 << m_nZ);
        }
        if( m_nYIndex ==
                (m_bUseReadDir ? m_aosSubDirContent.Count() : (1 << m_nZ)) )
        {
            m_nXIndex ++;
            ReadNewSubDir();
        }
        else
        {
            OpenTile();
        }
    }
}

/************************************************************************/
/*                         GetFeatureCount()                            */
/************************************************************************/

GIntBig OGRMVTDirectoryLayer::GetFeatureCount( int bForce )
{
    if( m_poFilterGeom == nullptr && m_poAttrQuery == nullptr )
    {
        GIntBig nFeatureCount = 0;
        ResetReading();
        while( true )
        {
            OpenTileIfNeeded();
            if( m_poCurrentTile == nullptr )
                break;
            OGRLayer* poUnderlyingLayer =
                m_poCurrentTile->GetLayerByName(GetName());
            nFeatureCount += poUnderlyingLayer->GetFeatureCount(bForce);
            delete m_poCurrentTile;
            m_poCurrentTile = nullptr;
        }
        ResetReading();
        return nFeatureCount;
    }
    return OGRLayer::GetFeatureCount( bForce );
}

/************************************************************************/
/*                         SetSpatialFilter()                           */
/************************************************************************/

void OGRMVTDirectoryLayer::SetSpatialFilter( OGRGeometry * poGeomIn )
{
    OGRLayer::SetSpatialFilter(poGeomIn);

    OGREnvelope sEnvelope;
    if( m_poFilterGeom != nullptr )
        sEnvelope = m_sFilterEnvelope;
    if( m_sExtent.IsInit() )
    {
        if( sEnvelope.IsInit() )
            sEnvelope.Intersect( m_sExtent );
        else
            sEnvelope = m_sExtent;
    }

    if( sEnvelope.IsInit() &&
        sEnvelope.MinX >= -10 * m_poDS->GetTileDim0() &&
        sEnvelope.MinY >= -10 * m_poDS->GetTileDim0() &&
        sEnvelope.MaxX <= 10 * m_poDS->GetTileDim0() &&
        sEnvelope.MaxY <= 10 * m_poDS->GetTileDim0() )
    {
        const double dfTileDim = m_poDS->GetTileDim0() / (1 << m_nZ);
        m_nFilterMinX = std::max(0, static_cast<int>(
            floor((sEnvelope.MinX - m_poDS->GetTopXOrigin()) / dfTileDim)));
        m_nFilterMinY = std::max(0, static_cast<int>(
            floor((m_poDS->GetTopYOrigin() - sEnvelope.MaxY) / dfTileDim)));
        m_nFilterMaxX = std::min(static_cast<int>(
            ceil((sEnvelope.MaxX - m_poDS->GetTopXOrigin()) / dfTileDim)),
            (1 << m_nZ)-1);
        m_nFilterMaxY = std::min(static_cast<int>(
            ceil((m_poDS->GetTopYOrigin() - sEnvelope.MinY) / dfTileDim)),
            (1 << m_nZ)-1);
    }
    else
    {
        m_nFilterMinX = 0;
        m_nFilterMinY = 0;
        m_nFilterMaxX = (1 << m_nZ)-1;
        m_nFilterMaxY = (1 << m_nZ)-1;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMVTDirectoryLayer::TestCapability(const char* pszCap)
{
    if( EQUAL(pszCap, OLCFastGetExtent))
    {
        return TRUE;
    }
    return OGRMVTLayerBase::TestCapability(pszCap);
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRMVTDirectoryLayer::GetExtent( OGREnvelope *psExtent, int bForce )
{
    if( m_sExtent.IsInit() )
    {
        *psExtent = m_sExtent;
        return OGRERR_NONE;
    }
    return OGRLayer::GetExtent(psExtent, bForce);
}

/************************************************************************/
/*                         CreateFeatureFrom()                          */
/************************************************************************/

OGRFeature* OGRMVTDirectoryLayer::CreateFeatureFrom(OGRFeature* poSrcFeature)
{

    return OGRMVTCreateFeatureFrom(poSrcFeature, m_poFeatureDefn,
                                   m_bJsonField, GetSpatialRef());
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature* OGRMVTDirectoryLayer::GetNextRawFeature()
{
    while( true )
    {
        OpenTileIfNeeded();
        if( m_poCurrentTile == nullptr )
            return nullptr;
        OGRLayer* poUnderlyingLayer =
            m_poCurrentTile->GetLayerByName(GetName());
        OGRFeature* poUnderlyingFeature = poUnderlyingLayer->GetNextFeature();
        if( poUnderlyingFeature != nullptr )
        {
            OGRFeature* poFeature = CreateFeatureFrom(poUnderlyingFeature);
            poFeature->SetFID(m_nFIDBase +
                (poUnderlyingFeature->GetFID() << (2 * m_nZ)));
            delete poUnderlyingFeature;
            return poFeature;
        }
        else
        {
            delete m_poCurrentTile;
            m_poCurrentTile = nullptr;
        }
    }
}

/************************************************************************/
/*                           GetFeature()                               */
/************************************************************************/

OGRFeature* OGRMVTDirectoryLayer::GetFeature(GIntBig nFID)
{
    const int nX = static_cast<int>(nFID & ((1 << m_nZ)-1));
    const int nY = static_cast<int>((nFID >> m_nZ) & ((1 << m_nZ)-1));
    const GIntBig nTileFID = nFID >> (2 * m_nZ);
    const CPLString osFilename = CPLFormFilename(
        CPLFormFilename( m_osDirName, CPLSPrintf("%d", nX), nullptr),
        CPLSPrintf("%d.%s", nY, m_poDS->m_osTileExtension.c_str()), nullptr);
    GDALOpenInfo oOpenInfo(("MVT:" + osFilename).c_str(), GA_ReadOnly);
    oOpenInfo.papszOpenOptions = CSLSetNameValue(nullptr,
            "METADATA_FILE",
            m_bJsonField ? "" : m_poDS->m_osMetadataMemFilename.c_str());
    oOpenInfo.papszOpenOptions = CSLSetNameValue(oOpenInfo.papszOpenOptions,
            "DO_NOT_ERROR_ON_MISSING_TILE", "YES");
    GDALDataset* poTile = OGRMVTDataset::Open(&oOpenInfo);
    CSLDestroy(oOpenInfo.papszOpenOptions);
    OGRFeature* poFeature = nullptr;
    if( poTile )
    {
        OGRLayer* poLayer = poTile->GetLayerByName(GetName());
        if( poLayer )
        {
            OGRFeature* poUnderlyingFeature =
                poLayer->GetFeature(nTileFID);
            if( poUnderlyingFeature )
            {
                poFeature = CreateFeatureFrom(poUnderlyingFeature);
                poFeature->SetFID(nFID);
            }
            delete poUnderlyingFeature;
        }
    }
    delete poTile;
    return poFeature;
}

/************************************************************************/
/*                           OGRMVTDataset()                            */
/************************************************************************/

OGRMVTDataset::OGRMVTDataset(GByte* pabyData):
    m_pabyData(pabyData),
    m_poSRS(new OGRSpatialReference())
{
    m_poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    m_bClip = CPLTestBool(CPLGetConfigOption("OGR_MVT_CLIP", "YES"));

    // Default WebMercator tiling scheme
    InitWebMercatorTilingScheme(m_poSRS,
                                m_dfTopXOrigin,
                                m_dfTopYOrigin,
                                m_dfTileDim0);
}

/************************************************************************/
/*                           ~OGRMVTDataset()                           */
/************************************************************************/

OGRMVTDataset::~OGRMVTDataset()
{
    VSIFree(m_pabyData);
    if( !m_osMetadataMemFilename.empty() )
        VSIUnlink(m_osMetadataMemFilename);
    if( m_poSRS )
        m_poSRS->Release();
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRMVTDataset::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= GetLayerCount() )
        return nullptr;
    return m_apoLayers[iLayer].get();
}


/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

static int OGRMVTDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "MVT:") )
        return TRUE;

    if( STARTS_WITH(poOpenInfo->pszFilename, "/vsicurl") )
    {
        if( CPLGetValueType(CPLGetFilename(poOpenInfo->pszFilename)) ==
                CPL_VALUE_INTEGER )
        {
            return TRUE;
        }
    }

    if( poOpenInfo->bIsDirectory )
    {
        if( CPLGetValueType(CPLGetFilename(poOpenInfo->pszFilename)) ==
                                                        CPL_VALUE_INTEGER )
        {
            VSIStatBufL sStat;
            CPLString osMetadataFile(
                CPLFormFilename(CPLGetPath(poOpenInfo->pszFilename),
                                "metadata.json", nullptr));
            const char* pszMetadataFile = CSLFetchNameValue(
                poOpenInfo->papszOpenOptions, "METADATA_FILE");
            if( pszMetadataFile )
            {
                osMetadataFile = pszMetadataFile;
            }
            if( !osMetadataFile.empty() &&
                (STARTS_WITH(osMetadataFile, "http://") ||
                 STARTS_WITH(osMetadataFile, "https://") ||
                 VSIStatL(osMetadataFile, &sStat) == 0) )
            {
                return TRUE;
            }
            if( pszMetadataFile == nullptr )
            {
                // tileserver-gl metadata file:
                // If opening /path/to/foo/0, try looking for /path/to/foo.json
                CPLString osParentDir(CPLGetPath(poOpenInfo->pszFilename));
                osMetadataFile = CPLFormFilename(
                                CPLGetPath(osParentDir),
                                CPLGetFilename(osParentDir),
                                "json");
                if( VSIStatL(osMetadataFile, &sStat) == 0 )
                {
                    return TRUE;
                }
            }

            // At least 3 files, to include the dummy . and ..
            CPLStringList aosDirContent(
                VSIReadDirEx(poOpenInfo->pszFilename, 3));
            aosDirContent = StripDummyEntries(aosDirContent);
            if( !aosDirContent.empty() &&
                CPLGetValueType(aosDirContent[0]) == CPL_VALUE_INTEGER )
            {
                CPLString osSubDir = CPLFormFilename(
                    poOpenInfo->pszFilename, aosDirContent[0], nullptr);
                // At least 3 files, to include the dummy . and ..
                CPLStringList aosSubDirContent(VSIReadDirEx(osSubDir, 10));
                aosSubDirContent = StripDummyEntries(aosSubDirContent);
                CPLString osTileExtension(CSLFetchNameValueDef(
                    poOpenInfo->papszOpenOptions, "TILE_EXTENSION", "pbf"));
                for( int i = 0; i < aosSubDirContent.Count(); i++ )
                {
                    if( CPLGetValueType(CPLGetBasename(aosSubDirContent[i]))
                            == CPL_VALUE_INTEGER )
                    {
                        CPLString osExtension(
                            CPLGetExtension(aosSubDirContent[i]));
                        if( EQUAL(osExtension, osTileExtension) ||
                            EQUAL(osExtension, "mvt") )
                        {
                            return TRUE;
                        }
                    }
                }
            }
        }
        return FALSE;
    }

    if( poOpenInfo->nHeaderBytes <= 2 )
        return FALSE;

    // GZip header ?
    if( poOpenInfo->pabyHeader[0] == 0x1F &&
        poOpenInfo->pabyHeader[1] == 0x8B )
    {
        // Prevent recursion
        if( STARTS_WITH(poOpenInfo->pszFilename, "/vsigzip/") )
        {
            return FALSE;
        }
        CPLConfigOptionSetter oSetter(
            "CPL_VSIL_GZIP_WRITE_PROPERTIES", "NO", false);
        GDALOpenInfo oOpenInfo( (CPLString("/vsigzip/") +
                            poOpenInfo->pszFilename).c_str(), GA_ReadOnly );
        return OGRMVTDriverIdentify( &oOpenInfo );
    }

    // The GPB macros assume that the buffer is nul terminated,
    // which is the case
    const GByte* pabyData = reinterpret_cast<GByte*>(poOpenInfo->pabyHeader);
    const GByte* const pabyDataStart = pabyData;
    const GByte* pabyLayerStart;
    const GByte* const pabyDataLimit = pabyData + poOpenInfo->nHeaderBytes;
    const GByte* pabyLayerEnd = pabyDataLimit;
    int nKey = 0;
    unsigned int nLayerLength = 0;
    bool bLayerNameFound = false;
    bool bKeyFound = false;
    bool bFeatureFound = false;
    bool bVersionFound = false;

    try
    {
        READ_FIELD_KEY(nKey);
        if( nKey != MAKE_KEY(knLAYER, WT_DATA) )
            return FALSE;
        READ_VARUINT32(pabyData, pabyDataLimit, nLayerLength);
        pabyLayerStart = pabyData;

        // Sanity check on layer length
        if( nLayerLength < static_cast<unsigned>(
                    poOpenInfo->nHeaderBytes - (pabyData - pabyDataStart)) )
        {
            if( pabyData[nLayerLength] != MAKE_KEY(knLAYER, WT_DATA) )
                return FALSE;
            pabyLayerEnd = pabyData + nLayerLength;
        }
        else if( nLayerLength > 10 * 1024 * 1024 )
        {
            return FALSE;
        }

        // Quick scan on partial layer content to see if it seems to conform to
        // the proto
        while( pabyData < pabyLayerEnd )
        {
            READ_VARUINT32(pabyData, pabyLayerEnd, nKey);
            auto nFieldNumber = GET_FIELDNUMBER(nKey);
            auto nWireType = GET_WIRETYPE(nKey);
            if( nFieldNumber == knLAYER_NAME )
            {
                if( nWireType != WT_DATA )
                {
                    CPLDebug("MVT", "Invalid wire type for layer_name field");
                }
                char* pszLayerName = nullptr;
                unsigned int nTextSize = 0;
                READ_TEXT_WITH_SIZE(pabyData, pabyLayerEnd, pszLayerName,
                                    nTextSize);
                if( nTextSize == 0 ||
                    !CPLIsUTF8(pszLayerName, nTextSize) )
                {
                    CPLFree(pszLayerName);
                    CPLDebug("MVT", "Protobuf error: line %d", __LINE__);
                    return FALSE;
                }
                CPLFree(pszLayerName);
                bLayerNameFound = true;
            }
            else if( nFieldNumber == knLAYER_FEATURES )
            {
                if( nWireType != WT_DATA )
                {
                    CPLDebug("MVT", "Invalid wire type for layer_features field");
                }
                unsigned int nFeatureLength = 0;
                unsigned int nGeomType = 0;
                READ_VARUINT32(pabyData, pabyLayerEnd, nFeatureLength);
                if( nFeatureLength > nLayerLength - (pabyData - pabyLayerStart) )
                {
                    CPLDebug("MVT", "Protobuf error: line %d", __LINE__);
                    return FALSE;
                }
                bFeatureFound = true;

                const GByte* const pabyDataFeatureStart = pabyData;
                const GByte* const pabyDataFeatureEnd = pabyDataStart + std::min(
                    static_cast<int>(pabyData + nFeatureLength - pabyDataStart),
                    poOpenInfo->nHeaderBytes);
                while( pabyData < pabyDataFeatureEnd )
                {
                    READ_VARUINT32(pabyData, pabyDataFeatureEnd, nKey);
                    nFieldNumber = GET_FIELDNUMBER(nKey);
                    nWireType = GET_WIRETYPE(nKey);
                    if( nFieldNumber == knFEATURE_TYPE)
                    {
                        if( nWireType != WT_VARINT )
                        {
                            CPLDebug("MVT", "Invalid wire type for feature_type field");
                            return FALSE;
                        }
                        READ_VARUINT32(pabyData, pabyDataFeatureEnd, nGeomType);
                        if( nGeomType > knGEOM_TYPE_POLYGON )
                        {
                            CPLDebug("MVT", "Protobuf error: line %d", __LINE__);
                            return FALSE;
                        }
                    }
                    else if( nFieldNumber == knFEATURE_TAGS )
                    {
                        if( nWireType != WT_DATA )
                        {
                            CPLDebug("MVT", "Invalid wire type for feature_tags field");
                            return FALSE;
                        }
                        unsigned int nTagsSize = 0;
                        READ_VARUINT32(pabyData, pabyDataFeatureEnd, nTagsSize);
                        if( nTagsSize == 0 || nTagsSize >
                            nFeatureLength - (pabyData - pabyDataFeatureStart) )
                        {
                            CPLDebug("MVT", "Protobuf error: line %d", __LINE__);
                            return FALSE;
                        }
                        const GByte* const pabyDataTagsEnd = pabyDataStart + std::min(
                            static_cast<int>(pabyData + nTagsSize - pabyDataStart),
                            poOpenInfo->nHeaderBytes);
                        while( pabyData < pabyDataTagsEnd )
                        {
                            unsigned int nKeyIdx = 0;
                            unsigned int nValIdx = 0;
                            READ_VARUINT32(pabyData, pabyDataTagsEnd, nKeyIdx);
                            READ_VARUINT32(pabyData, pabyDataTagsEnd, nValIdx);
                            if( nKeyIdx > 10 * 1024 * 1024 ||
                                nValIdx > 10 * 1024 * 1024 )
                            {
                                CPLDebug("MVT", "Protobuf error: line %d",
                                        __LINE__);
                                return FALSE;
                            }
                        }
                    }
                    else if( nFieldNumber == knFEATURE_GEOMETRY && nWireType != WT_DATA )
                    {
                        CPLDebug("MVT", "Invalid wire type for feature_geometry field");
                        return FALSE;
                    }
                    else if( nKey == MAKE_KEY(knFEATURE_GEOMETRY, WT_DATA) &&
                            nGeomType >= knGEOM_TYPE_POINT &&
                            nGeomType <= knGEOM_TYPE_POLYGON )
                    {
                        unsigned int nGeometrySize = 0;
                        READ_VARUINT32(pabyData, pabyDataFeatureEnd, nGeometrySize);
                        if( nGeometrySize == 0 || nGeometrySize >
                                nFeatureLength - (pabyData - pabyDataFeatureStart) )
                        {
                            CPLDebug("MVT", "Protobuf error: line %d", __LINE__);
                            return FALSE;
                        }
                        const GByte* const pabyDataGeometryEnd = pabyDataStart +
                            std::min(static_cast<int>(
                                        pabyData + nGeometrySize - pabyDataStart),
                            poOpenInfo->nHeaderBytes);

                        if( nGeomType == knGEOM_TYPE_POINT )
                        {
                            unsigned int nCmdCountCombined = 0;
                            unsigned int nCount;
                            READ_VARUINT32(pabyData, pabyDataGeometryEnd,
                                            nCmdCountCombined);
                            nCount = GetCmdCount(nCmdCountCombined);
                            if( GetCmdId(nCmdCountCombined) != knCMD_MOVETO ||
                                nCount == 0 ||
                                nCount > 10 * 1024 * 1024 )
                            {
                                CPLDebug("MVT", "Protobuf error: line %d",
                                        __LINE__);
                                return FALSE;
                            }
                            for( unsigned i = 0; i < 2 * nCount; i++ )
                            {
                                SKIP_VARINT(pabyData, pabyDataGeometryEnd);
                            }
                        }
                        else if( nGeomType == knGEOM_TYPE_LINESTRING )
                        {
                            while( pabyData < pabyDataGeometryEnd )
                            {
                                unsigned int nCmdCountCombined = 0;
                                unsigned int nLineToCount;
                                // Should be a moveto
                                READ_VARUINT32(pabyData, pabyDataGeometryEnd,
                                                nCmdCountCombined);
                                if( GetCmdId(nCmdCountCombined) != knCMD_MOVETO ||
                                    GetCmdCount(nCmdCountCombined) != 1 )
                                {
                                    CPLDebug("MVT", "Protobuf error: line %d",
                                            __LINE__);
                                    return FALSE;
                                }
                                SKIP_VARINT(pabyData, pabyDataGeometryEnd);
                                SKIP_VARINT(pabyData, pabyDataGeometryEnd);
                                READ_VARUINT32(pabyData, pabyDataGeometryEnd,
                                            nCmdCountCombined);
                                if( GetCmdId(nCmdCountCombined) != knCMD_LINETO )
                                {
                                    CPLDebug("MVT", "Protobuf error: line %d",
                                            __LINE__);
                                    return FALSE;
                                }
                                nLineToCount = GetCmdCount(nCmdCountCombined);
                                for( unsigned i = 0; i < 2 * nLineToCount; i++ )
                                {
                                    SKIP_VARINT(pabyData, pabyDataGeometryEnd);
                                }
                            }
                        }
                        else /* if( nGeomType == knGEOM_TYPE_POLYGON ) */
                        {
                            while( pabyData < pabyDataGeometryEnd )
                            {
                                unsigned int nCmdCountCombined = 0;
                                unsigned int nLineToCount;
                                // Should be a moveto
                                READ_VARUINT32(pabyData, pabyDataGeometryEnd,
                                                nCmdCountCombined);
                                if( GetCmdId(nCmdCountCombined) != knCMD_MOVETO ||
                                    GetCmdCount(nCmdCountCombined) != 1 )
                                {
                                    CPLDebug("MVT", "Protobuf error: line %d",
                                            __LINE__);
                                    return FALSE;
                                }
                                SKIP_VARINT(pabyData, pabyDataGeometryEnd);
                                SKIP_VARINT(pabyData, pabyDataGeometryEnd);
                                READ_VARUINT32(pabyData, pabyDataGeometryEnd,
                                            nCmdCountCombined);
                                if( GetCmdId(nCmdCountCombined) != knCMD_LINETO )
                                {
                                    CPLDebug("MVT", "Protobuf error: line %d",
                                            __LINE__);
                                    return FALSE;
                                }
                                nLineToCount = GetCmdCount(nCmdCountCombined);
                                for( unsigned i = 0; i < 2 * nLineToCount; i++ )
                                {
                                    SKIP_VARINT(pabyData, pabyDataGeometryEnd);
                                }
                                // Should be a closepath
                                READ_VARUINT32(pabyData, pabyDataGeometryEnd,
                                                nCmdCountCombined);
                                if( GetCmdId(nCmdCountCombined) != knCMD_CLOSEPATH ||
                                    GetCmdCount(nCmdCountCombined) != 1 )
                                {
                                    CPLDebug("MVT", "Protobuf error: line %d",
                                            __LINE__);
                                    return FALSE;
                                }
                            }
                        }

                        pabyData = pabyDataGeometryEnd;
                    }
                    else
                    {
                        SKIP_UNKNOWN_FIELD(pabyData, pabyDataFeatureEnd, FALSE);
                    }
                }

                pabyData = pabyDataFeatureEnd;
            }
            else if( nFieldNumber == knLAYER_KEYS )
            {
                if( nWireType != WT_DATA )
                {
                    CPLDebug("MVT", "Invalid wire type for keys field");
                    return FALSE;
                }
                char* pszKey = nullptr;
                unsigned int nTextSize = 0;
                READ_TEXT_WITH_SIZE(pabyData, pabyLayerEnd, pszKey, nTextSize);
                if( !CPLIsUTF8(pszKey, nTextSize) )
                {
                    CPLDebug("MVT", "Protobuf error: line %d", __LINE__);
                    CPLFree(pszKey);
                    return FALSE;
                }
                CPLFree(pszKey);
                bKeyFound = true;
            }
            else if( nFieldNumber == knLAYER_VALUES )
            {
                if( nWireType != WT_DATA )
                {
                    CPLDebug("MVT", "Invalid wire type for values field");
                    return FALSE;
                }
                unsigned int nValueLength = 0;
                READ_VARUINT32(pabyData, pabyLayerEnd, nValueLength);
                if( nValueLength == 0 ||
                    nValueLength > nLayerLength - (pabyData - pabyLayerStart) )
                {
                    CPLDebug("MVT", "Protobuf error: line %d", __LINE__);
                    return FALSE;
                }
                pabyData += nValueLength;
            }
            else if( GET_FIELDNUMBER(nKey) == knLAYER_EXTENT &&
                     GET_WIRETYPE(nKey) != WT_VARINT )
            {
                CPLDebug("MVT", "Invalid wire type for extent field");
                return FALSE;
            }
#if 0
            // The check on extent is too fragile. Values of 65536 can be found
            else if( nKey == MAKE_KEY(knLAYER_EXTENT, WT_VARINT) )
            {
                unsigned int nExtent = 0;
                READ_VARUINT32(pabyData, pabyLayerEnd, nExtent);
                if( nExtent < 128 || nExtent > 16834 )
                {
                    CPLDebug("MVT", "Invalid extent: %u", nExtent);
                    return FALSE;
                }
            }
#endif
            else if( nFieldNumber == knLAYER_VERSION )
            {
                if( nWireType != WT_VARINT )
                {
                    CPLDebug("MVT", "Invalid wire type for version field");
                    return FALSE;
                }
                unsigned int nVersion = 0;
                READ_VARUINT32(pabyData, pabyLayerEnd, nVersion);
                if( nVersion != 1 && nVersion != 2 )
                {
                    CPLDebug("MVT", "Invalid version: %u", nVersion);
                    return FALSE;
                }
                bVersionFound = true;
            }
            else
            {
                SKIP_UNKNOWN_FIELD(pabyData, pabyLayerEnd, FALSE);
            }
        }
    }
    catch( const GPBException& )
    {
    }

    return bLayerNameFound && (bKeyFound || bFeatureFound || bVersionFound);
}

/************************************************************************/
/*                     LongLatToSphericalMercator()                     */
/************************************************************************/

static void LongLatToSphericalMercator(double* x, double* y)
{
  double X = kmSPHERICAL_RADIUS * (*x) / 180 * M_PI;
  double Y = kmSPHERICAL_RADIUS *
                log( tan(M_PI / 4 + 0.5 * (*y) / 180 * M_PI) );
  *x = X;
  *y = Y;
}

/************************************************************************/
/*                          LoadMetadata()                              */
/************************************************************************/

static bool LoadMetadata(const CPLString& osMetadataFile,
                         const CPLString& osMetadataContent,
                         CPLJSONArray& oVectorLayers,
                         CPLJSONArray& oTileStatLayers,
                         CPLJSONObject& oBounds,
                         OGRSpatialReference* poSRS,
                         double& dfTopX,
                         double& dfTopY,
                         double& dfTileDim0,
                         const CPLString& osMetadataMemFilename)

{
    CPLJSONDocument oDoc;

    bool bLoadOK;
    if( !osMetadataContent.empty() )
    {
        bLoadOK = oDoc.LoadMemory(osMetadataContent);
    }
    else if( STARTS_WITH(osMetadataFile, "http://") ||
             STARTS_WITH(osMetadataFile, "https://") )
    {
        bLoadOK = oDoc.LoadUrl(osMetadataFile, nullptr);
    }
    else
    {
        bLoadOK = oDoc.Load(osMetadataFile);
    }
    if( !bLoadOK )
        return false;

    CPLJSONObject oCrs(oDoc.GetRoot().GetObj("crs"));
    CPLJSONObject oTopX(oDoc.GetRoot().GetObj("tile_origin_upper_left_x"));
    CPLJSONObject oTopY(oDoc.GetRoot().GetObj("tile_origin_upper_left_y"));
    CPLJSONObject oTileDim0(oDoc.GetRoot().GetObj("tile_dimension_zoom_0"));
    if( oCrs.IsValid() && oTopX.IsValid() && oTopY.IsValid() &&
        oTileDim0.IsValid() )
    {
        poSRS->SetFromUserInput( oCrs.ToString().c_str() );
        dfTopX = oTopX.ToDouble();
        dfTopY = oTopY.ToDouble();
        dfTileDim0 = oTileDim0.ToDouble();
    }

    oVectorLayers.Deinit();
    oTileStatLayers.Deinit();

    CPLJSONObject oJson = oDoc.GetRoot().GetObj("json");
    if( !(oJson.IsValid() && oJson.GetType() == CPLJSONObject::Type::String) )
    {
        oVectorLayers =
            oDoc.GetRoot().GetArray("vector_layers");

        oTileStatLayers =
            oDoc.GetRoot().GetArray("tilestats/layers");
    }
    else
    {
        CPLJSONDocument oJsonDoc;
        if( !oJsonDoc.LoadMemory(oJson.ToString()) )
        {
            return false;
        }

        oVectorLayers =
            oJsonDoc.GetRoot().GetArray("vector_layers");

        oTileStatLayers =
            oJsonDoc.GetRoot().GetArray("tilestats/layers");
    }

    oBounds = oDoc.GetRoot().GetObj("bounds");

    if( !osMetadataMemFilename.empty() )
    {
        oDoc.Save(osMetadataMemFilename);
    }

    return oVectorLayers.IsValid();
}

/************************************************************************/
/*                       ConvertFromWGS84()                             */
/************************************************************************/

static void ConvertFromWGS84(OGRSpatialReference* poTargetSRS,
                             double& dfX0, double& dfY0,
                             double& dfX1, double& dfY1)
{
    OGRSpatialReference oSRS_EPSG3857;
    oSRS_EPSG3857.SetFromUserInput(SRS_EPSG_3857);

    if( poTargetSRS->IsSame(&oSRS_EPSG3857) )
    {
        LongLatToSphericalMercator(&dfX0, &dfY0);
        LongLatToSphericalMercator(&dfX1, &dfY1);
    }
    else
    {
        OGRSpatialReference oSRS_EPSG4326;
        oSRS_EPSG4326.SetFromUserInput(SRS_WKT_WGS84_LAT_LONG);
        oSRS_EPSG4326.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        OGRCoordinateTransformation* poCT =
            OGRCreateCoordinateTransformation(&oSRS_EPSG4326, poTargetSRS);
        if( poCT )
        {
            poCT->Transform(1, &dfX0, &dfY0);
            poCT->Transform(1, &dfX1, &dfY1);
            delete poCT;
        }
    }
}

/************************************************************************/
/*                         OpenDirectory()                              */
/************************************************************************/

GDALDataset *OGRMVTDataset::OpenDirectory( GDALOpenInfo* poOpenInfo )

{
    const CPLString osZ(CPLGetFilename(poOpenInfo->pszFilename));
    if( CPLGetValueType(osZ) != CPL_VALUE_INTEGER )
        return nullptr;

    const int nZ = atoi(osZ);
    if( nZ < 0 || nZ > 30 )
        return nullptr;

    CPLString osMetadataFile(
        CPLFormFilename(CPLGetPath(poOpenInfo->pszFilename),
                        "metadata.json", nullptr));
    const char* pszMetadataFile = CSLFetchNameValue(
        poOpenInfo->papszOpenOptions, "METADATA_FILE");
    if( pszMetadataFile )
    {
        osMetadataFile = pszMetadataFile;
    }

    CPLString osTileExtension(CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, "TILE_EXTENSION", "pbf"));
    bool bJsonField = CPLFetchBool(
        poOpenInfo->papszOpenOptions, "JSON_FIELD", false);
    VSIStatBufL sStat;

    bool bMetadataFileExists = false;
    CPLString osMetadataContent;
    if( STARTS_WITH(osMetadataFile, "http://") ||
        STARTS_WITH(osMetadataFile, "https://") )
    {
        for( int i = 0; i < 2; i++ )
        {
            if( pszMetadataFile == nullptr )
                CPLPushErrorHandler(CPLQuietErrorHandler);
            CPLHTTPResult* psResult = CPLHTTPFetch( osMetadataFile, nullptr );
            if( pszMetadataFile == nullptr )
                CPLPopErrorHandler();
            if( psResult == nullptr )
            {
                osMetadataFile.clear();
            }
            else if( psResult->pszErrBuf != nullptr ||
                    psResult->pabyData == nullptr )
            {
                CPLHTTPDestroyResult(psResult);
                osMetadataFile.clear();

                if( i == 0 && pszMetadataFile == nullptr )
                {
                    // tileserver-gl metadata file:
                    // If opening /path/to/foo/0, try looking for /path/to/foo.json
                    CPLString osParentDir(CPLGetPath(poOpenInfo->pszFilename));
                    osMetadataFile = CPLFormFilename(
                                    CPLGetPath(osParentDir),
                                    CPLGetFilename(osParentDir),
                                    "json");
                    continue;
                }
            }
            else
            {
                bMetadataFileExists = true;
                osMetadataContent =
                    reinterpret_cast<const char*>(psResult->pabyData);
                CPLHTTPDestroyResult(psResult);
            }
            break;
        }
    }
    else if( !osMetadataFile.empty() )
    {
        bMetadataFileExists = (VSIStatL(osMetadataFile, &sStat) == 0);
        if( !bMetadataFileExists && pszMetadataFile == nullptr )
        {
            // tileserver-gl metadata file:
            // If opening /path/to/foo/0, try looking for /path/to/foo.json
            CPLString osParentDir(CPLGetPath(poOpenInfo->pszFilename));
            osMetadataFile = CPLFormFilename(
                            CPLGetPath(osParentDir),
                            CPLGetFilename(osParentDir),
                            "json");
            bMetadataFileExists = (VSIStatL(osMetadataFile, &sStat) == 0);
        }
    }

    if( !bMetadataFileExists )
    {
        // If we don't have a metadata file, iterate through all tiles to
        // establish the layer definitions.
        OGRMVTDataset   *poDS = nullptr;
        bool bTryToListDir =
            !STARTS_WITH(poOpenInfo->pszFilename, "/vsicurl/") &&
            !STARTS_WITH(poOpenInfo->pszFilename, "/vsicurl_streaming/") &&
            !STARTS_WITH(poOpenInfo->pszFilename, "/vsicurl?") &&
            !STARTS_WITH(poOpenInfo->pszFilename, "http://") &&
            !STARTS_WITH(poOpenInfo->pszFilename, "https://");
        CPLStringList aosDirContent;
        if( bTryToListDir )
        {
            aosDirContent = VSIReadDir(poOpenInfo->pszFilename);
            aosDirContent = StripDummyEntries(aosDirContent);
        }
        const int nMaxTiles = atoi(
            CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                            "TILE_COUNT_TO_ESTABLISH_FEATURE_DEFN", "1000"));
        int nCountTiles = 0;
        int nFailedAttempts = 0;
        for( int i = 0; i <
                (bTryToListDir ? aosDirContent.Count(): (1 << nZ)); i++ )
        {
            if( bTryToListDir )
            {
                if( CPLGetValueType(aosDirContent[i]) != CPL_VALUE_INTEGER )
                {
                    continue;
                }
            }
            CPLString osSubDir = CPLFormFilename(
                poOpenInfo->pszFilename,
                bTryToListDir ? aosDirContent[i] : CPLSPrintf("%d", i),
                nullptr);
            CPLStringList aosSubDirContent;
            if( bTryToListDir )
            {
                aosSubDirContent = VSIReadDir(osSubDir);
                aosSubDirContent = StripDummyEntries(aosSubDirContent);
            }
            for( int j = 0; j <
                    (bTryToListDir ? aosSubDirContent.Count() : (1 << nZ)); j++ )
            {
                if( bTryToListDir )
                {
                    if( CPLGetValueType(CPLGetBasename(aosSubDirContent[j]))
                                                    != CPL_VALUE_INTEGER )
                    {
                        continue;
                    }
                }
                CPLString osFilename( CPLFormFilename(
                    osSubDir,
                    bTryToListDir ? aosSubDirContent[j] :
                        CPLSPrintf("%d.%s", j, osTileExtension.c_str()),
                    nullptr) );
                GDALOpenInfo oOpenInfo(("MVT:" + osFilename).c_str(),
                               GA_ReadOnly);
                oOpenInfo.papszOpenOptions = CSLSetNameValue(nullptr,
                    "METADATA_FILE", "");
                oOpenInfo.papszOpenOptions = CSLSetNameValue(
                    oOpenInfo.papszOpenOptions,
                    "DO_NOT_ERROR_ON_MISSING_TILE", "YES");
                auto poTileDS = OGRMVTDataset::Open(&oOpenInfo);
                if( poTileDS )
                {
                    if( poDS == nullptr )
                    {
                        poDS = new OGRMVTDataset(nullptr);
                        poDS->m_osTileExtension = osTileExtension;
                        poDS->SetDescription(poOpenInfo->pszFilename);
                        poDS->m_bClip = CPLFetchBool(
                            poOpenInfo->papszOpenOptions, "CLIP",
                            poDS->m_bClip);
                    }

                    for( int k = 0; k < poTileDS->GetLayerCount(); k++ )
                    {
                        OGRLayer* poTileLayer = poTileDS->GetLayer(k);
                        OGRFeatureDefn* poTileLDefn =
                            poTileLayer->GetLayerDefn();
                        OGRwkbGeometryType eTileGeomType =
                            poTileLDefn->GetGeomType();
                        OGRwkbGeometryType eTileGeomTypeColl =
                            OGR_GT_GetCollection(eTileGeomType);
                        if( eTileGeomTypeColl != wkbUnknown &&
                            eTileGeomTypeColl != eTileGeomType )
                        {
                            eTileGeomType = eTileGeomTypeColl;
                        }

                        OGRLayer* poLayer = poDS->GetLayerByName(
                                                    poTileLayer->GetName());
                        OGRFeatureDefn* poLDefn;
                        if( poLayer == nullptr )
                        {
                            CPLJSONObject oFields;
                            oFields.Deinit();
                            poDS->m_apoLayers.push_back(
                              std::unique_ptr<OGRLayer>(
                                new OGRMVTDirectoryLayer(poDS,
                                                         poTileLayer->GetName(),
                                                         poOpenInfo->pszFilename,
                                                         oFields,
                                                         bJsonField,
                                                         wkbUnknown,
                                                         nullptr))
                            );
                            poLayer = poDS->m_apoLayers.back().get();
                            poLDefn = poLayer->GetLayerDefn();
                            poLDefn->SetGeomType(eTileGeomType);
                        }
                        else
                        {
                            poLDefn = poLayer->GetLayerDefn();
                            if( poLayer->GetGeomType() != eTileGeomType )
                            {
                                poLDefn->SetGeomType(wkbUnknown);
                            }
                        }

                        if( !bJsonField )
                        {
                            for( int l = 1;
                                    l < poTileLDefn->GetFieldCount(); l++ )
                            {
                                OGRFieldDefn* poTileFDefn =
                                    poTileLDefn->GetFieldDefn(l);
                                int nFieldIdx = poLDefn->GetFieldIndex(
                                            poTileFDefn->GetNameRef());
                                if( nFieldIdx < 0 )
                                {
                                    poLDefn->AddFieldDefn(poTileFDefn);
                                }
                                else
                                {
                                    MergeFieldDefn(
                                        poLDefn->GetFieldDefn(nFieldIdx),
                                        poTileFDefn->GetType(),
                                        poTileFDefn->GetSubType());
                                }
                            }
                        }
                    }
                    nCountTiles ++;
                }
                else if( !bTryToListDir )
                {
                    nFailedAttempts ++;
                }
                delete poTileDS;
                CSLDestroy(oOpenInfo.papszOpenOptions);

                if( nFailedAttempts == 10 )
                    break;
                if( nMaxTiles > 0 && nCountTiles == nMaxTiles )
                    break;
            }

            if( nFailedAttempts == 10 )
                break;
            if( nMaxTiles > 0 && nCountTiles == nMaxTiles )
                break;
        }
        return poDS;
    }

    CPLJSONArray oVectorLayers;
    CPLJSONArray oTileStatLayers;
    CPLJSONObject oBounds;

    OGRMVTDataset   *poDS = new OGRMVTDataset(nullptr);

    CPLString osMetadataMemFilename =
        CPLSPrintf("/vsimem/%p_metadata.json", poDS);
    if( !LoadMetadata(osMetadataFile, osMetadataContent,
                      oVectorLayers, oTileStatLayers, oBounds,
                      poDS->m_poSRS,
                      poDS->m_dfTopXOrigin,
                      poDS->m_dfTopYOrigin,
                      poDS->m_dfTileDim0,
                      osMetadataMemFilename) )
    {
        delete poDS;
        return nullptr;
    }

    OGREnvelope sExtent;
    bool bExtentValid = false;
    if( oBounds.IsValid() && oBounds.GetType() == CPLJSONObject::Type::String )
    {
        CPLStringList aosTokens(
            CSLTokenizeString2( oBounds.ToString().c_str(), ",", 0 ));
        if( aosTokens.Count() == 4 )
        {
            double dfX0 = CPLAtof(aosTokens[0]);
            double dfY0 = CPLAtof(aosTokens[1]);
            double dfX1 = CPLAtof(aosTokens[2]);
            double dfY1 = CPLAtof(aosTokens[3]);
            ConvertFromWGS84(poDS->m_poSRS,
                             dfX0, dfY0, dfX1, dfY1);
            bExtentValid = true;
            sExtent.MinX = dfX0;
            sExtent.MinY = dfY0;
            sExtent.MaxX = dfX1;
            sExtent.MaxY = dfY1;
        }
    }
    else if( oBounds.IsValid() && oBounds.GetType() == CPLJSONObject::Type::Array )
    {
        // Cf https://free.tilehosting.com/data/v3.json?key=THE_KEY
        CPLJSONArray oBoundArray = oBounds.ToArray();
        if( oBoundArray.Size() == 4 )
        {
            bExtentValid = true;
            sExtent.MinX = oBoundArray[0].ToDouble();
            sExtent.MinY = oBoundArray[1].ToDouble();
            sExtent.MaxX = oBoundArray[2].ToDouble();
            sExtent.MaxY = oBoundArray[3].ToDouble();
            ConvertFromWGS84(poDS->m_poSRS,
                             sExtent.MinX, sExtent.MinY,
                             sExtent.MaxX, sExtent.MaxY);
        }
    }

    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->m_bClip = CPLFetchBool(
        poOpenInfo->papszOpenOptions, "CLIP", poDS->m_bClip);
    poDS->m_osTileExtension = osTileExtension;
    poDS->m_osMetadataMemFilename = osMetadataMemFilename;
    for( int i = 0; i < oVectorLayers.Size(); i++ )
    {
        CPLJSONObject oId = oVectorLayers[i].GetObj("id");
        if( oId.IsValid() && oId.GetType() ==
                CPLJSONObject::Type::String )
        {
            OGRwkbGeometryType eGeomType = wkbUnknown;
            if( oTileStatLayers.IsValid() )
            {
                eGeomType = OGRMVTFindGeomTypeFromTileStat(
                    oTileStatLayers, oId.ToString().c_str());
            }

            CPLJSONObject oFields = oVectorLayers[i].GetObj("fields");
            poDS->m_apoLayers.push_back( std::unique_ptr<OGRLayer>(
                new OGRMVTDirectoryLayer(poDS,
                                         oId.ToString().c_str(),
                                         poOpenInfo->pszFilename,
                                         oFields,
                                         bJsonField,
                                         eGeomType,
                                         (bExtentValid) ? &sExtent : nullptr))
            );
        }
    }

    return poDS;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *OGRMVTDataset::Open( GDALOpenInfo* poOpenInfo )

{
    if( !OGRMVTDriverIdentify(poOpenInfo) || poOpenInfo->eAccess == GA_Update )
        return nullptr;

    VSILFILE* fp = poOpenInfo->fpL;
    CPLString osFilename(poOpenInfo->pszFilename);
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "MVT:") )
    {
        osFilename = poOpenInfo->pszFilename + strlen("MVT:");
        if( STARTS_WITH(osFilename, "/vsigzip/http://") ||
            STARTS_WITH(osFilename, "/vsigzip/https://") )
        {
            osFilename = osFilename.substr(strlen("/vsigzip/"));
        }

        // If the filename has no extension and is a directory, consider
        // we open a directory
        VSIStatBufL sStat;
        if( !STARTS_WITH(osFilename, "/vsigzip/") &&
            strchr((CPLGetFilename(osFilename)), '.') == nullptr &&
            VSIStatL(osFilename, &sStat) == 0 &&
            VSI_ISDIR(sStat.st_mode) )
        {
            GDALOpenInfo oOpenInfo( osFilename, GA_ReadOnly );
            oOpenInfo.papszOpenOptions = poOpenInfo->papszOpenOptions;
            GDALDataset* poDS = OpenDirectory(&oOpenInfo);
            if( poDS )
                poDS->SetDescription(poOpenInfo->pszFilename);
            return poDS;
        }

        // For a network resource, if the filename is an integer, consider it
        // is a directory and open as such
        if( (STARTS_WITH(osFilename, "/vsicurl") ||
             STARTS_WITH(osFilename, "http://") ||
             STARTS_WITH(osFilename, "https://")) &&
             CPLGetValueType(CPLGetFilename(osFilename)) ==
                CPL_VALUE_INTEGER )
        {
            GDALOpenInfo oOpenInfo( osFilename, GA_ReadOnly );
            oOpenInfo.papszOpenOptions = poOpenInfo->papszOpenOptions;
            GDALDataset* poDS = OpenDirectory(&oOpenInfo);
            if( poDS )
                poDS->SetDescription(poOpenInfo->pszFilename);
            return poDS;
        }

        if( !STARTS_WITH(osFilename, "http://") &&
            !STARTS_WITH(osFilename, "https://") )
        {
            CPLConfigOptionSetter oSetter(
                "CPL_VSIL_GZIP_WRITE_PROPERTIES", "NO", false);
            CPLConfigOptionSetter oSetter2(
                "CPL_VSIL_GZIP_SAVE_INFO", "NO", false);
            fp = VSIFOpenL(osFilename, "rb");
            // Is it a gzipped file ?
            if( fp && !STARTS_WITH(osFilename, "/vsigzip/") )
            {
                GByte abyHeaderBytes[2] = {0, 0 };
                VSIFReadL( abyHeaderBytes, 2, 1, fp );
                if( abyHeaderBytes[0] == 0x1F && abyHeaderBytes[1] == 0x8B )
                {
                    VSIFCloseL(fp);
                    fp = VSIFOpenL( ("/vsigzip/" + osFilename).c_str(), "rb");
                }
            }
        }
    }
    else if( poOpenInfo->bIsDirectory ||
            (STARTS_WITH(poOpenInfo->pszFilename, "/vsicurl")  &&
             CPLGetValueType(CPLGetFilename(poOpenInfo->pszFilename)) ==
                CPL_VALUE_INTEGER) )
    {
        return OpenDirectory(poOpenInfo);
    }
    // Is it a gzipped file ?
    else if( poOpenInfo->nHeaderBytes >= 2 &&
             poOpenInfo->pabyHeader[0] == 0x1F &&
             poOpenInfo->pabyHeader[1] == 0x8B )
    {
        CPLConfigOptionSetter oSetter(
            "CPL_VSIL_GZIP_WRITE_PROPERTIES", "NO", false);
        fp = VSIFOpenL( ("/vsigzip/" + osFilename).c_str(), "rb");
    }
    else
    {
        poOpenInfo->fpL = nullptr;
    }
    if( fp == nullptr &&
        !STARTS_WITH(osFilename, "http://") &&
        !STARTS_WITH(osFilename, "https://") )
    {
        return nullptr;
    }

    CPLString osY = CPLGetBasename(osFilename);
    CPLString osX = CPLGetBasename(CPLGetPath(osFilename));
    CPLString osZ = CPLGetBasename(CPLGetPath(CPLGetPath(osFilename)));
    size_t nPos = osY.find('.');
    if( nPos != std::string::npos )
        osY.resize(nPos);

    CPLString osMetadataFile;
    if( CSLFetchNameValue(poOpenInfo->papszOpenOptions, "METADATA_FILE") )
    {
        osMetadataFile = CSLFetchNameValue(poOpenInfo->papszOpenOptions,
                                           "METADATA_FILE");
    }
    else if( CPLGetValueType(osX) == CPL_VALUE_INTEGER &&
             CPLGetValueType(osY) == CPL_VALUE_INTEGER &&
             CPLGetValueType(osZ) == CPL_VALUE_INTEGER )
    {
        osMetadataFile = CPLFormFilename(
            CPLGetPath(CPLGetPath(CPLGetPath(osFilename))), "metadata.json",
            nullptr);
        if( osMetadataFile.find("/vsigzip/") == 0 )
        {
            osMetadataFile = osMetadataFile.substr(strlen("/vsigzip/"));
        }
        VSIStatBufL sStat;
        if( osMetadataFile.empty() || VSIStatL(osMetadataFile, &sStat) != 0 )
        {
            osMetadataFile.clear();
        }
    }

    if( CSLFetchNameValue(poOpenInfo->papszOpenOptions, "X") &&
        CSLFetchNameValue(poOpenInfo->papszOpenOptions, "Y") &&
        CSLFetchNameValue(poOpenInfo->papszOpenOptions, "Z") )
    {
        osX = CSLFetchNameValue(poOpenInfo->papszOpenOptions, "X");
        osY = CSLFetchNameValue(poOpenInfo->papszOpenOptions, "Y");
        osZ = CSLFetchNameValue(poOpenInfo->papszOpenOptions, "Z");
    }

    GByte* pabyDataMod;
    size_t nFileSize;

    if( fp == nullptr )
    {
        bool bSilenceErrors = CPLFetchBool(poOpenInfo->papszOpenOptions,
                                    "DO_NOT_ERROR_ON_MISSING_TILE",false);
        if( bSilenceErrors )
            CPLPushErrorHandler(CPLQuietErrorHandler);
        CPLHTTPResult* psResult = CPLHTTPFetch( osFilename, nullptr );
        if( bSilenceErrors )
            CPLPopErrorHandler();
        if( psResult == nullptr )
            return nullptr;
        if( psResult->pszErrBuf != nullptr )
        {
            CPLHTTPDestroyResult(psResult);
            return nullptr;
        }
        pabyDataMod = psResult->pabyData;
        if( pabyDataMod == nullptr )
        {
            CPLHTTPDestroyResult(psResult);
            return nullptr;
        }
        nFileSize = psResult->nDataLen;
        psResult->pabyData = nullptr;
        CPLHTTPDestroyResult(psResult);

        // zlib decompress if needed
        if( nFileSize > 2 && pabyDataMod[0] == 0x1F &&
            pabyDataMod[1] == 0x8B )
        {
            size_t nOutBytes = 0;
            void* pUncompressed = CPLZLibInflate( pabyDataMod, nFileSize,
                                                  nullptr, 0,
                                                  &nOutBytes );
            CPLFree(pabyDataMod);
            if( pUncompressed == nullptr )
            {
                return nullptr;
            }
            pabyDataMod = static_cast<GByte*>(pUncompressed);
            nFileSize = nOutBytes;
        }
    }
    else
    {
        // Check file size and ingest into memory
        VSIFSeekL(fp, 0, SEEK_END);
        vsi_l_offset nFileSizeL = VSIFTellL(fp);
        if( nFileSizeL > 10 * 1024 * 1024 )
        {
            VSIFCloseL(fp);
            return nullptr;
        }
        nFileSize = static_cast<size_t>(nFileSizeL);
        pabyDataMod = static_cast<GByte*>(VSI_MALLOC_VERBOSE(nFileSize+1));
        if( pabyDataMod == nullptr )
        {
            VSIFCloseL(fp);
            return nullptr;
        }
        VSIFSeekL(fp, 0, SEEK_SET);
        VSIFReadL(pabyDataMod, 1, nFileSize, fp);
        pabyDataMod[nFileSize] = 0;
        VSIFCloseL(fp);
    }

    const GByte* pabyData = pabyDataMod;

    // First scan to browse through layers
    const GByte* pabyDataLimit = pabyData + nFileSize;
    int nKey = 0;
    OGRMVTDataset   *poDS = new OGRMVTDataset(pabyDataMod);
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->m_bClip = CPLFetchBool(
        poOpenInfo->papszOpenOptions, "CLIP", poDS->m_bClip);

    if( !(CPLGetValueType(osX) == CPL_VALUE_INTEGER &&
          CPLGetValueType(osY) == CPL_VALUE_INTEGER &&
          CPLGetValueType(osZ) == CPL_VALUE_INTEGER ) )
    {
        // See https://github.com/mapbox/mvt-fixtures/tree/master/real-world/compressed
        int nX = 0;
        int nY = 0;
        int nZ = 0;
        CPLString osBasename(CPLGetBasename(CPLGetBasename(osFilename)));
        if( sscanf(osBasename, "%d-%d-%d", &nZ, &nX, &nY) == 3 ||
            sscanf(osBasename, "%d_%d_%d", &nZ, &nX, &nY) == 3 )
        {
            osX = CPLSPrintf("%d", nX);
            osY = CPLSPrintf("%d", nY);
            osZ = CPLSPrintf("%d", nZ);
        }
    }

    CPLJSONArray oVectorLayers;
    oVectorLayers.Deinit();

    CPLJSONArray oTileStatLayers;
    oTileStatLayers.Deinit();

    if( !osMetadataFile.empty() )
    {
        CPLJSONObject oBounds;
        LoadMetadata(osMetadataFile, CPLString(),
                     oVectorLayers, oTileStatLayers, oBounds,
                     poDS->m_poSRS,
                     poDS->m_dfTopXOrigin,
                     poDS->m_dfTopYOrigin,
                     poDS->m_dfTileDim0,
                     CPLString());
    }

    const char* pszGeorefTopX = CSLFetchNameValue(poOpenInfo->papszOpenOptions, "GEOREF_TOPX");
    const char* pszGeorefTopY = CSLFetchNameValue(poOpenInfo->papszOpenOptions, "GEOREF_TOPY");
    const char* pszGeorefTileDimX = CSLFetchNameValue(poOpenInfo->papszOpenOptions, "GEOREF_TILEDIMX");
    const char* pszGeorefTileDimY = CSLFetchNameValue(poOpenInfo->papszOpenOptions, "GEOREF_TILEDIMY");
    if( pszGeorefTopX && pszGeorefTopY && pszGeorefTileDimX && pszGeorefTileDimY )
    {
        poDS->m_bGeoreferenced = true;
        poDS->m_dfTileDimX = CPLAtof(pszGeorefTileDimX);
        poDS->m_dfTileDimY = CPLAtof(pszGeorefTileDimY);
        poDS->m_dfTopX = CPLAtof(pszGeorefTopX);
        poDS->m_dfTopY = CPLAtof(pszGeorefTopY);
        poDS->m_poSRS->Release();
        poDS->m_poSRS = nullptr;
    }
    else
    if( CPLGetValueType(osX) == CPL_VALUE_INTEGER &&
        CPLGetValueType(osY) == CPL_VALUE_INTEGER &&
        CPLGetValueType(osZ) == CPL_VALUE_INTEGER )
    {
        int nX = atoi(osX);
        int nY = atoi(osY);
        int nZ = atoi(osZ);
        if( nZ >= 0 && nZ < 30 &&
            nX >= 0 && nX < (1 << nZ) &&
            nY >= 0 && nY < (1 << nZ) )
        {
            poDS->m_bGeoreferenced = true;
            poDS->m_dfTileDimX = poDS->m_dfTileDim0 / (1 << nZ);
            poDS->m_dfTileDimY = poDS->m_dfTileDimX;
            poDS->m_dfTopX = poDS->m_dfTopXOrigin + nX * poDS->m_dfTileDimX;
            poDS->m_dfTopY = poDS->m_dfTopYOrigin - nY * poDS->m_dfTileDimY;
        }
    }

    try
    {
        while( pabyData < pabyDataLimit )
        {
            READ_FIELD_KEY(nKey);
            if( nKey == MAKE_KEY(knLAYER, WT_DATA) )
            {
                unsigned int nLayerSize = 0;
                READ_SIZE(pabyData, pabyDataLimit, nLayerSize);
                const GByte* pabyDataLayer = pabyData;
                const GByte* pabyDataLimitLayer = pabyData + nLayerSize;
                while( pabyData < pabyDataLimitLayer )
                {
                    READ_VARINT32(pabyData, pabyDataLimitLayer, nKey);
                    if( nKey == MAKE_KEY(knLAYER_NAME, WT_DATA) )
                    {
                        char* pszLayerName = nullptr;
                        READ_TEXT(pabyData, pabyDataLimitLayer, pszLayerName);

                        CPLJSONObject oFields;
                        oFields.Deinit();
                        if( oVectorLayers.IsValid() )
                        {
                            for( int i = 0; i < oVectorLayers.Size(); i++ )
                            {
                                CPLJSONObject oId = oVectorLayers[i].GetObj("id");
                                if( oId.IsValid() && oId.GetType() ==
                                        CPLJSONObject::Type::String )
                                {
                                    if( oId.ToString() == pszLayerName )
                                    {
                                        oFields = oVectorLayers[i].GetObj("fields");
                                        break;
                                    }
                                }
                            }
                        }

                        OGRwkbGeometryType eGeomType = wkbUnknown;
                        if( oTileStatLayers.IsValid() )
                        {
                            eGeomType = OGRMVTFindGeomTypeFromTileStat(
                                oTileStatLayers, pszLayerName);
                        }

                        poDS->m_apoLayers.push_back( std::unique_ptr<OGRLayer>(
                            new OGRMVTLayer(poDS, pszLayerName, pabyDataLayer,
                                            nLayerSize, oFields, eGeomType) ) );
                        CPLFree(pszLayerName);
                        break;
                    }
                    else
                    {
                        SKIP_UNKNOWN_FIELD(pabyData, pabyDataLimitLayer, FALSE);
                    }
                }
                pabyData = pabyDataLimitLayer;
            }
            else
            {
                SKIP_UNKNOWN_FIELD(pabyData, pabyDataLimit, FALSE);
            }
        }

        return poDS;
    }
    catch( const GPBException& e )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        delete poDS;
        return nullptr;
    }
}

#ifdef HAVE_MVT_WRITE_SUPPORT

/************************************************************************/
/*                           OGRMVTWriterDataset                        */
/************************************************************************/

class OGRMVTWriterLayer;

struct OGRMVTFeatureContent
{
    std::vector<std::pair<std::string, MVTTileLayerValue>> oValues;
    GIntBig nFID;
};

class OGRMVTWriterDataset final: public GDALDataset
{
        class MVTFieldProperties
        {
            public:
                CPLString m_osName;
                std::set<MVTTileLayerValue> m_oSetValues;
                std::set<MVTTileLayerValue> m_oSetAllValues;
                double m_dfMinVal = 0;
                double m_dfMaxVal = 0;
                bool m_bAllInt = false;
                MVTTileLayerValue::ValueType m_eType =
                    MVTTileLayerValue::ValueType::NONE;
        };

        class MVTLayerProperties
        {
            public:
                int m_nMinZoom = 0;
                int m_nMaxZoom = 0;
                std::map<MVTTileLayerFeature::GeomType, GIntBig>
                                                        m_oCountGeomType;
                std::map<CPLString, size_t> m_oMapFieldNameToIdx;
                std::vector<MVTFieldProperties> m_aoFields;
                std::set<CPLString> m_oSetFields;
        };

        std::vector<std::unique_ptr<OGRMVTWriterLayer>> m_apoLayers;
        CPLString                              m_osTempDB;
        mutable std::mutex                     m_oDBMutex;
        mutable bool                           m_bWriteFeatureError = false;
        sqlite3_vfs                           *m_pMyVFS = nullptr;
        sqlite3                               *m_hDB = nullptr;
        sqlite3_stmt                          *m_hInsertStmt = nullptr;
        int                                    m_nMinZoom = 0;
        int                                    m_nMaxZoom = 5;
        double                                 m_dfSimplification = 0.0;
        double                                 m_dfSimplificationMaxZoom = 0.0;
        CPLJSONDocument                        m_oConf;
        unsigned                               m_nExtent = knDEFAULT_EXTENT;
        int                                    m_nMetadataVersion = 2;
        int                                    m_nMVTVersion = 2;
        int                                    m_nBuffer = 5 * knDEFAULT_EXTENT / 256;
        bool                                   m_bGZip = true;
        mutable CPLWorkerThreadPool            m_oThreadPool;
        bool                                   m_bThreadPoolOK = false;
        mutable GIntBig                        m_nTempTiles = 0;
        CPLString                              m_osName;
        CPLString                              m_osDescription;
        CPLString                              m_osType{"overlay"};
        sqlite3                               *m_hDBMBTILES = nullptr;
        OGREnvelope                            m_oEnvelope;
        unsigned                               m_nMaxTileSize = 500000;
        unsigned                               m_nMaxFeatures = 200000;
        std::map<std::string,std::string>      m_oMapLayerNameToDesc;
        std::map<std::string,GIntBig>          m_oMapLayerNameToFeatureCount;
        CPLString                              m_osBounds;
        CPLString                              m_osCenter;
        CPLString                              m_osExtension{"pbf"};
        OGRSpatialReference*                   m_poSRS = nullptr;
        double                                 m_dfTopX = 0.0;
        double                                 m_dfTopY = 0.0;
        double                                 m_dfTileDim0 = 0.0;
        bool                                   m_bReuseTempFile = false; // debug only

        OGRErr              PreGenerateForTile(int nZ, int nX, int nY,
                                               const CPLString& osTargetName,
                                               bool bIsMaxZoomForLayer,
                                               std::shared_ptr<OGRMVTFeatureContent> poFeatureContent,
                                               GIntBig nSerial,
                                               std::shared_ptr<OGRGeometry> poGeom,
                                               const OGREnvelope& sEnvelope) const;

        static void         WriterTaskFunc(void* pParam);

        OGRErr              PreGenerateForTileReal(int nZ, int nX, int nY,
                                               const CPLString& osTargetName,
                                               bool bIsMaxZoomForLayer,
                                               const OGRMVTFeatureContent* poFeatureContent,
                                               GIntBig nSerial,
                                               const OGRGeometry* poGeom,
                                               const OGREnvelope& sEnvelope) const;

        void                ConvertToTileCoords(double dfX,
                                                  double dfY,
                                                  int& nX,
                                                  int& nY,
                                                  double dfTopX,
                                                  double dfTopY,
                                                  double dfTileDim) const;
        bool                EncodeLineString(MVTTileLayerFeature *poGPBFeature,
                                             const OGRLineString* poLS,
                                             OGRLineString* poOutLS,
                                             bool bWriteLastPoint,
                                             bool bReverseOrder,
                                             GUInt32 nMinLineTo,
                                             double dfTopX,
                                             double dfTopY,
                                             double dfTileDim,
                                             int& nLastX,
                                             int& nLastY) const;
        bool                EncodePolygon(MVTTileLayerFeature *poGPBFeature,
                                             const OGRPolygon* poPoly,
                                             OGRPolygon* poOutPoly,
                                             double dfTopX,
                                             double dfTopY,
                                             double dfTileDim,
                                             bool bCanRecurse,
                                             int& nLastX,
                                             int& nLastY,
                                             double& dfArea) const;
#ifdef notdef
        bool                EncodeRepairedOuterRing(
                                            MVTTileLayerFeature *poGPBFeature,
                                            OGRPolygon& oOutPoly,
                                            int& nLastX,
                                            int& nLastY) const;
#endif

        static
        void UpdateLayerProperties(MVTLayerProperties* poLayerProperties,
                                    const std::string& osKey,
                                    const MVTTileLayerValue& oValue);

        void EncodeFeature(
                        const void* pabyBlob,
                        int nBlobSize,
                        std::shared_ptr<MVTTileLayer> poTargetLayer,
                        std::map<CPLString, GUInt32>& oMapKeyToIdx,
                        std::map<MVTTileLayerValue, GUInt32>& oMapValueToIdx,
                        MVTLayerProperties* poLayerProperties,
                        GUInt32 nExtent,
                        unsigned& nFeaturesInTile);

        std::string EncodeTile(
                        int nZ, int nX, int nY,
                        sqlite3_stmt* hStmtLayer,
                        sqlite3_stmt* hStmtRows,
                        std::map<CPLString, MVTLayerProperties>& oMapLayerProps,
                        std::set<CPLString>& oSetLayers,
                        GIntBig& nTempTilesRead);

        std::string RecodeTileLowerResolution(
                                int nZ, int nX, int nY,
                                int nExtent,
                                sqlite3_stmt* hStmtLayer,
                                sqlite3_stmt* hStmtRows);

        bool                CreateOutput();

        bool                GenerateMetadata(
                        size_t nLayers,
                        const std::map<CPLString, MVTLayerProperties>& oMap);

    public:

        OGRMVTWriterDataset();
        ~OGRMVTWriterDataset();

        OGRLayer           *ICreateLayer( const char *,
                                       OGRSpatialReference * = nullptr,
                                       OGRwkbGeometryType = wkbUnknown,
                                       char ** = nullptr ) override;

        int                 TestCapability( const char * ) override;

        OGRErr              WriteFeature(OGRMVTWriterLayer* poLayer,
                                         OGRFeature* poFeature,
                                         GIntBig nSerial,
                                         OGRGeometry* poGeom);

        static GDALDataset* Create( const char * pszFilename,
                                   int nXSize,
                                   int nYSize,
                                   int nBandsIn,
                                   GDALDataType eDT,
                                   char **papszOptions );

        OGRSpatialReference* GetSRS() { return m_poSRS; }
};

/************************************************************************/
/*                           OGRMVTWriterLayer                          */
/************************************************************************/

class OGRMVTWriterLayer final: public OGRLayer
{
        friend class OGRMVTWriterDataset;

        OGRMVTWriterDataset         *m_poDS = nullptr;
        OGRFeatureDefn              *m_poFeatureDefn = nullptr;
        OGRCoordinateTransformation *m_poCT = nullptr;
        GIntBig                      m_nSerial = 0;
        int                          m_nMinZoom = 0;
        int                          m_nMaxZoom = 5;
        CPLString                    m_osTargetName;

    public:

        OGRMVTWriterLayer(OGRMVTWriterDataset* poDS,
                          const char* pszLayerName,
                          OGRSpatialReference* poSRS);
        ~OGRMVTWriterLayer();

        void                ResetReading() override {}
        OGRFeature         *GetNextFeature() override { return nullptr; }
        OGRFeatureDefn     *GetLayerDefn() override { return m_poFeatureDefn; }
        int                 TestCapability( const char * ) override;
        OGRErr              ICreateFeature( OGRFeature* ) override;
        OGRErr              CreateField( OGRFieldDefn*, int ) override;
};

/************************************************************************/
/*                          OGRMVTWriterLayer()                         */
/************************************************************************/

OGRMVTWriterLayer::OGRMVTWriterLayer(OGRMVTWriterDataset* poDS,
                                     const char* pszLayerName,
                                     OGRSpatialReference* poSRSIn)
{
    m_poDS = poDS;
    m_poFeatureDefn = new OGRFeatureDefn(pszLayerName);
    SetDescription(m_poFeatureDefn->GetName());
    m_poFeatureDefn->Reference();

    m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poDS->GetSRS());

    if( poSRSIn != nullptr && !poDS->GetSRS()->IsSame(poSRSIn) )
    {
        m_poCT = OGRCreateCoordinateTransformation( poSRSIn, poDS->GetSRS() );
        if( m_poCT == nullptr  )
        {
            // If we can't create a transformation, issue a warning - but
            // continue the transformation.
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "Failed to create coordinate transformation between the "
                "input and target coordinate systems.");
        }
    }
}

/************************************************************************/
/*                          ~OGRMVTWriterLayer()                        */
/************************************************************************/

OGRMVTWriterLayer::~OGRMVTWriterLayer()
{
    m_poFeatureDefn->Release();
    delete m_poCT;
}

/************************************************************************/
/*                            TestCapability()                          */
/************************************************************************/

int OGRMVTWriterLayer::TestCapability( const char *pszCap )
{

    if( EQUAL(pszCap,OLCSequentialWrite) || EQUAL(pszCap,OLCCreateField) )
        return true;
    return false;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRMVTWriterLayer::CreateField( OGRFieldDefn* poFieldDefn, int )
{
    m_poFeatureDefn->AddFieldDefn(poFieldDefn);
    return OGRERR_NONE;
}

/************************************************************************/
/*                            ICreateFeature()                          */
/************************************************************************/

OGRErr OGRMVTWriterLayer::ICreateFeature( OGRFeature* poFeature )
{
    OGRGeometry* poGeom = poFeature->GetGeometryRef();
    if( poGeom == nullptr || poGeom->IsEmpty() )
        return OGRERR_NONE;
    if( m_poCT )
    {
        poGeom->transform(m_poCT);
    }
    m_nSerial ++;
    return m_poDS->WriteFeature(this, poFeature, m_nSerial, poGeom);
}

/************************************************************************/
/*                          OGRMVTWriterDataset()                       */
/************************************************************************/

OGRMVTWriterDataset::OGRMVTWriterDataset()
{
    // Default WebMercator tiling scheme
    m_poSRS = new OGRSpatialReference();
    m_poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    InitWebMercatorTilingScheme(m_poSRS,
                                m_dfTopX,
                                m_dfTopY,
                                m_dfTileDim0);
}

/************************************************************************/
/*                         ~OGRMVTWriterDataset()                       */
/************************************************************************/

OGRMVTWriterDataset::~OGRMVTWriterDataset()
{
    if( GetDescription()[0] != '\0' )
    {
        CreateOutput();
    }
    if( m_hInsertStmt != nullptr )
    {
        sqlite3_finalize( m_hInsertStmt );
    }
    if( m_hDB )
    {
        sqlite3_close(m_hDB);
    }
    if( m_hDBMBTILES )
    {
        sqlite3_close(m_hDBMBTILES);
    }
    if( !m_osTempDB.empty() &&
        !m_bReuseTempFile &&
        CPLTestBool(CPLGetConfigOption("OGR_MVT_REMOVE_TEMP_FILE", "YES")) )
    {
        VSIUnlink(m_osTempDB);
    }

    if( m_pMyVFS )
    {
        sqlite3_vfs_unregister(m_pMyVFS);
        CPLFree(m_pMyVFS->pAppData);
        CPLFree(m_pMyVFS);
    }
    m_poSRS->Release();
}

/************************************************************************/
/*                        ConvertToTileCoords()                     */
/************************************************************************/

void OGRMVTWriterDataset::ConvertToTileCoords(double dfX,
                                                  double dfY,
                                                  int& nX,
                                                  int& nY,
                                                  double dfTopX,
                                                  double dfTopY,
                                                  double dfTileDim) const
{
    if( dfTileDim == 0 )
    {
        nX = static_cast<int>(dfX);
        nY = static_cast<int>(dfY);
    }
    else
    {
        nX = static_cast<int>(std::round((dfX - dfTopX) * m_nExtent / dfTileDim));
        nY = static_cast<int>(std::round((dfTopY - dfY) * m_nExtent / dfTileDim));
    }
}

/************************************************************************/
/*                       GetCmdCountCombined()                          */
/************************************************************************/

static unsigned GetCmdCountCombined(unsigned int nCmdId,
                                    unsigned int nCmdCount)
{
    return (nCmdId | (nCmdCount << 3));
}

/************************************************************************/
/*                          EncodeLineString()                          */
/************************************************************************/

bool OGRMVTWriterDataset::EncodeLineString(MVTTileLayerFeature *poGPBFeature,
                                           const OGRLineString* poLS,
                                           OGRLineString* poOutLS,
                                           bool bWriteLastPoint,
                                           bool bReverseOrder,
                                           GUInt32 nMinLineTo,
                                           double dfTopX,
                                           double dfTopY,
                                           double dfTileDim,
                                           int& nLastX,
                                           int& nLastY) const
{
    const GUInt32 nInitialSize = poGPBFeature->getGeometryCount();
    const int nLastXOri = nLastX;
    const int nLastYOri = nLastY;
    GUInt32 nLineToCount = 0;
    const int nPoints = poLS->getNumPoints() - (bWriteLastPoint ? 0 : 1);
    if( poOutLS )
        poOutLS->setNumPoints(nPoints);
    int nFirstX = 0;
    int nFirstY = 0;
    int nLastXValid = nLastX;
    int nLastYValid = nLastY;
    for( int i = 0; i < nPoints; i++ )
    {
        int nX, nY;
        int nSrcIdx = bReverseOrder ? poLS->getNumPoints() - 1 - i : i;
        double dfX = poLS->getX(nSrcIdx);
        double dfY = poLS->getY(nSrcIdx);
        ConvertToTileCoords(dfX, dfY, nX, nY,
                                dfTopX, dfTopY, dfTileDim);
        int nDiffX = nX - nLastX;
        int nDiffY = nY - nLastY;
        if( i == 0 || nDiffX != 0 || nDiffY != 0 )
        {
            if( i > 0 )
            {
                nLineToCount ++;
                if( nLineToCount == 1 )
                {
                    poGPBFeature->addGeometry(
                        GetCmdCountCombined(knCMD_MOVETO, 1) );
                    const int nLastDiffX = nLastX - nLastXOri;
                    const int nLastDiffY = nLastY - nLastYOri;
                    poGPBFeature->addGeometry( EncodeSInt(nLastDiffX) );
                    poGPBFeature->addGeometry( EncodeSInt(nLastDiffY) );
                    if( poOutLS )
                        poOutLS->setPoint(0, nLastX, nLastY );

                    // To be modified later
                    poGPBFeature->addGeometry(
                        GetCmdCountCombined(knCMD_LINETO, 0) );
                }

                poGPBFeature->addGeometry( EncodeSInt(nDiffX) );
                poGPBFeature->addGeometry( EncodeSInt(nDiffY) );
                if( poOutLS )
                    poOutLS->setPoint(nLineToCount, nX, nY );
            }
            else
            {
                nFirstX = nX;
                nFirstY = nY;
            }
            nLastXValid = nLastX;
            nLastYValid = nLastY;
            nLastX = nX;
            nLastY = nY;
        }
    }

    // If last point of ring is identical to first one, discard it
    if( nMinLineTo == 2 && nLineToCount > 0 &&
        nFirstX == nLastX && nFirstY == nLastY )
    {
        poGPBFeature->resizeGeometryArray(
                            poGPBFeature->getGeometryCount() - 2);
        nLineToCount --;
        nLastX = nLastXValid;
        nLastY = nLastYValid;
    }

    if( nLineToCount >= nMinLineTo )
    {
        if( poOutLS )
            poOutLS->setNumPoints( 1 + nLineToCount );
        // Patch actual number of points in LINETO command
        poGPBFeature->setGeometry( nInitialSize + 3,
            GetCmdCountCombined(knCMD_LINETO, nLineToCount) );
        return true;
    }
    else
    {
        poGPBFeature->resizeGeometryArray( nInitialSize );
        nLastX = nLastXOri;
        nLastY = nLastYOri;
        return false;
    }
}

#ifdef notdef
/************************************************************************/
/*                     EncodeRepairedOuterRing()                        */
/************************************************************************/

bool OGRMVTWriterDataset::EncodeRepairedOuterRing(
                                            MVTTileLayerFeature *poGPBFeature,
                                            OGRPolygon& oInPoly,
                                            int& nLastX,
                                            int& nLastY) const
{
    std::unique_ptr<OGRGeometry> poFixedGeom(oInPoly.Buffer(0));
    if( !poFixedGeom.get() || poFixedGeom->IsEmpty() )
    {
        return false;
    }

    OGRPolygon* poPoly = nullptr;
    if( wkbFlatten(poFixedGeom->getGeometryType()) == wkbMultiPolygon )
    {
        OGRMultiPolygon* poMP = poFixedGeom.get()->toMultiPolygon();
        poPoly = poMP->getGeometryRef(0)->toPolygon();
    }
    else if( wkbFlatten(poFixedGeom->getGeometryType()) == wkbPolygon )
    {
        poPoly = poFixedGeom.get()->toPolygon();
    }
    if( !poPoly )
        return false;

    OGRLinearRing* poRing = poPoly->getExteriorRing();
    const bool bReverseOrder = !poRing->isClockwise();

    const GUInt32 nInitialSize = poGPBFeature->getGeometryCount();
    const int nLastXOri = nLastX;
    const int nLastYOri = nLastY;
    GUInt32 nLineToCount = 0;
    const int nPoints = poRing->getNumPoints() - 1;
    auto poOutLinearRing = cpl::make_unique<OGRLinearRing>();
    poOutLinearRing->setNumPoints(nPoints);
    for( int i = 0; i < nPoints; i++ )
    {
        int nSrcIdx = bReverseOrder ? poRing->getNumPoints() - 1 - i : i;
        double dfX = poRing->getX(nSrcIdx);
        double dfY = poRing->getY(nSrcIdx);
        int nX = static_cast<int>(std::round(dfX));
        int nY = static_cast<int>(std::round(dfY));
        if( nX != dfX || nY != dfY )
            continue;
        int nDiffX = nX - nLastX;
        int nDiffY = nY - nLastY;
        if( i == 0 || nDiffX != 0 || nDiffY != 0 )
        {
            if( i > 0 )
            {
                nLineToCount ++;
                if( nLineToCount == 1 )
                {
                    poGPBFeature->addGeometry(
                        GetCmdCountCombined(knCMD_MOVETO, 1) );
                    const int nLastDiffX = nLastX - nLastXOri;
                    const int nLastDiffY = nLastY - nLastYOri;
                    poGPBFeature->addGeometry( EncodeSInt(nLastDiffX) );
                    poGPBFeature->addGeometry( EncodeSInt(nLastDiffY) );
                    poOutLinearRing->setPoint(0, nLastX, nLastY );

                    // To be modified later
                    poGPBFeature->addGeometry(
                        GetCmdCountCombined(knCMD_LINETO, 0) );
                }

                poGPBFeature->addGeometry( EncodeSInt(nDiffX) );
                poGPBFeature->addGeometry( EncodeSInt(nDiffY) );
                poOutLinearRing->setPoint(nLineToCount, nX, nY );
            }
            nLastX = nX;
            nLastY = nY;
        }
    }
    if( nLineToCount >= 2 )
    {
        poOutLinearRing->setNumPoints( 1 + nLineToCount );
        OGRPolygon oOutPoly;
        oOutPoly.addRingDirectly(poOutLinearRing.release());
        int bIsValid;
        {
            CPLErrorStateBackuper oErrorStateBackuper;
            bIsValid = oOutPoly.IsValid();
        }
        if( bIsValid )
        {
            // Patch actual number of points in LINETO command
            poGPBFeature->setGeometry( nInitialSize + 3,
                GetCmdCountCombined(knCMD_LINETO, nLineToCount) );
            poGPBFeature->addGeometry(
                            GetCmdCountCombined(knCMD_CLOSEPATH, 1) );
            return true;
        }
    }

    poGPBFeature->resizeGeometryArray( nInitialSize );
    nLastX = nLastXOri;
    nLastY = nLastYOri;
    return false;
}
#endif

/************************************************************************/
/*                          EncodePolygon()                             */
/************************************************************************/

bool OGRMVTWriterDataset::EncodePolygon(MVTTileLayerFeature *poGPBFeature,
                                           const OGRPolygon* poPoly,
                                           OGRPolygon* poOutPoly,
                                           double dfTopX,
                                           double dfTopY,
                                           double dfTileDim,
                                           bool bCanRecurse,
                                           int& nLastX,
                                           int& nLastY,
                                           double& dfArea) const
{
    dfArea = 0;
    auto poOutOuterRing = cpl::make_unique<OGRLinearRing>();
    for( int i = 0; i < 1 + poPoly->getNumInteriorRings(); i++ )
    {
        const OGRLinearRing* poRing = (i == 0) ? poPoly->getExteriorRing():
                                           poPoly->getInteriorRing(i-1);
        if( poRing->getNumPoints() < 4 ||
            poRing->getX(0) != poRing->getX(poRing->getNumPoints()-1) ||
            poRing->getY(0) != poRing->getY(poRing->getNumPoints()-1) )
        {
            if( i == 0 )
                return false;
            continue;
        }
        const bool bWriteLastPoint = false;
        const bool bReverseOrder = (i == 0 && !poRing->isClockwise()) ||
                                   (i > 0 && poRing->isClockwise());
        const GUInt32 nMinLineTo = 2;
        std::unique_ptr<OGRLinearRing> poOutInnerRing;
        if( i > 0 )
            poOutInnerRing = cpl::make_unique<OGRLinearRing>();
        OGRLinearRing* poOutRing =
            poOutInnerRing.get() ? poOutInnerRing.get() : poOutOuterRing.get();

        const GUInt32 nInitialSize = poGPBFeature->getGeometryCount();
        const int nLastXOri = nLastX;
        const int nLastYOri = nLastY;
        bool bSuccess =
            EncodeLineString(poGPBFeature, poRing,
                             poOutRing,
                             bWriteLastPoint, bReverseOrder,
                             nMinLineTo,
                             dfTopX, dfTopY, dfTileDim, nLastX, nLastY);
        if( !bSuccess )
        {
            if( i == 0 )
                return false;
            continue;
        }

        if( poOutPoly == nullptr )
        {
            poGPBFeature->addGeometry(
                            GetCmdCountCombined(knCMD_CLOSEPATH, 1) );
            continue;
        }

        poOutRing->closeRings();
        OGRPolygon oOutPoly;
        oOutPoly.addRing( poOutOuterRing.get() );
        if( i > 0 )
        {
            // If the inner ring turns to be a outer ring once reduced,
            // discard it
            if( !poOutInnerRing->isClockwise() )
            {
                poGPBFeature->resizeGeometryArray( nInitialSize );
                nLastX = nLastXOri;
                nLastY = nLastYOri;
                continue;
            }
            dfArea -= poOutInnerRing->get_Area();
            oOutPoly.addRingDirectly( poOutInnerRing.release() );
        }
        else
        {
            dfArea = poOutOuterRing->get_Area();
        }

        int bIsValid;
        {
            CPLErrorStateBackuper oErrorStateBackuper;
            CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
            bIsValid = oOutPoly.IsValid();
        }

        if( bIsValid || (i == 0 && !bCanRecurse) )
            poOutPoly->addRing(poOutRing);

        if( i > 0 && bIsValid )
        {
            // Adding the current inner ring to the outer ring might be valid
            // but it might also conflict with a previously added inner ring
            if( i > 1 )
            {
                {
                    CPLErrorStateBackuper oErrorStateBackuper;
                    CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
                    bIsValid = poOutPoly->IsValid();
                }
                if( !bIsValid )
                {
                    poOutPoly->removeRing(
                        poOutPoly->getNumInteriorRings() );
                    poGPBFeature->resizeGeometryArray( nInitialSize );
                    nLastX = nLastXOri;
                    nLastY = nLastYOri;
                    continue;
                }
            }
        }

        // Do not emit invalid polygons, except if it is an outer ring
        // and we tried hard to fix it.
        else if ( !bIsValid && !(i == 0 && !bCanRecurse && dfArea > 0 &&
                           poOutRing->getNumPoints() >= 4) )
        {
            poGPBFeature->resizeGeometryArray( nInitialSize );
            nLastX = nLastXOri;
            nLastY = nLastYOri;
#if !defined(HAVE_MAKE_VALID)
            if( i == 0 )
            {
#ifdef nodef
                if( !EncodeRepairedOuterRing(poGPBFeature, oOutPoly,
                                             nLastX, nLastY) )
#endif
                {
                    if( !bCanRecurse )
                        return false;

                    CPLErrorStateBackuper oErrorStateBackuper;
                    CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);

                    // Old logic when MakeValid is not available.
                    // The Buffer() calls have bad memory requirements on
                    // densified geometries such as in
                    // https://github.com/OSGeo/gdal/issues/5109

                    const double dfTol = 2 * dfTileDim / m_nExtent;
                    std::unique_ptr<OGRGeometry> poBufferedPlus(
                            poPoly->Buffer(dfTol));
                    if( !poBufferedPlus.get() )
                        return false;

                    std::unique_ptr<OGRGeometry> poBuffered(
                            poBufferedPlus->Buffer(-dfTol));
                    if( !poBuffered.get() )
                        return false;

                    std::unique_ptr<OGRGeometry> poSimplified(
                        poBuffered->SimplifyPreserveTopology(dfTol));
                    if( !poSimplified.get() || poSimplified->IsEmpty() )
                        return false;

                    if( wkbFlatten(poSimplified->getGeometryType()) == wkbPolygon )
                    {
                        OGRPolygon* poSimplifiedPoly =
                            poSimplified.get()->toPolygon();
                        poOutPoly->empty();
                        return EncodePolygon(poGPBFeature,
                                            poSimplifiedPoly, poOutPoly,
                                            dfTopX, dfTopY, dfTileDim,
                                            false,
                                            nLastX, nLastY, dfArea);
                    }

                    return false;
                }
            }
#endif
            continue;
        }

        poGPBFeature->addGeometry(
                        GetCmdCountCombined(knCMD_CLOSEPATH, 1) );
    }
    return true;
}

/************************************************************************/
/*                          PreGenerateForTile()                        */
/************************************************************************/

OGRErr OGRMVTWriterDataset::PreGenerateForTileReal(
                                               int nZ, int nTileX, int nTileY,
                                               const CPLString& osTargetName,
                                               bool bIsMaxZoomForLayer,
                                               const OGRMVTFeatureContent* poFeatureContent,
                                               GIntBig nSerial,
                                               const OGRGeometry* poGeom,
                                               const OGREnvelope& sEnvelope) const
{
    double dfTileDim = m_dfTileDim0 / (1 << nZ);
    double dfBuffer = dfTileDim * m_nBuffer / m_nExtent;
    double dfTopX = m_dfTopX + nTileX * dfTileDim;
    double dfTopY = m_dfTopY - nTileY * dfTileDim;
    double dfBottomRightX = dfTopX + dfTileDim;
    double dfBottomRightY = dfTopY - dfTileDim;
    double dfIntersectTopX = dfTopX - dfBuffer;
    double dfIntersectTopY = dfTopY + dfBuffer;
    double dfIntersectBottomRightX = dfBottomRightX + dfBuffer;
    double dfIntersectBottomRightY = dfBottomRightY - dfBuffer;

    const OGRGeometry* poIntersection;
    std::unique_ptr<OGRGeometry> poIntersectionHolder; // keep in that scope
    if( sEnvelope.MinX >= dfIntersectTopX &&
        sEnvelope.MinY >= dfIntersectBottomRightY &&
        sEnvelope.MaxX <= dfIntersectBottomRightX &&
        sEnvelope.MaxY <= dfIntersectTopY )
    {
        poIntersection = poGeom;
    }
    else
    {
        OGRLinearRing* poLR = new OGRLinearRing();
        poLR->addPoint( dfIntersectTopX, dfIntersectTopY );
        poLR->addPoint( dfIntersectTopX, dfIntersectBottomRightY );
        poLR->addPoint( dfIntersectBottomRightX, dfIntersectBottomRightY );
        poLR->addPoint( dfIntersectBottomRightX, dfIntersectTopY );
        poLR->addPoint( dfIntersectTopX, dfIntersectTopY );
        OGRPolygon oPoly;
        oPoly.addRingDirectly(poLR);

        CPLErrorStateBackuper oErrorStateBackuper;
        CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
        auto poTmp = poGeom->Intersection(&oPoly);
        poIntersection = poTmp;
        poIntersectionHolder.reset(poTmp);
        if( poIntersection == nullptr || poIntersection->IsEmpty() )
        {
            return OGRERR_NONE;
        }
    }

    // Create a layer with a single feature in it
    std::shared_ptr<MVTTileLayer> poLayer =
        std::shared_ptr<MVTTileLayer>(new MVTTileLayer());
    std::shared_ptr<MVTTileLayerFeature> poGPBFeature =
        std::shared_ptr<MVTTileLayerFeature>(new MVTTileLayerFeature());
    poLayer->addFeature(poGPBFeature);

    OGRwkbGeometryType eGeomType = wkbFlatten(poGeom->getGeometryType());
    if( eGeomType == wkbPoint || eGeomType == wkbMultiPoint )
        poGPBFeature->setType( MVTTileLayerFeature::GeomType::POINT );
    else if( eGeomType == wkbLineString || eGeomType == wkbMultiLineString )
        poGPBFeature->setType( MVTTileLayerFeature::GeomType::LINESTRING );
    else if( eGeomType == wkbPolygon || eGeomType == wkbMultiPolygon )
        poGPBFeature->setType( MVTTileLayerFeature::GeomType::POLYGON );
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported geometry type");
        return OGRERR_NONE;
    }

    OGRwkbGeometryType eGeomToEncodeType = wkbFlatten(poIntersection->getGeometryType());

    // Simplify contour if requested by user
    const OGRGeometry* poGeomToEncode = poIntersection;
    std::unique_ptr<OGRGeometry> poGeomSimplified;
    const double dfSimplification = bIsMaxZoomForLayer ?
                                m_dfSimplificationMaxZoom : m_dfSimplification;
    if( dfSimplification > 0 &&
        (eGeomType == wkbLineString || eGeomType == wkbMultiLineString ||
         eGeomType == wkbPolygon || eGeomType == wkbMultiPolygon) )
    {
        const double dfTol = dfTileDim / m_nExtent;
        poGeomSimplified = std::unique_ptr<OGRGeometry>(
            poIntersection->SimplifyPreserveTopology(dfTol * dfSimplification));
        if( poGeomSimplified.get() )
        {
            poGeomToEncode = poGeomSimplified.get();
            eGeomToEncodeType = wkbFlatten(poGeomSimplified->getGeometryType());
        }
    }

    bool bGeomOK = false;
    double dfAreaOrLength = 0.0;

#ifdef HAVE_MAKE_VALID
    const auto EmitValidPolygon = [this, &bGeomOK, &dfAreaOrLength, &poGPBFeature](
                                    const OGRGeometry* poValidGeom)
    {
        bGeomOK = false;
        dfAreaOrLength = 0;
        int nLastX = 0;
        int nLastY = 0;

        if( wkbFlatten(poValidGeom->getGeometryType()) == wkbPolygon )
        {
            const OGRPolygon* poPoly = poValidGeom->toPolygon();
            double dfPartArea = 0.0;
            bGeomOK = EncodePolygon(
                          poGPBFeature.get(), poPoly, nullptr,
                          0, 0, 0,
                          false,
                          nLastX, nLastY, dfPartArea);
            dfAreaOrLength = dfPartArea;
        }
        else if( OGR_GT_IsSubClassOf( poValidGeom->getGeometryType(), wkbGeometryCollection ) )
        {
            for( auto&& poSubGeom: poValidGeom->toGeometryCollection() )
            {
                if( wkbFlatten(poSubGeom->getGeometryType()) == wkbPolygon )
                {
                    const OGRPolygon* poPoly = poSubGeom->toPolygon();
                    double dfPartArea = 0.0;
                    bGeomOK |= EncodePolygon(
                                  poGPBFeature.get(), poPoly, nullptr,
                                  0, 0, 0,
                                  false,
                                  nLastX, nLastY, dfPartArea);
                    dfAreaOrLength += dfPartArea;
                }
                else if( wkbFlatten(poSubGeom->getGeometryType()) == wkbMultiPolygon )
                {
                    const OGRMultiPolygon* poMPoly = poSubGeom->toMultiPolygon();
                    for( const auto* poPoly: poMPoly )
                    {
                        double dfPartArea = 0.0;
                        bGeomOK |= EncodePolygon(
                                      poGPBFeature.get(), poPoly, nullptr,
                                      0, 0, 0,
                                      false,
                                      nLastX, nLastY, dfPartArea);
                        dfAreaOrLength += dfPartArea;
                    }
                }
            }
        }
    };
#endif

    if( eGeomType == wkbPoint || eGeomType == wkbMultiPoint )
    {
        if( eGeomToEncodeType == wkbPoint )
        {
            const OGRPoint* poPoint = poIntersection->toPoint();
            int nX, nY;
            double dfX = poPoint->getX();
            double dfY = poPoint->getY();
            bGeomOK = true;
            ConvertToTileCoords(dfX, dfY, nX, nY,
                                    dfTopX, dfTopY, dfTileDim);
            poGPBFeature->addGeometry( GetCmdCountCombined(knCMD_MOVETO, 1) );
            poGPBFeature->addGeometry( EncodeSInt(nX) );
            poGPBFeature->addGeometry( EncodeSInt(nY) );
        }
        else if( eGeomToEncodeType == wkbMultiPoint ||
                 eGeomToEncodeType == wkbGeometryCollection )
        {
            const OGRGeometryCollection* poGC = poIntersection->toGeometryCollection();
            std::set<std::pair<int,int>> oSetUniqueCoords;
            poGPBFeature->addGeometry(
                GetCmdCountCombined(knCMD_MOVETO, 0) ); // To be modified later
            int nLastX = 0;
            int nLastY = 0;
            for( auto&& poSubGeom: poGC )
            {
                if( wkbFlatten(poSubGeom->getGeometryType()) == wkbPoint )
                {
                    const OGRPoint* poPoint = poSubGeom->toPoint();
                    int nX, nY;
                    double dfX = poPoint->getX();
                    double dfY = poPoint->getY();
                    ConvertToTileCoords(dfX, dfY, nX, nY,
                                            dfTopX, dfTopY, dfTileDim);
                    if( oSetUniqueCoords.find(std::pair<int,int>(nX,nY)) ==
                            oSetUniqueCoords.end() )
                    {
                        oSetUniqueCoords.insert(std::pair<int,int>(nX,nY));

                        int nDiffX = nX - nLastX;
                        int nDiffY = nY - nLastY;
                        poGPBFeature->addGeometry( EncodeSInt(nDiffX) );
                        poGPBFeature->addGeometry( EncodeSInt(nDiffY) );
                        nLastX = nX;
                        nLastY = nY;
                    }
                }
            }
            GUInt32 nPoints = static_cast<GUInt32>(oSetUniqueCoords.size());
            bGeomOK = nPoints > 0;
            poGPBFeature->setGeometry( 0,
                GetCmdCountCombined(knCMD_MOVETO, nPoints) );
        }
    }
    else if( eGeomType == wkbLineString || eGeomType == wkbMultiLineString )
    {
        const bool bWriteLastPoint = true;
        const bool bReverseOrder = false;
        const GUInt32 nMinLineTo = 1;

        if( eGeomToEncodeType == wkbLineString )
        {
            const OGRLineString* poLS = poGeomToEncode->toLineString();
            int nLastX = 0;
            int nLastY = 0;
            OGRLineString oOutLS;
            bGeomOK = EncodeLineString(poGPBFeature.get(), poLS, &oOutLS,
                             bWriteLastPoint, bReverseOrder,
                             nMinLineTo,
                             dfTopX, dfTopY, dfTileDim,
                             nLastX, nLastY);
            dfAreaOrLength = oOutLS.get_Length();
        }
        else if( eGeomToEncodeType == wkbMultiLineString ||
                 eGeomToEncodeType == wkbGeometryCollection )
        {
            const OGRGeometryCollection* poGC = poGeomToEncode->toGeometryCollection();
            int nLastX = 0;
            int nLastY = 0;
            for( auto&& poSubGeom: poGC )
            {
                if( wkbFlatten(poSubGeom->getGeometryType()) == wkbLineString )
                {
                    const OGRLineString* poLS = poSubGeom->toLineString();
                    OGRLineString oOutLS;
                    bool bSubGeomOK = EncodeLineString(
                                     poGPBFeature.get(), poLS, &oOutLS,
                                     bWriteLastPoint, bReverseOrder,
                                     nMinLineTo,
                                     dfTopX, dfTopY, dfTileDim,
                                     nLastX, nLastY);
                    if( bSubGeomOK )
                        dfAreaOrLength += oOutLS.get_Length();
                    bGeomOK |= bSubGeomOK;
                }
            }
        }
    }
    else if( eGeomType == wkbPolygon || eGeomType == wkbMultiPolygon )
    {
#ifdef HAVE_MAKE_VALID
        constexpr bool bCanRecurse = false;
#else
        constexpr bool bCanRecurse = true;
#endif
        if( eGeomToEncodeType == wkbPolygon )
        {
            const OGRPolygon* poPoly = poGeomToEncode->toPolygon();
            int nLastX = 0;
            int nLastY = 0;
            OGRPolygon oOutPoly;
            const GUInt32 nInitialSize = poGPBFeature->getGeometryCount();
            CPL_IGNORE_RET_VAL(nInitialSize);
            bGeomOK = EncodePolygon(poGPBFeature.get(), poPoly, &oOutPoly,
                          dfTopX, dfTopY, dfTileDim,
                          bCanRecurse,
                          nLastX, nLastY, dfAreaOrLength);
#ifdef HAVE_MAKE_VALID
            int bIsValid;
            {
                CPLErrorStateBackuper oErrorStateBackuper;
                CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
                bIsValid = oOutPoly.IsValid();
            }
            if( !bIsValid )
            {
                // Build a valid geometry from the initial MVT geometry and emit it
                std::unique_ptr<OGRGeometry> poPolyValid(oOutPoly.MakeValid());
                if( poPolyValid )
                {
                    poGPBFeature->resizeGeometryArray( nInitialSize );
                    EmitValidPolygon(poPolyValid.get());
                }
            }
#endif
        }
        else if( eGeomToEncodeType == wkbMultiPolygon ||
                 eGeomToEncodeType == wkbGeometryCollection )
        {
            const OGRGeometryCollection* poGC = poGeomToEncode->toGeometryCollection();
            int nLastX = 0;
            int nLastY = 0;
            OGRMultiPolygon oOutMP;
            const GUInt32 nInitialSize = poGPBFeature->getGeometryCount();
            CPL_IGNORE_RET_VAL(nInitialSize);
            for( auto&& poSubGeom: poGC )
            {
                if( wkbFlatten(poSubGeom->getGeometryType()) == wkbPolygon )
                {
                    const OGRPolygon* poPoly = poSubGeom->toPolygon();
                    double dfPartArea = 0.0;
                    auto poOutPoly = cpl::make_unique<OGRPolygon>();
                    bGeomOK |= EncodePolygon(
                                  poGPBFeature.get(), poPoly, poOutPoly.get(),
                                  dfTopX, dfTopY, dfTileDim,
                                  bCanRecurse,
                                  nLastX, nLastY, dfPartArea);
                    dfAreaOrLength += dfPartArea;
                    oOutMP.addGeometryDirectly(poOutPoly.release());
                }
            }
#ifdef HAVE_MAKE_VALID
            int bIsValid;
            {
                CPLErrorStateBackuper oErrorStateBackuper;
                CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
                bIsValid = oOutMP.IsValid();
            }
            if( !bIsValid )
            {
                // Build a valid geometry from the initial MVT geometry and emit it
                std::unique_ptr<OGRGeometry> poMPValid(oOutMP.MakeValid());
                if( poMPValid )
                {
                    poGPBFeature->resizeGeometryArray( nInitialSize );
                    EmitValidPolygon(poMPValid.get());
                }
            }
#endif
        }
    }
    if( !bGeomOK )
        return OGRERR_NONE;

    for( const auto& pair: poFeatureContent->oValues )
    {
        GUInt32 nKey = poLayer->addKey(pair.first);
        GUInt32 nVal = poLayer->addValue(pair.second);
        poGPBFeature->addTag(nKey);
        poGPBFeature->addTag(nVal);
    }
    if( poFeatureContent->nFID >= 0 )
    {
        poGPBFeature->setId( poFeatureContent->nFID );
    }

#ifdef notdef
    {
        MVTTile oTile;
        poLayer->setName("x");
        oTile.addLayer(poLayer);

        CPLString oBuffer(oTile.write());

        VSILFILE* fp = VSIFOpenL( CPLSPrintf("/tmp/%d-%d-%d.pbf",
                                            nZ, nTileX, nTileY),
                                "wb");
        VSIFWriteL(oBuffer.data(), 1, oBuffer.size(), fp);
        VSIFCloseL(fp);
    }
#endif

    // GPB encode the layer with our single feature
    CPLString oBuffer(poLayer->write());

    // Compress buffer
    size_t nCompressedSize = 0;
    void* pCompressed = CPLZLibDeflate( oBuffer.data(), oBuffer.size(), -1,
                                        nullptr, 0, &nCompressedSize);
    oBuffer.assign( static_cast<char*>(pCompressed), nCompressedSize );
    CPLFree(pCompressed);

    if( m_bThreadPoolOK )
        m_oDBMutex.lock();

    m_nTempTiles ++;
    sqlite3_bind_int( m_hInsertStmt, 1, nZ);
    sqlite3_bind_int( m_hInsertStmt, 2, nTileX);
    sqlite3_bind_int( m_hInsertStmt, 3, nTileY);
    sqlite3_bind_text( m_hInsertStmt, 4, osTargetName.c_str(),
                       -1, SQLITE_STATIC );
    sqlite3_bind_int64( m_hInsertStmt, 5, nSerial );
    sqlite3_bind_blob( m_hInsertStmt, 6, oBuffer.data(),
                       static_cast<int>(oBuffer.size()), SQLITE_STATIC);
    sqlite3_bind_int( m_hInsertStmt, 7,
                      static_cast<int>(poGPBFeature->getType()));
    sqlite3_bind_double( m_hInsertStmt, 8, dfAreaOrLength );
    int rc = sqlite3_step( m_hInsertStmt );
    sqlite3_reset( m_hInsertStmt );

    if( m_bThreadPoolOK )
        m_oDBMutex.unlock();

    if( !(rc == SQLITE_OK || rc == SQLITE_DONE) )
    {
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           MVTWriterTask()                            */
/************************************************************************/

class MVTWriterTask
{
    public:
        const OGRMVTWriterDataset* poDS;
        int nZ;
        int nTileX;
        int nTileY;
        CPLString osTargetName;
        bool bIsMaxZoomForLayer;
        std::shared_ptr<OGRMVTFeatureContent> poFeatureContent;
        GIntBig nSerial;
        std::shared_ptr<OGRGeometry> poGeom;
        OGREnvelope sEnvelope;
};

/************************************************************************/
/*                          WriterTaskFunc()                            */
/************************************************************************/

void OGRMVTWriterDataset::WriterTaskFunc(void* pParam)
{
    MVTWriterTask* poTask = static_cast<MVTWriterTask*>(pParam);
    OGRErr eErr = poTask->poDS->PreGenerateForTileReal(
                           poTask->nZ,
                           poTask->nTileX,
                           poTask->nTileY,
                           poTask->osTargetName,
                           poTask->bIsMaxZoomForLayer,
                           poTask->poFeatureContent.get(),
                           poTask->nSerial,
                           poTask->poGeom.get(),
                           poTask->sEnvelope);
    if( eErr != OGRERR_NONE )
    {
        poTask->poDS->m_oDBMutex.lock();
        poTask->poDS->m_bWriteFeatureError = true;
        poTask->poDS->m_oDBMutex.unlock();
    }
    delete poTask;
}

/************************************************************************/
/*                         PreGenerateForTile()                         */
/************************************************************************/

OGRErr OGRMVTWriterDataset::PreGenerateForTile(int nZ, int nTileX, int nTileY,
                                               const CPLString& osTargetName,
                                               bool bIsMaxZoomForLayer,
                                               std::shared_ptr<OGRMVTFeatureContent> poFeatureContent,
                                               GIntBig nSerial,
                                               std::shared_ptr<OGRGeometry> poGeom,
                                               const OGREnvelope& sEnvelope) const
{
    if( !m_bThreadPoolOK )
    {
        return PreGenerateForTileReal(nZ, nTileX, nTileY,
                                      osTargetName,
                                      bIsMaxZoomForLayer,
                                      poFeatureContent.get(),
                                      nSerial,
                                      poGeom.get(), sEnvelope);
    }
    else
    {
        MVTWriterTask* poTask = new MVTWriterTask;
        poTask->poDS = this;
        poTask->nZ = nZ;
        poTask->nTileX = nTileX;
        poTask->nTileY = nTileY;
        poTask->osTargetName = osTargetName;
        poTask->bIsMaxZoomForLayer = bIsMaxZoomForLayer;
        poTask->poFeatureContent = poFeatureContent;
        poTask->nSerial = nSerial;
        poTask->poGeom = poGeom;
        poTask->sEnvelope = sEnvelope;
        m_oThreadPool.SubmitJob(OGRMVTWriterDataset::WriterTaskFunc, poTask);
        // Do not queue more than 1000 jobs to avoid memory exhaustion
        m_oThreadPool.WaitCompletion(1000);

        return m_bWriteFeatureError ? OGRERR_FAILURE : OGRERR_NONE;
    }
}

/************************************************************************/
/*                        UpdateLayerProperties()                       */
/************************************************************************/

void OGRMVTWriterDataset::UpdateLayerProperties(
                                        MVTLayerProperties* poLayerProperties,
                                        const std::string& osKey,
                                        const MVTTileLayerValue& oValue)
{
    auto oFieldIter =
        poLayerProperties->m_oMapFieldNameToIdx.find(osKey);
    MVTFieldProperties* poFieldProps = nullptr;
    if( oFieldIter ==
            poLayerProperties->m_oMapFieldNameToIdx.end() )
    {
        if( poLayerProperties->m_oSetFields.size() < knMAX_COUNT_FIELDS )
        {
            poLayerProperties->m_oSetFields.insert(osKey);
            if( poLayerProperties->m_oMapFieldNameToIdx.size() < knMAX_REPORT_FIELDS )
            {
                MVTFieldProperties oFieldProps;
                oFieldProps.m_osName = osKey;
                if( oValue.isNumeric() )
                {
                    oFieldProps.m_dfMinVal = oValue.getNumericValue();
                    oFieldProps.m_dfMaxVal = oValue.getNumericValue();
                    oFieldProps.m_bAllInt = true; // overridden just below
                }
                oFieldProps.m_eType =
                    oValue.isNumeric() ? MVTTileLayerValue::ValueType::DOUBLE :
                    oValue.isString() ?  MVTTileLayerValue::ValueType::STRING :
                                            MVTTileLayerValue::ValueType::BOOL;

                poLayerProperties->m_oMapFieldNameToIdx[osKey] =
                                        poLayerProperties->m_aoFields.size();
                poLayerProperties->m_aoFields.push_back(oFieldProps);
                poFieldProps = &(poLayerProperties->m_aoFields[
                                    poLayerProperties->m_aoFields.size()-1]);
            }
        }
    }
    else
    {
        poFieldProps = &(poLayerProperties->m_aoFields[oFieldIter->second]);
    }

    if( poFieldProps )
    {
        if( oValue.getType() == MVTTileLayerValue::ValueType::BOOL )
        {
            MVTTileLayerValue oUniqVal;
            oUniqVal.setBoolValue(oValue.getBoolValue());
            poFieldProps->m_oSetAllValues.insert(oUniqVal);
            poFieldProps->m_oSetValues.insert(oUniqVal);
        }
        else if( oValue.isNumeric() )
        {
            if( poFieldProps->m_bAllInt )
            {
                poFieldProps->m_bAllInt =
                    oValue.getType() == MVTTileLayerValue::ValueType::INT ||
                    oValue.getType() == MVTTileLayerValue::ValueType::SINT ||
                    (oValue.getType() == MVTTileLayerValue::ValueType::UINT &&
                    oValue.getUIntValue() < GINT64_MAX);
            }
            double dfVal = oValue.getNumericValue();
            poFieldProps->m_dfMinVal =
                std::min(poFieldProps->m_dfMinVal, dfVal);
            poFieldProps->m_dfMaxVal =
                std::max(poFieldProps->m_dfMaxVal, dfVal);
            if( poFieldProps->m_oSetAllValues.size() < knMAX_COUNT_VALUES )
            {
                MVTTileLayerValue oUniqVal;
                oUniqVal.setDoubleValue(dfVal);
                poFieldProps->m_oSetAllValues.insert(oUniqVal);
                if( poFieldProps->m_oSetValues.size() < knMAX_REPORT_VALUES )
                {
                    poFieldProps->m_oSetValues.insert(oUniqVal);
                }
            }
        }
        else if( oValue.isString() &&
                 poFieldProps->m_oSetAllValues.size() < knMAX_COUNT_VALUES )
        {
            auto osVal = oValue.getStringValue();
            MVTTileLayerValue oUniqVal;
            oUniqVal.setStringValue(osVal);
            poFieldProps->m_oSetAllValues.insert(oUniqVal);
            if( osVal.size() <= knMAX_STRING_VALUE_LENGTH &&
                poFieldProps->m_oSetValues.size() < knMAX_REPORT_VALUES )
            {
                poFieldProps->m_oSetValues.insert(oUniqVal);
            }
        }
    }
}

/************************************************************************/
/*                           GZIPCompress()                             */
/************************************************************************/

static void GZIPCompress(std::string& oTileBuffer)
{
    if( !oTileBuffer.empty() )
    {
        CPLString osTmpFilename(CPLSPrintf("/vsimem/%p.gz", &oTileBuffer));
        CPLString osTmpGZipFilename("/vsigzip/" + osTmpFilename);
        VSILFILE* fpGZip = VSIFOpenL(osTmpGZipFilename, "wb");
        if( fpGZip )
        {
            VSIFWriteL( oTileBuffer.data(), 1, oTileBuffer.size(), fpGZip );
            VSIFCloseL(fpGZip);

            vsi_l_offset nCompressedSize = 0;
            GByte* pabyCompressed = VSIGetMemFileBuffer(
                osTmpFilename, &nCompressedSize, false);
            oTileBuffer.assign( reinterpret_cast<char*>(pabyCompressed),
                                static_cast<size_t>(nCompressedSize) );
        }
        VSIUnlink(osTmpFilename);
    }
}

/************************************************************************/
/*                     GetReducedPrecisionGeometry()                    */
/************************************************************************/

static std::vector<GUInt32> GetReducedPrecisionGeometry(
                                    MVTTileLayerFeature::GeomType eGeomType,
                                    const std::vector<GUInt32>& anSrcGeometry,
                                    GUInt32 nSrcExtent,
                                    GUInt32 nDstExtent)
{
    std::vector<GUInt32> anDstGeometry;
    size_t nLastMoveToIdx = 0;
    int nX = 0;
    int nY = 0;
    int nFirstReducedX = 0;
    int nFirstReducedY = 0;
    int nLastReducedX = 0;
    int nLastReducedY = 0;
    int nLastReducedXValid = 0;
    int nLastReducedYValid = 0;
    std::unique_ptr<OGRLinearRing> poInRing;
    std::unique_ptr<OGRLinearRing> poOutRing;
    std::unique_ptr<OGRLinearRing> poOutOuterRing;
    bool bDiscardInnerRings = false;
    const bool bIsPoly = eGeomType == MVTTileLayerFeature::GeomType::POLYGON;
    for( size_t iSrc = 0; iSrc < anSrcGeometry.size(); )
    {
        const unsigned nCount = GetCmdCount(anSrcGeometry[iSrc]);
        switch( GetCmdId(anSrcGeometry[iSrc]) )
        {
            case knCMD_MOVETO:
            {
                nLastMoveToIdx = anDstGeometry.size();

                anDstGeometry.push_back( anSrcGeometry[iSrc] );
                iSrc ++;

                unsigned nDstPoints = 0;
                for( unsigned j = 0; iSrc + 1 < anSrcGeometry.size() &&
                            j < nCount; j++, iSrc += 2 )
                {
                    nX += DecodeSInt(anSrcGeometry[iSrc]);
                    nY += DecodeSInt(anSrcGeometry[iSrc+1]);

                    int nReducedX = static_cast<int>(
                        static_cast<GIntBig>(nX) * nDstExtent / nSrcExtent);
                    int nReducedY = static_cast<int>(
                        static_cast<GIntBig>(nY) * nDstExtent / nSrcExtent);
                    int nDiffX = nReducedX - nLastReducedX;
                    int nDiffY = nReducedY - nLastReducedY;
                    if( j == 0 )
                    {
                        if( bIsPoly )
                        {
                            poInRing = std::unique_ptr<OGRLinearRing>(
                                                        new OGRLinearRing());
                            poOutRing = std::unique_ptr<OGRLinearRing>(
                                                        new OGRLinearRing());
                        }
                        nFirstReducedX = nReducedX;
                        nFirstReducedY = nReducedY;
                    }
                    if( j == 0 || nDiffX != 0 || nDiffY != 0 )
                    {
                        if( bIsPoly )
                        {
                            poInRing->addPoint( nX, nY );
                            poOutRing->addPoint( nReducedX, nReducedY );
                        }
                        nDstPoints ++;
                        anDstGeometry.push_back(EncodeSInt(nDiffX));
                        anDstGeometry.push_back(EncodeSInt(nDiffY));
                        nLastReducedX = nReducedX;
                        nLastReducedY = nReducedY;
                    }
                }
                // Patch count of MOVETO
                anDstGeometry[nLastMoveToIdx] = GetCmdCountCombined(
                    GetCmdId(anDstGeometry[nLastMoveToIdx]),
                    nDstPoints);
                break;
            }
            case knCMD_LINETO:
            {
                size_t nIdxToPatch = anDstGeometry.size();
                anDstGeometry.push_back( anSrcGeometry[iSrc] );
                iSrc ++;
                unsigned nDstPoints = 0;
                int nLastReducedXBefore = nLastReducedX;
                int nLastReducedYBefore = nLastReducedY;
                for( unsigned j = 0; iSrc + 1 < anSrcGeometry.size() &&
                                j < nCount; j++, iSrc += 2 )
                {
                    nX += DecodeSInt(anSrcGeometry[iSrc]);
                    nY += DecodeSInt(anSrcGeometry[iSrc+1]);

                    int nReducedX = static_cast<int>(
                        static_cast<GIntBig>(nX) * nDstExtent / nSrcExtent);
                    int nReducedY = static_cast<int>(
                        static_cast<GIntBig>(nY) * nDstExtent / nSrcExtent);
                    int nDiffX = nReducedX - nLastReducedX;
                    int nDiffY = nReducedY - nLastReducedY;
                    if( nDiffX != 0 || nDiffY != 0 )
                    {
                        if( bIsPoly )
                        {
                            CPLAssert( poInRing );
                            CPLAssert( poOutRing );
                            poInRing->addPoint( nX, nY );
                            poOutRing->addPoint( nReducedX, nReducedY );
                        }
                        nDstPoints ++;
                        anDstGeometry.push_back(EncodeSInt(nDiffX));
                        anDstGeometry.push_back(EncodeSInt(nDiffY));
                        nLastReducedXBefore = nLastReducedX;
                        nLastReducedYBefore = nLastReducedY;
                        nLastReducedX = nReducedX;
                        nLastReducedY = nReducedY;
                    }
                }

                // If last point of ring is identical to first one, discard it
                if( nDstPoints > 0 &&
                    bIsPoly &&
                    nLastReducedX == nFirstReducedX &&
                    nLastReducedY == nFirstReducedY )
                {
                    nLastReducedX = nLastReducedXBefore;
                    nLastReducedY = nLastReducedYBefore;
                    nDstPoints -= 1;
                    anDstGeometry.resize( anDstGeometry.size() - 2 );
                    poOutRing->setNumPoints( poOutRing->getNumPoints() - 1 );
                }

                // Patch count of LINETO
                anDstGeometry[nIdxToPatch] = GetCmdCountCombined(
                    GetCmdId(anDstGeometry[nIdxToPatch]),
                    nDstPoints);

                // A valid linestring should have at least one MOVETO +
                // one coord pair + one LINETO + one coord pair
                if( eGeomType == MVTTileLayerFeature::GeomType::LINESTRING )
                {
                    if( anDstGeometry.size() < nLastMoveToIdx + 1 + 2 + 1 + 2 )
                    {
                        // Remove last linestring
                        nLastReducedX = nLastReducedXValid;
                        nLastReducedY = nLastReducedYValid;
                        anDstGeometry.resize( nLastMoveToIdx );
                    }
                    else
                    {
                        nLastReducedXValid = nLastReducedX;
                        nLastReducedYValid = nLastReducedY;
                    }
                }

                break;
            }
            case knCMD_CLOSEPATH:
            {
                CPLAssert( bIsPoly );
                CPLAssert( poInRing );
                CPLAssert( poOutRing );
                int bIsValid = true;

                // A valid ring should have at least one MOVETO + one
                // coord pair + one LINETO + two coord pairs
                if( anDstGeometry.size() < nLastMoveToIdx + 1 + 2 + 1 + 2 * 2 )
                {
                    // Remove ring. Normally if we remove an outer ring,
                    // its inner rings should also be removed, given they are
                    // smaller than the outer ring.
                    bIsValid = false;
                }
                else
                {
                    poInRing->closeRings();
                    poOutRing->closeRings();
                    bool bIsOuterRing = !poInRing->isClockwise();
                    // Normally the first ring of a polygon geometry should
                    // be a outer ring, except when it is degenerate enough
                    // in which case poOutOuterRing might be null.
                    if( bIsOuterRing )
                    {
                        // if the outer ring turned out to be a inner ring
                        // once reduced
                        if( poOutRing->isClockwise() )
                        {
                            bIsValid = false;
                            bDiscardInnerRings = true;
                        }
                        else
                        {
                            OGRPolygon oPoly;
                            oPoly.addRing( poOutRing.get() );
                            poOutOuterRing = std::unique_ptr<OGRLinearRing>(
                                                            poOutRing.release());
                            {
                                CPLErrorStateBackuper oErrorStateBackuper;
                                CPLErrorHandlerPusher
                                                oErrorHandler(CPLQuietErrorHandler);
                                bIsValid = oPoly.IsValid();
                            }
                            bDiscardInnerRings = !bIsValid;
                        }
                    }
                    else if( bDiscardInnerRings ||
                             poOutOuterRing.get() == nullptr ||
                             // if the inner ring turned out to be a outer ring
                             // once reduced
                             !poOutRing->isClockwise()  )
                    {
                        bIsValid = false;
                    }
                    else
                    {
                        OGRPolygon oPoly;
                        oPoly.addRing( poOutOuterRing.get() );
                        oPoly.addRingDirectly( poOutRing.release() );
                        {
                            CPLErrorStateBackuper oErrorStateBackuper;
                            CPLErrorHandlerPusher
                                            oErrorHandler(CPLQuietErrorHandler);
                            bIsValid = oPoly.IsValid();
                        }
                    }
                }

                if( bIsValid )
                {
                    nLastReducedXValid = nLastReducedX;
                    nLastReducedYValid = nLastReducedY;
                    anDstGeometry.push_back( anSrcGeometry[iSrc] );
                }
                else
                {
                    // Remove this ring
                    nLastReducedX = nLastReducedXValid;
                    nLastReducedY = nLastReducedYValid;
                    anDstGeometry.resize( nLastMoveToIdx );
                }

                iSrc ++;
                break;
            }
            default:
            {
                CPLAssert(false);
                break;
            }
        }
    }

    return anDstGeometry;
}

/************************************************************************/
/*                          EncodeFeature()                             */
/************************************************************************/

void OGRMVTWriterDataset::EncodeFeature(
                        const void* pabyBlob,
                        int nBlobSize,
                        std::shared_ptr<MVTTileLayer> poTargetLayer,
                        std::map<CPLString, GUInt32>& oMapKeyToIdx,
                        std::map<MVTTileLayerValue, GUInt32>& oMapValueToIdx,
                        MVTLayerProperties* poLayerProperties,
                        GUInt32 nExtent,
                        unsigned& nFeaturesInTile)
{
    size_t nUncompressedSize = 0;
    void* pCompressed = CPLZLibInflate( pabyBlob, nBlobSize,
                                        nullptr, 0,
                                        &nUncompressedSize);
    GByte* pabyUncompressed = static_cast<GByte*>(pCompressed);

    MVTTileLayer oSrcTileLayer;
    if( nUncompressedSize &&
        oSrcTileLayer.read(pabyUncompressed,
                            pabyUncompressed + nUncompressedSize) )
    {
        const auto& srcFeatures = oSrcTileLayer.getFeatures();
        if( srcFeatures.size() == 1 ) // should always be true !
        {
            const auto& poSrcFeature = srcFeatures[0];
            std::shared_ptr<MVTTileLayerFeature> poFeature(
                new MVTTileLayerFeature());

            if( poSrcFeature->hasId() )
                poFeature->setId(poSrcFeature->getId());
            poFeature->setType(poSrcFeature->getType());
            if( poLayerProperties )
            {
                poLayerProperties->
                    m_oCountGeomType[poSrcFeature->getType()] ++;
            }
            bool bOK = true;
            if( nExtent < m_nExtent )
            {
#ifdef for_debugging
                const auto& srcKeys = oSrcTileLayer.getKeys();
                const auto& srcValues = oSrcTileLayer.getValues();
                const auto& anSrcTags = poSrcFeature->getTags();
                for( size_t i = 0; i + 1 < anSrcTags.size(); i += 2 )
                {
                    GUInt32 nSrcIdxKey = anSrcTags[i];
                    GUInt32 nSrcIdxValue = anSrcTags[i+1];
                    if( nSrcIdxKey < srcKeys.size() &&
                        nSrcIdxValue < srcValues.size() )
                    {
                        auto& osKey = srcKeys[nSrcIdxKey];
                        auto& oValue = srcValues[nSrcIdxValue];
                        if( osKey == "tunnus" &&
                            oValue.getUIntValue() == 28799760 )
                        {
                            printf("foo\n"); /* ok */
                            break;
                        }
                    }
                }
#endif

                poFeature->setGeometry(
                    GetReducedPrecisionGeometry(poSrcFeature->getType(),
                                                poSrcFeature->getGeometry(),
                                                m_nExtent, nExtent));
                if( poFeature->getGeometry().empty() )
                {
                    bOK = false;
                }
            }
            else
            {
                poFeature->setGeometry(poSrcFeature->getGeometry());
            }
            if( bOK )
            {
                const auto& srcKeys = oSrcTileLayer.getKeys();
                for( const auto& osKey: srcKeys )
                {
                    auto oIter = oMapKeyToIdx.find(osKey);
                    if( oIter == oMapKeyToIdx.end() )
                    {
                        oMapKeyToIdx[osKey] = poTargetLayer->addKey(osKey);
                    }
                }

                const auto& srcValues = oSrcTileLayer.getValues();
                for( const auto& oValue: srcValues )
                {
                    auto oIter = oMapValueToIdx.find(oValue);
                    if( oIter == oMapValueToIdx.end() )
                    {
                        oMapValueToIdx[oValue] =
                            poTargetLayer->addValue(oValue);
                    }
                }

                const auto& anSrcTags = poSrcFeature->getTags();
                for( size_t i = 0; i + 1 < anSrcTags.size(); i += 2 )
                {
                    GUInt32 nSrcIdxKey = anSrcTags[i];
                    GUInt32 nSrcIdxValue = anSrcTags[i+1];
                    if( nSrcIdxKey < srcKeys.size() &&
                        nSrcIdxValue < srcValues.size() )
                    {
                        auto& osKey = srcKeys[nSrcIdxKey];
                        auto& oValue = srcValues[nSrcIdxValue];

                        if( poLayerProperties )
                        {
                            UpdateLayerProperties(poLayerProperties,
                                                    osKey,
                                                    oValue);
                        }

                        poFeature->addTag(oMapKeyToIdx[osKey]);
                        poFeature->addTag(oMapValueToIdx[oValue]);
                    }
                }

                nFeaturesInTile ++;
                poTargetLayer->addFeature(poFeature);
            }
        }
    }
    else
    {
        // Shouldn't fail
        CPLError(CE_Failure, CPLE_AppDefined,
                "Deserialization failure");
    }

    CPLFree(pabyUncompressed);
}

/************************************************************************/
/*                            EncodeTile()                              */
/************************************************************************/

std::string OGRMVTWriterDataset::EncodeTile(
                        int nZ, int nX, int nY,
                        sqlite3_stmt* hStmtLayer,
                        sqlite3_stmt* hStmtRows,
                        std::map<CPLString, MVTLayerProperties>& oMapLayerProps,
                        std::set<CPLString>& oSetLayers,
                        GIntBig& nTempTilesRead)
{
    MVTTile oTargetTile;

    sqlite3_bind_int(hStmtLayer, 1, nZ);
    sqlite3_bind_int(hStmtLayer, 2, nX);
    sqlite3_bind_int(hStmtLayer, 3, nY);

    unsigned nFeaturesInTile = 0;
    const GIntBig nProgressStep = std::max( static_cast<GIntBig>(1),
                                            m_nTempTiles / 10 );

    while( nFeaturesInTile < m_nMaxFeatures &&
           sqlite3_step(hStmtLayer) == SQLITE_ROW )
    {
        const char* pszLayerName = reinterpret_cast<const char*>(
            sqlite3_column_text(hStmtLayer, 0));
        sqlite3_bind_int(hStmtRows, 1, nZ);
        sqlite3_bind_int(hStmtRows, 2, nX);
        sqlite3_bind_int(hStmtRows, 3, nY);
        sqlite3_bind_text(hStmtRows, 4, pszLayerName, -1, SQLITE_STATIC);

        auto oIterMapLayerProps = oMapLayerProps.find(pszLayerName);
        MVTLayerProperties* poLayerProperties = nullptr;
        if( oIterMapLayerProps == oMapLayerProps.end() )
        {
            if( oSetLayers.size() < knMAX_COUNT_LAYERS )
            {
                oSetLayers.insert(pszLayerName);
                if( oMapLayerProps.size() < knMAX_REPORT_LAYERS )
                {
                    MVTLayerProperties props;
                    props.m_nMinZoom = nZ;
                    props.m_nMaxZoom = nZ;
                    oMapLayerProps[pszLayerName] = props;
                    poLayerProperties = &(oMapLayerProps[pszLayerName]);
                }
            }
        }
        else
        {
            poLayerProperties = &(oIterMapLayerProps->second);
        }
        if( poLayerProperties )
        {
            poLayerProperties->m_nMinZoom =
                std::min(nZ, poLayerProperties->m_nMinZoom);
            poLayerProperties->m_nMaxZoom =
                std::max(nZ, poLayerProperties->m_nMaxZoom);
        }

        std::shared_ptr<MVTTileLayer> poTargetLayer(new MVTTileLayer());
        oTargetTile.addLayer(poTargetLayer);
        poTargetLayer->setName(pszLayerName);
        poTargetLayer->setVersion(m_nMVTVersion);
        poTargetLayer->setExtent(m_nExtent);

        std::map<CPLString, GUInt32> oMapKeyToIdx;
        std::map<MVTTileLayerValue, GUInt32> oMapValueToIdx;

        while( nFeaturesInTile < m_nMaxFeatures &&
               sqlite3_step(hStmtRows) == SQLITE_ROW )
        {
            int nBlobSize = sqlite3_column_bytes(hStmtRows, 0);
            const void* pabyBlob = sqlite3_column_blob(hStmtRows, 0);

            EncodeFeature(pabyBlob, nBlobSize, poTargetLayer,
                          oMapKeyToIdx, oMapValueToIdx,
                          poLayerProperties, m_nExtent, nFeaturesInTile);

            nTempTilesRead ++;
            if( nTempTilesRead == m_nTempTiles||
                (nTempTilesRead % nProgressStep) == 0 )
            {
                const int nPct = static_cast<int>(
                                    (100 * nTempTilesRead) / m_nTempTiles);
                CPLDebug("MVT", "%d%%...", nPct);
            }
        }
        sqlite3_reset(hStmtRows);
    }

    sqlite3_reset(hStmtLayer);

    std::string oTileBuffer(oTargetTile.write());
    size_t nSizeBefore = oTileBuffer.size();
    if( m_bGZip)
        GZIPCompress(oTileBuffer);
    const size_t nSizeAfter = oTileBuffer.size();
    const double dfCompressionRatio =
        static_cast<double>(nSizeAfter) / nSizeBefore;

    // If the tile size is above the allowed values or there are too many
    // features, then sort by descending area / length until we get to the
    // limit.
    bool bTooBigTile = oTileBuffer.size() > m_nMaxTileSize;
    const bool bTooManyFeatures = nFeaturesInTile >= m_nMaxFeatures;

    GUInt32 nExtent = m_nExtent;
    while( bTooBigTile && !bTooManyFeatures && nExtent >= 256 )
    {
        nExtent /= 2;
        nSizeBefore = oTileBuffer.size();
        oTileBuffer = RecodeTileLowerResolution(nZ, nX, nY, nExtent,
                                                hStmtLayer, hStmtRows);
        bTooBigTile = oTileBuffer.size() > m_nMaxTileSize;
        CPLDebug("MVT", "Recoding tile %d/%d/%d with extent = %u. "
                 "From %u to %u bytes",
                 nZ, nX, nY, nExtent,
                 static_cast<unsigned>(nSizeBefore),
                 static_cast<unsigned>(oTileBuffer.size()));
    }

    if( bTooBigTile || bTooManyFeatures )
    {
        if( bTooBigTile )
        {
            CPLDebug("MVT", "For tile %d/%d/%d, tile size is %u > %u",
                     nZ, nX, nY,
                     static_cast<unsigned>(oTileBuffer.size()),
                     m_nMaxTileSize);
        }
        if( bTooManyFeatures )
        {
            CPLDebug("MVT",
                     "For tile %d/%d/%d, feature count limit of %u is reached",
                     nZ, nX, nY,
                     m_nMaxFeatures);
        }

        oTargetTile.clear();

        const unsigned nTotalFeaturesInTile =
                                std::min(m_nMaxFeatures, nFeaturesInTile);
        char* pszSQL = sqlite3_mprintf(
            "SELECT layer, feature FROM temp "
            "WHERE z = %d AND x = %d AND y = %d ORDER BY "
            "area_or_length DESC LIMIT %d",
            nZ, nX, nY, nTotalFeaturesInTile);
        sqlite3_stmt* hTmpStmt = nullptr;
        CPL_IGNORE_RET_VAL(
            sqlite3_prepare_v2( m_hDB, pszSQL, -1, &hTmpStmt, nullptr) );
        sqlite3_free(pszSQL);
        if( !hTmpStmt )
            return std::string();

        class TargetTileLayerProps
        {
            public:
                std::shared_ptr<MVTTileLayer> m_poLayer;
                std::map<CPLString, GUInt32> m_oMapKeyToIdx;
                std::map<MVTTileLayerValue, GUInt32> m_oMapValueToIdx;
        };

        std::map<std::string, TargetTileLayerProps>
                                        oMapLayerNameToTargetLayer;

        nFeaturesInTile = 0;
        const unsigned nCheckStep = std::max(1U, nTotalFeaturesInTile / 100);
        while( sqlite3_step(hTmpStmt) == SQLITE_ROW )
        {
            const char* pszLayerName = reinterpret_cast<const char*>(
                sqlite3_column_text(hTmpStmt, 0));
            int nBlobSize = sqlite3_column_bytes(hTmpStmt, 1);
            const void* pabyBlob = sqlite3_column_blob(hTmpStmt, 1);

            std::shared_ptr<MVTTileLayer> poTargetLayer;
            std::map<CPLString, GUInt32>* poMapKeyToIdx;
            std::map<MVTTileLayerValue, GUInt32>* poMapValueToIdx;
            auto oIter = oMapLayerNameToTargetLayer.find(pszLayerName);
            if( oIter == oMapLayerNameToTargetLayer.end() )
            {
                poTargetLayer =
                    std::shared_ptr<MVTTileLayer>(new MVTTileLayer());
                TargetTileLayerProps props;
                props.m_poLayer = poTargetLayer;
                oTargetTile.addLayer(poTargetLayer);
                poTargetLayer->setName(pszLayerName);
                poTargetLayer->setVersion(m_nMVTVersion);
                poTargetLayer->setExtent(nExtent);
                oMapLayerNameToTargetLayer[pszLayerName] = props;
                poMapKeyToIdx =
                    &oMapLayerNameToTargetLayer[pszLayerName].m_oMapKeyToIdx;
                poMapValueToIdx =
                    &oMapLayerNameToTargetLayer[pszLayerName].m_oMapValueToIdx;
            }
            else
            {
                poTargetLayer = oIter->second.m_poLayer;
                poMapKeyToIdx = &oIter->second.m_oMapKeyToIdx;
                poMapValueToIdx = &oIter->second.m_oMapValueToIdx;
            }

            EncodeFeature(pabyBlob, nBlobSize, poTargetLayer,
                          *poMapKeyToIdx, *poMapValueToIdx,
                          nullptr, nExtent, nFeaturesInTile);

            if( nFeaturesInTile == nTotalFeaturesInTile ||
                (bTooBigTile && (nFeaturesInTile % nCheckStep == 0)) )
            {
                if( oTargetTile.getSize() *
                        dfCompressionRatio > m_nMaxTileSize )
                {
                    break;
                }
            }
        }

        oTileBuffer = oTargetTile.write();
        if( m_bGZip)
            GZIPCompress(oTileBuffer);

        if( bTooBigTile )
        {
            CPLDebug("MVT", "For tile %d/%d/%d, final tile size is %u",
                     nZ, nX, nY,
                     static_cast<unsigned>(oTileBuffer.size()));
        }

        sqlite3_finalize(hTmpStmt);
    }

    return oTileBuffer;
}

/************************************************************************/
/*                    RecodeTileLowerResolution()                       */
/************************************************************************/

std::string OGRMVTWriterDataset::RecodeTileLowerResolution(
                                            int nZ, int nX, int nY,
                                            int nExtent,
                                            sqlite3_stmt* hStmtLayer,
                                            sqlite3_stmt* hStmtRows)
{
    MVTTile oTargetTile;

    sqlite3_bind_int(hStmtLayer, 1, nZ);
    sqlite3_bind_int(hStmtLayer, 2, nX);
    sqlite3_bind_int(hStmtLayer, 3, nY);

    unsigned nFeaturesInTile = 0;
    while( nFeaturesInTile < m_nMaxFeatures &&
           sqlite3_step(hStmtLayer) == SQLITE_ROW )
    {
        const char* pszLayerName = reinterpret_cast<const char*>(
            sqlite3_column_text(hStmtLayer, 0));
        sqlite3_bind_int(hStmtRows, 1, nZ);
        sqlite3_bind_int(hStmtRows, 2, nX);
        sqlite3_bind_int(hStmtRows, 3, nY);
        sqlite3_bind_text(hStmtRows, 4, pszLayerName, -1, SQLITE_STATIC);

        std::shared_ptr<MVTTileLayer> poTargetLayer(new MVTTileLayer());
        oTargetTile.addLayer(poTargetLayer);
        poTargetLayer->setName(pszLayerName);
        poTargetLayer->setVersion(m_nMVTVersion);
        poTargetLayer->setExtent(nExtent);

        std::map<CPLString, GUInt32> oMapKeyToIdx;
        std::map<MVTTileLayerValue, GUInt32> oMapValueToIdx;

        while( nFeaturesInTile < m_nMaxFeatures &&
               sqlite3_step(hStmtRows) == SQLITE_ROW )
        {
            int nBlobSize = sqlite3_column_bytes(hStmtRows, 0);
            const void* pabyBlob = sqlite3_column_blob(hStmtRows, 0);

            EncodeFeature(pabyBlob, nBlobSize, poTargetLayer,
                          oMapKeyToIdx, oMapValueToIdx,
                          nullptr, nExtent, nFeaturesInTile);

        }
        sqlite3_reset(hStmtRows);
    }

    sqlite3_reset(hStmtLayer);

    std::string oTileBuffer(oTargetTile.write());
    if( m_bGZip)
        GZIPCompress(oTileBuffer);

    return oTileBuffer;
}

/************************************************************************/
/*                            CreateOutput()                            */
/************************************************************************/

bool OGRMVTWriterDataset::CreateOutput()
{
    if( m_bThreadPoolOK )
        m_oThreadPool.WaitCompletion();

    std::map<CPLString, MVTLayerProperties> oMapLayerProps;
    std::set<CPLString> oSetLayers;

    if( !m_oEnvelope.IsInit() )
    {
        return GenerateMetadata(0, oMapLayerProps);
    }

    CPLDebug("MVT", "Building output file from temporary database...");

    sqlite3_stmt* hStmtZXY = nullptr;
    CPL_IGNORE_RET_VAL(
        sqlite3_prepare_v2( m_hDB,
            "SELECT DISTINCT z, x, y FROM temp ORDER BY z, x, y",
            -1, &hStmtZXY, nullptr) );
    if( hStmtZXY == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Prepared statement failed");
        return false;
    }

    sqlite3_stmt* hStmtLayer = nullptr;
    CPL_IGNORE_RET_VAL(
        sqlite3_prepare_v2( m_hDB,
            "SELECT DISTINCT layer FROM temp "
            "WHERE z = ? AND x = ? AND y = ? ORDER BY layer",
            -1, &hStmtLayer, nullptr) );
    if( hStmtLayer == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Prepared statement failed");
        sqlite3_finalize(hStmtZXY);
        return false;
    }
    sqlite3_stmt* hStmtRows = nullptr;
    CPL_IGNORE_RET_VAL(
        sqlite3_prepare_v2( m_hDB,
            "SELECT feature FROM temp "
            "WHERE z = ? AND x = ? AND y = ? AND layer = ? ORDER BY idx",
            -1, &hStmtRows, nullptr) );
    if( hStmtRows == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Prepared statement failed");
        sqlite3_finalize(hStmtZXY);
        sqlite3_finalize(hStmtLayer);
        return false;
    }

    sqlite3_stmt* hInsertStmt = nullptr;
    if( m_hDBMBTILES )
    {
        CPL_IGNORE_RET_VAL(
            sqlite3_prepare_v2( m_hDBMBTILES,
                "INSERT INTO tiles(zoom_level, tile_column, tile_row, "
                "tile_data) VALUES (?,?,?,?)",
                -1, &hInsertStmt, nullptr) );
        if( hInsertStmt == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Prepared statement failed");
            sqlite3_finalize(hStmtZXY);
            sqlite3_finalize(hStmtLayer);
            sqlite3_finalize(hStmtRows);
            return false;
        }
    }

    int nLastZ = -1;
    int nLastX = -1;
    bool bRet = true;
    GIntBig nTempTilesRead = 0;

    while( sqlite3_step(hStmtZXY) == SQLITE_ROW )
    {
        int nZ = sqlite3_column_int(hStmtZXY, 0);
        int nX = sqlite3_column_int(hStmtZXY, 1);
        int nY = sqlite3_column_int(hStmtZXY, 2);

        std::string oTileBuffer(
            EncodeTile(nZ, nX, nY,
                       hStmtLayer,
                       hStmtRows,
                       oMapLayerProps,
                       oSetLayers,
                       nTempTilesRead));

        if( oTileBuffer.empty() )
        {
            bRet = false;
        }
        else if( hInsertStmt )
        {
            sqlite3_bind_int(hInsertStmt, 1, nZ);
            sqlite3_bind_int(hInsertStmt, 2, nX);
            sqlite3_bind_int(hInsertStmt, 3, (1 << nZ) - 1 - nY);
            sqlite3_bind_blob(hInsertStmt, 4, oTileBuffer.data(),
                              static_cast<int>(oTileBuffer.size()),
                              SQLITE_STATIC);
            const int rc = sqlite3_step(hInsertStmt);
            bRet = (rc == SQLITE_OK || rc == SQLITE_DONE);
            sqlite3_reset(hInsertStmt);
        }
        else
        {
            CPLString osZDirname(
                CPLFormFilename(GetDescription(),
                                CPLSPrintf("%d", nZ), nullptr));
            CPLString osXDirname(
                CPLFormFilename(osZDirname, CPLSPrintf("%d", nX), nullptr));
            if( nZ != nLastZ )
            {
                VSIMkdir( osZDirname, 0755 );
                nLastZ = nZ;
                nLastX = -1;
            }
            if( nX != nLastX )
            {
                VSIMkdir( osXDirname, 0755 );
                nLastX = nX;
            }
            CPLString osTileFilename(
                CPLFormFilename(osXDirname, CPLSPrintf("%d", nY),
                                m_osExtension.c_str()));
            VSILFILE* fpOut = VSIFOpenL( osTileFilename, "wb" );
            if( fpOut )
            {
                const size_t nRet = VSIFWriteL(oTileBuffer.data(), 1,
                                        oTileBuffer.size(), fpOut );
                bRet = (nRet == oTileBuffer.size());
                VSIFCloseL(fpOut);
            }
            else
            {
                bRet = false;
            }
        }

        if( !bRet )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Error while writing tile %d/%d/%d", nZ, nX, nY);
            break;
        }
    }
    sqlite3_finalize(hStmtZXY);
    sqlite3_finalize(hStmtLayer);
    sqlite3_finalize(hStmtRows);
    if( hInsertStmt )
        sqlite3_finalize(hInsertStmt);

    bRet &= GenerateMetadata(oSetLayers.size(), oMapLayerProps);

    return bRet;
}


/************************************************************************/
/*                     SphericalMercatorToLongLat()                     */
/************************************************************************/

static void SphericalMercatorToLongLat(double* x, double* y)
{
  double lng = *x / kmSPHERICAL_RADIUS / M_PI * 180;
  double lat = 2 * (atan(exp(*y / kmSPHERICAL_RADIUS)) - M_PI / 4) / M_PI * 180;
  *x = lng;
  *y = lat;
}

/************************************************************************/
/*                          WriteMetadataItem()                         */
/************************************************************************/

template<class T> static
bool WriteMetadataItemT(const char* pszKey, T value, const char* pszValueFormat,
                        sqlite3* hDBMBTILES, CPLJSONObject& oRoot)
{
    if( hDBMBTILES )
    {
        char* pszSQL;

        pszSQL = sqlite3_mprintf(
            CPLSPrintf("INSERT INTO metadata(name, value) VALUES('%%q', '%s')",
                       pszValueFormat),
            pszKey, value);
        OGRErr eErr = SQLCommand( hDBMBTILES, pszSQL);
        sqlite3_free(pszSQL);
        return eErr == OGRERR_NONE;
    }
    else
    {
        oRoot.Add(pszKey, value);
        return true;
    }
}


/************************************************************************/
/*                          WriteMetadataItem()                         */
/************************************************************************/

static
bool WriteMetadataItem(const char* pszKey, const char* pszValue,
                       sqlite3* hDBMBTILES, CPLJSONObject& oRoot)
{
    return WriteMetadataItemT(pszKey, pszValue, "%q", hDBMBTILES, oRoot);
}

/************************************************************************/
/*                          WriteMetadataItem()                         */
/************************************************************************/

static
bool WriteMetadataItem(const char* pszKey, int nValue,
                       sqlite3* hDBMBTILES, CPLJSONObject& oRoot)
{
    return WriteMetadataItemT(pszKey, nValue, "%d", hDBMBTILES, oRoot);
}

/************************************************************************/
/*                          WriteMetadataItem()                         */
/************************************************************************/

static
bool WriteMetadataItem(const char* pszKey, double dfValue,
                       sqlite3* hDBMBTILES, CPLJSONObject& oRoot)
{
    return WriteMetadataItemT(pszKey, dfValue, "%.18g", hDBMBTILES, oRoot);
}

/************************************************************************/
/*                          GenerateMetadata()                          */
/************************************************************************/

bool OGRMVTWriterDataset::GenerateMetadata(
    size_t nLayers, const std::map<CPLString, MVTLayerProperties>& oMap)
{
    CPLJSONDocument oDoc;
    CPLJSONObject oRoot = oDoc.GetRoot();

    OGRSpatialReference oSRS_EPSG3857;
    double dfTopXWebMercator;
    double dfTopYWebMercator;
    double dfTileDim0WebMercator;
    InitWebMercatorTilingScheme(&oSRS_EPSG3857,
                                dfTopXWebMercator,
                                dfTopYWebMercator,
                                dfTileDim0WebMercator);
    const bool bIsStandardTilingScheme =
        m_poSRS->IsSame(&oSRS_EPSG3857) &&
        m_dfTopX == dfTopXWebMercator &&
        m_dfTopY == dfTopYWebMercator &&
        m_dfTileDim0 == dfTileDim0WebMercator;
    if( bIsStandardTilingScheme )
    {
        SphericalMercatorToLongLat(&(m_oEnvelope.MinX),&(m_oEnvelope.MinY));
        SphericalMercatorToLongLat(&(m_oEnvelope.MaxX),&(m_oEnvelope.MaxY));
        m_oEnvelope.MinY = std::max(-85.0, m_oEnvelope.MinY);
        m_oEnvelope.MaxY = std::min(85.0, m_oEnvelope.MaxY);
    }
    else
    {
        OGRSpatialReference oSRS_EPSG4326;
        oSRS_EPSG4326.SetFromUserInput(SRS_WKT_WGS84_LAT_LONG);
        oSRS_EPSG4326.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        OGRCoordinateTransformation* poCT =
            OGRCreateCoordinateTransformation(m_poSRS, &oSRS_EPSG4326);
        if( poCT )
        {
            OGRPoint oPoint1(m_oEnvelope.MinX, m_oEnvelope.MinY);
            oPoint1.transform(poCT);
            OGRPoint oPoint2(m_oEnvelope.MinX, m_oEnvelope.MaxY);
            oPoint2.transform(poCT);
            OGRPoint oPoint3(m_oEnvelope.MaxX, m_oEnvelope.MaxY);
            oPoint3.transform(poCT);
            OGRPoint oPoint4(m_oEnvelope.MaxX, m_oEnvelope.MinY);
            oPoint4.transform(poCT);
            m_oEnvelope.MinX =
                std::min(std::min(oPoint1.getX(), oPoint2.getX()),
                         std::min(oPoint3.getX(), oPoint4.getX()));
            m_oEnvelope.MinY =
                std::min(std::min(oPoint1.getY(), oPoint2.getY()),
                         std::min(oPoint3.getY(), oPoint4.getY()));
            m_oEnvelope.MaxX =
                std::max(std::max(oPoint1.getX(), oPoint2.getX()),
                         std::max(oPoint3.getX(), oPoint4.getX()));
            m_oEnvelope.MaxY =
                std::max(std::max(oPoint1.getY(), oPoint2.getY()),
                         std::max(oPoint3.getY(), oPoint4.getY()));
            delete poCT;
        }
    }
    const double dfCenterX = (m_oEnvelope.MinX + m_oEnvelope.MaxX) / 2;
    const double dfCenterY = (m_oEnvelope.MinY + m_oEnvelope.MaxY) / 2;
    CPLString osCenter(CPLSPrintf("%.7f,%.7f,%d", dfCenterX, dfCenterY,
                       m_nMinZoom));
    CPLString osBounds(CPLSPrintf("%.7f,%.7f,%.7f,%.7f",
                                    m_oEnvelope.MinX, m_oEnvelope.MinY,
                                    m_oEnvelope.MaxX, m_oEnvelope.MaxY));

    WriteMetadataItem("name", m_osName, m_hDBMBTILES, oRoot);
    WriteMetadataItem("description", m_osDescription, m_hDBMBTILES, oRoot);
    WriteMetadataItem("version", m_nMetadataVersion, m_hDBMBTILES, oRoot);
    WriteMetadataItem("minzoom", m_nMinZoom, m_hDBMBTILES, oRoot);
    WriteMetadataItem("maxzoom", m_nMaxZoom, m_hDBMBTILES, oRoot);
    WriteMetadataItem("center", !m_osCenter.empty() ? m_osCenter : osCenter,
                      m_hDBMBTILES, oRoot);
    WriteMetadataItem("bounds", !m_osBounds.empty() ? m_osBounds : osBounds,
                      m_hDBMBTILES, oRoot);
    WriteMetadataItem("type", m_osType, m_hDBMBTILES, oRoot);
    WriteMetadataItem("format", "pbf", m_hDBMBTILES, oRoot);
    if( m_hDBMBTILES )
    {
        WriteMetadataItem("scheme", "tms", m_hDBMBTILES, oRoot);
    }

    // GDAL extension for custom tiling schemes
    if( !bIsStandardTilingScheme )
    {
        const char* pszAuthName = m_poSRS->GetAuthorityName(nullptr);
        const char* pszAuthCode = m_poSRS->GetAuthorityCode(nullptr);
        if( pszAuthName && pszAuthCode )
        {
            WriteMetadataItem("crs",
                              CPLSPrintf("%s:%s", pszAuthName, pszAuthCode),
                              m_hDBMBTILES, oRoot);
        }
        else
        {
            char* pszWKT = nullptr;
            m_poSRS->exportToWkt(&pszWKT);
            WriteMetadataItem("crs", pszWKT, m_hDBMBTILES, oRoot);
            CPLFree(pszWKT);
        }
        WriteMetadataItem("tile_origin_upper_left_x",
                          m_dfTopX, m_hDBMBTILES, oRoot);
        WriteMetadataItem("tile_origin_upper_left_y",
                          m_dfTopY, m_hDBMBTILES, oRoot);
        WriteMetadataItem("tile_dimension_zoom_0",
                          m_dfTileDim0, m_hDBMBTILES, oRoot);
    }


    CPLJSONDocument oJsonDoc;
    CPLJSONObject oJsonRoot = oJsonDoc.GetRoot();

    CPLJSONArray oVectorLayers;
    oJsonRoot.Add("vector_layers", oVectorLayers);
    std::set<std::string> oAlreadyVisited;
    for( const auto& poLayer: m_apoLayers )
    {
        auto oIter = oMap.find( poLayer->m_osTargetName );
        if( oIter != oMap.end() &&
            oAlreadyVisited.find(poLayer->m_osTargetName) ==
                                                oAlreadyVisited.end() )
        {
            oAlreadyVisited.insert( poLayer->m_osTargetName );

            CPLJSONObject oLayerObj;
            oLayerObj.Add("id", poLayer->m_osTargetName);
            oLayerObj.Add("description",
                          m_oMapLayerNameToDesc[poLayer->m_osTargetName]);
            oLayerObj.Add("minzoom", oIter->second.m_nMinZoom);
            oLayerObj.Add("maxzoom", oIter->second.m_nMaxZoom);

            CPLJSONObject oFields;
            oLayerObj.Add("fields", oFields);
            auto poFDefn = poLayer->GetLayerDefn();
            for( int i = 0; i < poFDefn->GetFieldCount(); i++ )
            {
                auto poFieldDefn = poFDefn->GetFieldDefn(i);
                auto eType = poFieldDefn->GetType();
                if( eType == OFTInteger &&
                    poFieldDefn->GetSubType() == OFSTBoolean )
                {
                    oFields.Add(poFieldDefn->GetNameRef(), "Boolean");
                }
                else if( eType == OFTInteger ||
                         eType == OFTInteger64 ||
                         eType == OFTReal )
                {
                    oFields.Add(poFieldDefn->GetNameRef(), "Number");
                }
                else
                {
                    oFields.Add(poFieldDefn->GetNameRef(), "String");
                }
            }

            oVectorLayers.Add(oLayerObj);
        }
    }

    CPLJSONObject oTileStats;
    oJsonRoot.Add("tilestats", oTileStats);
    oTileStats.Add("layerCount", static_cast<int>(nLayers));
    CPLJSONArray oTileStatsLayers;
    oTileStats.Add("layers", oTileStatsLayers);
    oAlreadyVisited.clear();
    for( const auto& poLayer: m_apoLayers )
    {
        auto oIter = oMap.find( poLayer->m_osTargetName );
        if( oIter != oMap.end() &&
            oAlreadyVisited.find(poLayer->m_osTargetName) ==
                                                    oAlreadyVisited.end() )
        {
            oAlreadyVisited.insert( poLayer->m_osTargetName );
            auto& oLayerProps = oIter->second;
            CPLJSONObject oLayerObj;

            std::string osName(poLayer->m_osTargetName);
            osName.resize( std::min(knMAX_LAYER_NAME_LENGTH, osName.size()) );
            oLayerObj.Add("layer", osName);
            oLayerObj.Add("count",
                m_oMapLayerNameToFeatureCount[poLayer->m_osTargetName]);

            // Find majority geometry type
            MVTTileLayerFeature::GeomType eMaxGeomType =
                MVTTileLayerFeature::GeomType::UNKNOWN;
            GIntBig nMaxCountGeom = 0;
            for( int i = static_cast<int>(MVTTileLayerFeature::GeomType::POINT);
                     i <= static_cast<int>(
                            MVTTileLayerFeature::GeomType::POLYGON); i++ )
            {
                MVTTileLayerFeature::GeomType eGeomType =
                    static_cast<MVTTileLayerFeature::GeomType>(i);
                auto oIterCountGeom =
                    oLayerProps.m_oCountGeomType.find(eGeomType);
                if( oIterCountGeom != oLayerProps.m_oCountGeomType.end() )
                {
                    if( oIterCountGeom->second >= nMaxCountGeom )
                    {
                        eMaxGeomType = eGeomType;
                        nMaxCountGeom = oIterCountGeom->second ;
                    }
                }
            }
            if( eMaxGeomType == MVTTileLayerFeature::GeomType::POINT )
                oLayerObj.Add("geometry", "Point");
            else if( eMaxGeomType == MVTTileLayerFeature::GeomType::LINESTRING )
                oLayerObj.Add("geometry", "LineString");
            else if( eMaxGeomType == MVTTileLayerFeature::GeomType::POLYGON )
                oLayerObj.Add("geometry", "Polygon");

            oLayerObj.Add("attributeCount",
                          static_cast<int>(oLayerProps.m_oSetFields.size()));
            CPLJSONArray oAttributes;
            oLayerObj.Add("attributes", oAttributes);
            for( const auto& oFieldProps: oLayerProps.m_aoFields )
            {
                CPLJSONObject oFieldObj;
                oAttributes.Add( oFieldObj );
                std::string osFieldNameTruncated(oFieldProps.m_osName);
                osFieldNameTruncated.resize(
                    std::min(knMAX_FIELD_NAME_LENGTH,
                             osFieldNameTruncated.size()));
                oFieldObj.Add( "attribute", osFieldNameTruncated );
                oFieldObj.Add( "count",
                        static_cast<int>(oFieldProps.m_oSetAllValues.size()));
                oFieldObj.Add( "type",
                    oFieldProps.m_eType ==
                        MVTTileLayerValue::ValueType::DOUBLE ? "number" :
                    oFieldProps.m_eType ==
                        MVTTileLayerValue::ValueType::STRING ? "string" :
                                                               "boolean" );

                CPLJSONArray oValues;
                oFieldObj.Add( "values", oValues );
                for( const auto& oIterValue: oFieldProps.m_oSetValues )
                {
                    if( oIterValue.getType() ==
                                        MVTTileLayerValue::ValueType::BOOL )
                    {
                        oValues.Add( oIterValue.getBoolValue() );
                    }
                    else if( oIterValue.isNumeric() )
                    {
                        if( oFieldProps.m_bAllInt )
                        {
                            oValues.Add( static_cast<GInt64>(
                                            oIterValue.getNumericValue()) );
                        }
                        else
                        {
                            oValues.Add( oIterValue.getNumericValue() );
                        }
                    }
                    else if( oIterValue.isString() )
                    {
                        oValues.Add( oIterValue.getStringValue() );
                    }
                }

                if( oFieldProps.m_eType ==
                        MVTTileLayerValue::ValueType::DOUBLE )
                {
                    if( oFieldProps.m_bAllInt )
                    {
                        oFieldObj.Add( "min",
                                static_cast<GInt64>(oFieldProps.m_dfMinVal) );
                        oFieldObj.Add( "max",
                                static_cast<GInt64>(oFieldProps.m_dfMaxVal) );
                    }
                    else
                    {
                        oFieldObj.Add( "min", oFieldProps.m_dfMinVal );
                        oFieldObj.Add( "max", oFieldProps.m_dfMaxVal );
                    }
                }
            }

            oTileStatsLayers.Add(oLayerObj);
        }
    }

    WriteMetadataItem("json",oJsonDoc.SaveAsString().c_str(),
                      m_hDBMBTILES, oRoot);

    if( m_hDBMBTILES )
    {
        return true;
    }

    return oDoc.Save(
            CPLFormFilename(GetDescription(), "metadata.json", nullptr) );
}

/************************************************************************/
/*                            WriteFeature()                            */
/************************************************************************/

OGRErr OGRMVTWriterDataset::WriteFeature(OGRMVTWriterLayer* poLayer,
                                         OGRFeature* poFeature,
                                         GIntBig nSerial,
                                         OGRGeometry* poGeom)
{
    if( poFeature->GetGeometryRef() == poGeom )
    {
        m_oMapLayerNameToFeatureCount[poLayer->m_osTargetName] ++;
    }

    OGRwkbGeometryType eGeomType = wkbFlatten(poGeom->getGeometryType());
    if( eGeomType == wkbGeometryCollection )
    {
        OGRGeometryCollection* poGC = poGeom->toGeometryCollection();
        for( int i = 0; i < poGC->getNumGeometries(); i++ )
        {
            if( WriteFeature(poLayer, poFeature, nSerial,
                             poGC->getGeometryRef(i)) != OGRERR_NONE )
            {
                return OGRERR_FAILURE;
            }
        }
        return OGRERR_NONE;
    }

    OGREnvelope sExtent;
    poGeom->getEnvelope(&sExtent);

    if( !m_oEnvelope.IsInit() )
    {
        CPLDebug("MVT", "Creating temporary database...");
    }

    m_oEnvelope.Merge(sExtent);

    if( !m_bReuseTempFile )
    {
        auto poFeatureContent = std::shared_ptr<OGRMVTFeatureContent>(new OGRMVTFeatureContent());
        auto poSharedGeom = std::shared_ptr<OGRGeometry>(poGeom->clone());

        poFeatureContent->nFID = poFeature->GetFID();

        const OGRFeatureDefn* poFDefn = poFeature->GetDefnRef();
        for( int i = 0; i < poFeature->GetFieldCount(); i++ )
        {
            if( poFeature->IsFieldSetAndNotNull(i) )
            {
                MVTTileLayerValue oValue;
                const OGRFieldDefn* poFieldDefn = poFDefn->GetFieldDefn(i);
                OGRFieldType eFieldType = poFieldDefn->GetType();
                if( eFieldType == OFTInteger ||
                    eFieldType == OFTInteger64 )
                {
                    if( poFieldDefn->GetSubType() == OFSTBoolean )
                    {
                        oValue.setBoolValue(
                            poFeature->GetFieldAsInteger(i) != 0);
                    }
                    else
                    {
                        oValue.setValue( poFeature->GetFieldAsInteger64(i) );
                    }
                }
                else if( eFieldType == OFTReal )
                {
                    oValue.setValue( poFeature->GetFieldAsDouble(i) );
                }
                else if( eFieldType == OFTDate || eFieldType == OFTDateTime )
                {
                    int nYear, nMonth, nDay, nHour, nMin, nTZ;
                    float fSec;
                    poFeature->GetFieldAsDateTime(i, &nYear, &nMonth, &nDay,
                                                &nHour, &nMin, &fSec, &nTZ);
                    CPLString osFormatted;
                    if( eFieldType == OFTDate )
                    {
                        osFormatted.Printf("%04d-%02d-%02d", nYear, nMonth, nDay);
                    }
                    else
                    {
                        char* pszFormatted =
                            OGRGetXMLDateTime( poFeature->GetRawFieldRef(i) );
                        osFormatted = pszFormatted;
                        CPLFree(pszFormatted);
                    }
                    oValue.setStringValue( osFormatted );
                }
                else
                {
                    oValue.setStringValue(
                        std::string( poFeature->GetFieldAsString(i) ) );
                }

                poFeatureContent->oValues.emplace_back(
                    std::pair<std::string, MVTTileLayerValue>(
                        poFieldDefn->GetNameRef(), oValue));
            }
        }

        for( int nZ = poLayer->m_nMinZoom; nZ <= poLayer->m_nMaxZoom; nZ++ )
        {
            double dfTileDim = m_dfTileDim0 / (1 << nZ);
            double dfBuffer = dfTileDim * m_nBuffer / m_nExtent;
            int nTileMinX =
                static_cast<int>((sExtent.MinX - m_dfTopX - dfBuffer) / dfTileDim);
            int nTileMinY =
                static_cast<int>((m_dfTopY - sExtent.MaxY - dfBuffer) / dfTileDim);
            int nTileMaxX =
                static_cast<int>((sExtent.MaxX - m_dfTopX + dfBuffer) / dfTileDim);
            int nTileMaxY =
                static_cast<int>((m_dfTopY - sExtent.MinY + dfBuffer) / dfTileDim);
            for( int iX = nTileMinX; iX <= nTileMaxX; iX++ )
            {
                for( int iY = nTileMinY; iY <= nTileMaxY; iY++ )
                {
                    if( PreGenerateForTile(nZ, iX, iY, poLayer->m_osTargetName,
                            (nZ == poLayer->m_nMaxZoom),
                            poFeatureContent,
                            nSerial,
                            poSharedGeom,
                            sExtent) != OGRERR_NONE )
                    {
                        return OGRERR_FAILURE;
                    }
                }
            }
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                            TestCapability()                          */
/************************************************************************/

int OGRMVTWriterDataset::TestCapability( const char *pszCap )
{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return true;
    return false;
}

/************************************************************************/
/*                         ValidateMinMaxZoom()                         */
/************************************************************************/

static bool ValidateMinMaxZoom(int nMinZoom, int nMaxZoom)
{
    if( nMinZoom < 0 || nMinZoom > 22 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid MINZOOM");
        return false;
    }
    if( nMaxZoom < 0 || nMaxZoom > 22 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid MAXZOOM");
        return false;
    }
    if( nMaxZoom < nMinZoom )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid MAXZOOM < MINZOOM");
        return false;
    }
    return true;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer* OGRMVTWriterDataset::ICreateLayer( const char* pszLayerName,
                                             OGRSpatialReference* poSRS,
                                             OGRwkbGeometryType,
                                             char ** papszOptions )
{
    OGRSpatialReference* poSRSClone = poSRS;
    if ( poSRSClone )
    {
        poSRSClone = poSRS->Clone();
        poSRSClone->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }
    OGRMVTWriterLayer* poLayer =
        new OGRMVTWriterLayer(this, pszLayerName, poSRSClone);
    if( poSRSClone )
        poSRSClone->Release();
    poLayer->m_nMinZoom = m_nMinZoom;
    poLayer->m_nMaxZoom = m_nMaxZoom;
    poLayer->m_osTargetName = pszLayerName;

/*

        {
            "src_layer":
                { "target_name": "",
                  "description": "",
                  "minzoom": 0,
                  "maxzoom": 0
                }
        }
*/

    CPLJSONObject oObj = m_oConf.GetRoot().GetObj(pszLayerName);
    CPLString osDescription;
    if( oObj.IsValid() )
    {
        CPLString osTargetName = oObj.GetString("target_name");
        if( !osTargetName.empty() )
            poLayer->m_osTargetName = osTargetName;
        int nMinZoom = oObj.GetInteger("minzoom", -1);
        if( nMinZoom >= 0 )
            poLayer->m_nMinZoom = nMinZoom;
        int nMaxZoom = oObj.GetInteger("maxzoom", -1);
        if( nMaxZoom >= 0 )
            poLayer->m_nMaxZoom = nMaxZoom;
        osDescription = oObj.GetString("description");
    }

    poLayer->m_nMinZoom = atoi(CSLFetchNameValueDef(papszOptions, "MINZOOM",
                                        CPLSPrintf("%d",poLayer->m_nMinZoom)));
    poLayer->m_nMaxZoom = atoi(CSLFetchNameValueDef(papszOptions, "MAXZOOM",
                                        CPLSPrintf("%d",poLayer->m_nMaxZoom)));
    if( !ValidateMinMaxZoom( poLayer->m_nMinZoom,  poLayer->m_nMaxZoom) )
    {
        delete poLayer;
        return nullptr;
    }
    poLayer->m_osTargetName =
        CSLFetchNameValueDef(papszOptions, "NAME",
                             poLayer->m_osTargetName.c_str());
    osDescription =
        CSLFetchNameValueDef(papszOptions, "DESCRIPTION", osDescription);
    if( !osDescription.empty() )
        m_oMapLayerNameToDesc[poLayer->m_osTargetName] = osDescription;

    m_apoLayers.push_back( std::unique_ptr<OGRMVTWriterLayer>(poLayer) );
    return m_apoLayers.back().get();
}

/************************************************************************/
/*                                Create()                              */
/************************************************************************/

GDALDataset* OGRMVTWriterDataset::Create( const char * pszFilename,
                                   int nXSize,
                                   int nYSize,
                                   int nBandsIn,
                                   GDALDataType eDT,
                                   char ** papszOptions )
{
    if( nXSize != 0 || nYSize != 0 || nBandsIn != 0 || eDT != GDT_Unknown )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only vector creation supported");
        return nullptr;
    }

    const char* pszFormat = CSLFetchNameValue(papszOptions, "FORMAT");
    const bool bMBTILESExt = EQUAL(CPLGetExtension(pszFilename), "mbtiles");
    if( pszFormat == nullptr && bMBTILESExt )
    {
        pszFormat = "MBTILES";
    }
    const bool bMBTILES = pszFormat != nullptr && EQUAL(pszFormat, "MBTILES");

    // For debug only
    bool bReuseTempFile =
        CPLTestBool(CPLGetConfigOption("OGR_MVT_REUSE_TEMP_FILE", "NO"));

    if( bMBTILES )
    {
        if( !bMBTILESExt )
        {
            CPLError(CE_Failure, CPLE_FileIO, "%s should have mbtiles extension",
                     pszFilename);
            return nullptr;
        }

        VSIUnlink(pszFilename);
    }
    else
    {
        VSIStatBufL sStat;
        if( VSIStatL(pszFilename, &sStat) == 0 )
        {
            CPLError(CE_Failure, CPLE_FileIO, "%s already exists",
                        pszFilename);
            return nullptr;
        }

        if( VSIMkdir(pszFilename, 0755) != 0 )
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot create directory %s",
                    pszFilename);
            return nullptr;
        }
    }

    OGRMVTWriterDataset* poDS = new OGRMVTWriterDataset();
    poDS->m_pMyVFS = OGRSQLiteCreateVFS(nullptr, poDS);
    sqlite3_vfs_register(poDS->m_pMyVFS, 0);

    CPLString osTempDBDefault = CPLString(pszFilename) + ".temp.db";
    if( STARTS_WITH(osTempDBDefault, "/vsizip/") )
    {
        osTempDBDefault = CPLString(pszFilename + strlen("/vsizip/")) + ".temp.db";
    }
    CPLString osTempDB =
        CSLFetchNameValueDef(papszOptions, "TEMPORARY_DB",
                             osTempDBDefault.c_str());
    if( !bReuseTempFile )
        VSIUnlink(osTempDB);

    sqlite3* hDB = nullptr;
    CPL_IGNORE_RET_VAL(sqlite3_open_v2(osTempDB, &hDB,
                    SQLITE_OPEN_READWRITE |
                    (bReuseTempFile ? 0 : SQLITE_OPEN_CREATE) |
                    SQLITE_OPEN_NOMUTEX,
                    poDS->m_pMyVFS->zName));
    if( hDB == nullptr )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot create %s",
                 osTempDB.c_str());
        delete poDS;
        return nullptr;
    }
    poDS->m_osTempDB = osTempDB;
    poDS->m_hDB = hDB;
    poDS->m_bReuseTempFile = bReuseTempFile;

    // For Unix
    if( !poDS->m_bReuseTempFile &&
        CPLTestBool(CPLGetConfigOption("OGR_MVT_REMOVE_TEMP_FILE", "YES")) )
    {
        VSIUnlink(osTempDB);
    }

    if( poDS->m_bReuseTempFile )
    {
        poDS->m_nTempTiles = SQLGetInteger64(
            hDB, "SELECT COUNT(*) FROM temp", nullptr );
    }
    else
    {
        CPL_IGNORE_RET_VAL(SQLCommand(hDB,
            "PRAGMA page_size = 4096;" // 4096: default since sqlite 3.12
            "PRAGMA synchronous = OFF;"
            "PRAGMA journal_mode = OFF;"
            "PRAGMA temp_store = MEMORY;"
            "CREATE TABLE temp(z INTEGER, x INTEGER, y INTEGER, layer TEXT, "
            "idx INTEGER, feature BLOB, geomtype INTEGER, area_or_length DOUBLE);"
            "CREATE INDEX temp_index ON temp (z, x, y, layer, idx);"));
    }

    sqlite3_stmt* hInsertStmt = nullptr;
    CPL_IGNORE_RET_VAL(sqlite3_prepare_v2( hDB,
        "INSERT INTO temp (z,x,y,layer,idx,feature,geomtype,area_or_length) "
        "VALUES (?,?,?,?,?,?,?,?)",
        -1, &hInsertStmt, nullptr) );
    if( hInsertStmt == nullptr )
    {
        delete poDS;
        return nullptr;
    }
    poDS->m_hInsertStmt = hInsertStmt;

    poDS->m_nMinZoom = atoi(CSLFetchNameValueDef(papszOptions, "MINZOOM",
                                        CPLSPrintf("%d",poDS->m_nMinZoom)));
    poDS->m_nMaxZoom = atoi(CSLFetchNameValueDef(papszOptions, "MAXZOOM",
                                        CPLSPrintf("%d",poDS->m_nMaxZoom)));
    if( !ValidateMinMaxZoom( poDS->m_nMinZoom,  poDS->m_nMaxZoom) )
    {
        delete poDS;
        return nullptr;
    }

    const char* pszConf = CSLFetchNameValue(papszOptions, "CONF");
    if( pszConf )
    {
        VSIStatBufL sStat;
        bool bSuccess;
        if( VSIStatL(pszConf, &sStat) == 0 )
        {
            bSuccess = poDS->m_oConf.Load(pszConf);
        }
        else
        {
            bSuccess = poDS->m_oConf.LoadMemory(pszConf);
        }
        if( !bSuccess )
        {
            delete poDS;
            return nullptr;
        }
    }

    poDS->m_dfSimplification = CPLAtof(
        CSLFetchNameValueDef(papszOptions, "SIMPLIFICATION", "0"));
    poDS->m_dfSimplificationMaxZoom = CPLAtof(
        CSLFetchNameValueDef(papszOptions, "SIMPLIFICATION_MAX_ZOOM",
                             CPLSPrintf("%g", poDS->m_dfSimplification)));
    poDS->m_nExtent = static_cast<unsigned>(atoi(
        CSLFetchNameValueDef(papszOptions, "EXTENT",
                             CPLSPrintf("%u", poDS->m_nExtent))));
    poDS->m_nBuffer = static_cast<unsigned>(atoi(
        CSLFetchNameValueDef(papszOptions, "BUFFER",
                             CPLSPrintf("%u", 5 * poDS->m_nExtent / 256))));

    poDS->m_nMaxTileSize = std::max(100U, static_cast<unsigned>(atoi(
        CSLFetchNameValueDef(papszOptions, "MAX_SIZE",
                             CPLSPrintf("%u", poDS->m_nMaxTileSize)))));
    poDS->m_nMaxFeatures = std::max(1U, static_cast<unsigned>(atoi(
        CSLFetchNameValueDef(papszOptions, "MAX_FEATURES",
                             CPLSPrintf("%u", poDS->m_nMaxFeatures)))));

    poDS->m_osName = CSLFetchNameValueDef(papszOptions, "NAME",
                                          CPLGetBasename(pszFilename));
    poDS->m_osDescription = CSLFetchNameValueDef(papszOptions,
                                                 "DESCRIPTION",
                                                 poDS->m_osDescription.c_str());
    poDS->m_osType = CSLFetchNameValueDef(papszOptions,
                                          "TYPE",
                                          poDS->m_osType.c_str());
    poDS->m_bGZip = CPLFetchBool(papszOptions, "COMPRESS", poDS->m_bGZip);
    poDS->m_osBounds = CSLFetchNameValueDef(papszOptions, "BOUNDS", "");
    poDS->m_osCenter = CSLFetchNameValueDef(papszOptions, "CENTER", "");
    poDS->m_osExtension = CSLFetchNameValueDef(papszOptions, "TILE_EXTENSION",
                                               poDS->m_osExtension);

    const char* pszTilingScheme = CSLFetchNameValue(papszOptions,
                                                    "TILING_SCHEME");
    if( pszTilingScheme )
    {
        if( bMBTILES )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Custom TILING_SCHEME not supported with MBTILES output");
            delete poDS;
            return nullptr;
        }

        CPLStringList aoList(CSLTokenizeString2(pszTilingScheme, ",", 0));
        if( aoList.Count() == 4 )
        {
            poDS->m_poSRS->SetFromUserInput( aoList[0] );
            poDS->m_dfTopX = CPLAtof( aoList[1] );
            poDS->m_dfTopY = CPLAtof( aoList[2] );
            poDS->m_dfTileDim0 = CPLAtof( aoList[3] );
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Wrong format for TILING_SCHEME. "
                     "Expecting EPSG:XXXX,tile_origin_upper_left_x,"
                     "tile_origin_upper_left_y,tile_dimension_zoom_0");
            delete poDS;
            return nullptr;
        }
    }

    if( bMBTILES )
    {
        CPL_IGNORE_RET_VAL(sqlite3_open_v2(pszFilename, &poDS->m_hDBMBTILES,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                        SQLITE_OPEN_NOMUTEX,
                        poDS->m_pMyVFS->zName));
        if( poDS->m_hDBMBTILES == nullptr )
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot create %s",
                     pszFilename);
            delete poDS;
            return nullptr;
        }

        if( SQLCommand(poDS->m_hDBMBTILES,
            "PRAGMA page_size = 4096;" // 4096: default since sqlite 3.12
            "PRAGMA synchronous = OFF;"
            "PRAGMA journal_mode = OFF;"
            "PRAGMA temp_store = MEMORY;"
            "CREATE TABLE metadata (name text, value text);"
            "CREATE TABLE tiles (zoom_level integer, tile_column integer, "
                "tile_row integer, tile_data blob, "
                "UNIQUE (zoom_level, tile_column, tile_row))") != OGRERR_NONE )
        {
            delete poDS;
            return nullptr;
        }
    }

    int nThreads = CPLGetNumCPUs();
    const char* pszNumThreads =
            CPLGetConfigOption("GDAL_NUM_THREADS", nullptr);
    if( pszNumThreads && CPLGetValueType(pszNumThreads) == CPL_VALUE_INTEGER )
    {
        nThreads = atoi(pszNumThreads);
    }
    if( nThreads > 1 )
    {
        poDS->m_bThreadPoolOK =
            poDS->m_oThreadPool.Setup(nThreads, nullptr, nullptr);
    }

    poDS->SetDescription(pszFilename);
    return poDS;
}


GDALDataset* OGRMVTWriterDatasetCreate( const char * pszFilename,
                                   int nXSize,
                                   int nYSize,
                                   int nBandsIn,
                                   GDALDataType eDT,
                                   char ** papszOptions )
{
    return OGRMVTWriterDataset::Create(pszFilename, nXSize, nYSize, nBandsIn,
                                       eDT, papszOptions);
}

#endif // HAVE_MVT_WRITE_SUPPORT

/************************************************************************/
/*                           RegisterOGRMVT()                           */
/************************************************************************/

void RegisterOGRMVT()

{
    if( GDALGetDriverByName( "MVT" ) != nullptr )
        return;

    GDALDriver  *poDriver = new GDALDriver();

    poDriver->SetDescription( "MVT" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Mapbox Vector Tiles" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/mvt.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "mvt mvt.gz pbf" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='X' type='int' description='X coordinate of tile'/>"
"  <Option name='Y' type='int' description='Y coordinate of tile'/>"
"  <Option name='Z' type='int' description='Z coordinate of tile'/>"
//"  <Option name='@GEOREF_TOPX' type='float' description='X coordinate of top-left corner of tile'/>"
//"  <Option name='@GEOREF_TOPY' type='float' description='Y coordinate of top-left corner of  tile'/>"
//"  <Option name='@GEOREF_TILEDIMX' type='float' description='Tile width in georeferenced units'/>"
//"  <Option name='@GEOREF_TILEDIMY' type='float' description='Tile height in georeferenced units'/>"
"  <Option name='METADATA_FILE' type='string' "
                                "description='Path to metadata.json'/>"
"  <Option name='CLIP' type='boolean' "
    "description='Whether to clip geometries to tile extent' default='YES'/>"
"  <Option name='TILE_EXTENSION' type='string' default='pbf' description="
    "'For tilesets, extension of tiles'/>"
"  <Option name='TILE_COUNT_TO_ESTABLISH_FEATURE_DEFN' type='int' description="
    "'For tilesets without metadata file, maximum number of tiles to use to "
    "establish the layer schemas' default='1000'/>"
"  <Option name='JSON_FIELD' type='string' description='For tilesets, "
        "whether to put all attributes as a serialized JSon dictionary'/>"
"</OpenOptionList>" );

    poDriver->pfnIdentify = OGRMVTDriverIdentify;
    poDriver->pfnOpen = OGRMVTDataset::Open;
#ifdef HAVE_MVT_WRITE_SUPPORT
    poDriver->pfnCreate = OGRMVTWriterDataset::Create;
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES,
                               "Integer Integer64 Real String" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATASUBTYPES,
                               "Boolean Float32" );

    poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST, MVT_LCO);

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"  <Option name='NAME' type='string' description='Tileset name'/>"
"  <Option name='DESCRIPTION' type='string' "
        "description='A description of the tileset'/>"
"  <Option name='TYPE' type='string-select' description='Layer type' "
                                                        "default='overlay'>"
"    <Value>overlay</Value>"
"    <Value>baselayer</Value>"
"  </Option>"
"  <Option name='FORMAT' type='string-select' description='Format'>"
"    <Value>DIRECTORY</Value>"
"    <Value>MBTILES</Value>"
"  </Option>"
"  <Option name='TILE_EXTENSION' type='string' default='pbf' description="
    "'For tilesets as directories of files, extension of tiles'/>"
MVT_MBTILES_COMMON_DSCO
"  <Option name='BOUNDS' type='string' "
        "description='Override default value for bounds metadata item'/>"
"  <Option name='CENTER' type='string' "
        "description='Override default value for center metadata item'/>"
"  <Option name='TILING_SCHEME' type='string' "
        "description='Custom tiling scheme with following format "
        "\"EPSG:XXXX,tile_origin_upper_left_x,tile_origin_upper_left_y,"
        "tile_dimension_zoom_0\"'/>"
"</CreationOptionList>");
#endif // HAVE_MVT_WRITE_SUPPORT

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
