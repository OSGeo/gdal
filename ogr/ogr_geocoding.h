/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Client of geocoding service.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_GEOCODING_H_INCLUDED
#define OGR_GEOCODING_H_INCLUDED

#include "cpl_port.h"
#include "ogr_api.h"

/**
 * \file ogr_geocoding.h
 *
 * C API for geocoding client.
 */

CPL_C_START

/** Opaque type for a geocoding session */
typedef struct _OGRGeocodingSessionHS *OGRGeocodingSessionH;

OGRGeocodingSessionH CPL_DLL OGRGeocodeCreateSession(char **papszOptions);

void CPL_DLL OGRGeocodeDestroySession(OGRGeocodingSessionH hSession);

OGRLayerH CPL_DLL OGRGeocode(OGRGeocodingSessionH hSession,
                             const char *pszQuery, char **papszStructuredQuery,
                             char **papszOptions);

OGRLayerH CPL_DLL OGRGeocodeReverse(OGRGeocodingSessionH hSession, double dfLon,
                                    double dfLat, char **papszOptions);

void CPL_DLL OGRGeocodeFreeResult(OGRLayerH hLayer);

CPL_C_END

#endif  // OGR_GEOCODING_H_INCLUDED
