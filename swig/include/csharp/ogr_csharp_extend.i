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
    %apply (int nList, char *pList) {(int buffer, char *pBuffer)};
    OGRErr ExportToWkb( int buffer, char *pBuffer, OGRwkbByteOrder byte_order = wkbXDR ) {
      if (buffer < OGR_G_WkbSize( self )) {
        CPLError(CE_Failure, 1, "Array size is small (ExportToWkb).");
        return CE_Failure;
      }
      return OGR_G_ExportToWkb(self, byte_order, (unsigned char*) pBuffer );
    }
    %clear (int buffer, char *pBuffer);
}
