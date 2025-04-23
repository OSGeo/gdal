// SPDX-License-Identifier: MIT
// Copyright 2007, Mateusz Loskot
// Copyright 2008-2024, Even Rouault <even.rouault at spatialys.com>

#ifndef OGRLIBJSONUTILS_H_INCLUDED
#define OGRLIBJSONUTILS_H_INCLUDED

/*! @cond Doxygen_Suppress */

#include "cpl_port.h"
#include "cpl_json_header.h"

#include "ogr_api.h"

bool CPL_DLL OGRJSonParse(const char *pszText, json_object **ppoObj,
                          bool bVerboseError = true);

json_object CPL_DLL *CPL_json_object_object_get(struct json_object *obj,
                                                const char *key);
json_object CPL_DLL *json_ex_get_object_by_path(json_object *poObj,
                                                const char *pszPath);

/************************************************************************/
/*                 GeoJSON Parsing Utilities                            */
/************************************************************************/

lh_entry CPL_DLL *OGRGeoJSONFindMemberEntryByName(json_object *poObj,
                                                  const char *pszName);
json_object CPL_DLL *OGRGeoJSONFindMemberByName(json_object *poObj,
                                                const char *pszName);

/************************************************************************/
/*                           GeoJSONPropertyToFieldType                 */
/************************************************************************/

OGRFieldType CPL_DLL GeoJSONPropertyToFieldType(json_object *poObject,
                                                OGRFieldSubType &eSubType,
                                                bool bArrayAsString = false);

CPL_C_START
/* %.XXXf formatting */
json_object CPL_DLL *json_object_new_double_with_precision(double dfVal,
                                                           int nCoordPrecision);

/* %.XXXg formatting */
json_object CPL_DLL *
json_object_new_double_with_significant_figures(double dfVal,
                                                int nSignificantFigures);
CPL_C_END

/*! @endcond */

#endif
