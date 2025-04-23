/**********************************************************************
 *
 * Project:  GDAL
 * Purpose:  Display resource limits
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_multiproc.h"
#include "cpl_vsi.h"

#include <stdbool.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    bool bCPUs = argc == 1;
    bool bUsableRAM = argc == 1;
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--cpus") == 0)
            bCPUs = true;
        else if (strcmp(argv[i], "--usable-ram") == 0)
            bUsableRAM = true;
    }
    if (bCPUs)
        printf("CPLGetNumCPUs = %d\n", CPLGetNumCPUs());
    if (bUsableRAM)
    {
        GIntBig nUsableRAM = CPLGetUsablePhysicalRAM();
        int nUsableRAM_GB = (int)(nUsableRAM >> 30);
        if (nUsableRAM_GB > 1)
            printf("CPLGetUsablePhysicalRAM = %d GB\n", nUsableRAM_GB);
        else
        {
            int nUsableRAM_MB = (int)(nUsableRAM >> 20);
            printf("CPLGetUsablePhysicalRAM = %d MB\n", nUsableRAM_MB);
        }
    }
    return 0;
}
