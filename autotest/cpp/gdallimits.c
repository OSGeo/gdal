/**********************************************************************
 *
 * Project:  GDAL
 * Purpose:  Display resource limits
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
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
