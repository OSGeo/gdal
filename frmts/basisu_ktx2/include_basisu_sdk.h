/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Basis Universal / KTX2 driver.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"

#ifdef HAVE_GCC_SYSTEM_HEADER
#pragma GCC system_header
#endif

#include "basisu/basisu_comp.h"
#include "basisu/basisu_enc.h"
#include "basisu/basisu_transcoder.h"
