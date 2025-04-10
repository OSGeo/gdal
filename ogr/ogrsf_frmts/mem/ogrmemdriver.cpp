/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMemDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "memdataset.h"

/************************************************************************/
/*                           RegisterOGRMem()                           */
/************************************************************************/

void RegisterOGRMEM()

{
    GDALRegister_MEM_Internal("Memory");
}
