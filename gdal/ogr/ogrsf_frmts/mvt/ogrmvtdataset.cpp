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

#include "ogrsf_frmts.h"
#include "cpl_conv.h"
#include "cpl_json.h"
#include "ogr_p.h"

#include "mvtutils.h"

#define DO_NOT_DEFINE_READ_VARINT64
#include "gpb.h"

#include <algorithm>
#include <memory>
#include <vector>

CPL_CVSID("$Id$")

/* See https://github.com/mapbox/vector-tile-spec/blob/master/2.1/vector_tile.proto */
constexpr int knLAYER = 3;

constexpr int knLAYER_NAME = 1;
constexpr int knLAYER_FEATURES = 2;
constexpr int knLAYER_KEYS = 3;
constexpr int knLAYER_VALUES = 4;
constexpr int knLAYER_EXTENT = 5;
constexpr int knLAYER_VERSION = 15;

constexpr int knVALUE_STRING = 1;
constexpr int knVALUE_FLOAT = 2;
constexpr int knVALUE_DOUBLE = 3;
constexpr int knVALUE_INT = 4;
constexpr int knVALUE_UINT = 5;
constexpr int knVALUE_SINT = 6;
constexpr int knVALUE_BOOL = 7;

constexpr int knFEATURE_ID = 1;
constexpr int knFEATURE_TAGS = 2;
constexpr int knFEATURE_TYPE = 3;
constexpr int knFEATURE_GEOMETRY = 4;

constexpr int knGEOM_TYPE_POINT = 1;
constexpr int knGEOM_TYPE_LINESTRING = 2;
constexpr int knGEOM_TYPE_POLYGON = 3;

constexpr int knCMD_MOVETO = 1;
constexpr int knCMD_LINETO = 2;
constexpr int knCMD_CLOSEPATH = 7;

// WebMercator related constants
constexpr double kmSPHERICAL_RADIUS = 6378137.0;
constexpr double kmMAX_GM =  kmSPHERICAL_RADIUS * M_PI;  // 20037508.342789244

const char* SRS_EPSG_3857 = "PROJCS[\"WGS 84 / Pseudo-Mercator\",GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],AUTHORITY[\"EPSG\",\"4326\"]],PROJECTION[\"Mercator_1SP\"],PARAMETER[\"central_meridian\",0],PARAMETER[\"scale_factor\",1],PARAMETER[\"false_easting\",0],PARAMETER[\"false_northing\",0],UNIT[\"metre\",1,AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"X\",EAST],AXIS[\"Y\",NORTH],EXTENSION[\"PROJ4\",\"+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext  +no_defs\"],AUTHORITY[\"EPSG\",\"3857\"]]";

constexpr int knMAX_FILES_PER_DIR = 10000;

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

class OGRMVTLayerBase: public OGRLayer
{
        virtual OGRFeature         *GetNextRawFeature() = 0;

    protected:
        OGRFeatureDefn             *m_poFeatureDefn = nullptr;

        void                InitFields(const CPLJSONObject& oFields);

    public:
        virtual ~OGRMVTLayerBase();

        virtual OGRFeatureDefn *    GetLayerDefn() override
                                            { return m_poFeatureDefn; }

        virtual OGRFeature*         GetNextFeature() override;

        virtual int                 TestCapability( const char * ) override;
};

/************************************************************************/
/*                             OGRMVTLayer                              */
/************************************************************************/

class OGRMVTDataset;

class OGRMVTLayer : public OGRMVTLayerBase
{
    OGRMVTDataset       *m_poDS;
    GByte               *m_pabyDataStart;
    GByte               *m_pabyDataEnd;
    GByte               *m_pabyDataCur = nullptr;
    GByte               *m_pabyDataFeatureStart = nullptr;
    bool                 m_bError = false;
    unsigned int         m_nExtent = 4096;
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
    bool                QuickScanFeature(GByte* pabyData,
                                         GByte* pabyDataFeatureEnd,
                                         bool bScanFields,
                                         bool bScanGeometries,
                                         bool& bGeomTypeSet);
    void                GetXY(int nX, int nY, double& dfX, double& dfY);
    OGRGeometry        *ParseGeometry(unsigned int nGeomType,
                                      GByte* pabyDataGeometryEnd);
    void                SanitizeClippedGeometry(OGRGeometry*& poGeom);

    virtual OGRFeature         *GetNextRawFeature() override;

  public:
                        OGRMVTLayer(OGRMVTDataset* poDS,
                                    const char* pszLayerName,
                                    GByte* pabyData,
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

class OGRMVTDirectoryLayer: public OGRMVTLayerBase
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
    bool                        m_bExtentValid = false;
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

class OGRMVTDataset : public GDALDataset
{
    friend class OGRMVTLayer;
    friend class OGRMVTDirectoryLayer;

    GByte                                      *m_pabyData;
    std::vector<std::unique_ptr<OGRLayer>>      m_apoLayers;
    bool                                        m_bGeoreferenced = false;
    double                                      m_dfTileDim = 0.0;
    double                                      m_dfTopX = 0.0;
    double                                      m_dfTopY = 0.0;
    CPLString                                   m_osMetadataMemFilename;
    bool                                        m_bClip = true;
    CPLString                                   m_osTileExtension{"pbf"};

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
/*                          GetNextFeature()                            */
/************************************************************************/

OGRFeature* OGRMVTLayerBase::GetNextFeature()
{
    while( true )
    {
        OGRFeature *poFeature = GetNextRawFeature();
        if (poFeature == nullptr)
            return nullptr;

        if((m_poFilterGeom == nullptr
            || FilterGeometry( poFeature->GetGeometryRef() ) )
        && (m_poAttrQuery == nullptr
            || m_poAttrQuery->Evaluate( poFeature )) )
        {
            return poFeature;
        }

        delete poFeature;
    }
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
                         GByte* pabyData,
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
        OGRSpatialReference* poSRS = new OGRSpatialReference();
        poSRS->SetFromUserInput(SRS_EPSG_3857);
        m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
        poSRS->Release();
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
    GByte* pabyData = m_pabyDataStart;
    GByte* pabyDataLimit = m_pabyDataEnd;
    unsigned int nKey = 0;
    bool bGeomTypeSet = false;
    const bool bScanFields = !oFields.IsValid();
    const bool bScanGeometries = m_poFeatureDefn->GetGeomType() == wkbUnknown;
    const bool bQuickScanFeature = bScanFields || bScanGeometries;

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
            GByte* pabyDataValueEnd = pabyData + nValueLength;
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
                if( pabyData + sizeof(float) > pabyDataLimit )
                    goto end_error;
                float fValue;
                memcpy(&fValue, pabyData, sizeof(float));
                CPL_LSBPTR32(&fValue);
                Value sValue;
                sValue.eType = OFTReal;
                sValue.eSubType = OFSTFloat32;
                sValue.sValue.Real = fValue;
                m_asValues.push_back(sValue);
            }
            else if( nKey == MAKE_KEY(knVALUE_DOUBLE, WT_64BIT) )
            {
                if( pabyData + sizeof(double) > pabyDataLimit )
                    goto end_error;
                double dfValue;
                memcpy(&dfValue, pabyData, sizeof(double));
                CPL_LSBPTR64(&dfValue);
                Value sValue;
                sValue.eType = OFTReal;
                sValue.eSubType = OFSTNone;
                sValue.sValue.Real = dfValue;
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
            READ_VARUINT32(pabyData, pabyDataLimit, m_nExtent);
            m_nExtent = std::max(1U, m_nExtent); // to avoid divide by zero
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
        GByte* pabyDataBefore = pabyData;
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
            GByte* pabyDataFeatureEnd = pabyData + nFeatureLength;
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

end_error:
    return;
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

bool OGRMVTLayer::QuickScanFeature(GByte* pabyData,
                                   GByte* pabyDataFeatureEnd,
                                   bool bScanFields,
                                   bool bScanGeometries,
                                   bool& bGeomTypeSet)
{
    unsigned int nKey = 0;
    unsigned int nGeomType = 0;
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
            GByte* pabyDataTagsEnd = pabyData + nTagsSize;
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
                 nGeomType >= 1 && nGeomType <= 3 )
        {
            unsigned int nGeometrySize = 0;
            READ_SIZE(pabyData, pabyDataFeatureEnd, nGeometrySize);
            GByte* pabyDataGeometryEnd = pabyData + nGeometrySize;
            OGRwkbGeometryType eType = wkbUnknown;
            if( nGeomType == knGEOM_TYPE_POINT )
            {
                eType = wkbPoint;
            }
            else if( nGeomType == knGEOM_TYPE_LINESTRING )
            {
                eType = wkbLineString;
            }
            else if( nGeomType == knGEOM_TYPE_POLYGON )
            {
                eType = wkbPolygon;
            }

            if( eType == wkbPoint )
            {
                unsigned int nCmdCountCombined = 0;
                READ_VARUINT32(pabyData, pabyDataGeometryEnd,
                                nCmdCountCombined);
                if( GetCmdId(nCmdCountCombined) == knCMD_MOVETO &&
                    GetCmdCount(nCmdCountCombined) > 1 )
                {
                    eType = wkbMultiPoint;
                }
            }
            else if( eType == wkbLineString )
            {
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
            else if( eType == wkbPolygon )
            {
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
end_error:
    return false;
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
        dfX = m_poDS->m_dfTopX + nX * m_poDS->m_dfTileDim / m_nExtent;
        dfY = m_poDS->m_dfTopY - nY * m_poDS->m_dfTileDim / m_nExtent;
    }
    else
    {
        dfX = nX;
        dfY = static_cast<int>(m_nExtent) - nY;
    }
}

/************************************************************************/
/*                           ParseGeometry()                            */
/************************************************************************/

OGRGeometry* OGRMVTLayer::ParseGeometry(unsigned int nGeomType,
                                        GByte* pabyDataGeometryEnd)
{
    OGRMultiPoint* poMultiPoint = nullptr;
    OGRMultiLineString* poMultiLS = nullptr;
    OGRLineString* poLine = nullptr;
    OGRMultiPolygon* poMultiPoly = nullptr;
    OGRPolygon* poPoly = nullptr;
    OGRLinearRing* poRing = nullptr;

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
                    nX += nDX;
                    nY += nDY;
                    double dfX;
                    double dfY;
                    GetXY(nX, nY, dfX, dfY);
                    OGRPoint* poPoint = new OGRPoint(dfX, dfY);
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
            nX += nDX;
            nY += nDY;
            double dfX;
            double dfY;
            GetXY(nX, nY, dfX, dfY);
            if( poLine != nullptr )
            {
                poMultiLS = new OGRMultiLineString();
                poMultiLS->addGeometryDirectly(poLine);
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
                    nX += nDX;
                    nY += nDY;
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
            nX += nDX;
            nY += nDY;
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
                    nX += nDX;
                    nY += nDY;
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
end_error:
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
    return nullptr;
}

/************************************************************************/
/*                      SanitizeClippedGeometry()                       */
/************************************************************************/

void OGRMVTLayer::SanitizeClippedGeometry(OGRGeometry*& poGeom)
{
    OGRwkbGeometryType eInGeomType = poGeom->getGeometryType();
    const OGRwkbGeometryType eLayerGeomType = GetGeomType();
    if( eLayerGeomType == wkbUnknown )
    {
        return;
    }

    // GEOS intersection may return a mix of polygon and linestrings when
    // intersection a multipolygon and a polygon
    if( eInGeomType == wkbGeometryCollection )
    {
        OGRGeometryCollection* poGC =
            static_cast<OGRGeometryCollection*>(poGeom);
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
        for(int i=0; i < poGC->getNumGeometries(); ++i)
        {
            OGRGeometry* poSubGeom = poGC->getGeometryRef(i);
            if( poSubGeom->getGeometryType() == ePartGeom )
            {
                if( poTargetSingleGeom != nullptr )
                {
                    if( poTargetGC == nullptr )
                    {
                        poTargetGC = static_cast<OGRGeometryCollection*>(
                            OGRGeometryFactory::createGeometry(
                                OGR_GT_GetCollection(ePartGeom)));
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
        eInGeomType = poGeom->getGeometryType();
    }

    // Wrap single into multi if requested by the layer geometry type
    if( OGR_GT_GetCollection(eInGeomType) == eLayerGeomType )
    {
        OGRGeometryCollection* poGC = static_cast<OGRGeometryCollection*>(
            OGRGeometryFactory::createGeometry(eLayerGeomType));
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
    GByte* pabyDataLimit = m_pabyDataEnd;
    OGRFeature* poFeature = nullptr;
    GByte* pabyDataFeatureEnd;
    unsigned int nFeatureLength = 0;
    unsigned int nGeomType = 0;

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
        pabyDataFeatureEnd = m_pabyDataCur + nFeatureLength;
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
                GByte* pabyDataTagsEnd = m_pabyDataCur + nTagsSize;
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
                GByte* pabyDataGeometryEnd = m_pabyDataCur + nGeometrySize;
                OGRGeometry* poGeom =
                    ParseGeometry(nGeomType, pabyDataGeometryEnd);
                if( poGeom )
                {
                    poGeom->assignSpatialReference(GetSpatialRef());
                    poFeature->SetGeometryDirectly(poGeom);

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
                                }
                            }
                        }
                        else
                        {
                            bOK = false;
                        }
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

end_error:
    delete poFeature;
    return nullptr;
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
    return aosOutput;
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

    OGRSpatialReference* poSRS = new OGRSpatialReference();
    poSRS->SetFromUserInput(SRS_EPSG_3857);
    m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
    poSRS->Release();

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
                    !STARTS_WITH(m_osDirName, "/vsicurl") ? "YES": "NO"));
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
    ResetReading();

    if( psExtent )
    {
        m_bExtentValid = true;
        m_sExtent = *psExtent;
    }

    SetSpatialFilter(nullptr);

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
        ResetReading();
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
        m_nYIndex = 0;
        OpenTile();
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
        GDALOpenInfo oOpenInfo(("MVT:/vsigzip/" + osFilename).c_str(),
                               GA_ReadOnly);
        oOpenInfo.papszOpenOptions = CSLSetNameValue(nullptr,
                "METADATA_FILE",
                m_bJsonField ? "" : m_poDS->m_osMetadataMemFilename.c_str());
        m_poCurrentTile = OGRMVTDataset::Open(&oOpenInfo);
        CSLDestroy(oOpenInfo.papszOpenOptions);

        int nX = (m_bUseReadDir || !m_aosDirContent.empty()) ?
                        atoi(m_aosDirContent[m_nXIndex]) : m_nXIndex;
        int nY = m_bUseReadDir ? atoi(m_aosSubDirContent[m_nYIndex]) : m_nYIndex;
        m_nFIDBase = (static_cast<GIntBig>(nY) << m_nZ) | nX;
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

    if( m_poFilterGeom != nullptr &&
        m_sFilterEnvelope.MinX >= -10 * kmMAX_GM &&
        m_sFilterEnvelope.MinY >= -10 * kmMAX_GM &&
        m_sFilterEnvelope.MaxX <= 10 * kmMAX_GM &&
        m_sFilterEnvelope.MaxY <= 10 * kmMAX_GM )
    {
        const double dfTileDim = 2 * kmMAX_GM / (1 << m_nZ);
        m_nFilterMinX = std::max(0, static_cast<int>(
            floor((m_sFilterEnvelope.MinX + kmMAX_GM) / dfTileDim)));
        m_nFilterMinY = std::max(0, static_cast<int>(
            floor((kmMAX_GM - m_sFilterEnvelope.MaxY) / dfTileDim)));
        m_nFilterMaxX = std::min(static_cast<int>(
            ceil((m_sFilterEnvelope.MaxX + kmMAX_GM) / dfTileDim)),
            (1 << m_nZ)-1);
        m_nFilterMaxY = std::min(static_cast<int>(
            ceil((kmMAX_GM - m_sFilterEnvelope.MinY) / dfTileDim)),
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
    if( m_bExtentValid )
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
    GDALOpenInfo oOpenInfo(("MVT:/vsigzip/" + osFilename).c_str(), GA_ReadOnly);
    oOpenInfo.papszOpenOptions = CSLSetNameValue(nullptr,
            "METADATA_FILE",
            m_bJsonField ? "" : m_poDS->m_osMetadataMemFilename.c_str());
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
    m_pabyData(pabyData)
{
    m_bClip = CPLTestBool(CPLGetConfigOption("OGR_MVT_CLIP", "YES"));
}

/************************************************************************/
/*                           ~OGRMVTDataset()                           */
/************************************************************************/

OGRMVTDataset::~OGRMVTDataset()
{
    VSIFree(m_pabyData);
    if( !m_osMetadataMemFilename.empty() )
        VSIUnlink(m_osMetadataMemFilename);
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
                VSIStatL(osMetadataFile, &sStat) == 0 )
            {
                return TRUE;
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
        CPLConfigOptionSetter oSetter(
            "CPL_VSIL_GZIP_WRITE_PROPERTIES", "NO", false);
        GDALOpenInfo oOpenInfo( (CPLString("/vsigzip/") +
                            poOpenInfo->pszFilename).c_str(), GA_ReadOnly );
        return OGRMVTDriverIdentify( &oOpenInfo );
    }

    // The GPB macros assume that the buffer is nul terminated,
    // which is the case
    GByte* pabyData = reinterpret_cast<GByte*>(poOpenInfo->pabyHeader);
    GByte* const pabyDataStart = pabyData;
    const GByte* pabyLayerStart;
    GByte* const pabyDataLimit = pabyData + poOpenInfo->nHeaderBytes;
    GByte* pabyLayerEnd = pabyDataLimit;
    int nKey = 0;
    unsigned int nLayerLength = 0;
    bool bLayerNameFound = false;
    bool bKeyFound = false;
    bool bFeatureFound = false;
    bool bVersionFound = false;
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
        if( nKey == MAKE_KEY(knLAYER_NAME, WT_DATA) )
        {
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
        else if( nKey == MAKE_KEY(knLAYER_FEATURES, WT_DATA) )
        {
            unsigned int nFeatureLength = 0;
            unsigned int nGeomType = 0;
            READ_VARUINT32(pabyData, pabyLayerEnd, nFeatureLength);
            if( nFeatureLength > nLayerLength - (pabyData - pabyLayerStart) )
            {
                CPLDebug("MVT", "Protobuf error: line %d", __LINE__);
                return FALSE;
            }
            bFeatureFound = true;

            GByte* const pabyDataFeatureStart = pabyData;
            GByte* const pabyDataFeatureEnd = pabyDataStart + std::min(
                static_cast<int>(pabyData + nFeatureLength - pabyDataStart),
                poOpenInfo->nHeaderBytes);
            while( pabyData < pabyDataFeatureEnd )
            {
                READ_VARUINT32(pabyData, pabyDataFeatureEnd, nKey);
                if( nKey == MAKE_KEY(knFEATURE_TYPE, WT_VARINT) )
                {
                    READ_VARUINT32(pabyData, pabyDataFeatureEnd, nGeomType);
                    if( nGeomType > knGEOM_TYPE_POLYGON )
                    {
                        CPLDebug("MVT", "Protobuf error: line %d", __LINE__);
                        return FALSE;
                    }
                }
                else if( nKey == MAKE_KEY(knFEATURE_TAGS, WT_DATA) )
                {
                    unsigned int nTagsSize = 0;
                    READ_VARUINT32(pabyData, pabyDataFeatureEnd, nTagsSize);
                    if( nTagsSize == 0 || nTagsSize >
                        nFeatureLength - (pabyData - pabyDataFeatureStart) )
                    {
                        CPLDebug("MVT", "Protobuf error: line %d", __LINE__);
                        return FALSE;
                    }
                    GByte* const pabyDataTagsEnd = pabyDataStart + std::min(
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
                else if( nKey == MAKE_KEY(knFEATURE_GEOMETRY, WT_DATA) &&
                         nGeomType >= 1 && nGeomType <= 3 )
                {
                    unsigned int nGeometrySize = 0;
                    READ_VARUINT32(pabyData, pabyDataFeatureEnd, nGeometrySize);
                    if( nGeometrySize == 0 || nGeometrySize >
                            nFeatureLength - (pabyData - pabyDataFeatureStart) )
                    {
                        CPLDebug("MVT", "Protobuf error: line %d", __LINE__);
                        return FALSE;
                    }
                    GByte* const pabyDataGeometryEnd = pabyDataStart +
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
                    else if( nGeomType == knGEOM_TYPE_POLYGON )
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
        else if( nKey == MAKE_KEY(knLAYER_KEYS, WT_DATA) )
        {
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
        else if( nKey == MAKE_KEY(knLAYER_VALUES, WT_DATA) )
        {
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
        else if( nKey == MAKE_KEY(knLAYER_EXTENT, WT_VARINT) )
        {
            unsigned int nExtent = 0;
            READ_VARUINT32(pabyData, pabyLayerEnd, nExtent);
            if( nExtent < 256 || nExtent > 16384 )
            {
                CPLDebug("MVT", "Invalid extent: %u", nExtent);
                return FALSE;
            }
        }
        else if( nKey == MAKE_KEY(knLAYER_VERSION, WT_VARINT) )
        {
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

end_error:
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
    if( osMetadataFile.empty() || VSIStatL(osMetadataFile, &sStat) != 0 )
    {
        // If we don't have a metadata file, iterate through all tiles to
        // establish the layer definitions.
        OGRMVTDataset   *poDS = nullptr;
        bool bTryToListDir =
            !STARTS_WITH(poOpenInfo->pszFilename, "/vsicurl");
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
                GDALOpenInfo oOpenInfo(("MVT:/vsigzip/" + osFilename).c_str(),
                               GA_ReadOnly);
                oOpenInfo.papszOpenOptions = CSLSetNameValue(nullptr,
                    "METADATA_FILE", "");
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
                delete poTileDS;
                CSLDestroy(oOpenInfo.papszOpenOptions);

                if( nMaxTiles > 0 && nCountTiles == nMaxTiles )
                    break;
            }

            if( nMaxTiles > 0 && nCountTiles == nMaxTiles )
                break;
        }
        return poDS;
    }

    CPLJSONDocument oDoc;
    if( !oDoc.Load(osMetadataFile) )
        return nullptr;

    CPLJSONObject oJson = oDoc.GetRoot().GetObj("json");
    if( !(oJson.IsValid() && oJson.GetType() == CPLJSONObject::String) )
        return nullptr;

    CPLJSONDocument oJsonDoc;
    if( !oJsonDoc.LoadMemory(
            reinterpret_cast<const GByte*>(oJson.ToString().c_str())) )
    {
        return nullptr;
    }

    CPLJSONArray oVectorLayers;
    oVectorLayers.Deinit();

    CPLJSONArray oTileStatLayers;
    oTileStatLayers.Deinit();

    oVectorLayers =
        oJsonDoc.GetRoot().GetArray("vector_layers");

    oTileStatLayers =
        oJsonDoc.GetRoot().GetArray("tilestats/layers");

    if( !oVectorLayers.IsValid() )
        return nullptr;

    OGREnvelope sExtent;
    bool bExtentValid = false;
    CPLJSONObject oBounds = oDoc.GetRoot().GetObj("bounds");
    if( oBounds.IsValid() && oBounds.GetType() == CPLJSONObject::String )
    {
        CPLStringList aosTokens(
            CSLTokenizeString2( oBounds.ToString().c_str(), ",", 0 ));
        if( aosTokens.Count() == 4 )
        {
            double dfX0 = CPLAtof(aosTokens[0]);
            double dfY0 = CPLAtof(aosTokens[1]);
            double dfX1 = CPLAtof(aosTokens[2]);
            double dfY1 = CPLAtof(aosTokens[3]);
            LongLatToSphericalMercator(&dfX0, &dfY0);
            LongLatToSphericalMercator(&dfX1, &dfY1);
            bExtentValid = true;
            sExtent.MinX = dfX0;
            sExtent.MinY = dfY0;
            sExtent.MaxX = dfX1;
            sExtent.MaxY = dfY1;
        }
    }

    OGRMVTDataset   *poDS = new OGRMVTDataset(nullptr);
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->m_bClip = CPLFetchBool(
        poOpenInfo->papszOpenOptions, "CLIP", poDS->m_bClip);
    poDS->m_osTileExtension = osTileExtension;
    poDS->m_osMetadataMemFilename =
        CPLSPrintf("/vsimem/%p_metadata.json", poDS);
    oDoc.Save(poDS->m_osMetadataMemFilename);
    for( int i = 0; i < oVectorLayers.Size(); i++ )
    {
        CPLJSONObject oId = oVectorLayers[i].GetObj("id");
        if( oId.IsValid() && oId.GetType() ==
                CPLJSONObject::String )
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

        VSIStatBufL sStat;
        if( !STARTS_WITH(osFilename, "/vsigzip/") &&
            EQUAL( CPLGetExtension(CPLGetFilename(osFilename)), "") &&
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

        if( STARTS_WITH(osFilename, "/vsicurl")  &&
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

        CPLConfigOptionSetter oSetter(
            "CPL_VSIL_GZIP_WRITE_PROPERTIES", "NO", false);
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
    if( fp == nullptr )
        return nullptr;

    CPLString osY = CPLGetBasename(osFilename);
    CPLString osX = CPLGetBasename(CPLGetPath(osFilename));
    CPLString osZ = CPLGetBasename(CPLGetPath(CPLGetPath(osFilename)));

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

    // Check file size and ingest into memory
    VSIFSeekL(fp, 0, SEEK_END);
    vsi_l_offset nFileSizeL = VSIFTellL(fp);
    if( nFileSizeL > 10 * 1024 * 1024 )
    {
        VSIFCloseL(fp);
        return nullptr;
    }
    size_t nFileSize = static_cast<size_t>(nFileSizeL);
    GByte* pabyData = static_cast<GByte*>(VSI_MALLOC_VERBOSE(nFileSize+1));
    if( pabyData == nullptr )
    {
        VSIFCloseL(fp);
        return nullptr;
    }
    VSIFSeekL(fp, 0, SEEK_SET);
    VSIFReadL(pabyData, 1, nFileSize, fp);
    pabyData[nFileSize] = 0;
    VSIFCloseL(fp);

    // First scan to browse through layers
    GByte* pabyDataLimit = pabyData + nFileSize;
    int nKey = 0;
    OGRMVTDataset   *poDS = new OGRMVTDataset(pabyData);
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
            poDS->m_dfTileDim = 2 * kmMAX_GM / (1 << nZ);
            poDS->m_dfTopX = -kmMAX_GM + nX * poDS->m_dfTileDim;
            poDS->m_dfTopY = kmMAX_GM - nY * poDS->m_dfTileDim;
        }
    }

    CPLJSONArray oVectorLayers;
    oVectorLayers.Deinit();

    CPLJSONArray oTileStatLayers;
    oTileStatLayers.Deinit();

    if( !osMetadataFile.empty() )
    {
        CPLJSONDocument oDoc;
        if( oDoc.Load(osMetadataFile) )
        {
            CPLJSONObject oJson = oDoc.GetRoot().GetObj("json");
            if( oJson.IsValid() && oJson.GetType() == CPLJSONObject::String )
            {
                CPLJSONDocument oJsonDoc;
                if( oJsonDoc.LoadMemory(
                    reinterpret_cast<const GByte*>(oJson.ToString().c_str())) )
                {
                    oVectorLayers =
                        oJsonDoc.GetRoot().GetArray("vector_layers");

                    oTileStatLayers =
                        oJsonDoc.GetRoot().GetArray("tilestats/layers");
                }
            }
        }
    }

    while( pabyData < pabyDataLimit )
    {
        READ_FIELD_KEY(nKey);
        if( nKey == MAKE_KEY(knLAYER, WT_DATA) )
        {
            unsigned int nLayerSize = 0;
            READ_SIZE(pabyData, pabyDataLimit, nLayerSize);
            GByte* pabyDataLayer = pabyData;
            GByte* pabyDataLimitLayer = pabyData + nLayerSize;
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
                                    CPLJSONObject::String )
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

end_error:
    delete poDS;
    return nullptr;
}

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
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drv_mvt.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "mvt mvt.gz pbf" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='X' type='int' description='X coordinate of tile'/>"
"  <Option name='Y' type='int' description='Y coordinate of tile'/>"
"  <Option name='Z' type='int' description='Z coordinate of tile'/>"
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

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
