/******************************************************************************
 * $Id: rpftocdataset.cpp
 *
 * Project:  RPF TOC read Translator
 * Purpose:  Implementation of RPFTOCDataset and RPFTOCSubDataset.
 * Author:   Even Rouault, even.rouault at mines-paris.org
 *
 ******************************************************************************
 * Copyright (c) 2007, Even Rouault
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

#include "gdal_pam.h"
#include "gdal_proxy.h"
#include "rpftoclib.h"
#include "ogr_spatialref.h"
#include "cpl_string.h"
#include "vrtdataset.h"
#include "cpl_multiproc.h"

#define GEOTRSFRM_TOPLEFT_X            0
#define GEOTRSFRM_WE_RES               1
#define GEOTRSFRM_ROTATION_PARAM1      2
#define GEOTRSFRM_TOPLEFT_Y            3
#define GEOTRSFRM_ROTATION_PARAM2      4
#define GEOTRSFRM_NS_RES               5

CPL_CVSID("$Id$");


/** Overview of used classes :
   - RPFTOCDataset : lists the different subdatasets, listed in the A.TOC,
                     as subdatasets
   - RPFTOCSubDataset : one of these subdatasets, implemented as a VRT, of
                        the relevant NITF tiles
   - RPFTOCProxyRasterDataSet : a "proxy" dataset that maps to a NITF tile
   - RPFTOCProxyRasterBandPalette / RPFTOCProxyRasterBandRGBA : bands of RPFTOCProxyRasterDataSet
*/

/************************************************************************/
/* ==================================================================== */
/*                            RPFTOCDataset                             */
/* ==================================================================== */
/************************************************************************/

class RPFTOCDataset : public GDALPamDataset
{
  char	    **papszSubDatasets;
  char       *pszProjection;
  int         bGotGeoTransform;
  double      adfGeoTransform[6];
  
  char      **papszFileList;

  public:
    RPFTOCDataset()
    {
        papszSubDatasets = NULL;
        pszProjection = NULL;
        bGotGeoTransform = FALSE;
        papszFileList = NULL;
    }

    ~RPFTOCDataset()
    {
        CSLDestroy( papszSubDatasets );
        CPLFree(pszProjection);
        CSLDestroy(papszFileList);
    }

    virtual char      **GetMetadata( const char * pszDomain = "" );

    virtual char      **GetFileList() { return CSLDuplicate(papszFileList); }

    void                AddSubDataset(const char* pszFilename, RPFTocEntry* tocEntry );

    void SetSize(int rasterXSize, int rasterYSize)
    {
        nRasterXSize = rasterXSize;
        nRasterYSize = rasterYSize;
    }

    virtual CPLErr GetGeoTransform( double * padfGeoTransform)
    {
        if (bGotGeoTransform)
        {
            memcpy(padfGeoTransform, adfGeoTransform, 6 * sizeof(double));
            return CE_None;
        }
        return CE_Failure;
    }
    
    virtual CPLErr SetGeoTransform( double * padfGeoTransform)
    {
        bGotGeoTransform = TRUE;
        memcpy(adfGeoTransform, padfGeoTransform, 6 * sizeof(double));
        return CE_None;
    }
    
    virtual CPLErr SetProjection( const char * projectionRef )
    {
        CPLFree(pszProjection);
        pszProjection = CPLStrdup(projectionRef);
        return CE_None;
    }
    
    virtual const char *GetProjectionRef(void)
    {
        return (pszProjection) ? pszProjection : "";
    }
    
    static int IsNITFFileTOC(NITFFile *psFile);
    static int IsNonNITFFileTOC(GDALOpenInfo * poOpenInfo, const char* pszFilename );
    static GDALDataset* OpenFileTOC(NITFFile *psFile,
                                    const char* pszFilename,
                                    const char* entryName,
                                    const char* openInformationName);
    
    static int Identify( GDALOpenInfo * poOpenInfo );
    static GDALDataset* Open( GDALOpenInfo * poOpenInfo );
};

/************************************************************************/
/* ==================================================================== */
/*                            RPFTOCSubDataset                          */
/* ==================================================================== */
/************************************************************************/

class RPFTOCSubDataset : public VRTDataset
{

  int          cachedTileBlockXOff;
  int          cachedTileBlockYOff;
  void*        cachedTileData;
  int          cachedTileDataSize;
  const char*  cachedTileFileName;
  char**       papszFileList;

  public:
    RPFTOCSubDataset(int nXSize, int nYSize) : VRTDataset(nXSize, nYSize)
    {
        /* Don't try to write a VRT file */
        SetWritable(FALSE);

        /* The driver is set to VRT in VRTDataset constructor. */
        /* We have to set it to the expected value ! */
        poDriver = (GDALDriver *) GDALGetDriverByName( "RPFTOC" );

        cachedTileBlockXOff = cachedTileBlockYOff = -1;
        cachedTileData = NULL;
        cachedTileDataSize = 0;
        cachedTileFileName = NULL;

        papszFileList = NULL;
    }
    
    ~RPFTOCSubDataset()
    {
        CSLDestroy(papszFileList);
        CPLFree(cachedTileData);
    }

    virtual char      **GetFileList() { return CSLDuplicate(papszFileList); }

    void* GetCachedTile(const char* tileFileName, int nBlockXOff, int nBlockYOff)
    {
        if (cachedTileFileName == tileFileName  &&
            cachedTileBlockXOff == nBlockXOff &&
            cachedTileBlockYOff == nBlockYOff)
        {
            return cachedTileData;
        }
        else
        {
            return NULL;
        }
    }

    void SetCachedTile(const char* tileFileName, int nBlockXOff, int nBlockYOff,
                       const void* pData, int dataSize)
    {
        if (dataSize > cachedTileDataSize)
        {
            cachedTileData = CPLRealloc(cachedTileData, dataSize);
            cachedTileDataSize = dataSize;
        }
        memcpy(cachedTileData, pData, dataSize);
        cachedTileFileName = tileFileName;
        cachedTileBlockXOff = nBlockXOff;
        cachedTileBlockYOff = nBlockYOff;
    }

    
    static GDALDataset* CreateDataSetFromTocEntry(const char* openInformationName,
                                                  const char* pszTOCFileName, int nEntry,
                                                  const RPFTocEntry* entry, int isRGBA,
                                                  char** papszMetadataRPFTOCFile);
};

/************************************************************************/
/* ==================================================================== */
/*                        RPFTOCProxyRasterDataSet                       */
/* ==================================================================== */
/************************************************************************/

class RPFTOCProxyRasterDataSet : public GDALProxyPoolDataset
{
    /* The following parameters are only for sanity checking */
    int checkDone;
    int checkOK;
    double nwLong, nwLat;
    GDALColorTable* colorTableRef;
    int bHasNoDataValue;
    double noDataValue;
    RPFTOCSubDataset* subdataset;

    public:
        RPFTOCProxyRasterDataSet(RPFTOCSubDataset* subdataset,
                                 const char* fileName,
                                 int nRasterXSize, int nRasterYSize,
                                 int nBlockXSize, int nBlockYSize,
                                 const char* projectionRef, double nwLong, double nwLat,
                                 int nBands);

        void SetNoDataValue(double noDataValue) {
            this->noDataValue = noDataValue;
            bHasNoDataValue = TRUE;
        }

        double GetNoDataValue(int* bHasNoDataValue)
        {
            if (bHasNoDataValue)
                *bHasNoDataValue = this->bHasNoDataValue;
            return noDataValue;
        }

        GDALDataset* RefUnderlyingDataset()
        {
            return GDALProxyPoolDataset::RefUnderlyingDataset();
        }

        void UnrefUnderlyingDataset(GDALDataset* poUnderlyingDataset)
        {
            GDALProxyPoolDataset::UnrefUnderlyingDataset(poUnderlyingDataset);
        }

        void SetReferenceColorTable(GDALColorTable* colorTableRef) { this->colorTableRef = colorTableRef;}

        const GDALColorTable* GetReferenceColorTable() { return colorTableRef; }

        int SanityCheckOK(GDALDataset* sourceDS);

        RPFTOCSubDataset* GetSubDataset() { return subdataset; }
};

/************************************************************************/
/* ==================================================================== */
/*                     RPFTOCProxyRasterBandRGBA                        */
/* ==================================================================== */
/************************************************************************/

class RPFTOCProxyRasterBandRGBA : public GDALPamRasterBand
{
    int initDone;
    unsigned char colorTable[256];
    int blockByteSize;

    private:
        void Expand(void* pImage, const void* srcImage);

    public:
        RPFTOCProxyRasterBandRGBA(GDALProxyPoolDataset* poDS, int nBand,
                                  int nBlockXSize, int nBlockYSize)
        {
            this->poDS = poDS;
            nRasterXSize = poDS->GetRasterXSize();
            nRasterYSize = poDS->GetRasterYSize();
            this->nBlockXSize = nBlockXSize;
            this->nBlockYSize = nBlockYSize;
            eDataType = GDT_Byte;
            this->nBand = nBand;
            blockByteSize = nBlockXSize * nBlockYSize;
            initDone = FALSE;
        }

        virtual GDALColorInterp GetColorInterpretation()
        {
            return (GDALColorInterp)(GCI_RedBand + nBand - 1);
        }

    protected:
        virtual CPLErr IReadBlock( int nBlockXOff, int nBlockYOff,
                                   void * pImage );
};

/************************************************************************/
/*                    Expand()                                          */
/************************************************************************/

/* Expand the  array or indexed colors to an array of their corresponding R,G,B or A component */
void  RPFTOCProxyRasterBandRGBA::Expand(void* pImage, const void* srcImage)
{
    int i;
    if ((blockByteSize & (~3)) != 0)
    {
        for(i=0;i<blockByteSize;i++)
        {
            ((unsigned char*)pImage)[i] = colorTable[((unsigned char*)srcImage)[i]];
        }
    }
    else
    {
        int nIter = blockByteSize/4;
        for(i=0;i<nIter;i++)
        {
            unsigned int four_pixels = ((unsigned int*)srcImage)[i];
            ((unsigned int*)pImage)[i] =
                    (colorTable[four_pixels >> 24] << 24) |
                    (colorTable[(four_pixels >> 16) & 0xFF] << 16) |
                    (colorTable[(four_pixels >> 8) & 0xFF] << 8) |
                    colorTable[four_pixels & 0xFF];
        }
    }

}

/************************************************************************/
/*                    IReadBlock()                                      */
/************************************************************************/

CPLErr RPFTOCProxyRasterBandRGBA::IReadBlock( int nBlockXOff, int nBlockYOff,
                                         void * pImage )
{
    CPLErr ret;
    RPFTOCProxyRasterDataSet* proxyDS = (RPFTOCProxyRasterDataSet*)poDS;
    GDALDataset* ds = proxyDS->RefUnderlyingDataset();
    if (ds)
    {
        if (proxyDS->SanityCheckOK(ds) == FALSE)
        {
            proxyDS->UnrefUnderlyingDataset(ds);
            return CE_Failure;
        }

        GDALRasterBand* srcBand = ds->GetRasterBand(1);
        if (initDone == FALSE)
        {
            GDALColorTable* srcColorTable = srcBand->GetColorTable();
            int i;
            int bHasNoDataValue;
            int noDataValue = (int)srcBand->GetNoDataValue(&bHasNoDataValue);
            int nEntries = srcColorTable->GetColorEntryCount();
            for(i=0;i<nEntries;i++)
            {
                const GDALColorEntry* entry = srcColorTable->GetColorEntry(i);
                if (nBand == 1)
                    colorTable[i] = (unsigned char)entry->c1;
                else if (nBand == 2)
                    colorTable[i] = (unsigned char)entry->c2;
                else if (nBand == 3)
                    colorTable[i] = (unsigned char)entry->c3;
                else
                {
                    colorTable[i] = (bHasNoDataValue && i == noDataValue) ? 0 : (unsigned char)entry->c4;
                }
            }
            if (bHasNoDataValue && nEntries == noDataValue)
                colorTable[nEntries] = 0;
            initDone = TRUE;
        }

        /* We use a 1-tile cache as the same source tile will be consecutively asked for */
        /* computing the R tile, the G tile, the B tile and the A tile */
        void* cachedImage =
                proxyDS->GetSubDataset()->GetCachedTile(GetDescription(), nBlockXOff, nBlockYOff);
        if (cachedImage == NULL)
        {
            CPLDebug("RPFTOC", "Read (%d, %d) of band %d, of file %s",
                     nBlockXOff, nBlockYOff, nBand, GetDescription());
            ret = srcBand->ReadBlock(nBlockXOff, nBlockYOff, pImage);
            if (ret == CE_None)
            {
                proxyDS->GetSubDataset()->SetCachedTile
                        (GetDescription(), nBlockXOff, nBlockYOff, pImage, blockByteSize);
                Expand(pImage, pImage);
            }

            /* -------------------------------------------------------------------- */
            /*      Forceably load the other bands associated with this scanline.   */
            /* -------------------------------------------------------------------- */
            if(nBand == 1 )
            {
                GDALRasterBlock *poBlock;

                poBlock = 
                    poDS->GetRasterBand(2)->GetLockedBlockRef(nBlockXOff,nBlockYOff);
                if (poBlock)
                    poBlock->DropLock();

                poBlock = 
                    poDS->GetRasterBand(3)->GetLockedBlockRef(nBlockXOff,nBlockYOff);
                if (poBlock)
                    poBlock->DropLock();

                poBlock = 
                    poDS->GetRasterBand(4)->GetLockedBlockRef(nBlockXOff,nBlockYOff);
                if (poBlock)
                    poBlock->DropLock();
            }
        }
        else
        {
            Expand(pImage, cachedImage);
            ret = CE_None;
        }

    }
    else
        ret = CE_Failure;

    proxyDS->UnrefUnderlyingDataset(ds);

    return ret;
}

/************************************************************************/
/* ==================================================================== */
/*                 RPFTOCProxyRasterBandPalette                         */
/* ==================================================================== */
/************************************************************************/

class RPFTOCProxyRasterBandPalette : public GDALPamRasterBand
{
    int initDone;
    int blockByteSize;
    int samePalette;
    unsigned char remapLUT[256];

    public:
        RPFTOCProxyRasterBandPalette(GDALProxyPoolDataset* poDS, int nBand,
                                     int nBlockXSize, int nBlockYSize)
        {
            this->poDS = poDS;
            nRasterXSize = poDS->GetRasterXSize();
            nRasterYSize = poDS->GetRasterYSize();
            this->nBlockXSize = nBlockXSize;
            this->nBlockYSize = nBlockYSize;
            eDataType = GDT_Byte;
            this->nBand = nBand;
            blockByteSize = nBlockXSize * nBlockYSize;
            initDone = FALSE;
        }

        virtual GDALColorInterp GetColorInterpretation()
        {
            return GCI_PaletteIndex;
        }

        virtual double GetNoDataValue(int* bHasNoDataValue)
        {
            return ((RPFTOCProxyRasterDataSet*)poDS)->GetNoDataValue(bHasNoDataValue);
        }

        virtual GDALColorTable *GetColorTable()
        {
            return (GDALColorTable *) ((RPFTOCProxyRasterDataSet*)poDS)->GetReferenceColorTable();
        }

    protected:
        virtual CPLErr IReadBlock( int nBlockXOff, int nBlockYOff,
                                   void * pImage );
};

/************************************************************************/
/*                    IReadBlock()                                      */
/************************************************************************/

CPLErr RPFTOCProxyRasterBandPalette::IReadBlock( int nBlockXOff, int nBlockYOff,
                                                 void * pImage )
{
    CPLErr ret;
    RPFTOCProxyRasterDataSet* proxyDS = (RPFTOCProxyRasterDataSet*)poDS;
    GDALDataset* ds = proxyDS->RefUnderlyingDataset();
    if (ds)
    {
        if (proxyDS->SanityCheckOK(ds) == FALSE)
        {
            proxyDS->UnrefUnderlyingDataset(ds);
            return CE_Failure;
        }

        GDALRasterBand* srcBand = ds->GetRasterBand(1);
        ret = srcBand->ReadBlock(nBlockXOff, nBlockYOff, pImage);
        
        if (initDone == FALSE)
        {
            int approximateMatching;
            if (srcBand->GetIndexColorTranslationTo(this, remapLUT, &approximateMatching ))
            {
                samePalette = FALSE;
                if (approximateMatching)
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "Palette for %s is different from reference palette. "
                              "Coudln't remap exactly all colors. Trying to find closest matches.\n", GetDescription());
                }
            }
            else
            {
                samePalette = TRUE;
            }
            initDone = TRUE;
        }


        if (samePalette == FALSE)
        {
            unsigned char* data = (unsigned char*)pImage;
            int i;
            for(i=0;i<blockByteSize;i++)
            {
                data[i] = remapLUT[data[i]];
            }
        }

    }
    else
        ret = CE_Failure;

    proxyDS->UnrefUnderlyingDataset(ds);

    return ret;
}

/************************************************************************/
/*                    RPFTOCProxyRasterDataSet()                         */
/************************************************************************/

RPFTOCProxyRasterDataSet::RPFTOCProxyRasterDataSet
        (RPFTOCSubDataset* subdataset,
         const char* fileName,
         int nRasterXSize, int nRasterYSize,
         int nBlockXSize, int nBlockYSize,
         const char* projectionRef, double nwLong, double nwLat,
         int nBands) :
            /* Mark as shared since the VRT will take several references if we are in RGBA mode (4 bands for this dataset) */
                GDALProxyPoolDataset(fileName, nRasterXSize, nRasterYSize, GA_ReadOnly, TRUE, projectionRef)
{
    int i;
    this->subdataset = subdataset;
    this->nwLong = nwLong;
    this->nwLat = nwLat;
    bHasNoDataValue = FALSE;
    noDataValue = 0;
    colorTableRef = NULL;

    checkDone = FALSE;
    checkOK = FALSE;
    if (nBands == 4)
    {
        for(i=0;i<4;i++)
        {
            SetBand(i + 1, new RPFTOCProxyRasterBandRGBA(this, i+1, nBlockXSize, nBlockYSize));
        }
    }
    else
        SetBand(1, new RPFTOCProxyRasterBandPalette(this, 1, nBlockXSize, nBlockYSize));
}

/************************************************************************/
/*                    SanityCheckOK()                                   */
/************************************************************************/

#define WARN_ON_FAIL(x) do { if (!(x)) { CPLError(CE_Warning, CPLE_AppDefined, "For %s, assert '" #x "' failed", GetDescription()); } } while(0)
#define ERROR_ON_FAIL(x) do { if (!(x)) { CPLError(CE_Warning, CPLE_AppDefined, "For %s, assert '" #x "' failed", GetDescription()); checkOK = FALSE; } } while(0)

int RPFTOCProxyRasterDataSet::SanityCheckOK(GDALDataset* sourceDS)
{
    int src_nBlockXSize, src_nBlockYSize;
    int nBlockXSize, nBlockYSize;
    double adfGeoTransform[6];
    if (checkDone)
        return checkOK;
    
    checkOK = TRUE;
    checkDone = TRUE;
    
    sourceDS->GetGeoTransform(adfGeoTransform);
    WARN_ON_FAIL(fabs(adfGeoTransform[GEOTRSFRM_TOPLEFT_X] - nwLong) < adfGeoTransform[1] );
    WARN_ON_FAIL(fabs(adfGeoTransform[GEOTRSFRM_TOPLEFT_Y] - nwLat) < fabs(adfGeoTransform[5]) );
    WARN_ON_FAIL(adfGeoTransform[GEOTRSFRM_ROTATION_PARAM1] == 0 &&
                  adfGeoTransform[GEOTRSFRM_ROTATION_PARAM2] == 0); /* No rotation */
    ERROR_ON_FAIL(sourceDS->GetRasterCount() == 1); /* Just 1 band */
    ERROR_ON_FAIL(sourceDS->GetRasterXSize() == nRasterXSize);
    ERROR_ON_FAIL(sourceDS->GetRasterYSize() == nRasterYSize);
    WARN_ON_FAIL(EQUAL(sourceDS->GetProjectionRef(), GetProjectionRef()));
    sourceDS->GetRasterBand(1)->GetBlockSize(&src_nBlockXSize, &src_nBlockYSize);
    GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
    ERROR_ON_FAIL(src_nBlockXSize == nBlockXSize);
    ERROR_ON_FAIL(src_nBlockYSize == nBlockYSize);
    WARN_ON_FAIL(sourceDS->GetRasterBand(1)->GetColorInterpretation() == GCI_PaletteIndex);
    WARN_ON_FAIL(sourceDS->GetRasterBand(1)->GetRasterDataType() == GDT_Byte);

    return checkOK;
}

/************************************************************************/
/*                           MakeTOCEntryName()                         */
/************************************************************************/

static const char* MakeTOCEntryName(RPFTocEntry* tocEntry )
{
    char* str;
    if (tocEntry->seriesAbbreviation)
        str = (char*)CPLSPrintf( "%s_%s_%s_%s_%d", tocEntry->type, tocEntry->seriesAbbreviation, tocEntry->scale, tocEntry->zone, tocEntry->boundaryId );
    else
        str = (char*)CPLSPrintf( "%s_%s_%s_%d", tocEntry->type, tocEntry->scale, tocEntry->zone, tocEntry->boundaryId );
    char* c = str;
    while(*c)
    {
        if (*c == ':' || *c == ' ')
            *c = '_';
        c++;
    }
    return str;
}

/************************************************************************/
/*                           AddSubDataset()                            */
/************************************************************************/

void RPFTOCDataset::AddSubDataset( const char* pszFilename,  RPFTocEntry* tocEntry )

{
    char	szName[80];
    int		nCount = CSLCount(papszSubDatasets ) / 2;

    sprintf( szName, "SUBDATASET_%d_NAME", nCount+1 );
    papszSubDatasets = 
        CSLSetNameValue( papszSubDatasets, szName, 
              CPLSPrintf( "NITF_TOC_ENTRY:%s:%s", MakeTOCEntryName(tocEntry), pszFilename ) );

    sprintf( szName, "SUBDATASET_%d_DESC", nCount+1 );
    if (tocEntry->seriesName && tocEntry->seriesAbbreviation)
        papszSubDatasets = 
        CSLSetNameValue( papszSubDatasets, szName,
               CPLSPrintf( "%s:%s:%s:%s:%s:%d", tocEntry->type, tocEntry->seriesAbbreviation, tocEntry->seriesName, tocEntry->scale, tocEntry->zone, tocEntry->boundaryId ));
    else
        papszSubDatasets = 
            CSLSetNameValue( papszSubDatasets, szName,
                CPLSPrintf( "%s:%s:%s:%d", tocEntry->type, tocEntry->scale, tocEntry->zone, tocEntry->boundaryId ));
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **RPFTOCDataset::GetMetadata( const char *pszDomain )

{
    if( pszDomain != NULL && EQUAL(pszDomain,"SUBDATASETS") )
        return papszSubDatasets;

    return GDALPamDataset::GetMetadata( pszDomain );
}

/************************************************************************/
/*                  NITFCreateVRTDataSetFromTocEntry()                  */
/************************************************************************/


#define ASSERT_CREATE_VRT(x) do { if (!(x)) { CPLError(CE_Failure, CPLE_AppDefined, "For %s, assert '" #x "' failed", entry->frameEntries[i].fullFilePath); if (poSrcDS) GDALClose(poSrcDS); CPLFree(projectionRef); return NULL;} } while(0)

/* Builds a RPFTOCSubDataset from the set of files of the toc entry */
GDALDataset* RPFTOCSubDataset::CreateDataSetFromTocEntry(const char* openInformationName,
                                                         const char* pszTOCFileName, int nEntry,
                                                         const RPFTocEntry* entry, int isRGBA,
                                                         char** papszMetadataRPFTOCFile)
{
    int i, j;
    GDALDriver *poDriver;
    RPFTOCSubDataset *poVirtualDS;
    int sizeX, sizeY;
    int nBlockXSize = 0, nBlockYSize = 0;
    double geoTransf[6];
    char* projectionRef = NULL;
    int N;
    int index = 0;

    poDriver = GetGDALDriverManager()->GetDriverByName("VRT");
    if( poDriver == NULL )
        return NULL;

    N = entry->nVertFrames * entry->nHorizFrames;

    /* This may not be reliable. See below */
    sizeX = (int)((entry->seLong - entry->nwLong) / (entry->nHorizFrames * entry->horizInterval) + 0.5);
    sizeY = (int)((entry->nwLat - entry->seLat) / (entry->nVertFrames * entry->vertInterval) + 0.5);

    for(i=0;i<N; i++)
    {
        if (!entry->frameEntries[i].fileExists)
            continue;

        if (index == 0)
        {
            int ds_sizeX, ds_sizeY;
            /* Open the first available file to get its geotransform, projection ref and block size */
            /* Do a few sanity checks too */
            /* Ideally we should make these sanity checks now on ALL files, but it would be too slow */
            /* for large datasets. So these sanity checks will be done at the time we really need */
            /* to access the file (see SanityCheckOK metho) */
            GDALDataset *poSrcDS = (GDALDataset *) GDALOpenShared( entry->frameEntries[i].fullFilePath, GA_ReadOnly );
            ASSERT_CREATE_VRT(poSrcDS);
            poSrcDS->GetGeoTransform(geoTransf);
            projectionRef = CPLStrdup(poSrcDS->GetProjectionRef());
            ASSERT_CREATE_VRT(geoTransf[GEOTRSFRM_ROTATION_PARAM1] == 0 &&
                              geoTransf[GEOTRSFRM_ROTATION_PARAM2] == 0); /* No rotation */
            ASSERT_CREATE_VRT(poSrcDS->GetRasterCount() == 1); /* Just 1 band */
            
            /* Tolerance of 1%... This is necessary for CADRG_L22/RPF/A.TOC for example */
            ASSERT_CREATE_VRT((entry->horizInterval - geoTransf[GEOTRSFRM_WE_RES]) /
                                entry->horizInterval < 0.01); /* X interval same as in TOC */
            ASSERT_CREATE_VRT((entry->vertInterval - (-geoTransf[GEOTRSFRM_NS_RES])) /
                                entry->horizInterval < 0.01); /* Y interval same as in TOC */

            ds_sizeX = poSrcDS->GetRasterXSize();
            ds_sizeY = poSrcDS->GetRasterYSize();
            /* In the case the east longitude is 180, there's a great chance that it is in fact */
            /* truncated in the A.TOC. Thus, the only reliable way to find out the tile width, is to */
            /* read it from the tile dataset itself... */
            /* This is the case for the GNCJNCN dataset that has world coverage */
            if (entry->seLong == 180.00)
                sizeX = ds_sizeX;
            else
                ASSERT_CREATE_VRT(sizeX == ds_sizeX);
            ASSERT_CREATE_VRT(sizeY == ds_sizeY);
            poSrcDS->GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
            ASSERT_CREATE_VRT(poSrcDS->GetRasterBand(1)->GetColorInterpretation() == GCI_PaletteIndex);
            ASSERT_CREATE_VRT(poSrcDS->GetRasterBand(1)->GetRasterDataType() == GDT_Byte);
            GDALClose(poSrcDS);
        }

        index++;
    }

    if (index == 0)
        return NULL;

    /* ------------------------------------ */
    /* Create the VRT with the overall size */
    /* ------------------------------------ */
    poVirtualDS = new RPFTOCSubDataset( sizeX * entry->nHorizFrames,
                                        sizeY * entry->nVertFrames);

    if (papszMetadataRPFTOCFile)
        poVirtualDS->SetMetadata(papszMetadataRPFTOCFile);

    poVirtualDS->SetProjection(projectionRef);

    geoTransf[GEOTRSFRM_TOPLEFT_X] = entry->nwLong;
    geoTransf[GEOTRSFRM_TOPLEFT_Y] = entry->nwLat;
    poVirtualDS->SetGeoTransform(geoTransf);
    
    int nBands;
    
    /* In most cases, all the files inside a TOC entry share the same */
    /* palette and we could use it for the VRT. */
    /* In other cases like for CADRG801_France_250K (TOC entry CADRG_250K_2_2), */
    /* the file for Corsica and the file for Sardegna do not share the same palette */
    /* however they contain the same RGB triplets and are just ordered differently */
    /* So we can use the same palette */
    /* In the unlikely event where palettes would be incompatible, we can use the RGBA */
    /* option through the config option RPFTOC_FORCE_RGBA */
    if (isRGBA == FALSE)
    {
        poVirtualDS->AddBand(GDT_Byte, NULL);
        GDALRasterBand *poBand = poVirtualDS->GetRasterBand( 1 );
        poBand->SetColorInterpretation(GCI_PaletteIndex);
        nBands = 1;
        
        for(i=0;i<N; i++)
        {
            if (!entry->frameEntries[i].fileExists)
                continue;
            
            int bAllBlack = TRUE;
            GDALDataset *poSrcDS = (GDALDataset *) GDALOpenShared( entry->frameEntries[i].fullFilePath, GA_ReadOnly );
            if( poSrcDS != NULL )
            {
                if( poSrcDS->GetRasterCount() == 1 )
                {
                    int bHasNoDataValue;
                    double noDataValue = poSrcDS->GetRasterBand(1)->GetNoDataValue(&bHasNoDataValue);
                    if (bHasNoDataValue)
                        poBand->SetNoDataValue(noDataValue);

                    /* Avoid setting a color table that is all black (which might be */
                    /* the case of the edge tiles of a RPF subdataset) */
                    GDALColorTable* poCT = poSrcDS->GetRasterBand(1)->GetColorTable();
                    if( poCT != NULL )
                    {
                        for(int iC = 0; iC < poCT->GetColorEntryCount(); iC++)
                        {
                            if( bHasNoDataValue && iC == (int)noDataValue )
                                continue;

                            const GDALColorEntry* entry = poCT->GetColorEntry(i);
                            if( entry->c1 != 0 || entry->c2 != 0 || entry->c3 != 0)
                            {
                                bAllBlack = FALSE;
                                break;
                            }
                        }

                        /* Assign it temporarily, in the hope of a better match */
                        /* afterwards */
                        poBand->SetColorTable(poCT);
                        if( bAllBlack )
                        {
                            CPLDebug("RPFTOC",
                                     "Skipping %s. Its palette is all black.",
                                     poSrcDS->GetDescription());
                        }
                    }
                }
                GDALClose(poSrcDS);
            }
            if( !bAllBlack )
                break;
        }
    }
    else
    {
        for (i=0;i<4;i++)
        {
            poVirtualDS->AddBand(GDT_Byte, NULL);
            GDALRasterBand *poBand = poVirtualDS->GetRasterBand( i + 1 );
            poBand->SetColorInterpretation((GDALColorInterp)(GCI_RedBand+i));
        }
        nBands = 4;
    }

    CPLFree(projectionRef);
    projectionRef = NULL;

    /* -------------------------------------------------------------------- */
    /*      Check for overviews.                                            */
    /* -------------------------------------------------------------------- */

    poVirtualDS->oOvManager.Initialize( poVirtualDS,
                                        CPLString().Printf("%s.%d", pszTOCFileName, nEntry + 1));

    poVirtualDS->SetDescription(pszTOCFileName);
    poVirtualDS->papszFileList = poVirtualDS->GDALDataset::GetFileList();
    poVirtualDS->SetDescription(openInformationName);

    int iFile = 0;
    for(i=0;i<N; i++)
    {
        if (! entry->frameEntries[i].fileExists)
            continue;

        poVirtualDS->SetMetadataItem(CPLSPrintf("FILENAME_%d", iFile), entry->frameEntries[i].fullFilePath);
        poVirtualDS->papszFileList = CSLAddString(poVirtualDS->papszFileList, entry->frameEntries[i].fullFilePath);
        iFile++;

        /* We create proxy datasets and raster bands */
        /* Using real datasets and raster bands is possible in theory */
        /* However for large datasets, a TOC entry can include several hundreds of files */
        /* and we finally reach the limit of maximum file descriptors open at the same time ! */
        /* So the idea is to warp the datasets into a proxy and open the underlying dataset only when it is */
        /* needed (IRasterIO operation). To improve a bit efficiency, we have a cache of opened */
        /* underlying datasets */
        RPFTOCProxyRasterDataSet* ds = new RPFTOCProxyRasterDataSet(
                (RPFTOCSubDataset*)poVirtualDS,
                entry->frameEntries[i].fullFilePath,
                sizeX, sizeY,
                nBlockXSize, nBlockYSize,
                poVirtualDS->GetProjectionRef(),
                entry->nwLong + entry->frameEntries[i].frameCol * entry->horizInterval * sizeX, 
                entry->nwLat - entry->frameEntries[i].frameRow * entry->vertInterval * sizeY,
                nBands);
        if (nBands == 1)
        {
            GDALRasterBand *poBand = poVirtualDS->GetRasterBand( 1 );
            ds->SetReferenceColorTable(poBand->GetColorTable());
            int bHasNoDataValue;
            double noDataValue = poBand->GetNoDataValue(&bHasNoDataValue);
            if (bHasNoDataValue)
                ds->SetNoDataValue(noDataValue);
        }

        for(j=0;j<nBands;j++)
        {
            VRTSourcedRasterBand *poBand = (VRTSourcedRasterBand*)poVirtualDS->GetRasterBand( j + 1 );
            /* Place the raster band at the right position in the VRT */
            poBand->AddSimpleSource(ds->GetRasterBand(j + 1),
                                    0, 0, sizeX, sizeY,
                                    entry->frameEntries[i].frameCol * sizeX, 
                                    entry->frameEntries[i].frameRow * sizeY,
                                    sizeX, sizeY);
        }

        /* The RPFTOCProxyRasterDataSet will be destroyed when its last raster band will be */
        /* destroyed */
        ds->Dereference();
    }

    poVirtualDS->SetMetadataItem("NITF_SCALE", entry->scale);
    poVirtualDS->SetMetadataItem("NITF_SERIES_ABBREVIATION",
                        (entry->seriesAbbreviation) ? entry->seriesAbbreviation : "Unknown");
    poVirtualDS->SetMetadataItem("NITF_SERIES_NAME",
                        (entry->seriesName) ? entry->seriesName : "Unknown");

    return poVirtualDS;
}

/************************************************************************/
/*                          IsNonNITFFileTOC()                          */
/************************************************************************/

/* Check whether the file is a TOC file without NITF header */
int RPFTOCDataset::IsNonNITFFileTOC(GDALOpenInfo * poOpenInfo, const char* pszFilename )
{
    const char pattern[] = { 0, 0, '0', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 'A', '.', 'T', 'O', 'C' };
    if (poOpenInfo)
    {
        if (poOpenInfo->nHeaderBytes < 48 )
            return FALSE;
        return memcmp(pattern, poOpenInfo->pabyHeader, 15) == 0;
    }
    else
    {
        char buffer[48];
        VSILFILE* fp = NULL;
        fp = VSIFOpenL( pszFilename, "rb" );
        if( fp == NULL )
        {
            return FALSE;
        }

        int ret = (VSIFReadL(buffer, 1, 48, fp) == 48) &&
                   memcmp(pattern, buffer, 15) == 0;
        VSIFCloseL(fp);
        return ret;
    }
}

/************************************************************************/
/*                             IsNITFFileTOC()                          */
/************************************************************************/

/* Check whether this NITF file is a TOC file */
int RPFTOCDataset::IsNITFFileTOC(NITFFile *psFile)
{
    const char* fileTitle = CSLFetchNameValue(psFile->papszMetadata, "NITF_FTITLE");
    while(fileTitle && *fileTitle)
    {
        if (EQUAL(fileTitle, "A.TOC"))
        {
            return TRUE;
        }
        fileTitle++;
    }
    return FALSE;
}

/************************************************************************/
/*                                OpenFileTOC()                         */
/************************************************************************/

/* Create a dataset from a TOC file */
/* If psFile == NULL, the TOC file has no NITF header */
/* If entryName != NULL, the dataset will be made just of the entry of the TOC file */
GDALDataset* RPFTOCDataset::OpenFileTOC(NITFFile *psFile,
                                        const char* pszFilename,
                                        const char* entryName,
                                        const char* openInformationName)
{
    char buffer[48];
    VSILFILE* fp = NULL;
    if (psFile == NULL)
    {
        fp = VSIFOpenL( pszFilename, "rb" );

        if( fp == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                    "Failed to open file %s.", 
                    pszFilename );
            return NULL;
        }
        VSIFReadL(buffer, 1, 48, fp);
    }
    int isRGBA = CSLTestBoolean(CPLGetConfigOption("RPFTOC_FORCE_RGBA", "NO"));
    RPFToc* toc = (psFile) ? RPFTOCRead( pszFilename, psFile ) :
                              RPFTOCReadFromBuffer( pszFilename, fp, buffer);
    if (fp) VSIFCloseL(fp);
    fp = NULL;

    if (entryName != NULL)
    {
        if (toc)
        {
            int i;
            for(i=0;i<toc->nEntries;i++)
            {
                if (EQUAL(entryName, MakeTOCEntryName(&toc->entries[i])))
                {
                    GDALDataset* ds = RPFTOCSubDataset::CreateDataSetFromTocEntry(openInformationName, pszFilename, i,
                                                                                  &toc->entries[i], isRGBA,
                                                                                  (psFile) ? psFile->papszMetadata : NULL);

                    RPFTOCFree(toc);
                    return ds;
                }
            }
            CPLError( CE_Failure, CPLE_AppDefined, 
                        "The entry %s does not exist in file %s.", entryName, pszFilename );
        }
        RPFTOCFree(toc);
        return NULL;
    }

    if (toc)
    {
        RPFTOCDataset* ds = new RPFTOCDataset();
        if (psFile)
            ds->SetMetadata( psFile->papszMetadata );

        int i;
        int ok = FALSE;
        char* projectionRef = NULL;
        double nwLong = 0, nwLat = 0, seLong = 0, seLat = 0;
        double adfGeoTransform[6];

        ds->papszFileList = CSLAddString(ds->papszFileList, pszFilename);

        for(i=0;i<toc->nEntries;i++)
        {
            if (!toc->entries[i].isOverviewOrLegend)
            {
                GDALDataset* tmpDS = RPFTOCSubDataset::CreateDataSetFromTocEntry(openInformationName, pszFilename, i,
                                                                                 &toc->entries[i], isRGBA, NULL);
                if (tmpDS)
                {
                    char** papszSubDatasetFileList = tmpDS->GetFileList();
                    /* Yes, begin at 1, since the first is the a.toc */
                    ds->papszFileList = CSLInsertStrings(ds->papszFileList, -1, papszSubDatasetFileList + 1);
                    CSLDestroy(papszSubDatasetFileList);

                    tmpDS->GetGeoTransform(adfGeoTransform);
                    if (projectionRef == NULL)
                    {
                        ok = TRUE;
                        projectionRef = CPLStrdup(tmpDS->GetProjectionRef());
                        nwLong = adfGeoTransform[GEOTRSFRM_TOPLEFT_X];
                        nwLat = adfGeoTransform[GEOTRSFRM_TOPLEFT_Y];
                        seLong = nwLong + adfGeoTransform[GEOTRSFRM_WE_RES] * tmpDS->GetRasterXSize();
                        seLat = nwLat + adfGeoTransform[GEOTRSFRM_NS_RES] * tmpDS->GetRasterYSize();
                    }
                    else if (ok)
                    {
                        double _nwLong = adfGeoTransform[GEOTRSFRM_TOPLEFT_X];
                        double _nwLat = adfGeoTransform[GEOTRSFRM_TOPLEFT_Y];
                        double _seLong = _nwLong + adfGeoTransform[GEOTRSFRM_WE_RES] * tmpDS->GetRasterXSize();
                        double _seLat = _nwLat + adfGeoTransform[GEOTRSFRM_NS_RES] * tmpDS->GetRasterYSize();
                        if (! EQUAL(projectionRef, tmpDS->GetProjectionRef()) )
                            ok = FALSE;
                        if (_nwLong < nwLong)
                            nwLong = _nwLong;
                        if (_nwLat > nwLat)
                            nwLat = _nwLat;
                        if (_seLong > seLong)
                            seLong = _seLong;
                        if (_seLat < seLat)
                            seLat = _seLat;
                    }
                    delete tmpDS;
                    ds->AddSubDataset(pszFilename, &toc->entries[i]);
                }
            }
        }
        if (ok)
        {
            adfGeoTransform[GEOTRSFRM_TOPLEFT_X] = nwLong;
            adfGeoTransform[GEOTRSFRM_TOPLEFT_Y] = nwLat;
            ds->SetSize((int)(0.5 + (seLong - nwLong) / adfGeoTransform[GEOTRSFRM_WE_RES]), 
                        (int)(0.5 + (seLat - nwLat) / adfGeoTransform[GEOTRSFRM_NS_RES]));
            ds->SetGeoTransform(adfGeoTransform);
            ds->SetProjection(projectionRef);
        }
        CPLFree(projectionRef);
        RPFTOCFree(toc);

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
        ds->SetDescription( pszFilename );
        ds->TryLoadXML();

        return ds;
    }
    else
    {
        return NULL;
    }
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int RPFTOCDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    const char *pszFilename = poOpenInfo->pszFilename;

/* -------------------------------------------------------------------- */
/*      Is this a sub-dataset selector? If so, it is obviously RPFTOC.        */
/* -------------------------------------------------------------------- */

    if( EQUALN(pszFilename, "NITF_TOC_ENTRY:",strlen("NITF_TOC_ENTRY:")))
        return TRUE;

/* -------------------------------------------------------------------- */
/*	First we check to see if the file has the expected header	*/
/*	bytes.								*/    
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 48 )
        return FALSE;
    
    if ( IsNonNITFFileTOC( poOpenInfo, pszFilename) )
        return TRUE;
        
    if( !EQUALN((char *) poOpenInfo->pabyHeader,"NITF",4) 
        && !EQUALN((char *) poOpenInfo->pabyHeader,"NSIF",4)
        && !EQUALN((char *) poOpenInfo->pabyHeader,"NITF",4) )
        return FALSE;
    
    int i;
    /* If it's a NITF A.TOC file, it must contain A.TOC in it's header */
    for(i=0;i<(int)poOpenInfo->nHeaderBytes-(int)strlen("A.TOC");i++)
    {
        if (EQUALN((const char*)poOpenInfo->pabyHeader + i, "A.TOC", strlen("A.TOC")))
            return TRUE;
    }

    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *RPFTOCDataset::Open( GDALOpenInfo * poOpenInfo )

{
    const char *pszFilename = poOpenInfo->pszFilename;
    char* entryName = NULL;

    if( !Identify( poOpenInfo ) )
        return NULL;

    if( EQUALN(pszFilename, "NITF_TOC_ENTRY:",strlen("NITF_TOC_ENTRY:")))
    {
        pszFilename += strlen("NITF_TOC_ENTRY:");
        entryName = CPLStrdup(pszFilename);
        char* c = entryName;
        while( *c != '\0' && *c != ':' )
            c++;
        if( *c != ':' )
        {
            CPLFree(entryName);
            return NULL;
        }
        *c = 0;

        while( *pszFilename != '\0' && *pszFilename != ':' )
            pszFilename++;
        pszFilename++;
    }
    
    if (IsNonNITFFileTOC((entryName != NULL) ? NULL : poOpenInfo, pszFilename))
    {
        GDALDataset* poDS = OpenFileTOC(NULL, pszFilename, entryName, poOpenInfo->pszFilename);
        
        CPLFree(entryName);

        if (poDS && poOpenInfo->eAccess == GA_Update)
        {
            CPLError(CE_Failure, CPLE_NotSupported, "RPFTOC driver does not support update mode");
            delete poDS;
            return NULL;
        }
        
        return poDS;
    }

/* -------------------------------------------------------------------- */
/*      Open the file with library.                                     */
/* -------------------------------------------------------------------- */
    NITFFile *psFile;

    psFile = NITFOpen( pszFilename, FALSE );
    if( psFile == NULL )
    {
        CPLFree(entryName);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Check if it is a TOC file .                                     */
/* -------------------------------------------------------------------- */
    if (IsNITFFileTOC(psFile))
    {
        GDALDataset* poDS = OpenFileTOC(psFile, pszFilename, entryName, poOpenInfo->pszFilename);
        NITFClose( psFile );
        CPLFree(entryName);

        if (poDS && poOpenInfo->eAccess == GA_Update)
        {
            CPLError(CE_Failure, CPLE_NotSupported, "RPFTOC driver does not support update mode");
            delete poDS;
            return NULL;
        }
        
        return poDS;
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                          "File %s is not a TOC file.", pszFilename );
        NITFClose( psFile );
        CPLFree(entryName);
        return NULL;
    }

}

/************************************************************************/
/*                          GDALRegister_RPFTOC()                         */
/************************************************************************/

void GDALRegister_RPFTOC()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "RPFTOC" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "RPFTOC" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Raster Product Format TOC format" );
        
        poDriver->pfnIdentify = RPFTOCDataset::Identify;
        poDriver->pfnOpen = RPFTOCDataset::Open;

        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#RPFTOC" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "toc" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
