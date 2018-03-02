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

#include "gpb.h"

#include <limits>
#include <memory>
#include <vector>

#include "mvt_tile.h"

constexpr int knSIZE_KEY = 1;

/************************************************************************/
/*                        MVTTileLayerValue()                           */
/************************************************************************/

MVTTileLayerValue::MVTTileLayerValue(): m_nUIntValue(0)
{
}

MVTTileLayerValue::MVTTileLayerValue(const MVTTileLayerValue& oOther)
{
    operator=(oOther);
}

/************************************************************************/
/*                       ~MVTTileLayerValue()                           */
/************************************************************************/

MVTTileLayerValue::~MVTTileLayerValue()
{
    unset();
}

/************************************************************************/
/*                            operator=                                 */
/************************************************************************/

MVTTileLayerValue& MVTTileLayerValue::operator=(
                                            const MVTTileLayerValue& oOther)

{
    if( this != &oOther )
    {
        unset();
        m_eType = oOther.m_eType;
        if( m_eType == ValueType::STRING )
        {
            const size_t nSize = strlen(oOther.m_pszValue);
            m_pszValue = static_cast<char*>(CPLMalloc(1 + nSize));
            memcpy(m_pszValue, oOther.m_pszValue, nSize);
            m_pszValue[nSize] = 0;
        }
        else
        {
            m_nUIntValue = oOther.m_nUIntValue;
        }
    }
    return *this;
}

/************************************************************************/
/*                            operator<                                 */
/************************************************************************/

bool MVTTileLayerValue::operator <(const MVTTileLayerValue& rhs) const
{
    if( m_eType < rhs.m_eType )
        return false;
    if( m_eType > rhs.m_eType )
        return true;
    if( m_eType == ValueType::NONE )
        return false;
    if( m_eType == ValueType::STRING )
        return strcmp( m_pszValue, rhs.m_pszValue ) < 0;
    if( m_eType == ValueType::FLOAT )
        return m_fValue < rhs.m_fValue;
    if( m_eType == ValueType::DOUBLE )
        return m_dfValue < rhs.m_dfValue;
    if( m_eType == ValueType::INT )
        return m_nIntValue < rhs.m_nIntValue;
    if( m_eType == ValueType::UINT )
        return m_nUIntValue < rhs.m_nUIntValue;
    if( m_eType == ValueType::SINT )
        return m_nIntValue < rhs.m_nIntValue;
    if( m_eType == ValueType::BOOL )
        return m_bBoolValue < rhs.m_bBoolValue;
    if( m_eType == ValueType::STRING_MAX_8 )
        return strncmp( m_achValue, rhs.m_achValue, 8 ) < 0;
    CPLAssert(false);
    return false;
}

/************************************************************************/
/*                             unset()                                  */
/************************************************************************/

void MVTTileLayerValue::unset()
{
    if( m_eType == ValueType::STRING )
        CPLFree(m_pszValue);
    m_eType = ValueType::NONE;
    m_nUIntValue = 0;
}

/************************************************************************/
/*                            GetSizeMax8()                             */
/************************************************************************/

static size_t GetSizeMax8(const char achValue[8])
{
    size_t nSize = 0;
    while( nSize < 8 && achValue[nSize] != 0 )
        nSize ++;
    return nSize;
}

/************************************************************************/
/*                        setStringValue()                              */
/************************************************************************/

void MVTTileLayerValue::setStringValue(const std::string& osValue)
{
    unset();
    const size_t nSize = osValue.size();
    if( nSize <= 8 )
    {
        m_eType = ValueType::STRING_MAX_8;
        if( nSize )
            memcpy( m_achValue, osValue.c_str(), nSize );
        if( nSize < 8 )
            m_achValue[nSize] = 0;
    }
    else
    {
        m_eType = ValueType::STRING;
        m_pszValue = static_cast<char*>(CPLMalloc(1 + nSize));
        memcpy(m_pszValue, osValue.c_str(), nSize);
        m_pszValue[nSize] = 0;
    }
}

/************************************************************************/
/*                          setValue()                                  */
/************************************************************************/

void MVTTileLayerValue::setValue(double dfVal)
{
    if( dfVal >= 0 &&
        dfVal <= static_cast<double>(std::numeric_limits<GUInt64>::max()) &&
        dfVal == static_cast<double>(static_cast<GUInt64>(dfVal)) )
    {
        setUIntValue(static_cast<GUInt64>(dfVal));
    }
    else if( dfVal >= static_cast<double>(
                                        std::numeric_limits<GInt64>::min()) &&
             dfVal < 0 &&
             dfVal == static_cast<double>(static_cast<GInt64>(dfVal)) )
    {
        setSIntValue(static_cast<GInt64>(dfVal));
    }
    else if( !CPLIsFinite(dfVal) ||
             (dfVal >= -std::numeric_limits<float>::max() &&
              dfVal <= std::numeric_limits<float>::max() &&
              dfVal == static_cast<float>(dfVal)) )
    {
        setFloatValue( static_cast<float>(dfVal) );
    }
    else
    {
        setDoubleValue( dfVal );
    }
}

/************************************************************************/
/*                            getSize()                                 */
/************************************************************************/

size_t MVTTileLayerValue::getSize() const
{
    switch( m_eType )
    {
        case ValueType::NONE: return 0;
        case ValueType::STRING:
        {
            const size_t nSize = strlen(m_pszValue);
            return knSIZE_KEY + GetVarUIntSize(nSize) + nSize;
        }
        case ValueType::STRING_MAX_8:
        {
            const size_t nSize = GetSizeMax8(m_achValue);
            return knSIZE_KEY + GetVarUIntSize(nSize) + nSize;
        }
        case ValueType::FLOAT: return knSIZE_KEY + sizeof(float);
        case ValueType::DOUBLE: return knSIZE_KEY + sizeof(double);
        case ValueType::INT: return knSIZE_KEY + GetVarIntSize(m_nIntValue);
        case ValueType::UINT: return knSIZE_KEY + GetVarUIntSize(m_nUIntValue);
        case ValueType::SINT: return knSIZE_KEY + GetVarSIntSize(m_nIntValue);
        case ValueType::BOOL: return knSIZE_KEY + 1;
        default: return 0;
    }
}

/************************************************************************/
/*                             write()                                  */
/************************************************************************/

void MVTTileLayerValue::write(GByte** ppabyData) const
{
    GByte* pabyData = *ppabyData;

    switch( m_eType )
    {
        case ValueType::STRING:
        {
            const size_t nSize = strlen(m_pszValue);
            WriteVarUIntSingleByte(&pabyData,
                                   MAKE_KEY(knVALUE_STRING, WT_DATA));
            WriteVarUInt(&pabyData, nSize);
            memcpy(pabyData, m_pszValue, nSize);
            pabyData += nSize;
            break;
        }

        case ValueType::STRING_MAX_8:
        {
            const size_t nSize = GetSizeMax8(m_achValue);
            WriteVarUIntSingleByte(&pabyData,
                                   MAKE_KEY(knVALUE_STRING, WT_DATA));
            WriteVarUInt(&pabyData, nSize);
            if( nSize )
                memcpy(pabyData, m_achValue, nSize);
            pabyData += nSize;
            break;
        }

        case ValueType::FLOAT:
            WriteVarUIntSingleByte(&pabyData,
                                   MAKE_KEY(knVALUE_FLOAT, WT_32BIT));
            WriteFloat32(&pabyData, m_fValue);
            break;

        case ValueType::DOUBLE:
            WriteVarUIntSingleByte(&pabyData,
                                   MAKE_KEY(knVALUE_DOUBLE, WT_64BIT));
            WriteFloat64(&pabyData, m_dfValue);
            break;

        case ValueType::INT:
            WriteVarUIntSingleByte(&pabyData,
                                   MAKE_KEY(knVALUE_INT, WT_VARINT));
            WriteVarInt(&pabyData, m_nIntValue);
            break;

        case ValueType::UINT:
            WriteVarUIntSingleByte(&pabyData,
                                   MAKE_KEY(knVALUE_UINT, WT_VARINT));
            WriteVarUInt(&pabyData, m_nUIntValue);
            break;

        case ValueType::SINT:
            WriteVarUIntSingleByte(&pabyData,
                                   MAKE_KEY(knVALUE_SINT, WT_VARINT));
            WriteVarSInt(&pabyData, m_nIntValue);
            break;

        case ValueType::BOOL:
            WriteVarUIntSingleByte(&pabyData,
                                   MAKE_KEY(knVALUE_BOOL, WT_VARINT));
            WriteVarUIntSingleByte(&pabyData, m_bBoolValue ? 1 : 0);
            break;

        default:
            break;
    }

    CPLAssert(pabyData == *ppabyData + getSize());
    *ppabyData = pabyData;
}

/************************************************************************/
/*                             read()                                   */
/************************************************************************/

bool MVTTileLayerValue::read(const GByte** ppabyData, const GByte* pabyDataLimit)
{
    const GByte* pabyData = *ppabyData;

    try
    {
        unsigned int nKey = 0;
        if( pabyData < pabyDataLimit )
        {
            READ_FIELD_KEY(nKey);

            if( nKey == MAKE_KEY(knVALUE_STRING, WT_DATA) )
            {
                char* pszValue = nullptr;
                READ_TEXT(pabyData, pabyDataLimit, pszValue);
                // cppcheck-suppress nullPointer
                setStringValue(pszValue);
                CPLFree(pszValue);
            }
            else if( nKey == MAKE_KEY(knVALUE_FLOAT, WT_32BIT) )
            {
                setFloatValue(ReadFloat32(&pabyData, pabyDataLimit));
            }
            else if( nKey == MAKE_KEY(knVALUE_DOUBLE, WT_64BIT) )
            {
                setDoubleValue(ReadFloat64(&pabyData, pabyDataLimit));
            }
            else if( nKey == MAKE_KEY(knVALUE_INT, WT_VARINT) )
            {
                GIntBig nVal = 0;
                READ_VARINT64(pabyData, pabyDataLimit, nVal);
                setIntValue(nVal);
            }
            else if( nKey == MAKE_KEY(knVALUE_UINT, WT_VARINT) )
            {
                GUIntBig nVal = 0;
                READ_VARUINT64(pabyData, pabyDataLimit, nVal);
                setUIntValue(nVal);
            }
            else if( nKey == MAKE_KEY(knVALUE_SINT, WT_VARINT) )
            {
                GIntBig nVal = 0;
                READ_VARSINT64(pabyData, pabyDataLimit, nVal);
                setSIntValue(nVal);
            }
            else if( nKey == MAKE_KEY(knVALUE_BOOL, WT_VARINT) )
            {
                unsigned nVal = 0;
                READ_VARUINT32(pabyData, pabyDataLimit, nVal);
                setBoolValue(nVal != 0);
            }
        }
        *ppabyData = pabyData;
        return true;
    }
    catch( const GPBException& )
    {
        return false;
    }
}

/************************************************************************/
/*                           MVTTileLayer()                             */
/************************************************************************/

MVTTileLayerFeature::MVTTileLayerFeature()
{
}

/************************************************************************/
/*                             setOwner()                               */
/************************************************************************/

void MVTTileLayerFeature::setOwner(MVTTileLayer* poOwner)
{
    CPLAssert( !m_poOwner );
    m_poOwner = poOwner;
    m_poOwner->invalidateCachedSize();
}

/************************************************************************/
/*                       invalidateCachedSize()                         */
/************************************************************************/

void MVTTileLayerFeature::invalidateCachedSize()
{
    m_bCachedSize = false;
    m_nCachedSize = 0;
    if( m_poOwner )
        m_poOwner->invalidateCachedSize();
}

/************************************************************************/
/*                        GetPackedArraySize()                          */
/************************************************************************/

static size_t GetPackedArraySize(const std::vector<GUInt32>& anVals)
{
    size_t nPackedSize = 0;
    for( const auto& nVal: anVals )
    {
        nPackedSize += GetVarUIntSize(nVal);
    }
    return nPackedSize;
}

/************************************************************************/
/*                            getSize()                                 */
/************************************************************************/

size_t MVTTileLayerFeature::getSize() const
{
    if( m_bCachedSize )
        return m_nCachedSize;
    m_bCachedSize = true;
    m_nCachedSize = 0;
    if( m_bHasId )
        m_nCachedSize += knSIZE_KEY + GetVarUIntSize(m_nId);
    if( !m_anTags.empty() )
    {
        size_t nPackedSize = GetPackedArraySize(m_anTags);
        m_nCachedSize += knSIZE_KEY;
        m_nCachedSize += GetVarUIntSize(nPackedSize);
        m_nCachedSize += nPackedSize;
    }
    if( m_bHasType )
        m_nCachedSize += knSIZE_KEY + 1; // fixed size for m_eType
    if( !m_anGeometry.empty() )
    {
        size_t nPackedSize = GetPackedArraySize(m_anGeometry);
        m_nCachedSize += knSIZE_KEY;
        m_nCachedSize += GetVarUIntSize(nPackedSize);
        m_nCachedSize += nPackedSize;
    }
    return m_nCachedSize;
}

/************************************************************************/
/*                        WriteUIntPackedArray()                        */
/************************************************************************/

static void WriteUIntPackedArray(GByte** ppabyData, int nKey,
                                 const std::vector<GUInt32>& anVals)
{
    GByte* pabyData = *ppabyData;
    const size_t nPackedSize = GetPackedArraySize(anVals);
    WriteVarUIntSingleByte(&pabyData, nKey);
    WriteVarUInt(&pabyData, nPackedSize);
    for( const auto& nVal: anVals )
    {
        WriteVarUInt(&pabyData, nVal);
    }
    *ppabyData = pabyData;
}

/************************************************************************/
/*                             write()                                  */
/************************************************************************/

void MVTTileLayerFeature::write(GByte** ppabyData) const
{
    GByte* pabyData = *ppabyData;

    if( m_bHasId )
    {
        WriteVarUIntSingleByte(&pabyData, MAKE_KEY(knFEATURE_ID, WT_VARINT));
        WriteVarUInt(&pabyData, m_nId);
    }
    if( !m_anTags.empty() )
    {
        WriteUIntPackedArray(&pabyData, MAKE_KEY(knFEATURE_TAGS, WT_DATA),
                             m_anTags);
    }
    if( m_bHasType )
    {
        WriteVarUIntSingleByte(&pabyData, MAKE_KEY(knFEATURE_TYPE, WT_VARINT));
        WriteVarUIntSingleByte(&pabyData, static_cast<GUIntBig>(m_eType));
    }
    if( !m_anGeometry.empty() )
    {
        WriteUIntPackedArray(&pabyData, MAKE_KEY(knFEATURE_GEOMETRY, WT_DATA),
                             m_anGeometry);
    }

    CPLAssert(pabyData == *ppabyData + getSize());
    *ppabyData = pabyData;
}

/************************************************************************/
/*                             read()                                   */
/************************************************************************/

bool MVTTileLayerFeature::read(const GByte** ppabyData,
                               const GByte* pabyDataLimit)
{
    const GByte* pabyData = *ppabyData;

    try
    {
        unsigned int nKey = 0;
        while( pabyData < pabyDataLimit )
        {
            READ_FIELD_KEY(nKey);
            if( nKey == MAKE_KEY(knFEATURE_ID, WT_VARINT) )
            {
                GUIntBig nID = 0;
                READ_VARUINT64(pabyData, pabyDataLimit, nID);
                setId(nID);
            }
            else if( nKey == MAKE_KEY(knFEATURE_TAGS, WT_DATA) )
            {
                unsigned int nTagsSize = 0;
                READ_SIZE(pabyData, pabyDataLimit, nTagsSize);
                const GByte* pabyDataTagsEnd = pabyData + nTagsSize;
                while( pabyData < pabyDataTagsEnd )
                {
                    unsigned int nTag = 0;
                    READ_VARUINT32(pabyData, pabyDataTagsEnd, nTag);
                    addTag(nTag);
                }
                pabyData = pabyDataTagsEnd;
            }
            else if( nKey == MAKE_KEY(knFEATURE_TYPE, WT_VARINT) )
            {
                unsigned int nType = 0;
                READ_VARUINT32(pabyData, pabyDataLimit, nType);
                if( nType <= knGEOM_TYPE_POLYGON )
                    setType(static_cast<GeomType>(nType));
            }
            else if( nKey == MAKE_KEY(knFEATURE_GEOMETRY, WT_DATA) )
            {
                unsigned int nGeometrySize = 0;
                READ_SIZE(pabyData, pabyDataLimit, nGeometrySize);
                const GByte* pabyDataGeometryEnd = pabyData + nGeometrySize;
                while( pabyData < pabyDataGeometryEnd )
                {
                    unsigned int nGeometry = 0;
                    READ_VARUINT32(pabyData, pabyDataGeometryEnd, nGeometry);
                    addGeometry(nGeometry);
                }
                pabyData = pabyDataGeometryEnd;
            }
            else
            {
                SKIP_UNKNOWN_FIELD(pabyData, pabyDataLimit, FALSE);
            }
        }
        *ppabyData = pabyData;
        return true;
    }
    catch( const GPBException& )
    {
        return false;
    }
}

/************************************************************************/
/*                           MVTTileLayer()                             */
/************************************************************************/

MVTTileLayer::MVTTileLayer()
{
}

/************************************************************************/
/*                             setOwner()                               */
/************************************************************************/

void MVTTileLayer::setOwner(MVTTile* poOwner)
{
    CPLAssert( !m_poOwner );
    m_poOwner = poOwner;
    m_poOwner->invalidateCachedSize();
}

/************************************************************************/
/*                       invalidateCachedSize()                         */
/************************************************************************/

void MVTTileLayer::invalidateCachedSize()
{
    m_bCachedSize = false;
    m_nCachedSize = 0;
    if( m_poOwner )
        m_poOwner->invalidateCachedSize();
}

/************************************************************************/
/*                          addFeature()                                */
/************************************************************************/

size_t MVTTileLayer::addFeature(std::shared_ptr<MVTTileLayerFeature> poFeature)
{
    poFeature->setOwner(this);
    m_apoFeatures.push_back(poFeature);
    invalidateCachedSize();
    return m_apoFeatures.size() - 1;
}

/************************************************************************/
/*                            getSize()                                 */
/************************************************************************/

size_t MVTTileLayer::getSize() const
{
    if( m_bCachedSize )
        return m_nCachedSize;
    m_nCachedSize = knSIZE_KEY + GetTextSize(m_osName);
    for( const auto& poFeature: m_apoFeatures )
    {
        const size_t nFeatureSize = poFeature->getSize();
        m_nCachedSize += knSIZE_KEY +
                         GetVarUIntSize(nFeatureSize) + nFeatureSize;
    }
    for( const auto& osKey: m_aosKeys )
    {
        m_nCachedSize += knSIZE_KEY + GetTextSize(osKey);
    }
    for( const auto& oValue: m_aoValues )
    {
        const size_t nValueSize = oValue.getSize();
        m_nCachedSize += knSIZE_KEY + GetVarUIntSize(nValueSize) + nValueSize;
    }
    if( m_bHasExtent )
    {
        m_nCachedSize += knSIZE_KEY + GetVarUIntSize(m_nExtent);
    }
    m_nCachedSize += knSIZE_KEY + GetVarUIntSize(m_nVersion);
    m_bCachedSize = true;
    return m_nCachedSize;
}

/************************************************************************/
/*                             write()                                  */
/************************************************************************/

void MVTTileLayer::write(GByte** ppabyData) const
{
    GByte* pabyData = *ppabyData;

    WriteVarUIntSingleByte(&pabyData, MAKE_KEY(knLAYER_NAME, WT_DATA));
    WriteText(&pabyData, m_osName);

    for( const auto& poFeature: m_apoFeatures )
    {
        WriteVarUIntSingleByte(&pabyData, MAKE_KEY(knLAYER_FEATURES, WT_DATA));
        WriteVarUInt(&pabyData, poFeature->getSize());
        poFeature->write(&pabyData);
    }

    for( const auto& osKey: m_aosKeys )
    {
        WriteVarUIntSingleByte(&pabyData, MAKE_KEY(knLAYER_KEYS, WT_DATA));
        WriteText(&pabyData, osKey);
    }

    for( const auto& oValue: m_aoValues )
    {
        WriteVarUIntSingleByte(&pabyData, MAKE_KEY(knLAYER_VALUES, WT_DATA));
        WriteVarUInt(&pabyData, oValue.getSize());
        oValue.write(&pabyData);
    }

    if( m_bHasExtent )
    {
        WriteVarUIntSingleByte(&pabyData, MAKE_KEY(knLAYER_EXTENT, WT_VARINT));
        WriteVarUInt(&pabyData, m_nExtent);
    }

    WriteVarUIntSingleByte(&pabyData, MAKE_KEY(knLAYER_VERSION, WT_VARINT));
    WriteVarUInt(&pabyData, m_nVersion);

    CPLAssert(pabyData == *ppabyData + getSize());
    *ppabyData = pabyData;
}

/************************************************************************/
/*                             write()                                  */
/************************************************************************/

void MVTTileLayer::write(GByte* pabyData) const
{
    write(&pabyData);
}

/************************************************************************/
/*                             write()                                  */
/************************************************************************/

std::string MVTTileLayer::write() const
{
    std::string buffer;
    size_t nSize = getSize();
    buffer.resize(nSize);
    write( reinterpret_cast<GByte*>(&buffer[0]) );
    return buffer;
}

/************************************************************************/
/*                             read()                                   */
/************************************************************************/

bool MVTTileLayer::read(const GByte** ppabyData, const GByte* pabyDataLimit)
{
    const GByte* pabyData = *ppabyData;

    try
    {
        unsigned int nKey = 0;
        while( pabyData < pabyDataLimit )
        {
            READ_FIELD_KEY(nKey);
            if( nKey == MAKE_KEY(knLAYER_NAME, WT_DATA) )
            {
                char* pszLayerName = nullptr;
                READ_TEXT(pabyData, pabyDataLimit, pszLayerName);
                // cppcheck-suppress nullPointer
                setName(pszLayerName);
                CPLFree(pszLayerName);
            }
            else if( nKey == MAKE_KEY(knLAYER_FEATURES, WT_DATA) )
            {
                unsigned int nFeatureLength = 0;
                READ_SIZE(pabyData, pabyDataLimit, nFeatureLength);
                const GByte* pabyDataFeatureEnd = pabyData + nFeatureLength;
                std::shared_ptr<MVTTileLayerFeature> poFeature(
                    new MVTTileLayerFeature());
                addFeature(poFeature);
                if( !poFeature->read(&pabyData, pabyDataFeatureEnd) )
                    return false;
                pabyData = pabyDataFeatureEnd;
            }
            else if( nKey == MAKE_KEY(knLAYER_KEYS, WT_DATA) )
            {
                char* pszKey = nullptr;
                READ_TEXT(pabyData, pabyDataLimit, pszKey);
                // cppcheck-suppress nullPointer
                addKey(pszKey);
                CPLFree(pszKey);
            }
            else if( nKey == MAKE_KEY(knLAYER_VALUES, WT_DATA) )
            {
                unsigned int nValueLength = 0;
                READ_SIZE(pabyData, pabyDataLimit, nValueLength);
                const GByte* pabyDataValueEnd = pabyData + nValueLength;
                MVTTileLayerValue oValue;
                if( !oValue.read(&pabyData, pabyDataValueEnd) )
                    return false;
                addValue(oValue);
                pabyData = pabyDataValueEnd;
            }
            else if( nKey == MAKE_KEY(knLAYER_EXTENT, WT_VARINT) )
            {
                unsigned int nExtent = 0;
                READ_VARUINT32(pabyData, pabyDataLimit, nExtent);
                setExtent(nExtent);
            }
            else if( nKey == MAKE_KEY(knLAYER_VERSION, WT_VARINT) )
            {
                unsigned int nVersion = 0;
                READ_VARUINT32(pabyData, pabyDataLimit, nVersion);
                setVersion(nVersion);
            }
            else
            {
                SKIP_UNKNOWN_FIELD(pabyData, pabyDataLimit, FALSE);
            }
        }
        *ppabyData = pabyData;
        return true;
    }
    catch( const GPBException& )
    {
        return false;
    }
}

/************************************************************************/
/*                             read()                                   */
/************************************************************************/

bool MVTTileLayer::read(const GByte* pabyData, const GByte* pabyEnd)
{
    return read(&pabyData, pabyEnd);
}

/************************************************************************/
/*                             MVTTile()                                */
/************************************************************************/

MVTTile::MVTTile()
{
}

/************************************************************************/
/*                            addLayer()                                */
/************************************************************************/

void MVTTile::addLayer(std::shared_ptr<MVTTileLayer> poLayer)
{
    poLayer->setOwner(this);
    invalidateCachedSize();
    m_apoLayers.push_back(poLayer);
}

/************************************************************************/
/*                            getSize()                                 */
/************************************************************************/

size_t MVTTile::getSize() const
{
    if( m_bCachedSize )
        return m_nCachedSize;
    m_nCachedSize = 0;
    for( const auto& poLayer: m_apoLayers )
    {
        const size_t nLayerSize = poLayer->getSize();
        m_nCachedSize += knSIZE_KEY + GetVarUIntSize(nLayerSize) + nLayerSize;
    }
    m_bCachedSize = true;
    return m_nCachedSize;
}

/************************************************************************/
/*                             write()                                  */
/************************************************************************/

void MVTTile::write(GByte** ppabyData) const
{
    GByte* pabyData = *ppabyData;

    for( const auto& poLayer: m_apoLayers )
    {
        WriteVarUIntSingleByte(&pabyData, MAKE_KEY(knLAYER, WT_DATA));
        WriteVarUInt(&pabyData, poLayer->getSize());
        poLayer->write(&pabyData);
    }

    CPLAssert(pabyData == *ppabyData + getSize());
    *ppabyData = pabyData;
}

/************************************************************************/
/*                             write()                                  */
/************************************************************************/

void MVTTile::write(GByte* pabyData) const
{
    write(&pabyData);
}

/************************************************************************/
/*                             write()                                  */
/************************************************************************/

std::string MVTTile::write() const
{
    std::string buffer;
    size_t nSize = getSize();
    if( nSize )
    {
        buffer.resize(nSize);
        write( reinterpret_cast<GByte*>(&buffer[0]) );
    }
    return buffer;
}

#ifdef ADD_MVT_TILE_READ

/************************************************************************/
/*                             read()                                   */
/************************************************************************/

bool MVTTile::read(const GByte** ppabyData, const GByte* pabyDataLimit)
{
    const GByte* pabyData = *ppabyData;

    try
    {
        unsigned int nKey = 0;
        while( pabyData < pabyDataLimit )
        {
            READ_FIELD_KEY(nKey);
            if( nKey == MAKE_KEY(knLAYER, WT_DATA) )
            {
                unsigned int nLayerSize = 0;
                READ_SIZE(pabyData, pabyDataLimit, nLayerSize);
                const GByte* pabyDataLimitLayer = pabyData + nLayerSize;
                std::shared_ptr<MVTTileLayer> poLayer(new MVTTileLayer());
                addLayer(poLayer);
                if( !poLayer->read(&pabyData, pabyDataLimitLayer) )
                    return false;
                pabyData = pabyDataLimitLayer;
            }
            else
            {
                SKIP_UNKNOWN_FIELD(pabyData, pabyDataLimit, FALSE);
            }
        }
        *ppabyData = pabyData;
        return true;
    }
    catch( const GPBException& )
    {
        return false;
    }
}

/************************************************************************/
/*                             read()                                   */
/************************************************************************/

bool MVTTile::read(const GByte* pabyData, const GByte* pabyEnd)
{
    return read(&pabyData, pabyEnd);
}

#endif
