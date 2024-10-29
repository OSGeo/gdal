/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Includes Teigha headers
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2011,  Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#ifndef TEIGHA_HEADERS_H
#define TEIGHA_HEADERS_H

#include "cpl_port.h"

#ifdef HAVE_GCC_SYSTEM_HEADER
#pragma GCC system_header
#endif

#include "OdaCommon.h"
#include "diagnostics.h"
#include "DbDatabase.h"
#include "DbEntity.h"
#include "DbDimAssoc.h"
#include "DbObjectIterator.h"
#include "DbBlockTable.h"
#include "DbBlockTableRecord.h"
#include "DbSymbolTable.h"

#include "DbLayerTable.h"
#include "DbLayerTableRecord.h"
#include "DbLinetypeTable.h"
#include "DbLinetypeTableRecord.h"

#include "DbPolyline.h"
#include "Db2dPolyline.h"
#include "DbAttributeDefinition.h"
#include "Db3dPolyline.h"
#include "Db3dPolylineVertex.h"
#include "DbLine.h"
#include "DbPoint.h"
#include "DbEllipse.h"
#include "DbArc.h"
#include "DbMText.h"
#include "DbText.h"
#include "DbCircle.h"
#include "DbSpline.h"
#include "DbFace.h"
#include "DbBlockReference.h"
#include "DbAttribute.h"
#include "DbFiler.h"
#include "Ge/GeScale3d.h"

#include "DbDimension.h"
#include "DbRotatedDimension.h"
#include "DbAlignedDimension.h"

#include "DbHatch.h"
#include "Ge/GePoint2dArray.h"
#include "Ge/GeCurve2d.h"
#include "Ge/GeCircArc2d.h"
#include "Ge/GeEllipArc2d.h"

#include "OdCharMapper.h"
#include "RxObjectImpl.h"

#include "ExSystemServices.h"
#include "ExHostAppServices.h"
#include "OdFileBuf.h"
#include "RxDynamicModule.h"
#include "FdField.h"

#endif  // TEIGHA_HEADERS_H
