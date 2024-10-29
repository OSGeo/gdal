/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Generate a test .dgn file
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#if ((__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2)) &&               \
     !defined(_MSC_VER))
#define HAVE_GCC_SYSTEM_HEADER
#endif

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

#include "Ge/GeKnotVector.h"

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
#include "DgTagElement.h"
