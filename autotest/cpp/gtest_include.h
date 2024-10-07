/******************************************************************************
 *
 * Project:  PROJ
 * Purpose:  Wrapper for gtest/gtest.h
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2018, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

// Disable all warnings for gtest.h, so as to be able to still use them for
// our own code.

#if defined(__GNUC__)
#pragma GCC system_header
#endif

#include "gtest/gtest.h"
