/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Includes Teigha headers
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2011,  Frank Warmerdam
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
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

#endif // TEIGHA_HEADERS_H
