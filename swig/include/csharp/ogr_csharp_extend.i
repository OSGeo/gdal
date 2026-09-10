/******************************************************************************
 *
 * Name:     ogr_csharp_extend.i
 * Project:  GDAL CSharp Interface
 * Purpose:  C# specific OGR extensions.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Tamas Szekeres
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/


/******************************************************************************
 * OGR WKB export                                                             *
 *****************************************************************************/

%extend OGRGeometryShadow
{
%immutable;
    long long          WkbLongSize;
	OGRwkbGeometryType GeometryType;

%apply (size_t native_size) {(size_t length)};
    OGRErr ExportToWkb( size_t length, void *buffer_ptr, OGRwkbByteOrder byte_order = wkbNDR ) {
      if (length < OGR_G_WkbSizeEx( self )) {
        CPLError(CE_Failure, 1, "Array size is small (ExportToWkb).");
        return CE_Failure;
      }
      return OGR_G_ExportToWkb(self, byte_order, (unsigned char*) buffer_ptr );
    }
    OGRErr ExportToIsoWkb( size_t length, void *buffer_ptr, OGRwkbByteOrder byte_order = wkbNDR ) {
      if (length < OGR_G_WkbSizeEx( self )) {
        CPLError(CE_Failure, 1, "Array size is small (ExportToIsoWkb).");
        return CE_Failure;
      }
      return OGR_G_ExportToIsoWkb(self, byte_order, (unsigned char*) buffer_ptr );
    }
%clear (size_t length);
}

%{
  long long OGRGeometryShadow_WkbLongSize_get(OGRGeometryShadow *self) {
    return static_cast<long long>(OGR_G_WkbSizeEx(self));
  }
  OGRwkbGeometryType OGRGeometryShadow_GeometryType_get(OGRGeometryShadow *self) {
    return OGR_G_GetGeometryType(self);
  }
%}
