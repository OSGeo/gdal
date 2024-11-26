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
    %apply (void *buffer_ptr) {char *buffer};
    OGRErr ExportToWkb( int bufLen, char *buffer, OGRwkbByteOrder byte_order ) {
      if (bufLen < OGR_G_WkbSize( self ))
        CPLError(CE_Failure, 1, "Array size is small (ExportToWkb).");
      return OGR_G_ExportToWkb(self, byte_order, (unsigned char*) buffer );
    }
    %clear char *buffer;
}
