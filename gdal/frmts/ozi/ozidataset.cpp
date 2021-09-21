/******************************************************************************
 *
 * Project:   OZF2 and OZFx3 binary files driver
 * Purpose:  GDALDataset driver for OZF2 and OZFx3 binary files.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2010-2012, Even Rouault <even dot rouault at spatialys.com>
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

#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "zlib.h"

/* g++ -fPIC -g -Wall frmts/ozi/ozidataset.cpp -shared -o gdal_OZI.so -Iport -Igcore -Iogr -L. -lgdal  */

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                              OZIDataset                              */
/* ==================================================================== */
/************************************************************************/

class OZIRasterBand;

class OZIDataset final: public GDALPamDataset
{
    friend class OZIRasterBand;

    VSILFILE* fp;
    int       nZoomLevelCount;
    int*      panZoomLevelOffsets;
    OZIRasterBand** papoOvrBands;
    vsi_l_offset nFileSize;

    int bOzi3;
    GByte nKeyInit;

  public:
                 OZIDataset();
    virtual     ~OZIDataset();

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                         OZIRasterBand                                */
/* ==================================================================== */
/************************************************************************/

class OZIRasterBand final: public GDALPamRasterBand
{
    friend class OZIDataset;

    int nXBlocks;
    int nZoomLevel;
    GDALColorTable* poColorTable;
    GByte* pabyTranslationTable;

  public:

                OZIRasterBand( OZIDataset *, int nZoomLevel,
                               int nRasterXSize, int nRasterYSize,
                               int nXBlocks,
                               GDALColorTable* poColorTable);
    virtual    ~OZIRasterBand();

    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual GDALColorInterp GetColorInterpretation() override;
    virtual GDALColorTable *GetColorTable() override;

    virtual int         GetOverviewCount() override;
    virtual GDALRasterBand* GetOverview(int nLevel) override;
};

/************************************************************************/
/*                             I/O functions                            */
/************************************************************************/

constexpr GByte abyKey[] =
{
    0x2D, 0x4A, 0x43, 0xF1, 0x27, 0x9B, 0x69, 0x4F,
    0x36, 0x52, 0x87, 0xEC, 0x5F, 0x42, 0x53, 0x22,
    0x9E, 0x8B, 0x2D, 0x83, 0x3D, 0xD2, 0x84, 0xBA,
    0xD8, 0x5B
};

static void OZIDecrypt(void *pabyVal, int n, GByte nKeyInit)
{
    for(int i = 0; i < n; i++)
    {
        reinterpret_cast<GByte*>( pabyVal )[i]
            ^= abyKey[i % sizeof(abyKey)] + nKeyInit;
    }
}

static int ReadInt(GByte**pptr)
{
    int nVal;
    memcpy(&nVal, *pptr, 4);
    *pptr += 4;
    CPL_LSBPTR32(&nVal);
    return nVal;
}

static short ReadShort(GByte**pptr)
{
    short nVal;
    memcpy(&nVal, *pptr, 2);
    *pptr += 2;
    CPL_LSBPTR16(&nVal);
    return nVal;
}

static int ReadInt( VSILFILE* fp, int bOzi3 = FALSE, int nKeyInit = 0 )
{
    int nVal;
    VSIFReadL(&nVal, 1, 4, fp);
    if (bOzi3)
    {
        GByte abyVal[4];
        memcpy(&abyVal[0], &nVal, 4);
        OZIDecrypt(&abyVal[0], 4, static_cast<GByte>( nKeyInit ) );
        memcpy(&nVal, &abyVal[0], 4);
    }
    CPL_LSBPTR32(&nVal);
    return nVal;
}

static short ReadShort( VSILFILE* fp, int bOzi3 = FALSE, int nKeyInit = 0 )
{
    short nVal;
    VSIFReadL(&nVal, 1, 2, fp);
    if (bOzi3)
    {
        GByte abyVal[2];
        memcpy(&abyVal[0], &nVal, 2);
        OZIDecrypt(&abyVal[0], 2, static_cast<GByte>( nKeyInit ) );
        memcpy(&nVal, &abyVal[0], 2);
    }
    CPL_LSBPTR16(&nVal);
    return nVal;
}

/************************************************************************/
/*                         OZIRasterBand()                             */
/************************************************************************/

OZIRasterBand::OZIRasterBand( OZIDataset *poDSIn, int nZoomLevelIn,
                              int nRasterXSizeIn, int nRasterYSizeIn,
                              int nXBlocksIn,
                              GDALColorTable* poColorTableIn ) :
    nXBlocks(nXBlocksIn),
    nZoomLevel(nZoomLevelIn),
    poColorTable(poColorTableIn),
    pabyTranslationTable(nullptr)
{
    poDS = poDSIn;
    nBand = 1;

    eDataType = GDT_Byte;

    nBlockXSize = 64;
    nBlockYSize = 64;

    nRasterXSize = nRasterXSizeIn;
    nRasterYSize = nRasterYSizeIn;
}

/************************************************************************/
/*                        ~OZIRasterBand()                             */
/************************************************************************/

OZIRasterBand::~OZIRasterBand()
{
    delete poColorTable;
    CPLFree(pabyTranslationTable);
}

/************************************************************************/
/*                        GetColorInterpretation()                      */
/************************************************************************/

GDALColorInterp OZIRasterBand::GetColorInterpretation()
{
    return GCI_PaletteIndex;
}

/************************************************************************/
/*                            GetColorTable()                           */
/************************************************************************/

GDALColorTable* OZIRasterBand::GetColorTable()
{
    return poColorTable;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr OZIRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    OZIDataset *poGDS = reinterpret_cast<OZIDataset *>( poDS );

    const int nBlock = nBlockYOff * nXBlocks + nBlockXOff;

    VSIFSeekL(poGDS->fp, poGDS->panZoomLevelOffsets[nZoomLevel] +
                         12 + 1024 + 4 * nBlock, SEEK_SET);
    const int nPointer = ReadInt(poGDS->fp, poGDS->bOzi3, poGDS->nKeyInit);
    if (nPointer < 0  || (vsi_l_offset)nPointer >= poGDS->nFileSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid offset for block (%d, %d) : %d",
                 nBlockXOff, nBlockYOff, nPointer);
        return CE_Failure;
    }
    int nNextPointer = ReadInt(poGDS->fp, poGDS->bOzi3, poGDS->nKeyInit);
    if (nNextPointer <= nPointer + 16  ||
        (vsi_l_offset)nNextPointer >= poGDS->nFileSize ||
        nNextPointer - nPointer > 10 * 64 * 64)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid next offset for block (%d, %d) : %d",
                 nBlockXOff, nBlockYOff, nNextPointer);
        return CE_Failure;
    }

    VSIFSeekL(poGDS->fp, nPointer, SEEK_SET);

    const int nToRead = nNextPointer - nPointer;
    GByte* pabyZlibBuffer = reinterpret_cast<GByte *>( CPLMalloc( nToRead ) );
    if (VSIFReadL(pabyZlibBuffer, nToRead, 1, poGDS->fp) != 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Not enough byte read for block (%d, %d)",
                 nBlockXOff, nBlockYOff);
        CPLFree(pabyZlibBuffer);
        return CE_Failure;
    }

    if (poGDS->bOzi3)
        OZIDecrypt(pabyZlibBuffer, 16, poGDS->nKeyInit);

    if (pabyZlibBuffer[0] != 0x78 ||
        pabyZlibBuffer[1] != 0xDA)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Bad ZLIB signature for block (%d, %d) : 0x%02X 0x%02X",
                 nBlockXOff, nBlockYOff, pabyZlibBuffer[0], pabyZlibBuffer[1]);
        CPLFree(pabyZlibBuffer);
        return CE_Failure;
    }

    z_stream      stream;
    memset(&stream, 0, sizeof(stream));
    stream.zalloc = (alloc_func)nullptr;
    stream.zfree = (free_func)nullptr;
    stream.opaque = (voidpf)nullptr;
    stream.next_in = pabyZlibBuffer + 2;
    stream.avail_in = nToRead - 2;

    int err = inflateInit2(&(stream), -MAX_WBITS);

    for( int i = 0; i < 64 && err == Z_OK; i++ )
    {
        stream.next_out = reinterpret_cast<Bytef *>( pImage ) + (63 - i) * 64;
        stream.avail_out = 64;
        err = inflate(& (stream), Z_NO_FLUSH);
        if (err != Z_OK && err != Z_STREAM_END)
            break;

        if (pabyTranslationTable)
        {
            GByte* ptr = reinterpret_cast<GByte *>( pImage ) + (63 - i) * 64;
            for( int j = 0; j < 64; j++ )
            {
                *ptr = pabyTranslationTable[*ptr];
                ptr ++;
            }
        }
    }

    inflateEnd(&stream);

    CPLFree(pabyZlibBuffer);

    return (err == Z_OK || err == Z_STREAM_END) ? CE_None : CE_Failure;
}

/************************************************************************/
/*                         GetOverviewCount()                            */
/************************************************************************/

int OZIRasterBand::GetOverviewCount()
{
    if (nZoomLevel != 0)
        return 0;

    OZIDataset *poGDS = reinterpret_cast<OZIDataset *>( poDS );
    return poGDS->nZoomLevelCount - 1;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand* OZIRasterBand::GetOverview(int nLevel)
{
    if (nZoomLevel != 0)
        return nullptr;

    OZIDataset *poGDS = reinterpret_cast<OZIDataset *>( poDS );
    if (nLevel < 0 || nLevel >= poGDS->nZoomLevelCount - 1)
        return nullptr;

    return poGDS->papoOvrBands[nLevel + 1];
}

/************************************************************************/
/*                            ~OZIDataset()                            */
/************************************************************************/

OZIDataset::OZIDataset() :
    fp(nullptr),
    nZoomLevelCount(0),
    panZoomLevelOffsets(nullptr),
    papoOvrBands(nullptr),
    nFileSize(0),
    bOzi3(FALSE),
    nKeyInit(0)
{}

/************************************************************************/
/*                            ~OZIDataset()                            */
/************************************************************************/

OZIDataset::~OZIDataset()
{
    if (fp)
        VSIFCloseL(fp);
    if (papoOvrBands != nullptr )
    {
        /* start at 1: do not destroy the base band ! */
        for(int i=1;i<nZoomLevelCount;i++)
            delete papoOvrBands[i];
        CPLFree(papoOvrBands);
    }
    CPLFree(panZoomLevelOffsets);
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int OZIDataset::Identify( GDALOpenInfo * poOpenInfo )
{
    if (poOpenInfo->nHeaderBytes < 14)
        return FALSE;

    if (poOpenInfo->pabyHeader[0] == 0x80 &&
        poOpenInfo->pabyHeader[1] == 0x77)
        return TRUE;

    return poOpenInfo->pabyHeader[0] == 0x78 &&
           poOpenInfo->pabyHeader[1] == 0x77 &&
           poOpenInfo->pabyHeader[6] == 0x40 &&
           poOpenInfo->pabyHeader[7] == 0x00 &&
           poOpenInfo->pabyHeader[8] == 0x01 &&
           poOpenInfo->pabyHeader[9] == 0x00 &&
           poOpenInfo->pabyHeader[10] == 0x36 &&
           poOpenInfo->pabyHeader[11] == 0x04 &&
           poOpenInfo->pabyHeader[12] == 0x00 &&
           poOpenInfo->pabyHeader[13] == 0x00;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *OZIDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if (!Identify(poOpenInfo))
        return nullptr;

    GByte abyHeader[14];
    memcpy(abyHeader, poOpenInfo->pabyHeader, 14);

    int bOzi3 = (abyHeader[0] == 0x80 &&
                 abyHeader[1] == 0x77);

    const CPLString osImgFilename = poOpenInfo->pszFilename;
    VSILFILE* fp = VSIFOpenL(osImgFilename.c_str(), "rb");
    if (fp == nullptr)
        return nullptr;

    OZIDataset* poDS = new OZIDataset();
    poDS->fp = fp;

    GByte nKeyInit = 0;
    if (bOzi3)
    {
        VSIFSeekL(fp, 14, SEEK_SET);

        GByte nRandomNumber = 0;
        VSIFReadL(&nRandomNumber, 1, 1, fp);
        //printf("nRandomNumber = %d\n", nRandomNumber);
        if (nRandomNumber < 0x94)
        {
            delete poDS;
            return nullptr;
        }
        VSIFSeekL(fp, 0x93, SEEK_CUR);
        VSIFReadL(&nKeyInit, 1, 1, fp);

        VSIFSeekL(fp, 0, SEEK_SET);
        VSIFReadL(abyHeader, 1, 14, fp);
        OZIDecrypt(abyHeader, 14, nKeyInit);
        if (!(abyHeader[6] == 0x40 &&
           abyHeader[7] == 0x00 &&
           abyHeader[8] == 0x01 &&
           abyHeader[9] == 0x00 &&
           abyHeader[10] == 0x36 &&
           abyHeader[11] == 0x04 &&
           abyHeader[12] == 0x00 &&
           abyHeader[13] == 0x00))
        {
            delete poDS;
            return nullptr;
        }

        VSIFSeekL(fp, 14 + 1 + nRandomNumber, SEEK_SET);
        const int nMagic = ReadInt(fp, bOzi3, nKeyInit);
        CPLDebug("OZI", "OZI version code : 0x%08X", nMagic);

        poDS->bOzi3 = bOzi3;
    }
    else
    {
        VSIFSeekL(fp, 14, SEEK_SET);
    }

    GByte abyHeader2[40], abyHeader2_Backup[40];
    VSIFReadL(abyHeader2, 40, 1, fp);
    memcpy(abyHeader2_Backup, abyHeader2, 40);

    /* There's apparently a relationship between the nMagic number */
    /* and the nKeyInit, but I'm too lazy to add switch/cases that might */
    /* be not exhaustive, so let's try the 'brute force' attack !!! */
    /* It is much so funny to be able to run one in a few microseconds :-) */
    for( int i = 0; i < 256; i++ )
    {
        nKeyInit = static_cast<GByte>( i );
        GByte* pabyHeader2 = abyHeader2;
        if (bOzi3)
            OZIDecrypt(abyHeader2, 40, nKeyInit);

        const int nHeaderSize = ReadInt(&pabyHeader2); /* should be 40 */
        poDS->nRasterXSize = ReadInt(&pabyHeader2);
        poDS->nRasterYSize = ReadInt(&pabyHeader2);
        const int nDepth = ReadShort(&pabyHeader2); /* should be 1 */
        const int nBPP = ReadShort(&pabyHeader2); /* should be 8 */
        ReadInt(&pabyHeader2); /* reserved */
        ReadInt(&pabyHeader2); /* pixel number (height * width) : unused */
        ReadInt(&pabyHeader2); /* reserved */
        ReadInt(&pabyHeader2); /* reserved */
        ReadInt(&pabyHeader2); /* ?? 0x100 */
        ReadInt(&pabyHeader2); /* ?? 0x100 */

        if (nHeaderSize != 40 || nDepth != 1 || nBPP != 8)
        {
            if (bOzi3)
            {
                if (nKeyInit != 255)
                {
                    memcpy(abyHeader2, abyHeader2_Backup,40);
                    continue;
                }
                else
                {
                  CPLDebug( "OZI", "Cannot decipher 2nd header. Sorry..." );
                    delete poDS;
                    return nullptr;
                }
            }
            else
            {
                CPLDebug("OZI", "nHeaderSize = %d, nDepth = %d, nBPP = %d",
                        nHeaderSize, nDepth, nBPP);
                delete poDS;
                return nullptr;
            }
        }
        else
            break;
    }
    poDS->nKeyInit = nKeyInit;

    int nSeparator = ReadInt(fp);
    if (!bOzi3 && nSeparator != 0x77777777)
    {
        CPLDebug("OZI", "didn't get end of header2 marker");
        delete poDS;
        return nullptr;
    }

    poDS->nZoomLevelCount = ReadShort(fp);

#ifdef DEBUG_VERBOSE
    CPLDebug("OZI", "nZoomLevelCount = %d", poDS->nZoomLevelCount);
#endif

    if (poDS->nZoomLevelCount < 0 || poDS->nZoomLevelCount >= 256)
    {
        CPLDebug("OZI", "nZoomLevelCount = %d", poDS->nZoomLevelCount);
        delete poDS;
        return nullptr;
    }

    /* Skip array of zoom level percentage. We don't need it for GDAL */
    VSIFSeekL(fp, sizeof(float) * poDS->nZoomLevelCount, SEEK_CUR);

    nSeparator = ReadInt(fp);
    if (!bOzi3 && nSeparator != 0x77777777)
    {
        /* Some files have 8 extra bytes before the marker. I'm not sure */
        /* what they are used for. So just skip them and hope that */
        /* we'll find the marker */
        CPL_IGNORE_RET_VAL(ReadInt(fp));
        nSeparator = ReadInt(fp);
        if (nSeparator != 0x77777777)
        {
            CPLDebug("OZI", "didn't get end of zoom levels marker");
            delete poDS;
            return nullptr;
        }
    }

    VSIFSeekL(fp, 0, SEEK_END);
    const vsi_l_offset nFileSize = VSIFTellL(fp);
    poDS->nFileSize = nFileSize;
    VSIFSeekL(fp, nFileSize - 4, SEEK_SET);
    const int nZoomLevelTableOffset = ReadInt(fp, bOzi3, nKeyInit);
    if (nZoomLevelTableOffset < 0 ||
        (vsi_l_offset)nZoomLevelTableOffset >= nFileSize)
    {
        CPLDebug("OZI", "nZoomLevelTableOffset = %d",
                 nZoomLevelTableOffset);
        delete poDS;
        return nullptr;
    }

    VSIFSeekL(fp, nZoomLevelTableOffset, SEEK_SET);

    poDS->panZoomLevelOffsets =
        reinterpret_cast<int *>(
            CPLMalloc( sizeof(int) * poDS->nZoomLevelCount ) );

    for( int i = 0; i < poDS->nZoomLevelCount; i++ )
    {
        poDS->panZoomLevelOffsets[i] = ReadInt(fp, bOzi3, nKeyInit);
        if (poDS->panZoomLevelOffsets[i] < 0 ||
            (vsi_l_offset)poDS->panZoomLevelOffsets[i] >= nFileSize)
        {
            CPLDebug("OZI", "panZoomLevelOffsets[%d] = %d",
                     i, poDS->panZoomLevelOffsets[i]);
            delete poDS;
            return nullptr;
        }
    }

    poDS->papoOvrBands =
        reinterpret_cast<OZIRasterBand **>(
            CPLCalloc( sizeof( OZIRasterBand * ), poDS->nZoomLevelCount ) );

    for( int i = 0; i < poDS->nZoomLevelCount; i++ )
    {
        VSIFSeekL(fp, poDS->panZoomLevelOffsets[i], SEEK_SET);
        const int nW = ReadInt(fp, bOzi3, nKeyInit);
        const int nH = ReadInt(fp, bOzi3, nKeyInit);
        const short nTileX = ReadShort(fp, bOzi3, nKeyInit);
        const short nTileY = ReadShort(fp, bOzi3, nKeyInit);
        if (i == 0 && (nW != poDS->nRasterXSize || nH != poDS->nRasterYSize))
        {
            CPLDebug("OZI", "zoom[%d] inconsistent dimensions for zoom level 0 "
                     ": nW=%d, nH=%d, nTileX=%d, nTileY=%d, nRasterXSize=%d, "
                     "nRasterYSize=%d",
                     i, nW, nH, nTileX, nTileY, poDS->nRasterXSize,
                     poDS->nRasterYSize);
            delete poDS;
            return nullptr;
        }
        /* Note (#3895): some files such as world.ozf2 provided with OziExplorer */
        /* expose nTileY=33, but have nH=2048, so only require 32 tiles in vertical dimension. */
        /* So there's apparently one extra and useless tile that will be ignored */
        /* without causing apparent issues */
        /* Some other files have more tile in horizontal direction than needed, so let's */
        /* accept that. But in that case we really need to keep the nTileX value for IReadBlock() */
        /* to work properly */
        if ((nW + 63) / 64 > nTileX || (nH + 63) / 64 > nTileY)
        {
            CPLDebug("OZI", "zoom[%d] unexpected number of tiles : nW=%d, "
                     "nH=%d, nTileX=%d, nTileY=%d",
                     i, nW, nH, nTileX, nTileY);
            delete poDS;
            return nullptr;
        }

        GDALColorTable* poColorTable = new GDALColorTable();
        GByte abyColorTable[256*4];
        VSIFReadL(abyColorTable, 1, 1024, fp);
        if (bOzi3)
            OZIDecrypt(abyColorTable, 1024, nKeyInit);
        for( int j = 0; j < 256; j++ )
        {
            GDALColorEntry sEntry;
            sEntry.c1 = abyColorTable[4*j + 2];
            sEntry.c2 = abyColorTable[4*j + 1];
            sEntry.c3 = abyColorTable[4*j + 0];
            sEntry.c4 = 255;
            poColorTable->SetColorEntry(j, &sEntry);
        }

        poDS->papoOvrBands[i] = new OZIRasterBand(poDS, i, nW, nH, nTileX, poColorTable);

        if (i > 0)
        {
            GByte* pabyTranslationTable =
                poDS->papoOvrBands[i]->GetIndexColorTranslationTo(poDS->papoOvrBands[0], nullptr, nullptr);

            delete poDS->papoOvrBands[i]->poColorTable;
            poDS->papoOvrBands[i]->poColorTable = poDS->papoOvrBands[0]->poColorTable->Clone();
            poDS->papoOvrBands[i]->pabyTranslationTable = pabyTranslationTable;
        }
    }

    poDS->SetBand(1, poDS->papoOvrBands[0]);

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Support overviews.                                              */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );
    return poDS;
}

/************************************************************************/
/*                         GDALRegister_OZI()                           */
/************************************************************************/

void GDALRegister_OZI()

{
    if( !GDAL_CHECK_VERSION( "OZI driver" ) )
        return;

    if( GDALGetDriverByName( "OZI" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "OZI" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "OziExplorer Image File" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/ozi.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = OZIDataset::Open;
    poDriver->pfnIdentify = OZIDataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
