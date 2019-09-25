/*****************************************************************************
* Copyright 2016 Pitney Bowes Inc.
*
* Licensed under the MIT License (the “License”); you may not use this file
* except in the compliance with the License.
* You may obtain a copy of the License at https://opensource.org/licenses/MIT

* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an “AS IS” WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*****************************************************************************/

#include "ogrsf_frmts.h"

#ifndef OGR_GEOPACKAGEUTILITY_H_INCLUDED
#define OGR_GEOPACKAGEUTILITY_H_INCLUDED

typedef struct
{
    OGRBoolean bEmpty;
    OGRBoolean bExtended;
    OGRwkbByteOrder eByteOrder;
    int iSrsId;
    bool bExtentHasXY;
    bool bExtentHasZ;
#ifdef notdef
    bool bExtentHasM;
#endif
    double MinX, MaxX, MinY, MaxY, MinZ, MaxZ;
#ifdef notdef
    double MinM, MaxM;
#endif
    size_t nHeaderLen;
} GPkgHeader;

OGRFieldType        GPkgFieldToOGR(const char *pszGpkgType, OGRFieldSubType& eSubType, int& nMaxWidth);
const char*         GPkgFieldFromOGR(OGRFieldType eType, OGRFieldSubType eSubType, int nMaxWidth);
OGRwkbGeometryType  GPkgGeometryTypeToWKB(const char *pszGpkgType, bool bHasZ, bool bHasM);

GByte*              GPkgGeometryFromOGR(const OGRGeometry *poGeometry, int iSrsId, size_t *pnWkbLen);
OGRGeometry*        GPkgGeometryToOGR(const GByte *pabyGpkg, size_t nGpkgLen, OGRSpatialReference *poSrs);

OGRErr              GPkgHeaderFromWKB(const GByte *pabyGpkg, size_t nGpkgLen, GPkgHeader *poHeader);

#endif
