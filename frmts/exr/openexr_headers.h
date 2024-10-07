/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Includes OpenEXR headers
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2020, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#ifndef OPENEXR_HEADERS_H
#define OPENEXR_HEADERS_H

#include "cpl_port.h"

#ifdef HAVE_GCC_SYSTEM_HEADER
#pragma GCC system_header
#endif

#ifdef _MSC_VER
#pragma warning(push)
// conversion from 'int' to 'unsigned short', possible loss of data
#pragma warning(disable : 4244)
#endif

#include "ImathMatrix.h"
#include "ImfChannelList.h"
#include "ImfFloatAttribute.h"
#include "ImfInputPart.h"
#include "ImfOutputPart.h"
#include "ImfMatrixAttribute.h"
#include "ImfMultiPartInputFile.h"
#include "ImfMultiPartOutputFile.h"
#include "ImfPartType.h"
#include "ImfPreviewImage.h"
#include "ImfRgbaFile.h"
#include "ImfStringAttribute.h"
#include "ImfTiledInputPart.h"
#include "ImfTiledOutputPart.h"

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
