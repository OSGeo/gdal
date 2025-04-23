/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGR Driver for DGNv8
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
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

#endif  // DGNV8_HEADERS_H
