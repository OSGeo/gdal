/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver, ZarrV3Codec class
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "zarr_v3_codec.h"

#include <cstdlib>
#include <limits>

/************************************************************************/
/*                            ZarrV3Codec()                             */
/************************************************************************/

ZarrV3Codec::ZarrV3Codec(const std::string &osName) : m_osName(osName)
{
}

/************************************************************************/
/*                            ~ZarrV3Codec()                            */
/************************************************************************/

ZarrV3Codec::~ZarrV3Codec() = default;

/************************************************************************/
/*                      ZarrV3Codec::ParseScalar()                      */
/************************************************************************/

/* static */ bool ZarrV3Codec::ParseScalar(const CPLJSONObject &oObj,
                                           const DtypeElt &oElt,
                                           std::vector<GByte> &abyVal)
{
    const GDALDataType eDT = oElt.gdalType.GetNumericDataType();
    const bool bIsFloat = oElt.nativeType == DtypeElt::NativeType::IEEEFP;
    abyVal.resize(GDALGetDataTypeSizeBytes(eDT));
    switch (oObj.GetType())
    {
        case CPLJSONObject::Type::Integer:
        case CPLJSONObject::Type::Long:
        case CPLJSONObject::Type::Double:
        {
            if (eDT == GDT_Int64 &&
                oObj.GetType() != CPLJSONObject::Type::Double)
            {
                const int64_t nVal = oObj.ToLong();
                memcpy(abyVal.data(), &nVal, sizeof(nVal));
                return true;
            }
            if (eDT == GDT_UInt64 &&
                oObj.GetType() != CPLJSONObject::Type::Double)
            {
                const uint64_t nVal = oObj.ToUInt64();
                memcpy(abyVal.data(), &nVal, sizeof(nVal));
                return true;
            }
            const double dfVal = oObj.ToDouble();
            if (!bIsFloat && !GDALIsValueExactAs(dfVal, eDT))
                return false;
            GDALCopyWords64(&dfVal, GDT_Float64, 0, abyVal.data(), eDT, 0, 1);
            return true;
        }

        case CPLJSONObject::Type::String:
        {
            if (!bIsFloat)
                return false;
            const std::string osVal = oObj.ToString();
            if (STARTS_WITH(osVal.c_str(), "0x"))
            {
                // Hexadecimal representation of the bit pattern
                if (osVal.size() != 2 + 2 * abyVal.size())
                    return false;
                const uint64_t nBits = static_cast<uint64_t>(
                    std::strtoull(osVal.c_str() + 2, nullptr, 16));
                if (abyVal.size() == sizeof(uint16_t))
                {
                    const uint16_t nTmp = static_cast<uint16_t>(nBits);
                    memcpy(abyVal.data(), &nTmp, sizeof(nTmp));
                }
                else if (abyVal.size() == sizeof(uint32_t))
                {
                    const uint32_t nTmp = static_cast<uint32_t>(nBits);
                    memcpy(abyVal.data(), &nTmp, sizeof(nTmp));
                }
                else
                {
                    memcpy(abyVal.data(), &nBits, sizeof(nBits));
                }
                return true;
            }
            double dfVal;
            if (osVal == "NaN")
                dfVal = std::numeric_limits<double>::quiet_NaN();
            else if (osVal == "Infinity" || osVal == "+Infinity")
                dfVal = std::numeric_limits<double>::infinity();
            else if (osVal == "-Infinity")
                dfVal = -std::numeric_limits<double>::infinity();
            else
                return false;
            GDALCopyWords64(&dfVal, GDT_Float64, 0, abyVal.data(), eDT, 0, 1);
            return true;
        }

        default:
            break;
    }
    return false;
}

/************************************************************************/
/*                     ZarrV3Codec::DecodePartial()                     */
/************************************************************************/

bool ZarrV3Codec::DecodePartial(VSIVirtualHandle *,
                                const ZarrByteVectorQuickResize &,
                                ZarrByteVectorQuickResize &,
                                std::vector<size_t> &, std::vector<size_t> &)
{
    // Normally we should not hit that...
    CPLError(CE_Failure, CPLE_NotSupported,
             "Codec %s does not support partial decoding", m_osName.c_str());
    return false;
}
