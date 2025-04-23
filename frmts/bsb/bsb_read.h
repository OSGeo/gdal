/******************************************************************************
 *
 * Project:  BSB Reader
 * Purpose:  non-GDAL BSB API Declarations
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef BSBREAD_H_INCLUDED
#define BSBREAD_H_INCLUDED

#include "cpl_port.h"
#include "cpl_vsi.h"

CPL_C_START

typedef struct
{
    VSILFILE *fp;

    GByte *pabyBuffer;
    int nBufferOffset;
    int nBufferSize;
    int nBufferAllocation;
    int nSavedCharacter;
    int nSavedCharacter2;

    int nXSize;
    int nYSize;

    int nPCTSize;
    unsigned char *pabyPCT;

    char **papszHeader;

    int *panLineOffset;

    int nColorSize;

    int nVersion; /* times 100 */

    int bNO1;

    int bNewFile;
    int nLastLineWritten;
} BSBInfo;

BSBInfo CPL_DLL *BSBOpen(const char *pszFilename);
int CPL_DLL BSBReadScanline(BSBInfo *psInfo, int nScanline,
                            unsigned char *pabyScanlineBuf);
void CPL_DLL BSBClose(BSBInfo *psInfo);

BSBInfo CPL_DLL *BSBCreate(const char *pszFilename, int nCreationFlags,
                           int nVersion, int nXSize, int nYSize);
int CPL_DLL BSBWritePCT(BSBInfo *psInfo, int nPCTSize, unsigned char *pabyPCT);
int CPL_DLL BSBWriteScanline(BSBInfo *psInfo, unsigned char *pabyScanlineBuf);

CPL_C_END

#endif /* ndef BSBREAD_H_INCLUDED */
