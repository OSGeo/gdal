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


#include "gdal_priv.h"
#include "cpl_port.h"
#include "cpl_conv.h"
#include "cpl_vsi.h"
#include "cpl_string.h"
#include <string.h>

/* Various offsets, in bytes */
#define BIB_OFFSET 	0  /* Bytes in burst, valid only for ScanSAR */
#define RSRI_OFFSET 	4  /* Range Sample Relative Index */
#define RS_OFFSET 	8  /* Range Samples, the length of a range line */
#define AS_OFFSET 	12 /* Azimuth Samples, the length of an azimth column */
#define BI_OFFSET	16 /* Burst Index, the index number of the burst */
#define RTNB_OFFSET	20 /* Rangeline total number of bytes, incl. annot. */
#define TNL_OFFSET	24 /* Total Number of Lines */
#define MAGIC1_OFFSET	28 /* Magic number 1: 0x43534152 */
#define MAGIC2_OFFSET	32 /* Magic number 2: Version number */ 

#define COSAR_MAGIC 	0x43534152 /* String CSAR */
#define FILLER_MAGIC	0x7F7F7F7F /* Filler value, we'll use this for a test */

CPL_C_START
void GDALRegister_COSAR(void);
CPL_C_END

class COSARDataset : public GDALDataset
{
	long nSize;
public:
	FILE *fp;
/*	~COSARDataset(); */
	
	static GDALDataset *Open( GDALOpenInfo * );
	long GetSizeInBytes() { return nSize; }
};

class COSARRasterBand : public GDALRasterBand
{
	unsigned long nRTNB;
	int nBurstNumber;
public:
	COSARRasterBand(COSARDataset *, unsigned long nRTNB);
	virtual CPLErr IReadBlock(int, int, void *);
};

/*****************************************************************************
 * COSARRasterBand Implementation
 *****************************************************************************/

COSARRasterBand::COSARRasterBand(COSARDataset *pDS, unsigned long nRTNB) {
	this->nRTNB = nRTNB;
	nBurstNumber = 1;
	nBlockXSize = pDS->GetRasterXSize();
	nBlockYSize = 1;
	eDataType = GDT_CInt16;
}

CPLErr COSARRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff, 
	void *pImage) {

    unsigned long nRSFV = 0;
    unsigned long nRSLV = 0;
    COSARDataset *pCDS = (COSARDataset *) poDS;

    /* Find the line we want to be at */
    /* To explain some magic numbers:
     *   4 bytes for an entire sample (2 I, 2 Q)
     *   nBlockYOff + 4 = Y offset + 4 annotation lines at beginning
     *    of file
     */

    VSIFSeek(pCDS->fp,(this->nRTNB * (nBlockYOff + 4)), SEEK_SET);


    /* Read RSFV and RSLV (TX-GS-DD-3307) */
    VSIFRead(&nRSFV,1,4,pCDS->fp);
    VSIFRead(&nRSLV,1,4,pCDS->fp);

#ifdef CPL_LSB
    nRSFV = CPL_SWAP32(nRSFV);
    nRSLV = CPL_SWAP32(nRSLV);
#endif

    if (nRSLV < nRSFV || nRSFV == 0
        || nRSFV - 1 >= ((unsigned long) nBlockXSize)
        || nRSLV - nRSFV > ((unsigned long) nBlockXSize)
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
        VSIFSeek(pCDS->fp,(this->nRTNB*(nBlockYOff+4)+(nRSFV+1)*4), SEEK_SET);
    }

    /* Read the valid samples: */
    VSIFRead(((char *)pImage)+((nRSFV - 1)*4),1,((nRSLV-1)*4)-((nRSFV-1)*4),pCDS->fp);

#ifdef CPL_LSB
    // GDALSwapWords( pImage, 4, nBlockXSize * nBlockYSize, 4 );
    GDALSwapWords( pImage, 2, nBlockXSize * nBlockYSize * 2, 2 );
#endif


    return CE_None;
}


/*****************************************************************************
 * COSARDataset Implementation
 *****************************************************************************/

GDALDataset *COSARDataset::Open( GDALOpenInfo * pOpenInfo ) {
    long nRTNB;
    /* Check if we're actually a COSAR data set. */
    if( pOpenInfo->nHeaderBytes < 4 )
        return NULL;

    if (!EQUALN((char *)pOpenInfo->pabyHeader+MAGIC1_OFFSET, "CSAR",4)) 
        return NULL;

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( pOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The COSAR driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }
    
    /* this is a cosar dataset */
    COSARDataset *pDS;
    pDS = new COSARDataset();
	
    /* steal fp */
    pDS->fp = pOpenInfo->fp;
    pOpenInfo->fp = NULL;

    /* Calculate the file size */
    VSIFSeek(pDS->fp,0,SEEK_END);
    pDS->nSize = VSIFTell(pDS->fp);

    VSIFSeek(pDS->fp, RS_OFFSET, SEEK_SET);
    VSIFRead(&pDS->nRasterXSize, 1, 4, pDS->fp);  
#ifdef CPL_LSB
    pDS->nRasterXSize = CPL_SWAP32(pDS->nRasterXSize);
#endif
	
	
    VSIFRead(&pDS->nRasterYSize, 1, 4, pDS->fp);
#ifdef CPL_LSB
    pDS->nRasterYSize = CPL_SWAP32(pDS->nRasterYSize);
#endif

    VSIFSeek(pDS->fp, RTNB_OFFSET, SEEK_SET);
    VSIFRead(&nRTNB, 1, 4, pDS->fp);
#ifdef CPL_LSB
    nRTNB = CPL_SWAP32(nRTNB);
#endif

    /* Add raster band */
    pDS->SetBand(1, new COSARRasterBand(pDS, nRTNB));
    return pDS;	
}


/* register the driver with GDAL */
void GDALRegister_COSAR() {
	GDALDriver *pDriver;
	if (GDALGetDriverByName("cosar") == NULL) {
		pDriver = new GDALDriver();
		pDriver->SetDescription("COSAR");
		pDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
			"COSAR Annotated Binary Matrix (TerraSAR-X)");
		pDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
			"frmt_cosar.html");
		pDriver->pfnOpen = COSARDataset::Open;
		GetGDALDriverManager()->RegisterDriver(pDriver);
	}
}

