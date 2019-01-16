/* TerraSAR-X COSAR Format Driver
 * (C)2007 Philippe P. Vachon <philippe@cowpig.ca>
 * ---------------------------------------------------------------------------
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
 */

#include "cpl_conv.h"
#include "cpl_port.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal_frmts.h"
#include "gdal_priv.h"

#include <string.h>

CPL_CVSID("$Id$")

/* Various offsets, in bytes */
// Commented out the unused defines.
// #define BIB_OFFSET   0  /* Bytes in burst, valid only for ScanSAR */
// #define RSRI_OFFSET  4  /* Range Sample Relative Index */
const static int RS_OFFSET = 8;  /* Range Samples, the length of a range line */
// #define AS_OFFSET    12 /* Azimuth Samples, the length of an azimuth column */
// #define BI_OFFSET    16 /* Burst Index, the index number of the burst */
const static int RTNB_OFFSET = 20;  /* Rangeline total number of bytes, incl. annot. */
// #define TNL_OFFSET   24 /* Total Number of Lines */
const static int MAGIC1_OFFSET = 28; /* Magic number 1: 0x43534152 */
// #define MAGIC2_OFFSET 32 /* Magic number 2: Version number */

// #define COSAR_MAGIC  0x43534152  /* String CSAR */
// #define FILLER_MAGIC 0x7F7F7F7F  /* Filler value, we'll use this for a test */

class COSARDataset : public GDALDataset
{
public:
        COSARDataset() : fp(nullptr) { }
        ~COSARDataset();
        VSILFILE *fp;

        static GDALDataset *Open( GDALOpenInfo * );
};

class COSARRasterBand : public GDALRasterBand
{
    unsigned long nRTNB;

  public:
    COSARRasterBand( COSARDataset *, unsigned long nRTNB );
    CPLErr IReadBlock( int, int, void * ) override;
};

/*****************************************************************************
 * COSARRasterBand Implementation
 *****************************************************************************/

COSARRasterBand::COSARRasterBand( COSARDataset *pDS, unsigned long nRTNBIn ) :
    nRTNB(nRTNBIn)
{
        nBlockXSize = pDS->GetRasterXSize();
        nBlockYSize = 1;
        eDataType = GDT_CInt16;
}

CPLErr COSARRasterBand::IReadBlock(CPL_UNUSED int nBlockXOff,
                                   int nBlockYOff,
                                   void *pImage) {

    COSARDataset *pCDS = (COSARDataset *) poDS;

    /* Find the line we want to be at */
    /* To explain some magic numbers:
     *   4 bytes for an entire sample (2 I, 2 Q)
     *   nBlockYOff + 4 = Y offset + 4 annotation lines at beginning
     *    of file
     */

    VSIFSeekL(pCDS->fp,(this->nRTNB * (nBlockYOff + 4)), SEEK_SET);

    /* Read RSFV and RSLV (TX-GS-DD-3307) */
    unsigned long nRSFV = 0;
    unsigned long nRSLV = 0;
    VSIFReadL(&nRSFV,1,4,pCDS->fp);
    VSIFReadL(&nRSLV,1,4,pCDS->fp);

#ifdef CPL_LSB
    nRSFV = CPL_SWAP32(nRSFV);
    nRSLV = CPL_SWAP32(nRSLV);
#endif

    if (nRSLV < nRSFV || nRSFV == 0 || nRSLV == 0
        || nRSFV - 1 >= ((unsigned long) nBlockXSize)
        || nRSLV - 1 >= ((unsigned long) nBlockXSize)
        || nRSFV >= this->nRTNB || nRSLV > this->nRTNB)
    {
        /* throw an error */
        CPLError(CE_Failure, CPLE_AppDefined,
                 "RSLV/RSFV values are not sane... oh dear.\n");
        return CE_Failure;
    }

    /* zero out the range line */
    for (int i = 0; i < this->nRasterXSize; i++)
    {
        ((GUInt32 *)pImage)[i] = 0;
    }

    /* properly account for validity mask */
    if (nRSFV > 1)
    {
        VSIFSeekL(pCDS->fp,(this->nRTNB*(nBlockYOff+4)+(nRSFV+1)*4), SEEK_SET);
    }

    /* Read the valid samples: */
    VSIFReadL(((char *)pImage)+((nRSFV - 1)*4),1,((nRSLV-1)*4)-((nRSFV-1)*4),pCDS->fp);

#ifdef CPL_LSB
    // GDALSwapWords( pImage, 4, nBlockXSize * nBlockYSize, 4 );
    GDALSwapWords( pImage, 2, nBlockXSize * nBlockYSize * 2, 2 );
#endif

    return CE_None;
}

/*****************************************************************************
 * COSARDataset Implementation
 *****************************************************************************/

COSARDataset::~COSARDataset()
{
    if( fp != nullptr )
    {
        VSIFCloseL(fp);
    }
}

GDALDataset *COSARDataset::Open( GDALOpenInfo * pOpenInfo ) {
    long nRTNB;
    /* Check if we're actually a COSAR data set. */
    if( pOpenInfo->nHeaderBytes < 4 || pOpenInfo->fpL == nullptr)
        return nullptr;

    if (!STARTS_WITH_CI((char *)pOpenInfo->pabyHeader+MAGIC1_OFFSET, "CSAR"))
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( pOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The COSAR driver does not support update access to existing"
                  " datasets.\n" );
        return nullptr;
    }

    /* this is a cosar dataset */
    COSARDataset *pDS = new COSARDataset();

    /* steal fp */
    pDS->fp = pOpenInfo->fpL;
    pOpenInfo->fpL = nullptr;

    VSIFSeekL(pDS->fp, RS_OFFSET, SEEK_SET);
    VSIFReadL(&pDS->nRasterXSize, 1, 4, pDS->fp);
#ifdef CPL_LSB
    pDS->nRasterXSize = CPL_SWAP32(pDS->nRasterXSize);
#endif

    VSIFReadL(&pDS->nRasterYSize, 1, 4, pDS->fp);
#ifdef CPL_LSB
    pDS->nRasterYSize = CPL_SWAP32(pDS->nRasterYSize);
#endif

    if( !GDALCheckDatasetDimensions(pDS->nRasterXSize, pDS->nRasterYSize) )
    {
        delete pDS;
        return nullptr;
    }

    VSIFSeekL(pDS->fp, RTNB_OFFSET, SEEK_SET);
    VSIFReadL(&nRTNB, 1, 4, pDS->fp);
#ifdef CPL_LSB
    nRTNB = CPL_SWAP32(nRTNB);
#endif

    /* Add raster band */
    pDS->SetBand(1, new COSARRasterBand(pDS, nRTNB));
    return pDS;
}

/* register the driver with GDAL */
void GDALRegister_COSAR()

{
    if( GDALGetDriverByName( "cosar" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();
    poDriver->SetDescription("COSAR");
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "COSAR Annotated Binary Matrix (TerraSAR-X)");
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "frmt_cosar.html");
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->pfnOpen = COSARDataset::Open;
    GetGDALDriverManager()->RegisterDriver(poDriver);
}
