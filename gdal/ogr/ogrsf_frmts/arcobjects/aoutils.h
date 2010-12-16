
#ifndef _AO_UTILS_H_INCLUDED
#define _AO_UTILS_H_INCLUDED

#include "ogr_ao.h"
#include <iostream>

// ArcObjects to OGR Geometry Mapping 
bool AOToOGRGeometry(IGeometryDef* pGeoDef, OGRwkbGeometryType* outOGRType);
bool AOGeometryToOGRGeometry(bool forceMulti, esriGeometry::IGeometry* pInAOGeo, OGRSpatialReference* pOGRSR, unsigned char* & pInWorkingBuffer, long & inOutBufferSize, OGRGeometry** ppOutGeometry); //working buffer is an optimization to avoid reallocating mem
bool AOToOGRSpatialReference(esriGeometry::ISpatialReference* pSR, OGRSpatialReference** ppSR);
bool OGRGeometryToAOGeometry(OGRGeometry* pOGRGeom, esriGeometry::IGeometry** ppGeometry);


// ArcObjects to OGR Field Mapping
bool AOToOGRFields(IFields* pFields, OGRFeatureDefn* pOGRFeatureDef, std::vector<long> & ogrToESRIFieldMapping);
bool AOToOGRFieldType(esriFieldType aoType, OGRFieldType* ogrType);


// COM error to OGR
bool AOErr(HRESULT hr, std::string desc);

// Init driver and check out license
bool InitializeDriver(esriLicenseExtensionCode license = 
                   (esriLicenseExtensionCode)0);

// Exit app and check in license
HRESULT ShutdownDriver(esriLicenseExtensionCode license = 
                    (esriLicenseExtensionCode)0);


// Helper functions to initialize
bool InitAttemptWithoutExtension(esriLicenseProductCode product);
bool InitAttemptWithExtension(esriLicenseProductCode product,
                              esriLicenseExtensionCode extension);
int GetInitedProductCode();


#endif
