/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Defines GeoJSON reader within OGR OGRGeoJSON Driver.
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_GEOJSONWRITER_H_INCLUDED
#define OGR_GEOJSONWRITER_H_INCLUDED

/*! @cond Doxygen_Suppress */

#include "ogr_core.h"

#include "cpl_json_header.h"
#include "cpl_string.h"

class OGRFeature;
class OGRGeometry;
class OGRPolygon;

/************************************************************************/
/*                 GeoJSON Geometry Translators                         */
/************************************************************************/

class CPL_DLL OGRGeoJSONWriteOptions
{
  public:
    bool bWriteBBOX = false;
    bool bBBOXRFC7946 = false;
    int nXYCoordPrecision = -1;
    int nZCoordPrecision = -1;
    int nSignificantFigures = -1;
    bool bPolygonRightHandRule = false;
    bool bCanPatchCoordinatesWithNativeData = true;
    bool bHonourReservedRFC7946Members = false;
    CPLString osIDField{};
    bool bForceIDFieldType = false;
    bool bGenerateID = false;
    OGRFieldType eForcedIDFieldType = OFTString;
    bool bAllowNonFiniteValues = false;
    bool bAutodetectJsonStrings = true;

    void SetRFC7946Settings();
    void SetIDOptions(CSLConstList papszOptions);
};

OGREnvelope3D CPL_DLL OGRGeoJSONGetBBox(const OGRGeometry *poGeometry,
                                        const OGRGeoJSONWriteOptions &oOptions);

json_object CPL_DLL *
OGRGeoJSONWriteFeature(OGRFeature *poFeature,
                       const OGRGeoJSONWriteOptions &oOptions);

void OGRGeoJSONWriteId(const OGRFeature *poFeature, json_object *poObj,
                       bool bIdAlreadyWritten,
                       const OGRGeoJSONWriteOptions &oOptions);

json_object *OGRGeoJSONWriteAttributes(
    OGRFeature *poFeature, bool bWriteIdIfFoundInAttributes = true,
    const OGRGeoJSONWriteOptions &oOptions = OGRGeoJSONWriteOptions());

json_object CPL_DLL *
OGRGeoJSONWriteGeometry(const OGRGeometry *poGeometry,
                        const OGRGeoJSONWriteOptions &oOptions);

json_object *OGRGeoJSONWritePolygon(const OGRPolygon *poPolygon,
                                    const OGRGeoJSONWriteOptions &oOptions);

/*! @endcond */

#endif /* OGR_GEOJSONWRITER_H_INCLUDED */
