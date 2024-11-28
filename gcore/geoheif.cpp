/**********************************************************************
 *
 * Name:     geoheif.cpp
 * Project:  GDAL
 * Purpose:  OGC GeoHEIF shared implementation.
 * Author:   Brad Hards <bradh@silvereye.tech>
 *
 **********************************************************************
 * Copyright (c) 2024, Brad Hards
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#include "geoheif.h"

namespace gdal
{

//! @cond Doxygen_Suppress

GeoHEIF::GeoHEIF() : gcps(0)
{
}

GeoHEIF::~GeoHEIF()
{
}

bool GeoHEIF::has_SRS() const
{
    return !m_oSRS.IsEmpty();
}

bool GeoHEIF::has_GCPs() const
{
    return haveGCPs;
}

static double to_double(const uint8_t *data, uint32_t index)
{
    double d = 0;
    memcpy(&d, data + index, sizeof(d));
    CPL_MSBPTR64(&d);
    return d;
}

static double int_as_double(const uint8_t *data, uint32_t index)
{
    uint32_t v = 0;
    memcpy(&v, data + index, sizeof(uint32_t));
    CPL_MSBPTR32(&v);
    return static_cast<double>(v);
}

void GeoHEIF::setModelTransformation(const uint8_t *payload, size_t length)
{
    // TODO: this only handles the 2D case.
    if (length != (6 * 8 + 4))
    {
        return;
    }
    // Match version
    if (payload[0] == 0x00)
    {
        uint32_t index = 0;
        if (payload[index + 3] == 0x01)
        {
            index += 4;
            modelTransform[1] = to_double(payload, index);
            index += 8;
            modelTransform[2] = to_double(payload, index);
            index += 8;
            modelTransform[0] = to_double(payload, index);
            index += 8;
            modelTransform[4] = to_double(payload, index);
            index += 8;
            modelTransform[5] = to_double(payload, index);
            index += 8;
            modelTransform[3] = to_double(payload, index);
        }
    }
    else
    {
        CPLDebug("GeoHEIF", "Unsupported mtxf version %d", payload[0]);
    }
}

CPLErr GeoHEIF::GetGeoTransform(double *padfTransform) const
{
    std::vector<int> axes =
        has_SRS() ? m_oSRS.GetDataAxisToSRSAxisMapping() : std::vector<int>();

    if (axes.size() && axes[0] != 1)
    {
        padfTransform[1] = modelTransform[4];
        padfTransform[2] = modelTransform[5];
        padfTransform[0] = modelTransform[3];
        padfTransform[4] = modelTransform[1];
        padfTransform[5] = modelTransform[2];
        padfTransform[3] = modelTransform[0];
    }
    else
    {
        padfTransform[1] = modelTransform[1];
        padfTransform[2] = modelTransform[2];
        padfTransform[0] = modelTransform[0];
        padfTransform[4] = modelTransform[4];
        padfTransform[5] = modelTransform[5];
        padfTransform[3] = modelTransform[3];
    }
    return CE_None;
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/
const OGRSpatialReference *GeoHEIF::GetSpatialRef() const
{
    return !m_oSRS.IsEmpty() ? &m_oSRS : nullptr;
}

void GeoHEIF::extractSRS(const uint8_t *payload, size_t length) const
{
    // TODO: more sophisticated length checks
    if (length < 12)
    {
        CPLDebug("GeoHEIF", "Infeasible length CRS payload %u",
                 static_cast<unsigned>(length));
        return;
    }
    std::string crsEncoding(payload + 4, payload + 8);
    std::string crs(payload + 8, payload + length);
    if (crsEncoding == "wkt2")
    {
        m_oSRS.importFromWkt(crs.c_str());
    }
    else if (crsEncoding == "crsu")
    {
        m_oSRS.importFromCRSURL(crs.c_str());
    }
    else if (crsEncoding == "curi")
    {
        // null terminated string in the form "[EPSG:4326]"
        if ((crs.at(0) != '[') || (crs.at(crs.length() - 2) != ']') ||
            (crs.at(crs.length() - 1) != '\0'))
        {
            CPLDebug("GeoHEIF", "CRS CURIE is not a safe CURIE");
            return;
        }
        std::string curie = crs.substr(1, crs.length() - 3);
        size_t separatorPos = curie.find(':');
        if (separatorPos == std::string::npos)
        {
            CPLDebug("GeoHEIF",
                     "CRS CURIE does not contain required separator");
            return;
        }
        std::string authority = curie.substr(0, separatorPos);
        std::string code = curie.substr(separatorPos + 1);
        std::string osURL("http://www.opengis.net/def/crs/");
        osURL.append(authority);
        osURL += "/0/";
        osURL.append(code);
        m_oSRS.importFromCRSURL(osURL.c_str());
    }
    else
    {
        CPLDebug("GeoHEIF", "CRS encoding is not supported");
        return;
    }
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
}

void GeoHEIF::addGCPs(const uint8_t *data, size_t length)
{
    constexpr size_t MIN_VALID_SIZE = sizeof(uint32_t) + sizeof(uint16_t) +
                                      2 * sizeof(uint32_t) + 2 * sizeof(double);
    if (length < MIN_VALID_SIZE)
    {
        CPLDebug("GeoHEIF", "GCP data length is too short");
        return;
    }
    if (data[0] == 0x00)
    {
        uint32_t index = 0;
        bool is_3D = (data[index + 3] == 0x00);
        if (is_3D)
        {
            if (length < MIN_VALID_SIZE + sizeof(double))
            {
                CPLDebug("GeoHEIF", "GCP data length is too short for 3D");
                return;
            }
        }
        index += sizeof(uint32_t);
        uint16_t count = (data[index] << 8) + (data[index + 1]);
        index += sizeof(uint16_t);
        for (uint16_t j = 0; (j < count) && (index < length); j++)
        {
            double dfGCPPixel = int_as_double(data, index);
            index += sizeof(uint32_t);
            double dfGCPLine = int_as_double(data, index);
            index += sizeof(uint32_t);
            double dfGCPX = to_double(data, index);
            index += sizeof(double);
            double dfGCPY = to_double(data, index);
            index += sizeof(double);
            double dfGCPZ = 0.0;
            if (is_3D)
            {
                dfGCPZ = to_double(data, index);
                index += sizeof(double);
            }
            gcps.emplace_back("", "", dfGCPPixel, dfGCPLine, dfGCPX, dfGCPY,
                              dfGCPZ);
            haveGCPs = true;
        }
    }
    else
    {
        CPLDebug("GeoHEIF", "Unsupported tiep version %d", data[0]);
    }
}

int GeoHEIF::GetGCPCount() const
{
    return static_cast<int>(gcps.size());
}

const GDAL_GCP *GeoHEIF::GetGCPs()
{
    return gdal::GCP::c_ptr(gcps);
}

const OGRSpatialReference *GeoHEIF::GetGCPSpatialRef() const
{
    return this->GetSpatialRef();
}

}  // namespace gdal

//! @endcond
