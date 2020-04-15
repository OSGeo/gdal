/******************************************************************************
 *
 * Project:  DRDC Configurable Airborne SAR Processor (COASP) data reader
 * Purpose:  Support in GDAL for the DRDC COASP format data, both Metadata
 *           and complex imagery.
 * Author:   Philippe Vachon <philippe@cowpig.ca>
 * Notes:    I have seen a grand total of 2 COASP scenes (3 sets of headers).
 *           This is based on my best observations, some educated guesses and
 *           such. So if you have a scene that doesn't work, send it to me
 *           please and I will make it work... with violence.
 *
 ******************************************************************************
 * Copyright (c) 2007, Philippe Vachon
 * Copyright (c) 2009-2012, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_conv.h"
#include "cpl_port.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal_frmts.h"
#include "gdal_priv.h"

CPL_CVSID("$Id$")

constexpr int TYPE_GENERIC = 0;
constexpr int TYPE_GEOREF = 1;

enum ePolarization {
    hh = 0,
    hv,
    vh,
    vv
};

/*******************************************************************
 * Declaration of the COASPMetadata classes                        *
 *******************************************************************/

class COASPMetadataItem;

class COASPMetadataReader
{
        char **papszMetadata;
        int nMetadataCount;
        int nCurrentItem;
public:
        explicit COASPMetadataReader(char *pszFname);
        ~COASPMetadataReader();
        COASPMetadataItem *GetNextItem();
        COASPMetadataItem *GetItem(int nItem);
        int GotoMetadataItem(const char *pszName);
        int GetCurrentItem() const { return nCurrentItem; }
};

/* Your average metadata item */
class COASPMetadataItem
{
protected:
    char *pszItemName;
    char *pszItemValue;

public:
    COASPMetadataItem() : pszItemName(nullptr), pszItemValue(nullptr) { }
    COASPMetadataItem(char *pszItemName, char *pszItemValue);
    ~COASPMetadataItem();

    char *GetItemName();
    char *GetItemValue();
    static int GetType() { return TYPE_GENERIC; }
};

/* Same as MetadataItem class except parses GCP properly and returns
 * a GDAL_GCP struct
 */
class COASPMetadataGeorefGridItem : public COASPMetadataItem
{
#ifdef unused
        int nId;
        int nPixels;
        int nLines;
        double ndLat;
        double ndLong;
#endif

public:
        COASPMetadataGeorefGridItem( int nId, int nPixels, int nLines,
                                     double ndLat, double ndLong );
        static const char *GetItemName() { return "georef_grid"; }
        GDAL_GCP *GetItemValue();
        static int GetType() { return TYPE_GEOREF; }
};

/********************************************************************
 * ================================================================ *
 * Implementation of the COASPMetadataItem Classes                  *
 * ================================================================ *
 ********************************************************************/

COASPMetadataItem::COASPMetadataItem(char *pszItemName_, char *pszItemValue_) :
    pszItemName(VSIStrdup(pszItemName_)),
    pszItemValue(VSIStrdup(pszItemValue_))
{}

COASPMetadataItem::~COASPMetadataItem()
{
    CPLFree(pszItemName);
    CPLFree(pszItemValue);
}

char *COASPMetadataItem::GetItemName()
{
        return VSIStrdup(pszItemName);
}

char *COASPMetadataItem::GetItemValue()
{
        return VSIStrdup(pszItemValue);
}

COASPMetadataGeorefGridItem::COASPMetadataGeorefGridItem(
    int /*nIdIn*/, int /*nPixelsIn*/,
    int /*nLinesIn*/, double /*ndLatIn*/, double /*ndLongIn*/ )
#ifdef unused
:
    nId(nIdIn),
    nPixels(nPixelsIn),
    nLines(nLinesIn),
    ndLat(ndLatIn),
    ndLong(ndLongIn)
#endif
{
    pszItemName = VSIStrdup("georef_grid");
}

GDAL_GCP *COASPMetadataGeorefGridItem::GetItemValue()
{
    return nullptr;
}

/********************************************************************
 * ================================================================ *
 * Implementation of the COASPMetadataReader Class                  *
 * ================================================================ *
 ********************************************************************/

COASPMetadataReader::COASPMetadataReader(char *pszFname) :
    papszMetadata(CSLLoad(pszFname)),
    nMetadataCount(0),
    nCurrentItem(0)
{
    nMetadataCount = CSLCount(papszMetadata);
}

COASPMetadataReader::~COASPMetadataReader()
{
    CSLDestroy(papszMetadata);
}

COASPMetadataItem *COASPMetadataReader::GetNextItem()
{
        if (nCurrentItem < 0 || nCurrentItem >= nMetadataCount)
            return nullptr;

        COASPMetadataItem *poMetadata = nullptr;

        char **papszMDTokens
            = CSLTokenizeString2(papszMetadata[nCurrentItem], " ",
                                 CSLT_HONOURSTRINGS );
        char *pszItemName = papszMDTokens[0];
        if (STARTS_WITH_CI(pszItemName, "georef_grid") &&
            CSLCount(papszMDTokens) >= 8 )
        {
            // georef_grid ( pixels lines ) ( lat long )
            // 0           1 2      3     4 5 6   7    8
            int nPixels = atoi(papszMDTokens[2]);
            int nLines = atoi(papszMDTokens[3]);
            double dfLat = CPLAtof(papszMDTokens[6]);
            double dfLong = CPLAtof(papszMDTokens[7]);
            poMetadata = new COASPMetadataGeorefGridItem(
                nCurrentItem, nPixels, nLines, dfLat, dfLong);
        }
        else
        {
            int nCount = CSLCount(papszMDTokens);
            if( nCount >= 2 )
            {
                char *pszItemValue = CPLStrdup(papszMDTokens[1]);
                for (int i = 2; i < nCount; i++)
                {
                    const size_t nSize = strlen(pszItemValue) + 1 + strlen(papszMDTokens[i]);
                    pszItemValue = (char *)CPLRealloc(pszItemValue, nSize);
                    snprintf(pszItemValue + strlen(pszItemValue),
                            nSize - strlen(pszItemValue), " %s",
                            papszMDTokens[i]);
                }

                poMetadata = new COASPMetadataItem(pszItemName, pszItemValue);

                CPLFree(pszItemValue);
            }
        }
        CSLDestroy(papszMDTokens);
        nCurrentItem++;
        return poMetadata;
}

/* Goto the first metadata item with a particular name */
int COASPMetadataReader::GotoMetadataItem(const char *pszName)
{
        nCurrentItem = CSLPartialFindString(papszMetadata, pszName);
        return nCurrentItem;
}

/*******************************************************************
 * Declaration of the COASPDataset class                           *
 *******************************************************************/

class COASPRasterBand;

/* A couple of observations based on the data I have available to me:
 * a) the headers don't really change, beyond indicating data sources
 *    and such. As such, I only read the first header specified by the
 *    user. Note that this is agnostic: you can specify hh, vv, vh, hv and
 *    all the data needed will be immediately available.
 * b) Lots of GCPs are present in the headers. This is most excellent.
 * c) There is no documentation on this format. All the knowledge contained
 *    herein is from harassing various Defence Scientists at DRDC Ottawa.
 */

class COASPDataset final: public GDALDataset
{
        friend class COASPRasterBand;
        VSILFILE *fpHdr; /* File pointer for the header file */
        VSILFILE *fpBinHH; /* File pointer for the binary matrix */
        VSILFILE *fpBinHV;
        VSILFILE *fpBinVH;
        VSILFILE *fpBinVV;

        char *pszFileName; /* line and mission ID, mostly, i.e. l27p7 */

public:
        COASPDataset():
            fpHdr(nullptr),
            fpBinHH(nullptr),
            fpBinHV(nullptr),
            fpBinVH(nullptr),
            fpBinVV(nullptr),
            pszFileName(nullptr) {}
        ~COASPDataset();

        static GDALDataset *Open( GDALOpenInfo * );
        static int Identify( GDALOpenInfo * poOpenInfo );
};


/********************************************************************
 * ================================================================ *
 * Declaration and implementation of the COASPRasterBand Class      *
 * ================================================================ *
 ********************************************************************/

class COASPRasterBand final: public GDALRasterBand {
    VSILFILE *fp;
    // int ePol;
  public:
    COASPRasterBand( COASPDataset *poDS, GDALDataType eDataType,
                     int ePol, VSILFILE *fp );
    CPLErr IReadBlock( int nBlockXOff, int nBlockYOff, void *pImage) override;
};

COASPRasterBand::COASPRasterBand( COASPDataset *poDSIn,
                                  GDALDataType eDataTypeIn,
                                  int /*ePolIn*/, VSILFILE *fpIn ) :
        fp(fpIn)/*,
        ePol(ePolIn)*/
{
        poDS = poDSIn;
        eDataType = eDataTypeIn;
        nBlockXSize = poDS->GetRasterXSize();
        nBlockYSize = 1;
}

CPLErr COASPRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff,
                                    int nBlockYOff,
                                    void *pImage )
{
        if (this->fp == nullptr)
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "File pointer freed unexpectedly");
            return CE_Fatal;
        }

        /* 8 bytes per pixel: 4 bytes I, 4 bytes Q */
        unsigned long nByteNum = poDS->GetRasterXSize() * 8 * nBlockYOff;

        VSIFSeekL(this->fp, nByteNum, SEEK_SET);
        int nReadSize = (GDALGetDataTypeSize(eDataType)/8) * poDS->GetRasterXSize();
        VSIFReadL((char *)pImage, 1, nReadSize, this->fp);

#ifdef CPL_LSB
        GDALSwapWords( pImage, 4, nBlockXSize * 2, 4 );
#endif
        return CE_None;
}

/********************************************************************
 * ================================================================ *
 * Implementation of the COASPDataset Class                         *
 * ================================================================ *
 ********************************************************************/

/************************************************************************/
/*                          ~COASPDataset()                             */
/************************************************************************/

COASPDataset::~COASPDataset()
{
    CPLFree(pszFileName);
    if( fpHdr )
        VSIFCloseL(fpHdr);
    if( fpBinHH )
        VSIFCloseL(fpBinHH);
    if( fpBinHV )
        VSIFCloseL(fpBinHV);
    if( fpBinVH )
        VSIFCloseL(fpBinVH);
    if( fpBinVV )
        VSIFCloseL(fpBinVV);
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int COASPDataset::Identify( GDALOpenInfo *poOpenInfo )
{
    if(poOpenInfo->fpL == nullptr || poOpenInfo->nHeaderBytes < 256)
        return 0;

    // With a COASP .hdr file, the first line or so is: time_first_datarec

    if(STARTS_WITH_CI((char *)poOpenInfo->pabyHeader, "time_first_datarec"))
        return 1;

    return 0;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *COASPDataset::Open( GDALOpenInfo *poOpenInfo )
{
    if (!COASPDataset::Identify(poOpenInfo))
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The COASP driver does not support update access to existing"
                  " datasets.\n" );
        return nullptr;
    }

    /* Create a fresh dataset for us to work with */
    COASPDataset *poDS = new COASPDataset();

    /* Steal the file pointer for the header */
    poDS->fpHdr = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

    poDS->pszFileName = VSIStrdup(poOpenInfo->pszFilename);

    /* determine the file name prefix */
    char *pszBaseName = VSIStrdup(CPLGetBasename(poDS->pszFileName));
    char *pszDir = VSIStrdup(CPLGetPath(poDS->pszFileName));
    const char *pszExt = "rc";
    int nNull = static_cast<int>(strlen(pszBaseName)) - 1;
    if( nNull <= 0 )
    {
        VSIFree(pszDir);
        VSIFree(pszBaseName);
        delete poDS;
        return nullptr;
    }
    char *pszBase = (char *)CPLMalloc(nNull);
    strncpy(pszBase, pszBaseName, nNull);
    pszBase[nNull - 1] = '\0';
    VSIFree(pszBaseName);

    char *psChan = strstr(pszBase,"hh");
    if( psChan == nullptr )
    {
        psChan = strstr(pszBase, "hv");
    }
    if (psChan == nullptr)
    {
        psChan = strstr(pszBase, "vh");
    }
    if (psChan == nullptr)
    {
        psChan = strstr(pszBase, "vv");
    }

    if (psChan == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to recognize file as COASP.");
        VSIFree(pszBase);
        VSIFree(pszDir);
        delete poDS;
        return nullptr;
    }

    /* Read Metadata, set GCPs as is appropriate */
    COASPMetadataReader oReader(poDS->pszFileName);

    /* Get Image X and Y widths */
    oReader.GotoMetadataItem("number_lines");
    COASPMetadataItem *poItem = oReader.GetNextItem();
    if( poItem == nullptr )
    {
        VSIFree(pszBase);
        VSIFree(pszDir);
        delete poDS;
        return nullptr;
    }
    char *nValue = poItem->GetItemValue();
    poDS->nRasterYSize = atoi(nValue);
    delete poItem;
    VSIFree(nValue);

    oReader.GotoMetadataItem("number_samples");
    poItem = oReader.GetNextItem();
    if( poItem == nullptr )
    {
        VSIFree(pszBase);
        VSIFree(pszDir);
        delete poDS;
        return nullptr;
    }
    nValue = poItem->GetItemValue();
    poDS->nRasterXSize = atoi(nValue);
    delete poItem;
    VSIFree(nValue);

    if( !GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize) )
    {
        VSIFree(pszBase);
        VSIFree(pszDir);
        delete poDS;
        return nullptr;
    }

    /* Horizontal transmit, horizontal receive */
    psChan[0] = 'h';
    psChan[1] = 'h';
    const char *pszFilename = CPLFormFilename(pszDir, pszBase, pszExt);

    poDS->fpBinHH = VSIFOpenL(pszFilename, "r");

    if (poDS->fpBinHH != nullptr)
    {
        /* Set raster band */
        poDS->SetBand(1, new COASPRasterBand(poDS, GDT_CFloat32,
                                             hh , poDS->fpBinHH));
    }

    /* Horizontal transmit, vertical receive */
    psChan[0] = 'h';
    psChan[1] = 'v';
    pszFilename = CPLFormFilename(pszDir, pszBase, pszExt);

    poDS->fpBinHV = VSIFOpenL(pszFilename, "r");

    if (poDS->fpBinHV != nullptr)
    {
        poDS->SetBand(2, new COASPRasterBand(poDS, GDT_CFloat32,
                                             hv, poDS->fpBinHV));
    }

    /* Vertical transmit, horizontal receive */
    psChan[0] = 'v';
    psChan[1] = 'h';
    pszFilename = CPLFormFilename(pszDir, pszBase, pszExt);

    poDS->fpBinVH = VSIFOpenL(pszFilename, "r");

    if (poDS->fpBinVH != nullptr)
    {
        poDS->SetBand(3, new COASPRasterBand(poDS, GDT_CFloat32,
                                             vh, poDS->fpBinVH));
    }

    /* Vertical transmit, vertical receive */
    psChan[0] = 'v';
    psChan[1] = 'v';
    pszFilename = CPLFormFilename(pszDir, pszBase, pszExt);

    poDS->fpBinVV = VSIFOpenL(pszFilename, "r");

    if (poDS->fpBinVV != nullptr)
    {
        poDS->SetBand(4, new COASPRasterBand(poDS, GDT_CFloat32,
                                             vv, poDS->fpBinVV));
    }

    /* Oops, missing all the data? */
    if (poDS->fpBinHH == nullptr && poDS->fpBinHV == nullptr
        && poDS->fpBinVH == nullptr && poDS->fpBinVV == nullptr)
    {
        CPLError(CE_Failure,CPLE_AppDefined,"Unable to find any data!");
        VSIFree(pszBase);
        VSIFree(pszDir);
        delete poDS;
        return nullptr;
    }

    if( poDS->GetRasterCount() == 4 )
    {
        poDS->SetMetadataItem( "MATRIX_REPRESENTATION", "SCATTERING" );
    }

    VSIFree(pszBase);
    VSIFree(pszDir);

    return poDS;
}

/************************************************************************/
/*                         GDALRegister_COASP()                         */
/************************************************************************/

void GDALRegister_COASP()
{
    if( GDALGetDriverByName( "COASP" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "COASP" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "DRDC COASP SAR Processor Raster" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION,
                               "hdr" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/coasp.html");
    poDriver->pfnIdentify = COASPDataset::Identify;
    poDriver->pfnOpen = COASPDataset::Open;
    GetGDALDriverManager()->RegisterDriver( poDriver );
}
