/******************************************************************************
 * $Id$
 *
 * Project:  NOAA Polar Orbiter Level 1b Dataset Reader
 * Purpose:  Partial implementation, can read NOAA-9,10,11,12,13,14 GAC/LAC/HRPT
 * Author:   Andrey Kiselev, a_kissel@eudoramail.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Andrey Kiselev <a_kissel@eudoramail.com>
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
 ******************************************************************************
 * 
 * $Log$
 * Revision 1.2  2002/05/16 01:26:57  warmerda
 * move up variable declaration to avoid VC++ error
 *
 * Revision 1.1  2002/05/08 16:32:20  dron
 * NOAA Polar Orbiter Dataset reader added (not full implementation yet).
 *
 *
 */

#include "gdal_priv.h"

CPL_CVSID("$Id$");

static GDALDriver	*poL1BDriver = NULL;

CPL_C_START
void	GDALRegister_L1B(void);
CPL_C_END

enum {		// Spacecrafts
    NOAA9,
    NOAA10,
    NOAA11,
    NOAA12,
    NOAA13,
    NOAA14,
    NOAA15
};

enum {		// Types of datasets
    HRPT,
    LAC,
    GAC
};

enum {		// Data format
    PACKED10BIT,
    UNPACKED16BIT
};  

#define TBM_HEADER_SIZE 122
#define DATASET_HEADER_SIZE 148

/************************************************************************/
/* ==================================================================== */
/*				L1BDataset				*/
/* ==================================================================== */
/************************************************************************/

class L1BDataset : public GDALDataset
{
    friend class L1BRasterBand;

    GByte	 pabyTBMHeader[TBM_HEADER_SIZE];
//    GByte	 pabyDataHeader[DATASET_HEADER_SIZE];

    int         nGCPCount;
    GDAL_GCP    *pasGCPList;
    int		nGCPStart;
    int		nGCPStep;

    int		nBufferSize;
    int		iSpacecraftID;
    int		iDataType;	// LAC, GAC, HRPT
    int		iDataFormat;	// 10-bit packed or 16-bit unpacked
    int		nRecordDataStart;
    int		nRecordDataEnd;
    int		nDataStartOffset;
    int		nRecordSize;

    double      adfGeoTransform[6];
    char        *pszProjection; 

    FILE	*fp;

    void        FetchGCPs();

  public:
                L1BDataset();
		~L1BDataset();
    
    virtual int   GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();
    static GDALDataset *Open( GDALOpenInfo * );

//    CPLErr 	GetGeoTransform( double * padfTransform );
    const char *GetProjectionRef();

};

/************************************************************************/
/* ==================================================================== */
/*                            L1BRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class L1BRasterBand : public GDALRasterBand
{
    friend class L1BDataset;

  public:

    		L1BRasterBand( L1BDataset *, int );
    
//    virtual const char *GetUnitType();
//    virtual double GetNoDataValue( int *pbSuccess = NULL );
    virtual CPLErr IReadBlock( int, int, void * );
};


/************************************************************************/
/*                           L1BRasterBand()                            */
/************************************************************************/

L1BRasterBand::L1BRasterBand( L1BDataset *poDS, int nBand )

{
    this->poDS = poDS;
    this->nBand = nBand;
    eDataType = GDT_UInt16;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

/*    nBlocksPerRow = 1;
    nBlocksPerColumn = 0;*/

}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr L1BRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                      void * pImage )
{
    L1BDataset *poGDS = (L1BDataset *) poDS;
    GUInt32 *iscan;		// Packed scanline buffer
    GUInt32 iword, jword;
    int i, j;
    GUInt16 *scan;
	    
/* -------------------------------------------------------------------- */
/*      Seek to data.                                                   */
/* -------------------------------------------------------------------- */
    VSIFSeek(poGDS->fp,
	poGDS->nDataStartOffset + nBlockYOff * poGDS->nRecordSize, SEEK_SET);

/* -------------------------------------------------------------------- */
/*      Read data into the buffer.					*/
/* -------------------------------------------------------------------- */
    // Read packed scanline
    int nBlockSize = nBlockXSize * nBlockYSize;
    switch (poGDS->iDataFormat)
    {
        case PACKED10BIT:
            iscan = (GUInt32 *)CPLMalloc(poGDS->nRecordSize);
            scan = (GUInt16 *)CPLMalloc(poGDS->nBufferSize);
            VSIFRead(iscan, 1, poGDS->nRecordSize, poGDS->fp);
            j = 0;
            for(i = poGDS->nRecordDataStart / sizeof(iscan[0]);
		i < poGDS->nRecordDataEnd / sizeof(iscan[0]); i++)
            {
                iword = iscan[i];
#ifdef CPL_LSB
                CPL_SWAP32PTR(&iword);
#endif
                jword = iword & 0x3FF00000;
                scan[j++] = jword >> 20;
                jword = iword & 0x000FFC00;
                scan[j++] = jword >> 10;
                scan[j++] = iword & 0x000003FF;
             }
    
             for( i = 0, j = 0; i < nBlockSize; i++ )
             {
                 ((GUInt16 *) pImage)[i] = scan[j + nBand - 1];
	         j += poGDS->nBands;
             }
    
             CPLFree(iscan);
             CPLFree(scan);
	break;
	case UNPACKED16BIT: // FIXME: Not implemented yet, need a sample image
	default:
	break;
    }
    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*				L1BDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           L1BDataset()                           */
/************************************************************************/

L1BDataset::L1BDataset()

{
    fp = NULL;
    nGCPCount = 0;
    pasGCPList = NULL;
    pszProjection = NULL;
    nBands = 0;
}

/************************************************************************/
/*                            ~L1BDataset()                         */
/************************************************************************/

L1BDataset::~L1BDataset()

{
    if ( pasGCPList != NULL )
        CPLFree( pasGCPList );
    CPLFree( pszProjection );
    if( fp != NULL )
        VSIFClose( fp );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

/*CPLErr L1BDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
    return CE_None;
}*/

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *L1BDataset::GetProjectionRef()

{
    return pszProjection;
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int L1BDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *L1BDataset::GetGCPProjection()

{
    if( nGCPCount > 0 )
        return "GEOGCS[\"WGS 72\",DATUM[\"WGS_1972\",SPHEROID[\"WGS 72\",6378135,298.26,AUTHORITY[\"EPSG\",7043]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",6326]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",8901]],UNIT[\"DMSH\",0.0174532925199433,AUTHORITY[\"EPSG\",9108]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST],AUTHORITY[\"EPSG\",4326]]";
    else
        return "";
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *L1BDataset::GetGCPs()
{
    return pasGCPList;
}

/************************************************************************/
/*                          FetchGCPs()		                        */
/************************************************************************/

void L1BDataset::FetchGCPs()

{
//    double	dfLat, dfLong;
    int		nGoodGCPs, iLine, iPixel;
    GInt16	*piRecordHeader;

/* -------------------------------------------------------------------- */
/*      Fetch the GCP from the individual scanlines                     */
/* -------------------------------------------------------------------- */
    piRecordHeader = (GInt16 *) CPLMalloc(nRecordDataStart);
    pasGCPList = (GDAL_GCP *) CPLCalloc( nRasterYSize * 51, sizeof(GDAL_GCP) );
    GDALInitGCPs( nRasterYSize * 51, pasGCPList );
    
    for ( iLine = 0; iLine < nRasterYSize; iLine++ )
    {
	VSIFSeek(fp, nDataStartOffset + iLine * nRecordSize, SEEK_SET);
	VSIFRead(piRecordHeader, 1, nRecordDataStart, fp);
	nGoodGCPs = (piRecordHeader[53] <= 51)?piRecordHeader[53]:51;
        iPixel = nGCPStart;
	int j = 52;
	while ( j < 52 + 2 * nGoodGCPs )
	{
#ifdef CPL_LSB
            pasGCPList[nGCPCount].dfGCPY = CPL_SWAP16(piRecordHeader[j]) / 128.0;
            pasGCPList[nGCPCount].dfGCPX = CPL_SWAP16(piRecordHeader[++j]) / 128.0;
#else
            pasGCPList[nGCPCount].dfGCPY = piRecordHeader[j] / 128.0;
            pasGCPList[nGCPCount].dfGCPX = piRecordHeader[++j] / 128.0;
#endif
//	    pasGCPList[nGCPCount].pszId;
	    pasGCPList[nGCPCount].dfGCPZ = 0.0;
	    pasGCPList[nGCPCount].dfGCPPixel = iPixel;
	    iPixel += nGCPStep;
	    pasGCPList[nGCPCount].dfGCPLine = iLine;
	    nGCPCount++;
	}
    }
    
    CPLFree(piRecordHeader);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *L1BDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int i = 0;

    if( poOpenInfo->fp == NULL /*|| poOpenInfo->nHeaderBytes < 200*/ )
        return NULL;

    if( !EQUALN((const char *) poOpenInfo->pabyHeader + 30, "NSS", 3) && 
        !EQUALN((const char *) poOpenInfo->pabyHeader + 33, ".", 1) &&
	!EQUALN((const char *) poOpenInfo->pabyHeader + 38, ".", 1) && 
	!EQUALN((const char *) poOpenInfo->pabyHeader + 41, ".", 1) && 
	!EQUALN((const char *) poOpenInfo->pabyHeader + 48, ".", 1) && 
	!EQUALN((const char *) poOpenInfo->pabyHeader + 54, ".", 1) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    L1BDataset 	*poDS;
    VSIStatBuf  sStat;

    poDS = new L1BDataset();

    poDS->poDriver = poL1BDriver;
    poDS->fp = poOpenInfo->fp;
    poOpenInfo->fp = NULL;
    
/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    VSIFSeek( poDS->fp, 0, SEEK_SET );
    // Skip TBM (Terabit memory) Header record
//    VSIFSeek( poDS->fp, TBM_HEADER_SIZE, SEEK_SET );
    VSIFRead( poDS->pabyTBMHeader, 1, TBM_HEADER_SIZE, poDS->fp );
//    VSIFRead( poDS->abyDataHeader, 1, DATASET_HEADER_SIZE, poDS->fp );

    // Determine spacecraft type
    if ( EQUALN((const char *)poDS->pabyTBMHeader + 39, "NF", 2) )
         poDS->iSpacecraftID = NOAA9;
    else if ( EQUALN((const char *)poDS->pabyTBMHeader + 39, "NG", 2) )
	 poDS->iSpacecraftID = NOAA10;
    else if ( EQUALN((const char *)poDS->pabyTBMHeader + 39, "NH", 2) )
	 poDS->iSpacecraftID = NOAA11;
    else if ( EQUALN((const char *)poDS->pabyTBMHeader + 39, "ND", 2) )
	 poDS->iSpacecraftID = NOAA12;
    else if ( EQUALN((const char *)poDS->pabyTBMHeader + 39, "NI", 2) )
	 poDS->iSpacecraftID = NOAA13;
    else if ( EQUALN((const char *)poDS->pabyTBMHeader + 39, "NJ", 2) )
	 poDS->iSpacecraftID = NOAA14;
    else if ( EQUALN((const char *)poDS->pabyTBMHeader + 39, "NK", 2) )
	 poDS->iSpacecraftID = NOAA15;
    else
	 goto bad;
	   
    // Determine dataset type
    if ( EQUALN((const char *)poDS->pabyTBMHeader + 34, "HRPT", 4) )
	 poDS->iDataType = HRPT;
    else if ( EQUALN((const char *)poDS->pabyTBMHeader + 34, "LHRR", 4) )
	 poDS->iDataType = LAC;
    else if ( EQUALN((const char *)poDS->pabyTBMHeader + 34, "GHRR", 4) )
	 poDS->iDataType = GAC;
    else
	 goto bad;

    // Determine number of channels and data format
    // (10-bit packed or 16-bit unpacked)
    /*if ( EQUALN((const char *)poDS->pabyTBMHeader + 74, "T", 1) )
    {*/
	 poDS->nBands = 5;
	 poDS->iDataFormat = PACKED10BIT;
    /*}
    else if ( EQUALN((const char *)poDS->pabyTBMHeader + 74, "S", 1) )
    {
        for ( int i = 97; i < 117; i++ ) // FIXME: should report type of the band
	    if (poDS->pabyTBMHeader[i] == 1)
	        poDS->nBands++;
	poDS->iDataFormat = UNPACKED16BIT;
    }
    else
	 goto bad;*/

    switch(poDS->iDataType)
    {
	case HRPT:
	case LAC:
            poDS->nRasterXSize = 2048;
	    poDS->nBufferSize = 20484;
	    poDS->nGCPStart = 25;
	    poDS->nGCPStep = 40;
	    if (poDS->iSpacecraftID < NOAA15)
	    {
                poDS->nDataStartOffset = 14922;
                poDS->nRecordSize = 14800;
	        poDS->nRecordDataStart = 448;
	        poDS->nRecordDataEnd = 14104;
	    }
	    else
	    {
		poDS->nDataStartOffset = 15872;
                poDS->nRecordSize = 15872;
	        poDS->nRecordDataStart = 1264;
	        poDS->nRecordDataEnd = 14920;
	    }
        break;
	case GAC:
    	    poDS->nRasterXSize = 409;
	    poDS->nBufferSize = 4092;
	    poDS->nGCPStart = 5;
	    poDS->nGCPStep = 8;
	    if (poDS->iSpacecraftID < NOAA15)
	    {
	        poDS->nDataStartOffset = 6562;
                poDS->nRecordSize = 3220;
	        poDS->nRecordDataStart = 448;
	        poDS->nRecordDataEnd = 3176;
	    }
	    else
		return NULL; // FIXME
	break;
	default:
	    goto bad;
    }
    // Compute number of lines dinamycally, so we can read partially
    // downloaded files
    CPLStat(poOpenInfo->pszFilename, &sStat);
    poDS->nRasterYSize =
	(sStat.st_size - poDS->nDataStartOffset) / poDS->nRecordSize;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( i = 1; i <= poDS->nBands; i++ )
        poDS->SetBand( i, new L1BRasterBand( poDS, i ));

/* -------------------------------------------------------------------- */
/*      Do we have GCPs?		                                */
/* -------------------------------------------------------------------- */
    if ( EQUALN((const char *)poDS->pabyTBMHeader + 96, "Y", 1) )
        poDS->FetchGCPs();

    return( poDS );
bad:
    delete poDS;
    return NULL;
}

/************************************************************************/
/*                        GDALRegister_L1B()				*/
/************************************************************************/

void GDALRegister_L1B()

{
    GDALDriver	*poDriver;

    if( poL1BDriver == NULL )
    {
        poL1BDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "L1B";
        poDriver->pszLongName = "NOAA Polar Orbiter Level 1b Data Set";
        poDriver->pszHelpTopic = "frmt_various.html#L1B";
        
        poDriver->pfnOpen = L1BDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

