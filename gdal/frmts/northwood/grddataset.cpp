/******************************************************************************
 *
 * Project:  GRD Reader
 * Purpose:  GDAL driver for Northwood Grid Format
 * Author:   Perry Casson
 *
 ******************************************************************************
 * Copyright (c) 2006, Waypoint Information Technology
 * Copyright (c) 2009-2012, Even Rouault <even dot rouault at mines-paris dot org>
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
 ****************************************************************************/

#include <string>
#include <cstring>
#include <cstdio>
#include <climits>
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "northwood.h"

#ifdef MSVC
#include "..\..\ogr\ogrsf_frmts\mitab\mitab.h"
#else
#include "../../ogr/ogrsf_frmts/mitab/mitab.h"
#endif

CPL_CVSID("$Id$");

#define NODATA -1.e37f
#define SCALE16BIT 65534.0
#define SCALE32BIT 4294967294.0

void replaceExt(std::string& s, const std::string& newExt);
/************************************************************************/
/* Replace the extension on a filepath with an alternative extension    */
/************************************************************************/
void replaceExt(std::string& s, const std::string& newExt) {

    std::string::size_type i = s.rfind('.', s.length());

    if (i != std::string::npos) {
        s.replace(i + 1, newExt.length(), newExt);
    }
}

/************************************************************************/
/* ==================================================================== */
/*                      NWT_GRDDataset                                  */
/* ==================================================================== */
/************************************************************************/
class NWT_GRDRasterBand;

class NWT_GRDDataset: public GDALPamDataset {
    friend class NWT_GRDRasterBand;

    VSILFILE *fp;
    GByte abyHeader[1024];
    NWT_GRID *pGrd;
    NWT_RGB ColorMap[4096];
    bool bUpdateHeader;
    CPLString m_osProjection;

    // Update the header data with latest changes
    int UpdateHeader();
    int WriteTab();

public:
    NWT_GRDDataset();
    ~NWT_GRDDataset();

    static GDALDataset *Open(GDALOpenInfo *);
    static int Identify(GDALOpenInfo *);
    static GDALDataset *Create(const char * pszFilename, int nXSize, int nYSize,
            int nBands, GDALDataType eType, char ** papszParmList);
    static GDALDataset *CreateCopy(const char * pszFilename,
            GDALDataset * poSrcDS, int bStrict, char **papszOptions,
            GDALProgressFunc pfnProgress, void * pProgressData);

    CPLErr GetGeoTransform(double *padfTransform);
    CPLErr SetGeoTransform(double *padfTransform);
    void FlushCache();
    const char *GetProjectionRef();
    CPLErr SetProjection(const char *pszProjection);
};

/************************************************************************/
/* ==================================================================== */
/*                            NWT_GRDRasterBand                         */
/* ==================================================================== */
/************************************************************************/

class NWT_GRDRasterBand: public GDALPamRasterBand {
    friend class NWT_GRDDataset;

    int bHaveOffsetScale;
    double dfOffset;
    double dfScale;
    double dfNoData;

public:

    NWT_GRDRasterBand(NWT_GRDDataset *, int, int);

    virtual CPLErr IReadBlock(int, int, void *);
    virtual CPLErr IWriteBlock(int, int, void *);
    virtual double GetNoDataValue(int *pbSuccess);
    virtual CPLErr SetNoDataValue(double dfNoData);

    virtual GDALColorInterp GetColorInterpretation();
};

/************************************************************************/
/*                           NWT_GRDRasterBand()                        */
/************************************************************************/
NWT_GRDRasterBand::NWT_GRDRasterBand( NWT_GRDDataset * poDSIn, int nBandIn,
                                      int nBands ) :
    bHaveOffsetScale(FALSE),
    dfOffset(0.0),
    dfScale(1.0),
    dfNoData(0.0)
{
    poDS = poDSIn;
    nBand = nBandIn;

    /*
    * If nBand = 4 we have opened in read mode and have created the 3 'virtual' RGB bands.
    * so the 4th band is the actual data
    * Otherwise, if we have opened in update mode, there is only 1 band, which is the actual data
    */
    if (nBand == 4 || nBands == 1) {
        bHaveOffsetScale = TRUE;
        dfOffset = poDSIn->pGrd->fZMin;

        if (poDSIn->pGrd->cFormat == 0x00) {
            eDataType = GDT_Float32;
            dfScale = (poDSIn->pGrd->fZMax - poDSIn->pGrd->fZMin)
                    / (double) SCALE16BIT;
        } else {
            eDataType = GDT_Float32;
            dfScale = (poDSIn->pGrd->fZMax - poDSIn->pGrd->fZMin)
                    / (double) SCALE32BIT;
        }
    }
    else
    {
        bHaveOffsetScale = FALSE;
        dfOffset = 0;
        dfScale = 1.0;
        eDataType = GDT_Byte;
    }
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

double NWT_GRDRasterBand::GetNoDataValue(int *pbSuccess) {
    NWT_GRDDataset *poGDS = reinterpret_cast<NWT_GRDDataset *>(poDS);
    double dRetval;
    if ((nBand == 4) || (poGDS->nBands == 1)) {
        if (pbSuccess != NULL)
            *pbSuccess = TRUE;
        if (dfNoData != 0.0) {
            dRetval = dfNoData;
        } else {
            dRetval = (double) NODATA;
        }

        return dRetval;
    }

    if (pbSuccess != NULL)
        *pbSuccess = FALSE;

    return 0;
}

CPLErr NWT_GRDRasterBand::SetNoDataValue(double dfNoDataIn) {
    // This is essentially a 'virtual' no data value.
    // Once set, when writing an value == dfNoData will
    // be converted to the no data value (0) on disk.
    // If opened again; the no data value will always be the
    // default (-1.e37f)
    dfNoData = dfNoDataIn;
    return CE_None;
}

GDALColorInterp NWT_GRDRasterBand::GetColorInterpretation() {
    NWT_GRDDataset *poGDS = reinterpret_cast<NWT_GRDDataset *>(poDS);
    //return GCI_RGB;
    if ((nBand == 4) || (poGDS->nBands == 1))
        return GCI_GrayIndex;
    else if (nBand == 1)
        return GCI_RedBand;
    else if (nBand == 2)
        return GCI_GreenBand;
    else if (nBand == 3)
        return GCI_BlueBand;

    return GCI_Undefined;
}

/************************************************************************/
/*                             IWriteBlock()                            */
/************************************************************************/
CPLErr NWT_GRDRasterBand::IWriteBlock(CPL_UNUSED int nBlockXOff, int nBlockYOff,
        void * pImage) {

    // Each block is an entire row of the dataset, so the x offset should always be 0
    CPLAssert(nBlockXOff == 0);
    NWT_GRDDataset *poGDS = reinterpret_cast<NWT_GRDDataset *>(poDS);

    // Ensure the blocksize is not beyond the system limits and
    // initialise the size of the record
    if (nBlockXSize > INT_MAX / 2) {
        return CE_Failure;
    }
    const int nRecordSize = nBlockXSize * 2;

    // Seek to the write position in the GRD file
    VSIFSeekL(poGDS->fp,
            1024 + nRecordSize * static_cast<vsi_l_offset>(nBlockYOff),
            SEEK_SET);

    // Cast pImage to float
    float *pfImage = reinterpret_cast<float *>(pImage);

    // Initialise output array
    GByte *pabyRecord = reinterpret_cast<GByte *>(VSI_MALLOC_VERBOSE(
                    nRecordSize));
    if (pabyRecord == NULL)
    return CE_Failure;

    // We only ever write to band 4; RGB bands are basically 'virtual'
    // (i.e. the RGB colour is computed from the raw data).
    // For all intents and purposes, there is essentially 1 band on disk.
    float fValue;// A single pixel value
    unsigned short nWrite;// The stretched value to be written
    if (nBand == 1) {
        for (int i = 0; i < nBlockXSize; i++) {
            fValue = pfImage[i];

            // We allow data to be interpreted by a user-defined null value
            // (a 'virtual' value, since it is always 0 on disk) or
            // if not defined we default to the GRD standard of -1E37.
            // We allow a little bit of flexibility in that if it is below -1E37
            // it is in all probability still intended as a null value.
            if ((fValue == dfNoData) || (fValue <= (double)NODATA)) {
                nWrite = (unsigned short) 0;
            }
            else {
                if (fValue < poGDS->pGrd->fZMin) {
                    poGDS->pGrd->fZMin = fValue;
                }
                else if (fValue > poGDS->pGrd->fZMax) {
                    poGDS->pGrd->fZMax = fValue;
                }
                // Data on disk is stretched within the unsigned short range so
                // we must convert (the inverse of what is done in IReadBlock),
                // based on the Z value range
                nWrite = (unsigned short) (((fValue - dfOffset) / dfScale) + 1);
            }
            // Copy the result to the byte array (2 bytes per value)
            memcpy(reinterpret_cast<void *>(pabyRecord + 2 * i),
                    reinterpret_cast<void *>(&nWrite), 2);
        }

        // Write the buffer to disk
        if (VSIFWriteL(pabyRecord, 1, nRecordSize, poGDS->fp)
                != (size_t) (nRecordSize)) {
            CPLError(CE_Failure, CPLE_FileIO,
                    "Failed to write scanline %d to file.\n", nBlockYOff);
            CPLFree(pabyRecord);
            return CE_Failure;
        }
    } else {
        CPLError(CE_Failure, CPLE_IllegalArg, "Writing to band %d is not valid",
                nBand);
        CPLFree(pabyRecord);
        return CE_Failure;
    }
    CPLFree(pabyRecord);
    return CE_None;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/
CPLErr NWT_GRDRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff, int nBlockYOff,
        void *pImage) {
    NWT_GRDDataset *poGDS = reinterpret_cast<NWT_GRDDataset *>(poDS);
    if (nBlockXSize > INT_MAX / 2)
        return CE_Failure;
    const int nRecordSize = nBlockXSize * 2;
    unsigned short raw1;

    // Seek to the data position
    VSIFSeekL(poGDS->fp,
            1024 + nRecordSize * static_cast<vsi_l_offset>(nBlockYOff),
            SEEK_SET);

    GByte *pabyRecord = reinterpret_cast<GByte *>(VSI_MALLOC_VERBOSE(
                    nRecordSize));
    if (pabyRecord == NULL)
    return CE_Failure;

    // Read the data
    if ((int) VSIFReadL(pabyRecord, 1, nRecordSize, poGDS->fp) != nRecordSize) {
        CPLFree(pabyRecord);
        return CE_Failure;
    }

    if ((nBand == 4) || (poGDS->nBands == 1))            //Z values
    {
        int bSuccess;
        double dNoData = GetNoDataValue(&bSuccess);
        for (int i = 0; i < nBlockXSize; i++) {
            memcpy(reinterpret_cast<void *>(&raw1),
                    reinterpret_cast<void *>(pabyRecord + 2 * i), 2);
            CPL_LSBPTR16(&raw1);
            if (raw1 == 0) {
                reinterpret_cast<float *>(pImage)[i] = (float) dNoData; // null value
            } else {
                reinterpret_cast<float *>(pImage)[i] =
                static_cast<float>(dfOffset + ((raw1 - 1) * dfScale));
            }
        }
    } else if (nBand == 1)            // red values
    {
        for (int i = 0; i < nBlockXSize; i++) {
            memcpy(reinterpret_cast<void *>(&raw1),
                    reinterpret_cast<void *>(pabyRecord + 2 * i), 2);
            CPL_LSBPTR16(&raw1);
            reinterpret_cast<char *>(pImage)[i] = poGDS->ColorMap[raw1 / 16].r;
        }
    } else if (nBand == 2)            // green
    {
        for (int i = 0; i < nBlockXSize; i++) {
            memcpy(reinterpret_cast<void *>(&raw1),
                    reinterpret_cast<void *>(pabyRecord + 2 * i), 2);
            CPL_LSBPTR16(&raw1);
            reinterpret_cast<char *>(pImage)[i] = poGDS->ColorMap[raw1 / 16].g;
        }
    } else if (nBand == 3)            // blue
    {
        for (int i = 0; i < nBlockXSize; i++) {
            memcpy(reinterpret_cast<void *>(&raw1),
                    reinterpret_cast<void *>(pabyRecord + 2 * i), 2);
            CPL_LSBPTR16(&raw1);
            reinterpret_cast<char *>(pImage)[i] = poGDS->ColorMap[raw1 / 16].b;
        }
    } else {
        CPLError(CE_Failure, CPLE_IllegalArg, "No band number %d", nBand);
        CPLFree(pabyRecord);
        return CE_Failure;
    }

    CPLFree(pabyRecord);

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                             NWT_GRDDataset                           */
/* ==================================================================== */
/************************************************************************/

NWT_GRDDataset::NWT_GRDDataset() :
    fp(NULL),
    pGrd(NULL),
    bUpdateHeader(false)
{
    //poCT = NULL;
    for( size_t i = 0; i < CPL_ARRAYSIZE(ColorMap); ++i )
    {
        ColorMap[i].r = 0;
        ColorMap[i].g = 0;
        ColorMap[i].b = 0;
    }
}

/************************************************************************/
/*                            ~NWT_GRDDataset()                         */
/************************************************************************/

NWT_GRDDataset::~NWT_GRDDataset() {

    // Make sure any changes to the header etc are written
    // if we are in update mode.
    if (eAccess == GA_Update) {
        FlushCache();
    }
    pGrd->fp = NULL;       // this prevents nwtCloseGrid from closing the fp
    nwtCloseGrid(pGrd);

    if (fp != NULL)
        VSIFCloseL(fp);
}

/************************************************************************/
/*                            ~FlushCache()                             */
/************************************************************************/
void NWT_GRDDataset::FlushCache() {
    // Ensure the header and TAB file are up to date
    if (bUpdateHeader) {
        UpdateHeader();
    }

    // Call the parent method
    GDALPamDataset::FlushCache();
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr NWT_GRDDataset::GetGeoTransform(double *padfTransform) {
    padfTransform[0] = pGrd->dfMinX - (pGrd->dfStepSize * 0.5);
    padfTransform[3] = pGrd->dfMaxY + (pGrd->dfStepSize * 0.5);
    padfTransform[1] = pGrd->dfStepSize;
    padfTransform[2] = 0.0;

    padfTransform[4] = 0.0;
    padfTransform[5] = -1 * pGrd->dfStepSize;

    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr NWT_GRDDataset::SetGeoTransform(double *padfTransform) {
    if (padfTransform[2] != 0.0 || padfTransform[4] != 0.0) {

        CPLError(CE_Failure, CPLE_NotSupported,
                "GRD datasets do not support skew/rotation");
        return CE_Failure;
    }
    pGrd->dfStepSize = padfTransform[1];

    // GRD format sets the min/max coordinates to the centre of the
    // cell; We must account for this when copying the GDAL geotransform
    // which references the top left corner
    pGrd->dfMinX = padfTransform[0] + (pGrd->dfStepSize * 0.5);
    pGrd->dfMaxY = padfTransform[3] - (pGrd->dfStepSize * 0.5);

    // Now set the miny and maxx
    pGrd->dfMaxX = pGrd->dfMinX + (pGrd->dfStepSize * (nRasterXSize - 1));
    pGrd->dfMinY = pGrd->dfMaxY - (pGrd->dfStepSize * (nRasterYSize - 1));
    bUpdateHeader = true;

    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/
const char *NWT_GRDDataset::GetProjectionRef() {

    // First try getting it from the PAM dataset
    const char *pszProjection = GDALPamDataset::GetProjectionRef();

    // If that isn't possible, read it from the GRD file. This may be a less
    //  complete projection string.
    if( strlen(pszProjection) == 0 )
    {
        OGRSpatialReference *poSpatialRef =
            MITABCoordSys2SpatialRef( pGrd->cMICoordSys );
        if( poSpatialRef )
        {
            char* pszProjectionTmp = NULL;
            poSpatialRef->exportToWkt( &pszProjectionTmp );
            poSpatialRef->Release();
            if( pszProjectionTmp )
                m_osProjection = pszProjectionTmp;
            CPLFree(pszProjectionTmp);
            return m_osProjection;
        }
    }
    return pszProjection;
}

/************************************************************************/
/*                          SetProjectionRef()                          */
/************************************************************************/

CPLErr NWT_GRDDataset::SetProjection( const char *pszProjection ) {
    OGRSpatialReference oSpatialRef;
    char *pszTmp = const_cast<char*>(pszProjection);
    oSpatialRef.importFromWkt( &pszTmp );
    char *psTABProj = MITABSpatialRef2CoordSys( &oSpatialRef );
    strncpy( pGrd->cMICoordSys, psTABProj, sizeof(pGrd->cMICoordSys) -1 );
    pGrd->cMICoordSys[255] = '\0';

    // Free temp projection.
    CPLFree(psTABProj);
    // Set projection in PAM dataset, so that
    // GDAL can always retrieve the complete projection.
    GDALPamDataset::SetProjection( pszProjection );
    bUpdateHeader = true;

    return CE_None;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int NWT_GRDDataset::Identify(GDALOpenInfo * poOpenInfo) {
    /* -------------------------------------------------------------------- */
    /*  Look for the header                                                 */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->nHeaderBytes < 1024)
        return FALSE;

    if (poOpenInfo->pabyHeader[0] != 'H' || poOpenInfo->pabyHeader[1] != 'G'
            || poOpenInfo->pabyHeader[2] != 'P'
            || poOpenInfo->pabyHeader[3] != 'C'
            || poOpenInfo->pabyHeader[4] != '1')
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/
GDALDataset *NWT_GRDDataset::Open(GDALOpenInfo * poOpenInfo) {
    if (!Identify(poOpenInfo))
        return NULL;

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    int nBandsToCreate = 0;

    NWT_GRDDataset *poDS = new NWT_GRDDataset();

    if (poOpenInfo->eAccess == GA_Update) {
        poDS->fp = VSIFOpenL(poOpenInfo->pszFilename, "rb+");
        nBandsToCreate = 1;
    } else {
        poDS->fp = VSIFOpenL(poOpenInfo->pszFilename, "rb");
        nBandsToCreate = atoi(CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "BAND_COUNT", "4"));
        if( nBandsToCreate != 1 && nBandsToCreate != 4 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Wrong value for BAND_COUNT");
            delete poDS;
            return NULL;
        }
    }
    if (poDS->fp == NULL) {
        delete poDS;
        return NULL;
    }

    poDS->eAccess = poOpenInfo->eAccess;

    /* -------------------------------------------------------------------- */
    /*      Read the header.                                                */
    /* -------------------------------------------------------------------- */
    VSIFSeekL(poDS->fp, 0, SEEK_SET);
    VSIFReadL(poDS->abyHeader, 1, 1024, poDS->fp);
    poDS->pGrd = reinterpret_cast<NWT_GRID *>(calloc(1, sizeof(NWT_GRID)));

    poDS->pGrd->fp = poDS->fp;

    if (!nwt_ParseHeader(poDS->pGrd, reinterpret_cast<char *>(poDS->abyHeader))
            || !GDALCheckDatasetDimensions(poDS->pGrd->nXSide,
                    poDS->pGrd->nYSide)) {
        delete poDS;
        return NULL;
    }

    poDS->nRasterXSize = poDS->pGrd->nXSide;
    poDS->nRasterYSize = poDS->pGrd->nYSide;

    // create a colorTable
    // if( poDS->pGrd->iNumColorInflections > 0 )
    //   poDS->CreateColorTable();
    nwt_LoadColors(poDS->ColorMap, 4096, poDS->pGrd);
    /* -------------------------------------------------------------------- */
    /*      Create band information objects.                                */
    /* If opening in read-only mode, then we create 4 bands (RGBZ)          */
    /* with data values being available in band 4. If opening in update mode*/
    /* we create 1 band (the data values). This is because in reality, there*/
    /* is only 1 band stored on disk. The RGB bands are 'virtual' - derived */
    /* from the data values on the fly                                      */
    /* -------------------------------------------------------------------- */
    for (int i = 0; i < nBandsToCreate; ++i) {
        poDS->SetBand(i + 1, new NWT_GRDRasterBand(poDS, i + 1, nBandsToCreate));
    }

    /* -------------------------------------------------------------------- */
    /*      Initialize any PAM information.                                 */
    /* -------------------------------------------------------------------- */
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML();

    /* -------------------------------------------------------------------- */
    /*      Check for external overviews.                                   */
    /* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize(poDS, poOpenInfo->pszFilename,
            poOpenInfo->GetSiblingFiles());

    return poDS;
}

/************************************************************************/
/*                                UpdateHeader()                        */
/************************************************************************/
int NWT_GRDDataset::UpdateHeader() {
    int iStatus = 0;
    TABRawBinBlock *poHeaderBlock = new TABRawBinBlock(TABReadWrite, TRUE);
    poHeaderBlock->InitNewBlock(fp, 1024);

    // Write the header string
    poHeaderBlock->WriteBytes(5, (GByte *) "HGPC1\0");

    // Version number
    poHeaderBlock->WriteFloat(pGrd->fVersion);

    // Dimensions
    poHeaderBlock->WriteInt16(static_cast<GInt16>(pGrd->nXSide));
    poHeaderBlock->WriteInt16(static_cast<GInt16>(pGrd->nYSide));

    // Extents
    poHeaderBlock->WriteDouble(pGrd->dfMinX);
    poHeaderBlock->WriteDouble(pGrd->dfMaxX);
    poHeaderBlock->WriteDouble(pGrd->dfMinY);
    poHeaderBlock->WriteDouble(pGrd->dfMaxY);

    // Z value range
    poHeaderBlock->WriteFloat(pGrd->fZMin);
    poHeaderBlock->WriteFloat(pGrd->fZMax);
    poHeaderBlock->WriteFloat(pGrd->fZMinScale);
    poHeaderBlock->WriteFloat(pGrd->fZMaxScale);

    // Description String
    int nChar = static_cast<int>(strlen(pGrd->cDescription));
    poHeaderBlock->WriteBytes(nChar, (GByte*) pGrd->cDescription);
    poHeaderBlock->WriteZeros(32 - nChar);

    // Unit Name String
    nChar = static_cast<int>(strlen(pGrd->cZUnits));
    poHeaderBlock->WriteBytes(nChar, (GByte *) pGrd->cZUnits);
    poHeaderBlock->WriteZeros(32 - nChar);

    //Ignore 126 - 141 as unknown usage
    poHeaderBlock->WriteZeros(15);

    // Hill shading
    poHeaderBlock->WriteInt16(pGrd->bHillShadeExists ? 1 : 0);
    poHeaderBlock->WriteInt16((short) 0);

    poHeaderBlock->WriteByte(pGrd->cHillShadeBrightness);
    poHeaderBlock->WriteByte(pGrd->cHillShadeContrast);

    //Ignore 147 - 257 as unknown usage
    poHeaderBlock->WriteZeros(110);

    // Write spatial reference
    poHeaderBlock->WriteBytes((int) strlen(pGrd->cMICoordSys),
            (GByte*) pGrd->cMICoordSys);
    poHeaderBlock->WriteZeros(256 - (int) strlen(pGrd->cMICoordSys));

    // Unit code
    poHeaderBlock->WriteByte(static_cast<GByte>(pGrd->iZUnits));

    // Info on shading
    GByte byDisplayStatus = 0;
    if (pGrd->bShowHillShade) {
        byDisplayStatus |= 1 << 6;
    }
    if (pGrd->bShowGradient) {
        byDisplayStatus |= 1 << 7;
    }

    poHeaderBlock->WriteByte(byDisplayStatus);
    poHeaderBlock->WriteInt16((short) 0); //Data Type?

    // Colour inflections
    poHeaderBlock->WriteInt16(pGrd->iNumColorInflections);
    for (int i = 0; i < pGrd->iNumColorInflections; i++) {
        poHeaderBlock->WriteFloat(pGrd->stInflection[i].zVal);
        poHeaderBlock->WriteByte(pGrd->stInflection[i].r);
        poHeaderBlock->WriteByte(pGrd->stInflection[i].g);
        poHeaderBlock->WriteByte(pGrd->stInflection[i].b);
    }

    // Fill in unused blanks
    poHeaderBlock->WriteZeros((966 - poHeaderBlock->GetCurAddress()));

    // Azimuth and Inclination
    poHeaderBlock->WriteFloat(pGrd->fHillShadeAzimuth);
    poHeaderBlock->WriteFloat(pGrd->fHillShadeAngle);

    // Write to disk
    iStatus = poHeaderBlock->CommitToFile();

    delete poHeaderBlock;

    // Update the TAB file to catch any changes
    if( WriteTab() != 0 )
        iStatus = -1;

    return iStatus;
}

int NWT_GRDDataset::WriteTab() {
    // Create the filename for the .tab file.
    const std::string sTabFile(CPLResetExtension(pGrd->szFileName, "tab"));

    VSILFILE *tabfp = VSIFOpenL(sTabFile.c_str(), "wt");
    if( tabfp == NULL)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed to create file `%s'", sTabFile.c_str());
        return -1;
    }

    bool bOK = true;
    bOK &= VSIFPrintfL(tabfp, "!table\n") > 0;
    bOK &= VSIFPrintfL(tabfp, "!version 500\n") > 0;
    bOK &= VSIFPrintfL(tabfp, "!charset %s\n", "Neutral") > 0;
    bOK &= VSIFPrintfL(tabfp, "\n") > 0;

    bOK &= VSIFPrintfL(tabfp, "Definition Table\n") > 0;
    const std::string path(pGrd->szFileName);
    const std::string basename = path.substr(path.find_last_of("/\\") + 1);
    bOK &= VSIFPrintfL(tabfp, "  File \"%s\"\n", basename.c_str()) > 0;
    bOK &= VSIFPrintfL(tabfp, "  Type \"RASTER\"\n") > 0;

    double dMapUnitsPerPixel =
        (pGrd->dfMaxX - pGrd->dfMinX) /
        (static_cast<double>(pGrd->nXSide) - 1);
    double dShift = dMapUnitsPerPixel / 2.0;

    bOK &= VSIFPrintfL(tabfp, "  (%f,%f) (%d,%d) Label \"Pt 1\",\n",
                pGrd->dfMinX - dShift, pGrd->dfMaxY + dShift, 0, 0) > 0;
    bOK &= VSIFPrintfL(tabfp, "  (%f,%f) (%d,%d) Label \"Pt 2\",\n",
                pGrd->dfMaxX - dShift, pGrd->dfMinY + dShift, pGrd->nXSide - 1,
                pGrd->nYSide - 1) > 0;
    bOK &= VSIFPrintfL(tabfp, "  (%f,%f) (%d,%d) Label \"Pt 3\"\n",
                pGrd->dfMinX - dShift, pGrd->dfMinY + dShift, 0,
                pGrd->nYSide - 1) > 0;

    bOK &= VSIFPrintfL(tabfp, "  CoordSys %s\n",pGrd->cMICoordSys) > 0;
    bOK &= VSIFPrintfL(tabfp, "  Units \"m\"\n") > 0;

    // Raster Styles.

    // Raster is a grid, which is style 6.
    bOK &= VSIFPrintfL(tabfp, "  RasterStyle 6 1\n") > 0;

    // Brightness - style 1
    if( pGrd->style.iBrightness > 0 )
    {
        bOK &= VSIFPrintfL(tabfp, "  RasterStyle 1 %d\n",pGrd->style.iBrightness) > 0;
    }

    // Contrast - style 2
    if( pGrd->style.iContrast > 0 )
    {
        bOK &= VSIFPrintfL(tabfp, "  RasterStyle 2 %d\n",pGrd->style.iContrast) > 0;
    }

    // Greyscale - style 3; only need to write if TRUE
    if( pGrd->style.bGreyscale == TRUE )
    {
        bOK &= VSIFPrintfL(tabfp, "  RasterStyle 3 1\n") > 0;
    }

    // Flag to render one colour transparent - style 4
    if( pGrd->style.bTransparent == TRUE )
    {
        bOK &= VSIFPrintfL(tabfp, "  RasterStyle 4 1\n") > 0;
        if( pGrd->style.iTransColour > 0 )
        {
            bOK &= VSIFPrintfL(tabfp, "  RasterStyle 7 %d\n",pGrd->style.iTransColour) > 0;
        }
    }

    // Transparency of immage
    if( pGrd->style.iTranslucency > 0 )
    {
        bOK &= VSIFPrintfL(tabfp, "  RasterStyle 8 %d\n",pGrd->style.iTranslucency) > 0;
    }

    bOK &= VSIFPrintfL(tabfp, "begin_metadata\n") > 0;
    bOK &= VSIFPrintfL(tabfp, "\"\\MapInfo\" = \"\"\n") > 0;
    bOK &= VSIFPrintfL(tabfp, "\"\\Vm\" = \"\"\n") > 0;
    bOK &= VSIFPrintfL(tabfp, "\"\\Vm\\Grid\" = \"Numeric\"\n") > 0;
    bOK &= VSIFPrintfL(tabfp, "\"\\Vm\\GridName\" = \"%s\"\n", basename.c_str()) > 0;
    bOK &= VSIFPrintfL(tabfp, "\"\\IsReadOnly\" = \"FALSE\"\n") > 0;
    bOK &= VSIFPrintfL(tabfp, "end_metadata\n") > 0;

    if( VSIFCloseL(tabfp) != 0 )
        bOK = false;

    return (bOK) ? 0 : -1;
}

/************************************************************************/
/*                                Create()                              */
/************************************************************************/
GDALDataset *NWT_GRDDataset::Create(const char * pszFilename, int nXSize,
        int nYSize, int nBands, GDALDataType eType, char ** papszParmList) {
    if (nBands != 1) {
        CPLError(CE_Failure, CPLE_FileIO,
                "Only single band datasets are supported for writing");
        return NULL;
    }
    if (eType != GDT_Float32) {
        CPLError(CE_Failure, CPLE_FileIO,
                "Float32 is the only supported data type");
        return NULL;
    }
    NWT_GRDDataset *poDS = new NWT_GRDDataset();
    poDS->eAccess = GA_Update;
    poDS->pGrd = reinterpret_cast<NWT_GRID *>(calloc(1, sizeof(NWT_GRID)));

    // We currently only support GRD grid types (could potentially support GRC in the papszParmList).
    // Also only support GDT_Float32 as the data type. GRD format allows for data to be stretched to
    // 32bit or 16bit integers on disk, so it would be feasible to support other data types
    poDS->pGrd->cFormat = 0x00;

    // File version
    poDS->pGrd->fVersion = 2.0;

    // Dimensions
    poDS->pGrd->nXSide = nXSize;
    poDS->pGrd->nYSide = nYSize;
    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;

    // Some default values to get started with. These will
    // be altered when SetGeoTransform is called.
    poDS->pGrd->dfMinX = -2E+307;
    poDS->pGrd->dfMinY = -2E+307;
    poDS->pGrd->dfMaxX = 2E+307;
    poDS->pGrd->dfMaxY = 2E+307;

    float fZMin, fZMax;
    // See if the user passed the min/max values
    if (CSLFetchNameValue(papszParmList, "ZMIN") == NULL) {
        fZMin = (float) -2E+37;
    } else {
        fZMin = static_cast<float>(CPLAtof(CSLFetchNameValue(papszParmList, "ZMIN")));
    }

    if (CSLFetchNameValue(papszParmList, "ZMAX") == NULL) {
        fZMax = (float) 2E+38;
    } else {
        fZMax = static_cast<float>(CPLAtof(CSLFetchNameValue(papszParmList, "ZMAX")));
    }

    poDS->pGrd->fZMin = fZMin;
    poDS->pGrd->fZMax = fZMax;
    //pGrd->dfStepSize = (pGrd->dfMaxX - pGrd->dfMinX) / (pGrd->nXSide - 1);
    poDS->pGrd->fZMinScale = fZMin;
    poDS->pGrd->fZMaxScale = fZMax;
    //poDS->pGrd->iZUnits
    memset(poDS->pGrd->cZUnits, 0, 32);
    memset(poDS->pGrd->cMICoordSys, 0, 256);

    // Some default colour inflections; Basic scale from blue to red
    poDS->pGrd->iNumColorInflections = 3;

    // Lowest inflection
    poDS->pGrd->stInflection[0].zVal = poDS->pGrd->fZMin;
    poDS->pGrd->stInflection[0].r = 0;
    poDS->pGrd->stInflection[0].g = 0;
    poDS->pGrd->stInflection[0].b = 255;

    // Mean inflection
    poDS->pGrd->stInflection[1].zVal = (poDS->pGrd->fZMax - poDS->pGrd->fZMin)
            / 2;
    poDS->pGrd->stInflection[1].r = 255;
    poDS->pGrd->stInflection[1].g = 255;
    poDS->pGrd->stInflection[1].b = 0;

    // Highest inflection
    poDS->pGrd->stInflection[2].zVal = poDS->pGrd->fZMax;
    poDS->pGrd->stInflection[2].r = 255;
    poDS->pGrd->stInflection[2].g = 0;
    poDS->pGrd->stInflection[2].b = 0;

    poDS->pGrd->bHillShadeExists = FALSE;
    poDS->pGrd->bShowGradient = FALSE;
    poDS->pGrd->bShowHillShade = FALSE;
    poDS->pGrd->cHillShadeBrightness = 0;
    poDS->pGrd->cHillShadeContrast = 0;
    poDS->pGrd->fHillShadeAzimuth = 0;
    poDS->pGrd->fHillShadeAngle = 0;

    // Set the raster style settings. These aren't used anywhere other than to write the TAB file
    if (CSLFetchNameValue(papszParmList, "BRIGHTNESS") == NULL) {
        poDS->pGrd->style.iBrightness = 50;
    } else {
        poDS->pGrd->style.iBrightness = atoi(
                CSLFetchNameValue(papszParmList, "BRIGHTNESS"));
    }

    if (CSLFetchNameValue(papszParmList, "CONTRAST") == NULL) {
        poDS->pGrd->style.iContrast = 50;
    } else {
        poDS->pGrd->style.iContrast = atoi(
                CSLFetchNameValue(papszParmList, "CONTRAST"));
    }

    if (CSLFetchNameValue(papszParmList, "TRANSCOLOR") == NULL) {
        poDS->pGrd->style.iTransColour = 0;
    } else {
        poDS->pGrd->style.iTransColour = atoi(
                CSLFetchNameValue(papszParmList, "TRANSCOLOR"));
    }

    if (CSLFetchNameValue(papszParmList, "TRANSLUCENCY") == NULL) {
        poDS->pGrd->style.iTranslucency = 0;
    } else {
        poDS->pGrd->style.iTranslucency = atoi(
                CSLFetchNameValue(papszParmList, "TRANSLUCENCY"));
    }

    poDS->pGrd->style.bGreyscale = FALSE;
    poDS->pGrd->style.bGrey = FALSE;
    poDS->pGrd->style.bColour = FALSE;
    poDS->pGrd->style.bTransparent = FALSE;

    // Open the grid file
    poDS->fp = VSIFOpenL(pszFilename, "wb");
    if (poDS->fp == NULL) {
        CPLError(CE_Failure, CPLE_FileIO, "Failed to create GRD file");
        delete poDS;
        return NULL;
    }

    poDS->pGrd->fp = poDS->fp;
    strncpy(poDS->pGrd->szFileName, pszFilename,
            sizeof(poDS->pGrd->szFileName));
    poDS->pGrd->szFileName[sizeof(poDS->pGrd->szFileName) - 1] = '\0';

// Seek to the start of the file and enter the default header info
    VSIFSeekL(poDS->fp, 0, SEEK_SET);
    if (poDS->UpdateHeader() != 0) {
        CPLError(CE_Failure, CPLE_FileIO, "Failed to create GRD file");
        delete poDS;
        return NULL;
    }

    /* -------------------------------------------------------------------- */
    /*      Create band information objects;                                */
    /*      Only 1 band is allowed                                          */
    /* -------------------------------------------------------------------- */
    poDS->SetBand(1, new NWT_GRDRasterBand(poDS, 1, 1));    //z

    poDS->oOvManager.Initialize(poDS, pszFilename);
    poDS->FlushCache(); // Write the header to disk.

    return poDS;
}

/************************************************************************/
/*                                CreateCopy()                          */
/************************************************************************/
GDALDataset * NWT_GRDDataset::CreateCopy(const char * pszFilename,
        GDALDataset * poSrcDS, int bStrict, char **papszOptions,
        GDALProgressFunc pfnProgress, void * pProgressData) {

    if( poSrcDS->GetRasterCount() != 1 )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                "Only single band datasets are supported for writing");
        return NULL;
    }

    char **tmpOptions = CSLDuplicate(papszOptions);

    /*
    * Compute the statistics if ZMAX and ZMIN are not provided
    */
    double dfMin = 0.0;
    double dfMax = 0.0;
    double dfMean = 0.0;
    double dfStdDev = 0.0;
    GDALRasterBand *pBand = poSrcDS->GetRasterBand(1);
    char sMax[10] = {};
    char sMin[10] = {};

    if ((CSLFetchNameValue(papszOptions, "ZMAX") == NULL)
            || (CSLFetchNameValue(papszOptions, "ZMIN") == NULL)) {
        CPL_IGNORE_RET_VAL(pBand->GetStatistics(FALSE, TRUE, &dfMin, &dfMax, &dfMean,
                &dfStdDev));
    }

    if (CSLFetchNameValue(papszOptions, "ZMAX") == NULL) {
        CPLsnprintf(sMax, sizeof(sMax), "%f", dfMax);
        tmpOptions = CSLSetNameValue(tmpOptions, "ZMAX", sMax);
    }
    if (CSLFetchNameValue(papszOptions, "ZMIN") == NULL) {
        CPLsnprintf(sMin, sizeof(sMin), "%f", dfMin);
        tmpOptions = CSLSetNameValue(tmpOptions, "ZMIN", sMin);
    }

    GDALDriver *poDriver = (GDALDriver *) GDALGetDriverByName("NWT_GRD");
    GDALDataset *poDstDS = poDriver->DefaultCreateCopy(pszFilename, poSrcDS,
            bStrict, tmpOptions, pfnProgress, pProgressData);

    CSLDestroy(tmpOptions);

    return poDstDS;
}

/************************************************************************/
/*                          GDALRegister_GRD()                          */
/************************************************************************/
void GDALRegister_NWT_GRD() {
    if (GDALGetDriverByName("NWT_GRD") != NULL)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("NWT_GRD");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
            "Northwood Numeric Grid Format .grd/.tab");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "frmt_nwtgrd.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "grd");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Float32");

    poDriver->SetMetadataItem(GDAL_DMD_OPENOPTIONLIST,
      "<OpenOptionList>"
      "    <Option name='BAND_COUNT' type='int' description='1 (Z) or 4 (RGBZ). Only used in read-only mode' default='4'/>"
      "</OpenOptionList>");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
      "<CreationOptionList>"
      "    <Option name='ZMIN' type='float' description='Minimum cell value of raster for defining RGB scaling' default='-2E+37'/>"
      "    <Option name='ZMAX' type='float' description='Maximum cell value of raster for defining RGB scaling' default='2E+38'/>"
      "    <Option name='BRIGHTNESS' type='int' description='Brightness to be recorded in TAB file. Only affects reading with MapInfo' default='50'/>"
      "    <Option name='CONTRAST' type='int' description='Contrast to be recorded in TAB file. Only affects reading with MapInfo' default='50'/>"
      "    <Option name='TRANSCOLOR' type='int' description='Transparent color to be recorded in TAB file. Only affects reading with MapInfo' default='0'/>"
      "    <Option name='TRANSLUCENCY' type='int' description='Level of translucency to be recorded in TAB file. Only affects reading with MapInfo' default='0'/>"
      "</CreationOptionList>");

    poDriver->pfnOpen = NWT_GRDDataset::Open;
    poDriver->pfnIdentify = NWT_GRDDataset::Identify;
    poDriver->pfnCreate = NWT_GRDDataset::Create;
    poDriver->pfnCreateCopy = NWT_GRDDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
