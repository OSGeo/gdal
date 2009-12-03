/******************************************************************************
 * File :    wktrasterwrapper.cpp
 * Project:  WKT Raster driver
 * Purpose:  Implementation of a wrapper around the WKTRaster and its bands
 * Author:   Jorge Arevalo, jorgearevalo@gis4free.org
 *
 * Last changes: $Id$
 *
 ******************************************************************************
 * Copyright (c) 2009, Jorge Arevalo, jorgearevalo@gis4free.org
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************/
#include "wktraster.h"

CPL_CVSID("$Id$");

/************************************************************************
 * ====================================================================
 *                            WKTRasterWrapper
 * ====================================================================
 *
 * This class wraps the HEXWKB representation of a PostGIS WKT Raster.
 *
 * It splits the hexwkb string into fields, and reconstructs this string
 * from the fields each time that the representation is required (see
 * GetBinaryRepresentation method).
 *
 * The best way to get the representation of the raster is by using the
 * GetBinaryRepresentation and GetHexWkbRepresentation methods. This
 * methods construct the representation based on the class properties.
 *
 * If you access the pszHexWkb or pbyHexWkb variables directly, you may
 * get a non-updated version of the raster. Anyway, you only can access
 * this variables from friend classes.
 *
 * TODO:
 * - Check assertions that sometimes fail (in several places of the code):
 *   CPLAssert(nTransformedBytes == nLengthByWkbString)
 * - Check that there is enough data in buffer (method Initialize)
 * - FIXME: dfNoDataValue is a double. Doesn't make sense to fetch less than 8 bytes
 *   if 1 byte, should be put into a Byte and then casted to a double,
 *   if 2 bytes, should be put into a Int16/UInt16 and then casted to a double, etc...
 * - Modify GetWktExtent method to accept non-regular blocking
 ************************************************************************/

/**
 * Class constructor.
 */
WKTRasterWrapper::WKTRasterWrapper() {
    papoBands = NULL;
    pszHexWkb = NULL;
    pbyHexWkb = NULL;
    pszWktExtent = NULL;
}

/**
 * Fill all the raster properties with the string
 * hexwkb representation given as input.
 * This method swaps words if the raster endianess is distinct from
 * the machine endianess
 * Properties:
 *  const char *: the string hexwkb representation of the raster
 */
int WKTRasterWrapper::Initialize(const char* pszHex) { 
    int nBufferDataWithoutHeaderLen = 0;
    int nRasterHeaderLen = 0; 
    int nRasterBandHeaderLen = 0; 
    int nRasterDataLen = 0;
    GByte * pbyAuxPtr;
    GByte byMachineEndianess = NDR; // by default

    // Check machine endianess
#ifdef CPL_LSB
    byMachineEndianess = NDR;
#else
    byMachineEndianess = XDR;
#endif

    /*************************************************************************
     *  Check parameters
     *************************************************************************/
    if (pszHex == NULL || strlen(pszHex) % 2) {
        CPLError(CE_Failure, CPLE_NotSupported,
                "Couldn't create raster wrapper, invalid raster hexwkb string");
        return FALSE;
    }

    /*************************************************************************
     * Check if raster has enough data
     *************************************************************************/
    nRasterHeaderLen =
        sizeof (GByte) +
        4 * sizeof (GUInt16) +
 	sizeof (GInt32) +
 	6 * sizeof (double);

    nBufferDataWithoutHeaderLen =
        strlen(pszHex +2*nRasterHeaderLen) / 2;

    pbyHexWkb = CPLHexToBinary(pszHex, &nLengthByWkbString);    
    
    if (nRasterHeaderLen > nLengthByWkbString || 
        nLengthByWkbString != nRasterHeaderLen + nBufferDataWithoutHeaderLen)
    {        
        CPLError(CE_Failure, CPLE_ObjectNull,
                "Raster object is corrupted, not enough data");
        return FALSE;
    }


    /*************************************************************************
     * Copy raster as class attribute, and transform it to binary
     *************************************************************************/
    nLengthHexWkbString = strlen(pszHex);   

    pszHexWkb = (char *) VSIMalloc(nLengthHexWkbString);
    if (pszHexWkb == NULL) {        
        CPLError(CE_Failure, CPLE_ObjectNull,
                "Couldn't allocate memory for raster wrapper, aborting");
        return FALSE;
    }

    memcpy(pszHexWkb, pszHex, nLengthHexWkbString * sizeof (GByte));

   
    /***********************************************************************
     * Get endianess. This is important, because we could need to swap
     * words if the data endianess is distinct from machine endianess
     ***********************************************************************/
    memcpy(&byEndianess, pbyHexWkb, sizeof(GByte));

    // We are going to use this pointer to move over the string
    pbyAuxPtr = pbyHexWkb + sizeof (GByte);

    /*************************************************************************
     * Parse HexWkb string in binary format and fill the rest of class fields
     *************************************************************************/
    memcpy(&nVersion, pbyAuxPtr, sizeof (GUInt16));
    if (byEndianess != byMachineEndianess)
        GDALSwapWords(&nVersion, sizeof (GUInt16), 1, sizeof (GUInt16));
    pbyAuxPtr += sizeof (GUInt16);


    /**
     * Check WKT Raster version
     */
    if (nVersion != WKT_RASTER_VERSION) {
        CPLError(CE_Failure, CPLE_NotSupported,
                "WKT Raster version not supported (%d). Supported raster\
                version is %d\n", nVersion, WKT_RASTER_VERSION);

        return FALSE;
    }


    memcpy(&nBands, pbyAuxPtr, sizeof (GUInt16));
    if (byEndianess != byMachineEndianess)
        GDALSwapWords(&nBands, sizeof (GUInt16), 1, sizeof (GUInt16));
    pbyAuxPtr += sizeof (GUInt16);

    memcpy(&dfScaleX, pbyAuxPtr, sizeof (double));
    if (byEndianess != byMachineEndianess)
        GDALSwapWords(&dfScaleX, sizeof (double), 1, sizeof (double));
    pbyAuxPtr += sizeof (double);

    memcpy(&dfScaleY, pbyAuxPtr, sizeof (double));
    if (byEndianess != byMachineEndianess)
        GDALSwapWords(&dfScaleY, sizeof (double), 1, sizeof (double));
    pbyAuxPtr += sizeof (double);

    memcpy(&dfIpX, pbyAuxPtr, sizeof (double));
    if (byEndianess != byMachineEndianess)
        GDALSwapWords(&dfIpX, sizeof (double), 1, sizeof (double));
    pbyAuxPtr += sizeof (double);

    memcpy(&dfIpY, pbyAuxPtr, sizeof (double));
    if (byEndianess != byMachineEndianess)
        GDALSwapWords(&dfIpY, sizeof (double), 1, sizeof (double));
    pbyAuxPtr += sizeof (double);

    memcpy(&dfSkewX, pbyAuxPtr, sizeof (double));
    if (byEndianess != byMachineEndianess)
        GDALSwapWords(&dfSkewX, sizeof (double), 1, sizeof (double));
    pbyAuxPtr += sizeof (double);

    memcpy(&dfSkewY, pbyAuxPtr, sizeof (double));
    if (byEndianess != byMachineEndianess)
        GDALSwapWords(&dfSkewY, sizeof (double), 1, sizeof (double));
    pbyAuxPtr += sizeof (double);

    memcpy(&nSrid, pbyAuxPtr, sizeof (GInt32));
    if (byEndianess != byMachineEndianess)
        GDALSwapWords(&nSrid, sizeof (GInt32), 1, sizeof (GInt32));
    pbyAuxPtr += sizeof (GInt32);

    memcpy(&nWidth, pbyAuxPtr, sizeof (GUInt16));
    if (byEndianess != byMachineEndianess)
        GDALSwapWords(&nWidth, sizeof (GUInt16), 1, sizeof (GUInt16));
    pbyAuxPtr += sizeof (GUInt16);

    memcpy(&nHeight, pbyAuxPtr, sizeof (GUInt16));
    if (byEndianess != byMachineEndianess)
        GDALSwapWords(&nHeight, sizeof (GUInt16), 1, sizeof (GUInt16));
    pbyAuxPtr += sizeof (GUInt16);
    
    // Allocate memory for bands
    papoBands = (WKTRasterBandWrapper **) VSICalloc(nBands,
            sizeof (WKTRasterBandWrapper *));
    if (papoBands == NULL) {
        CPLError(CE_Failure, CPLE_ObjectNull,
                "Couldn't allocate memory for raster wrapper bands, aborting");
        return FALSE;
    }

    // Create band objects
    for (int i = 0; i < nBands; i++) {
        GByte byFirstByteBandHeader = 0;
        int nPixTypeBytes = 0;
        double dfNoDataValue = 0.0;

        // TODO : check that there are enough bytes in the buffer
        memcpy(&byFirstByteBandHeader, pbyAuxPtr, sizeof (GByte));
        pbyAuxPtr += sizeof (GByte);

        switch (byFirstByteBandHeader & 0x0f) {

            /*
             * GByte is defined as "unsigned char" in cpl_port.h. The data
             * fetched from database (pointed by pbyAuxPtr) can be a "signed
             * char" (case 3) or "unsigned char" (cases 0, 1, 2, 4)
             */
            case 0: case 1: case 2: case 4:
                GByte byNoData;
                memcpy(&byNoData, pbyAuxPtr, sizeof(GByte));
                dfNoDataValue = (double)byNoData;                
                nPixTypeBytes = sizeof(GByte);
                break;
            case 3:
                signed char sbyNoData;
                memcpy(&sbyNoData, pbyAuxPtr, sizeof(signed char));
                dfNoDataValue = (double)sbyNoData;
                nPixTypeBytes = sizeof(signed char);
                break;
            case 5:
                GInt16 n16NoData;
                memcpy(&n16NoData, pbyAuxPtr, sizeof(GInt16));
                dfNoDataValue = (double)n16NoData;
                nPixTypeBytes = sizeof(GInt16);
                break;
            case 6:
                GUInt16 un16NoData;
                memcpy(&un16NoData, pbyAuxPtr, sizeof(GUInt16));
                dfNoDataValue = (double)un16NoData;
                nPixTypeBytes = sizeof(GUInt16);
                break;
            case 7:
                GInt32 n32NoData;
                memcpy(&n32NoData, pbyAuxPtr, sizeof(GInt32));
                dfNoDataValue = (double)n32NoData;
                nPixTypeBytes = sizeof(GInt32);
                break;
            case 8:
                GUInt32 un32NoData;
                memcpy(&un32NoData, pbyAuxPtr, sizeof(GUInt32));
                dfNoDataValue = (double)un32NoData;
                nPixTypeBytes = sizeof(GUInt32);
                break;
            case 10:
                float fNoData;
                memcpy(&fNoData, pbyAuxPtr, sizeof(float));
                dfNoDataValue = (double)fNoData;
                nPixTypeBytes = sizeof(float);
                break;
            case 11:
                memcpy(&dfNoDataValue, pbyAuxPtr, sizeof(double));
                nPixTypeBytes = sizeof(double);
                break;
            default:
                // TODO: stop creating bands. Free memory for previous bands

                CPLError(CE_Failure, CPLE_NotSupported,
                    "Nodata size not supported for band %d, aborting\n", i);
                return FALSE;               
        }
        
        /***************************************************************
         * Swap nodata word, if needed
         ***************************************************************/
        if (byEndianess != byMachineEndianess)
            GDALSwapWords(&dfNoDataValue, nPixTypeBytes, 1, nPixTypeBytes);
        pbyAuxPtr += nPixTypeBytes;

        nRasterBandHeaderLen = (1 + nPixTypeBytes) * sizeof (GByte);
        nRasterDataLen = ((nLengthByWkbString - nRasterHeaderLen) / nBands) -
                ((1 + nPixTypeBytes));

        // TODO : check that there are enough bytes in the buffer       

        /**************************************************************
         * In-db raster. Next bytes are the raster data and must be
         * swapped, if needed
         **************************************************************/
        if ((byFirstByteBandHeader >> 7) == FALSE) {

            // TODO: check this assertion, sometimes fails
            // In this case, data are a nWidth * nHeight array
            //CPLAssert(nRasterDataLen == (nWidth * nHeight * nPixTypeBytes));

            // Swap words of data, if needed
            if (byEndianess != byMachineEndianess)
                GDALSwapWords(pbyAuxPtr, nPixTypeBytes,
                    nRasterDataLen / nPixTypeBytes, nPixTypeBytes);

        }

        // Create raster band wrapper object and set data
        // NOTE: All words has been swapped before creating band
        papoBands[i] = new WKTRasterBandWrapper(this, i + 1,
                byFirstByteBandHeader, dfNoDataValue);
        papoBands[i]->SetData(pbyAuxPtr, nRasterDataLen);

        pbyAuxPtr += nRasterDataLen;

    }

    // Set raster extent
    pszWktExtent = NULL;

    return TRUE;
}

/**
 * Class destructor. Frees the memory and resources allocated.
 */
WKTRasterWrapper::~WKTRasterWrapper() {
    if (papoBands) {
        for (int i = 0; i < nBands; i++)
            delete papoBands[i];
        CPLFree(papoBands);
    }

    CPLFree(pszHexWkb);
    CPLFree(pbyHexWkb);
    CPLFree(pszWktExtent);
}

/**
 * Creates a polygon in WKT representation that wrapps all the extent
 * covered by the raster
 * Parameters: nothing
 * Returns:
 *  char *: The polygon in WKT format
 */
const char * WKTRasterWrapper::GetWktExtent() {
    /**
     * Create WKT string for raster extent
     * TODO: With irregular blocking is not valid in this way...
     */
    double dfRasterWidth = ABS((int) (dfScaleX * nWidth + 0.5));
    double dfRasterHeight = ABS((int) (dfScaleY * nHeight + 0.5));

    double dfBlockEndX = dfIpX + dfRasterWidth;
    double dfBlockEndY = dfIpY - dfRasterHeight;
    char szTemp[1024];

    sprintf(szTemp,
            "POLYGON((%f %f, %f %f ,%f %f ,%f %f, %f %f))", dfIpX, dfBlockEndY,
            dfIpX, dfIpY, dfBlockEndX, dfBlockEndY, dfBlockEndX, dfIpY, dfIpX,
            dfBlockEndY);

    CPLFree(pszWktExtent);
    pszWktExtent = CPLStrdup(szTemp);

    return pszWktExtent;
}

/**
 * Constructs the binary representation of the PostGIS WKT raster wrapped
 * by this class, based on all the class properties.
 * This method swaps words if the raster endianess is distinct from
 * the machine endianess.
 * Parameters: nothing
 * Returns:
 *  - GByte *: Binary representation of the hexwkb string
 */
GByte * WKTRasterWrapper::GetBinaryRepresentation() {

    GByte byMachineEndianess = NDR; // by default

    // Check machine endianess
#ifdef CPL_LSB
    byMachineEndianess = NDR;
#else
    byMachineEndianess = XDR;
#endif

    int nTransformedBytes = 0;
    int nPixTypeBytes = 0;

    GByte * pbyTmpRepresentation = (GByte *) VSICalloc(nLengthByWkbString,
            sizeof (GByte));
    if (pbyTmpRepresentation == NULL) {
        CPLError(CE_Warning, CPLE_ObjectNull,
                "Couldn't allocate memory for generating the binary \
                representation of the raster. Using the original one");

        return pbyHexWkb;
    }

    // We'll use this pointer for moving over the representation
    GByte * pbyAuxPtr = pbyTmpRepresentation;

    // Copy the attributes in the array
    memcpy(pbyAuxPtr, &byEndianess, sizeof (GByte));
    nTransformedBytes += sizeof (GByte);
    pbyAuxPtr += sizeof (GByte);

    memcpy(pbyAuxPtr, &nVersion, sizeof (GUInt16));
    if (byEndianess != byMachineEndianess)
        GDALSwapWords(pbyAuxPtr, sizeof (GUInt16), 1, sizeof (GUInt16));
    nTransformedBytes += sizeof (GUInt16);
    pbyAuxPtr += sizeof (GUInt16);

    memcpy(pbyAuxPtr, &nBands, sizeof (GUInt16));
    if (byEndianess != byMachineEndianess)
        GDALSwapWords(pbyAuxPtr, sizeof (GUInt16), 1, sizeof (GUInt16));
    nTransformedBytes += sizeof (GUInt16);
    pbyAuxPtr += sizeof (GUInt16);

    memcpy(pbyAuxPtr, &dfScaleX, sizeof (double));
    if (byEndianess != byMachineEndianess)
        GDALSwapWords(pbyAuxPtr, sizeof (double), 1, sizeof (double));
    nTransformedBytes += sizeof (double);
    pbyAuxPtr += sizeof (double);

    memcpy(pbyAuxPtr, &dfScaleY, sizeof (double));
    if (byEndianess != byMachineEndianess)
        GDALSwapWords(pbyAuxPtr, sizeof (double), 1, sizeof (double));
    nTransformedBytes += sizeof (double);
    pbyAuxPtr += sizeof (double);

    memcpy(pbyAuxPtr, &dfIpX, sizeof (double));
    if (byEndianess != byMachineEndianess)
        GDALSwapWords(pbyAuxPtr, sizeof (double), 1, sizeof (double));
    nTransformedBytes += sizeof (double);
    pbyAuxPtr += sizeof (double);

    memcpy(pbyAuxPtr, &dfIpY, sizeof (double));
    if (byEndianess != byMachineEndianess)
        GDALSwapWords(pbyAuxPtr, sizeof (double), 1, sizeof (double));
    nTransformedBytes += sizeof (double);
    pbyAuxPtr += sizeof (double);

    memcpy(pbyAuxPtr, &dfSkewX, sizeof (double));
    if (byEndianess != byMachineEndianess)
        GDALSwapWords(pbyAuxPtr, sizeof (double), 1, sizeof (double));
    nTransformedBytes += sizeof (double);
    pbyAuxPtr += sizeof (double);

    memcpy(pbyAuxPtr, &dfSkewY, sizeof (double));
    if (byEndianess != byMachineEndianess)
        GDALSwapWords(pbyAuxPtr, sizeof (double), 1, sizeof (double));
    nTransformedBytes += sizeof (double);
    pbyAuxPtr += sizeof (double);

    memcpy(pbyAuxPtr, &nSrid, sizeof (GInt32));
    if (byEndianess != byMachineEndianess)
        GDALSwapWords(pbyAuxPtr, sizeof (GInt32), 1, sizeof (GInt32));
    nTransformedBytes += sizeof (GInt32);
    pbyAuxPtr += sizeof (GInt32);

    memcpy(pbyAuxPtr, &nWidth, sizeof (GUInt16));
    if (byEndianess != byMachineEndianess)
        GDALSwapWords(pbyAuxPtr, sizeof (GUInt16), 1, sizeof (GUInt16));
    nTransformedBytes += sizeof (GUInt16);
    pbyAuxPtr += sizeof (GUInt16);

    memcpy(pbyAuxPtr, &nHeight, sizeof (GUInt16));
    if (byEndianess != byMachineEndianess)
        GDALSwapWords(pbyAuxPtr, sizeof (GUInt16), 1, sizeof (GUInt16));
    nTransformedBytes += sizeof (GUInt16);
    pbyAuxPtr += sizeof (GUInt16);

    for (int i = 0; i < nBands; i++) {
        /**
         * We're going to create one byte using some bits of three fields:
         *  - bIsOffline:   _ _ _ _ _ _ _ X
         *  - byReserved:   _ _ _ _ _ X X X
         *  - byPixelType:  _ _ _ _ X X X X
         * So, we'll have to move each byte a number of bits to put the 'X'
         * into consecutive positions in the final result byte
         */
        GByte byFirstByteOfBandHeader =
                (papoBands[i]->bIsOffline << 7) +
                (papoBands[i]->byReserved << 4) +
                (papoBands[i]->byPixelType & 0x0f);
        memcpy(pbyAuxPtr, &byFirstByteOfBandHeader, sizeof (GByte));
        nTransformedBytes += sizeof (GByte);
        pbyAuxPtr += sizeof (GByte);

        /**
         * Copy nodata value. Its size depends on the pixel type
         * NOTE: But this size CAN'T BE CHANGED, it will be the same of the
         * original nodata size's value. If not, we couldn't know how much
         * memory need.
         */

        // FIXME : this is likely incorrect. see previous FIXME
        switch (papoBands[i]->byPixelType & 0x0f) {
            case 0: case 1: case 2: case 4:
                nPixTypeBytes = 1;
                memcpy(pbyAuxPtr, (GByte *) (&papoBands[i]->dfNodata),
                        sizeof (GByte));
                if (byEndianess != byMachineEndianess)
                    GDALSwapWords(pbyAuxPtr, sizeof (GByte), 1,
                        sizeof (GByte));
                pbyAuxPtr += sizeof (GByte);
                nTransformedBytes += sizeof (GByte);
                break;
            case 3:
                nPixTypeBytes = 1;
                memcpy(pbyAuxPtr, (signed char *) (&papoBands[i]->dfNodata),
                        sizeof (signed char));
                if (byEndianess != byMachineEndianess)
                    GDALSwapWords(pbyAuxPtr, sizeof (signed char), 1,
                        sizeof (signed char));
                pbyAuxPtr += sizeof (signed char);
                nTransformedBytes += sizeof (signed char);
                break;
            case 5:
                nPixTypeBytes = 2;
                memcpy(pbyAuxPtr, (GInt16 *) (&papoBands[i]->dfNodata),
                        sizeof (GInt16));
                if (byEndianess != byMachineEndianess)
                    GDALSwapWords(pbyAuxPtr, sizeof (GInt16), 1,
                        sizeof (GInt16));
                nTransformedBytes += sizeof (GInt16);
                pbyAuxPtr += sizeof (GInt16);
                break;
            case 6:
                nPixTypeBytes = 2;
                memcpy(pbyAuxPtr, (GUInt16 *) (&papoBands[i]->dfNodata),
                        sizeof (GUInt16));
                if (byEndianess != byMachineEndianess)
                    GDALSwapWords(pbyAuxPtr, sizeof (GUInt16), 1,
                        sizeof (GUInt16));
                nTransformedBytes += sizeof (GUInt16);
                pbyAuxPtr += sizeof (GUInt16);
                break;
            case 7:
                nPixTypeBytes = 4;
                memcpy(pbyAuxPtr, (GInt32 *) (&papoBands[i]->dfNodata),
                        sizeof (GInt32));
                if (byEndianess != byMachineEndianess)
                    GDALSwapWords(pbyAuxPtr, sizeof (GInt32), 1,
                        sizeof (GInt32));
                nTransformedBytes += sizeof (GInt32);
                pbyAuxPtr += sizeof (GInt32);
                break;
            case 8:
                nPixTypeBytes = 4;
                memcpy(pbyAuxPtr, (GUInt32 *) (&papoBands[i]->dfNodata),
                        sizeof (GUInt32));
                if (byEndianess != byMachineEndianess)
                    GDALSwapWords(pbyAuxPtr, sizeof (GUInt32), 1,
                        sizeof (GUInt32));
                nTransformedBytes += sizeof (GUInt32);
                pbyAuxPtr += sizeof (GUInt32);
                break;
            case 10:
                nPixTypeBytes = 8;
                memcpy(pbyAuxPtr, &(papoBands[i]->dfNodata), sizeof (float));
                if (byEndianess != byMachineEndianess)
                    GDALSwapWords(pbyAuxPtr, sizeof (float), 1,
                        sizeof (float));
                pbyAuxPtr += sizeof (float);
                nTransformedBytes += sizeof (float);
                break;
            case 11:
                nPixTypeBytes = 8;
                memcpy(pbyAuxPtr, &(papoBands[i]->dfNodata), sizeof (double));
                if (byEndianess != byMachineEndianess)
                    GDALSwapWords(pbyAuxPtr, sizeof (double), 1,
                        sizeof (double));
                pbyAuxPtr += sizeof (double);
                nTransformedBytes += sizeof (double);
                break;
            default:
                CPLError(CE_Failure, CPLE_NotSupported,
                    "Nodata size not supported for band %d, using the original \
                    one\n",i);
                return pbyHexWkb;
        }


        // out-db band
        if (papoBands[i]->bIsOffline == TRUE) {

            // copy outdb band number
            memcpy(pbyAuxPtr, &(papoBands[i]->nOutDbBandNumber), sizeof (GByte));
            pbyAuxPtr += sizeof (GByte);
            nTransformedBytes += sizeof (GByte);

            // copy path to the external file (pbyData). Don't need to swap
            memcpy(pbyAuxPtr, papoBands[i]->pbyData, papoBands[i]->nDataSize);
            pbyAuxPtr += papoBands[i]->nDataSize * sizeof (GByte);
            nTransformedBytes += papoBands[i]->nDataSize * sizeof (GByte);

        }
            // in-db band
        else {
            // Copy data
            memcpy(pbyAuxPtr, papoBands[i]->pbyData, papoBands[i]->nDataSize);
            if (byEndianess != byMachineEndianess)
                GDALSwapWords(pbyAuxPtr, nPixTypeBytes,
                    papoBands[i]->nDataSize / nPixTypeBytes, nPixTypeBytes);
            pbyAuxPtr += papoBands[i]->nDataSize * sizeof (GByte);
            nTransformedBytes += papoBands[i]->nDataSize * sizeof (GByte);
        }

    }

    /**
     * Now, copy the new binary array into the old one and free the allocated
     * memory
     */
    // TODO: Check this assert. Sometimes fails
    //CPLAssert(nTransformedBytes == nLengthByWkbString);
    memcpy(pbyHexWkb, pbyTmpRepresentation, nTransformedBytes *
            sizeof (GByte));
    CPLFree(pbyTmpRepresentation);

    return pbyHexWkb;
}

/**
 * Constructs the hexwkb representation of the PostGIS WKT raster wrapped
 * by this class, based on all the class properties.
 * This method swaps words if the raster endianess is distinct from
 * the machine endianess.
 * Parameters: nothing
 * Returns:
 *  - GByte *: Hexwkb string
 */
char * WKTRasterWrapper::GetHexWkbRepresentation() {
    GetBinaryRepresentation();
    char * pszAuxRepresentation = CPLBinaryToHex(nLengthByWkbString,
            pbyHexWkb);

    if (pszAuxRepresentation == NULL) {
        CPLError(CE_Warning, CPLE_ObjectNull,
                "Couldn't allocate memory for generating the HexWkb \
                representation of the raster. Using the original one");

        return pszHexWkb;
    }

    CPLAssert((int)strlen(pszAuxRepresentation) == nLengthHexWkbString);
    memcpy(pszHexWkb, pszAuxRepresentation, nLengthByWkbString *
            sizeof (char));

    CPLFree(pszAuxRepresentation);

    return pszHexWkb;
}

/**
 * Gets a wrapper of a RasterBand, as a WKTRasterBandWrapper object.
 * Properties:
 *  - GUInt16: the band number.
 * Returns:
 *  - WKTRasterWrapper *: an object that wrapps the RasterBand
 */
WKTRasterBandWrapper * WKTRasterWrapper::GetBand(GUInt16 nBandNumber) {
    if (nBandNumber == 0 || nBandNumber > nBands) {
        CPLError(CE_Failure, CPLE_IllegalArg,
                "Couldn't get band number %d", nBandNumber);
        return NULL;
    }

    return papoBands[nBandNumber - 1];
}

/**************************************************************************
 * ======================================================================
 *                            WKTRasterBandWrapper
 * ======================================================================
 *
 * This class wrapps the HEXWKB representation of a PostGIS WKT Raster
 * Band, that is a part of the wrapper of a WKT Raster.
 *
 * TODO:
 *  - Allow changing the size of the nodatavalue, that implies modify the
 *  allocated memory for HexWkb representation of the WKT Raster. Now, you
 *  only can change the value, not the size.
 *  - Avoid the use of double instead of double, to ensure compatibility
 *  with WKTRasterRasterBand types. Discuss it.
 ***************************************************************************/

/**
 * Constructor.
 * Parameters:
 *  - WKTRasterWrapper *: the WKT Raster wrapper this band belongs to
 *  - GUInt16: band number
 *  - GByte: The first byte of the band header (contains the value for
 *          other class properties).
 *  - double: The nodata value. Could be any kind of data (GByte,
 *          GUInt16, GInt32...) but the variable has the bigger type
 */
WKTRasterBandWrapper::WKTRasterBandWrapper(WKTRasterWrapper * poWKTRW,
        GUInt16 nBandNumber, GByte byFirstByteOfHeader, double fNodataValue) {

    bIsOffline = byFirstByteOfHeader >> 7;
    byReserved = (byFirstByteOfHeader >> 4) & 0x07;
    byPixelType = byFirstByteOfHeader & 0x0f;
    dfNodata = fNodataValue;
    nBand = nBandNumber;
    poRW = poWKTRW;
    pbyData = NULL;
    nOutDbBandNumber = -1;
}

/**
 * Class destructor. Frees the memory and resources allocated.
 */
WKTRasterBandWrapper::~WKTRasterBandWrapper() {
    CPLFree(pbyData);
}

/**
 * Set the raster band data. This method updates the data of the raster
 * band. Then, when the HexWkb representation of the raster is
 * required (via WKTRasterWrapper::GetBinaryRepresentation or
 * WKTRasterWrapper::GetHexWkbRepresentation), the new data will
 * be packed instead the data of the original HexWkb representation
 * used to create the WKTRasterWrapper.
 * Parameters:
 *  - GByte *: The data to set
 *  - GUInt32: data size
 * Returns:
 *  - CE_None if the data is set, CE_Failure otherwise
 */
CPLErr WKTRasterBandWrapper::SetData(GByte * pbyDataArray, GUInt32 nSize) {

    /**************************************************************
     * If we have an out-db raster, the next bytes will represent:
     *  - bandNumber: 0-based band number to use from ext. file
     *  - path to the external file
     * In this case, we must extract the band number from the data
     **************************************************************/
    GByte * pbyAux;
    int nSizeMemToAllocate = 0;

    // Security checking
    if (pbyDataArray == NULL) {
        CPLError(CE_Warning, CPLE_ObjectNull,
                "Couldn't set data for raster band %d", nBand);
        nDataSize = 0;
        return CE_Failure;
    }
        /**********************************************************
         * Out-db raster: extract the band number first, and
         * copy the rest of the data (will be the path to file)
         **********************************************************/
    else if (bIsOffline == TRUE) {
        memcpy(&nOutDbBandNumber, pbyDataArray, sizeof (GByte));
        nSizeMemToAllocate = nSize - sizeof (GByte);
        pbyAux = pbyDataArray + sizeof (GByte);

        // the bandnumber read is 0-based
        nOutDbBandNumber++;
    }

        /*********************************************************
         * In-db raster: all the buffer is the data.
         *********************************************************/
    else {
        nOutDbBandNumber = -1;
        nSizeMemToAllocate = nSize;
        pbyAux = pbyDataArray;
    }


    /*********************************************************
     * Now, really copy the data buffer to the class property
     *********************************************************/    
    CPLFree(pbyData);
    pbyData = (GByte *) VSICalloc(nSizeMemToAllocate, sizeof (GByte));
    if (pbyData == NULL) {
        CPLError(CE_Failure, CPLE_ObjectNull,
                "Couldn't allocate memory for raster data in band %d",
                nBand);
        nDataSize = 0;
        return CE_Failure;
    } else {
        memcpy(pbyData, pbyAux, nSizeMemToAllocate);
        nDataSize = nSizeMemToAllocate;
        return CE_None;
    }

}

/**
 * Get the raster band data.
 * Parameters: nothing
 * Returns:
 *  - GByte *: The raster band data
 */
GByte * WKTRasterBandWrapper::GetData() {
    /**
     * NOTE: The data could be a path to a file. In this case, the string
     * representing the path may contain zeros at the end. But I need that
     * is not necessary to "cut" these zeros (the functions that uses the
     * name of a file should read until the first zero of the string...
     */
    return pbyData;
}
