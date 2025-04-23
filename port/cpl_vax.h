/******************************************************************************
 *
 * Project:  CPL
 * Purpose:  Convert between VAX and IEEE floating point formats
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Avenza Systems Inc, http://www.avenza.com/
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_VAX_H
#define CPL_VAX_H

#include "cpl_port.h"

void CPL_DLL CPLVaxToIEEEDouble(void *);
void CPL_DLL CPLIEEEToVaxDouble(void *);

void CPL_DLL CPLVaxToIEEEFloat(void *);
void CPL_DLL CPLIEEEToVaxFloat(void *);

#endif  // CPL_VAX_H
