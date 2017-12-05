/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGR Driver for DGNv8
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
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
 ****************************************************************************/

#ifndef DGNV8_HEADERS_H
#define DGNV8_HEADERS_H

#include "cpl_port.h"

#ifdef HAVE_GCC_SYSTEM_HEADER
#pragma GCC system_header
#endif

#include "OdaCommon.h"

#include "StaticRxObject.h"
#include "RxInit.h"
#include "RxDynamicModule.h"
#include "DynamicLinker.h"
#include "DgDatabase.h"
#include "RxDynamicModule.h"

#include "DgGraphicsElement.h"
#include "DgComplexCurve.h"

#include "ExDgnServices.h"
#include "ExDgnHostAppServices.h"

#include "DgSummaryInfo.h"
#include "Gs/GsBaseInclude.h"

#include "DgArc.h"
#include "DgAttributeLinkage.h"
#include "DgBSplineCurve.h"
#include "DgCellHeader.h"
#include "DgColorTable.h"
#include "DgComplexCurve.h"
#include "DgComplexShape.h"
#include "DgComplexString.h"
#include "DgCurve.h"
#include "DgCurveElement2d.h"
#include "DgCurveElement3d.h"
#include "DgElementIterator.h"
#include "DgEllipse.h"
#include "DgFontTableRecord.h"
#include "DgLine.h"
#include "DgLineString.h"
#include "DgMultiline.h"
#include "DgPointString.h"
#include "DgShape.h"
#include "DgSharedCellReference.h"
#include "DgText.h"
#include "DgTextNode.h"

#endif // DGNV8_HEADERS_H
