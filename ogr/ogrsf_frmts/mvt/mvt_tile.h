/******************************************************************************
 *
 * Project:  MVT Translator
 * Purpose:  Mapbox Vector Tile decoder and encoder
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

#ifndef MVT_TILE_H
#define MVT_TILE_H

#include "cpl_port.h"

#include <memory>
#include <string>
#include <vector>

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

constexpr int knGEOM_TYPE_UNKNOWN = 0;
constexpr int knGEOM_TYPE_POINT = 1;
constexpr int knGEOM_TYPE_LINESTRING = 2;
constexpr int knGEOM_TYPE_POLYGON = 3;

constexpr int knCMD_MOVETO = 1;
constexpr int knCMD_LINETO = 2;
constexpr int knCMD_CLOSEPATH = 7;

constexpr unsigned knDEFAULT_EXTENT = 4096;

/************************************************************************/
/*                         MVTTileLayerValue                            */
/************************************************************************/

class MVTTileLayerValue
{
    public:
        enum class ValueType
        {
            NONE,
            STRING,
            FLOAT,
            DOUBLE,
            INT,
            UINT,
            SINT,
            BOOL,
            STRING_MAX_8, // optimization for short strings.
        };

    private:
        // Layout optimized for small memory footprint
        union
        {
            float m_fValue;
            double m_dfValue;
            GInt64 m_nIntValue;
            GUInt64 m_nUIntValue;
            bool m_bBoolValue;
            char* m_pszValue;
            char m_achValue[8]; // optimization for short strings
        };
        ValueType m_eType = ValueType::NONE;

        void unset();

    public:
        MVTTileLayerValue();
       ~MVTTileLayerValue();
        MVTTileLayerValue(const MVTTileLayerValue& oOther);
        MVTTileLayerValue& operator=(const MVTTileLayerValue& oOther);

        bool operator <(const MVTTileLayerValue& rhs) const;

        ValueType getType() const { return m_eType; }
        bool isNumeric() const { return m_eType == ValueType::FLOAT ||
                                        m_eType == ValueType::DOUBLE ||
                                        m_eType == ValueType::INT ||
                                        m_eType == ValueType::UINT ||
                                        m_eType == ValueType::SINT; }
        bool isString() const { return m_eType == ValueType::STRING ||
                                       m_eType == ValueType::STRING_MAX_8; }

        float getFloatValue() const { return m_fValue; }
        double getDoubleValue() const { return m_dfValue; }
        GInt64 getIntValue() const { return m_nIntValue; }
        GUInt64 getUIntValue() const { return m_nUIntValue; }
        bool getBoolValue() const { return m_bBoolValue; }

        double getNumericValue() const
            { if( m_eType == ValueType::FLOAT )
                return m_fValue;
              if( m_eType == ValueType::DOUBLE )
                return m_dfValue;
              if( m_eType == ValueType::INT || m_eType == ValueType::SINT )
                return static_cast<double>(m_nIntValue);
              if( m_eType == ValueType::UINT )
                return static_cast<double>(m_nUIntValue);
              return 0.0;
            }

        std::string getStringValue() const
            { if( m_eType == ValueType::STRING )
                  return m_pszValue;
              else if( m_eType == ValueType::STRING_MAX_8 )
              {
                  char szBuf[8+1];
                  memcpy(szBuf, m_achValue, 8);
                  szBuf[8] = 0;
                  return szBuf;
              }
              return std::string();
            }

        void setStringValue(const std::string& osValue);
        void setFloatValue(float fValue)
            { unset(); m_eType = ValueType::FLOAT; m_fValue = fValue; }
        void setDoubleValue(double dfValue)
            { unset(); m_eType = ValueType::DOUBLE; m_dfValue = dfValue; }
        void setIntValue(GInt64 nVal)
            { unset(); m_eType = ValueType::INT; m_nIntValue = nVal; }
        void setUIntValue(GUInt64 nVal)
            { unset(); m_eType = ValueType::UINT; m_nUIntValue = nVal; }
        void setSIntValue(GInt64 nVal)
            { unset(); m_eType = ValueType::SINT; m_nIntValue = nVal; }
        void setBoolValue(bool bVal)
            { unset(); m_eType = ValueType::BOOL; m_bBoolValue = bVal; }

        void setValue(double dfVal);
        void setValue(int nVal) { setValue(static_cast<GInt64>(nVal)); }
        void setValue(GInt64 nVal)
            { if (nVal < 0)
                setSIntValue(nVal);
              else
                setUIntValue(nVal);
            }

        size_t getSize() const;
        void write(GByte** ppabyData) const;
        bool read(const GByte** ppabyData, const GByte* pabyEnd);
};

/************************************************************************/
/*                       MVTTileLayerFeature                            */
/************************************************************************/

class MVTTileLayer;

class MVTTileLayerFeature
{
    public:
        enum class GeomType: char
        {
            UNKNOWN = 0,
            POINT = 1,
            LINESTRING = 2,
            POLYGON = 3
        };

    private:
        mutable size_t m_nCachedSize = 0;
        GUInt64 m_nId = 0;
        std::vector<GUInt32> m_anTags;
        std::vector<GUInt32> m_anGeometry;
        GeomType m_eType = GeomType::UNKNOWN;
        mutable bool m_bCachedSize = false;
        bool m_bHasId = false;
        bool m_bHasType = false;
        MVTTileLayer* m_poOwner = nullptr;

    public:
        MVTTileLayerFeature();
        void setOwner(MVTTileLayer* poOwner);

        bool hasId() const { return m_bHasId; }
        GUInt64 getId() const { return m_nId; }
        const std::vector<GUInt32>& getTags() const { return m_anTags; }
        bool hasType() const { return m_bHasType; }
        GeomType getType() const { return m_eType; }
        GUInt32 getGeometryCount() const
            { return static_cast<GUInt32>(m_anGeometry.size()); }
        const std::vector<GUInt32>& getGeometry() const { return m_anGeometry; }

        void setId(GUInt64 nId)
            { m_bHasId = true;
              m_nId = nId;
              invalidateCachedSize(); }
        void addTag(GUInt32 nTag)
            { m_anTags.push_back(nTag);
              invalidateCachedSize(); }
        void setType(GeomType eType)
            { m_bHasType = true;
              m_eType = eType;
              invalidateCachedSize(); }
        void resizeGeometryArray(GUInt32 nNewSize)
            { m_anGeometry.resize(nNewSize);
              invalidateCachedSize(); }
        void addGeometry(GUInt32 nGeometry)
            { m_anGeometry.push_back(nGeometry);
              invalidateCachedSize(); }
        void setGeometry(GUInt32 nIdx, GUInt32 nVal)
            { m_anGeometry[nIdx] = nVal;
              invalidateCachedSize(); }
        void setGeometry(const std::vector<GUInt32>& anGeometry )
            { m_anGeometry = anGeometry;
              invalidateCachedSize(); }

        size_t getSize() const;
        void write(GByte** ppabyData) const;
        bool read(const GByte** ppabyData, const GByte* pabyEnd);

        void invalidateCachedSize();
};

/************************************************************************/
/*                           MVTTileLayer                               */
/************************************************************************/

class MVTTile;

class MVTTileLayer
{
        mutable bool m_bCachedSize = false;
        mutable size_t m_nCachedSize = 0;
        GUInt32 m_nVersion = 1;
        std::string m_osName;
        std::vector<std::shared_ptr<MVTTileLayerFeature>> m_apoFeatures;
        std::vector<std::string> m_aosKeys;
        std::vector<MVTTileLayerValue> m_aoValues;
        bool m_bHasExtent = false;
        GUInt32 m_nExtent = 4096;
        MVTTile* m_poOwner = nullptr;

    public:
        MVTTileLayer();
        void setOwner(MVTTile* poOwner);

        GUInt32 getVersion() const { return m_nVersion; }
        const std::string& getName() const { return m_osName; }
        const std::vector<std::shared_ptr<MVTTileLayerFeature>>& getFeatures()
            const
            { return m_apoFeatures; }
        const std::vector<std::string>& getKeys() const { return m_aosKeys; }
        const std::vector<MVTTileLayerValue>& getValues() const
            { return m_aoValues; }
        GUInt32 getExtent() const { return m_nExtent; }

        void setVersion(GUInt32 nVersion)
                            { m_nVersion = nVersion;
                              invalidateCachedSize(); }
        void setName(const std::string& osName)
                            { m_osName = osName;
                              invalidateCachedSize(); }
        size_t addFeature(std::shared_ptr<MVTTileLayerFeature> poFeature);
        GUInt32 addKey(const std::string& osKey)
            {
                m_aosKeys.push_back(osKey);
                invalidateCachedSize();
                return static_cast<GUInt32>(m_aosKeys.size()) - 1;
            }

        GUInt32 addValue(const MVTTileLayerValue& oValue)
            {
                m_aoValues.push_back(oValue);
                invalidateCachedSize();
                return static_cast<GUInt32>(m_aoValues.size()) - 1;
            }

        void setExtent(GUInt32 nExtent)
            {
                m_nExtent = nExtent;
                m_bHasExtent = true;
                invalidateCachedSize();
            }

        size_t getSize() const;
        void write(GByte** ppabyData) const;
        void write(GByte* pabyData) const;
        std::string write() const;
        bool read(const GByte** ppabyData, const GByte* pabyEnd);
        bool read(const GByte* pabyData, const GByte* pabyEnd);

        void invalidateCachedSize();
};

/************************************************************************/
/*                              MVTTile                                 */
/************************************************************************/

class MVTTile
{
        std::vector<std::shared_ptr<MVTTileLayer>> m_apoLayers;
        mutable size_t m_nCachedSize = 0;
        mutable bool m_bCachedSize = false;

    public:
        MVTTile();

        const std::vector<std::shared_ptr<MVTTileLayer>>& getLayers() const
            { return m_apoLayers; }

        void clear() { m_apoLayers.clear(); invalidateCachedSize(); }
        void addLayer(std::shared_ptr<MVTTileLayer> poLayer);
        size_t getSize() const;
        void write(GByte** ppabyData) const;
        void write(GByte* pabyData) const;
        std::string write() const;
#ifdef ADD_MVT_TILE_READ
        bool read(const GByte** ppabyData, const GByte* pabyEnd);
        bool read(const GByte* pabyData, const GByte* pabyEnd);
#endif
        void invalidateCachedSize() { m_bCachedSize = false; m_nCachedSize = 0; }
};

#endif // MVT_TILE_H
