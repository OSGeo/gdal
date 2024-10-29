/******************************************************************************
 * $Id$
 *
 * Project:  FIT Driver
 * Purpose:  Implement FIT Support - not using the SGI iflFIT library.
 * Author:   Philip Nemec, nemec@keyholecorp.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Keyhole, Inc.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef gstTypes_h_
#define gstTypes_h_

#include <stdarg.h>
#include "cpl_conv.h"

typedef int (*gstItemGetFunc)(void *data, int tag, ...);

typedef GUInt16 uint16;
typedef GInt16 int16;
typedef GUInt32 uint32;
typedef GInt32 int32;
typedef GUIntBig uint64;
typedef GIntBig int64;

typedef unsigned char uchar;

#endif  // !gstTypes_h_
