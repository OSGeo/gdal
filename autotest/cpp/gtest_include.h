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

#if defined(__GNUC__)
// Workaround https://github.com/google/googletest/issues/4701
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcpp"
#endif

#include "gtest/gtest.h"

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
