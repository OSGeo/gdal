/******************************************************************************
 * $Id$
 *
 * Project:  NOAA Polar Orbiter Level 1b Dataset Reader
 * Purpose:  Partial implementation, can read NOAA-9(F)-NOAA-17(M) GAC/LAC/HRPT
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
 * Revision 1.3  2002/05/18 14:01:21  dron
 * NOAA-15 fixes, georeferencing
 *
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
    NOAA9,	// NOAA-F
    NOAA10,	// NOAA-G
    NOAA11,	// NOAA-H
    NOAA12,	// NOAA-D
    NOAA13,	// NOAA-I
    NOAA14,	// NOAA-J
    NOAA15,	// NOAA-K
    NOAA16,	// NOAA-L
    NOAA17,	// NOAA-M
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

    GDAL_GCP    *pasGCPList;
    GDAL_GCP	*pasCorners;
    int         nGCPCount;
    int		iGCPOffset;
    int		iGCPCodeOffset;
    int		nGCPStart;
    int		nGCPStep;
    int		nGCPPerLine;
    double	dfTLDist, dfTRDist, dfBLDist, dfBRDist;

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

    void        ProcessHeader();
    void	FetchNOAA9GCPs(GDAL_GCP *pasGCPList, GInt16 *piRecordHeader, int iLine);
    void	FetchNOAA15GCPs(GDAL_GCP *pasGCPList, GInt32 *piRecordHeader, int iLine);
    void	UpdateCorners(GDAL_GCP *psGCP);
    void	ComputeGeoref();

  public:
                L1BDataset();
		~L1BDataset();
    
    virtual int   GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();
    static GDALDataset *Open( GDALOpenInfo * );

    CPLErr 	GetGeoTransform( double * padfTransform );
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
            for(i = poGDS->nRecordDataStart / (int)sizeof(iscan[0]);
		i < poGDS->nRecordDataEnd / (int)sizeof(iscan[0]); i++)
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
    pasCorners = (GDAL_GCP *) CPLCalloc( 4, sizeof(GDAL_GCP) );
    pszProjection = "GEOGCS[\"WGS 72\",DATUM[\"WGS_1972\",SPHEROID[\"WGS 72\",6378135,298.26,AUTHORITY[\"EPSG\",7043]],TOWGS84[0,0,4.5,0,0,0.554,0.2263],AUTHORITY[\"EPSG\",6322]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",8901]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",9108]],AXIS[\"Lat\",\"NORTH\"],AXIS[\"Long\",\"EAST\"],AUTHORITY[\"EPSG\",4322]";
    nBands = 0;
}

/************************************************************************/
/*                            ~L1BDataset()                         */
/************************************************************************/

L1BDataset::~L1BDataset()

{
    if ( pasGCPList != NULL )
        CPLFree( pasGCPList );
    if( fp != NULL )
        VSIFClose( fp );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr L1BDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
    return CE_None;
}

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
        return pszProjection;
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

void L1BDataset::ComputeGeoref()
{
/*	adfGeoTransform[0]; //lon
	adfGeoTransform[3]; //lat*/
	;
}

/************************************************************************/
/*		Is this GCP closer to one of the corners?		*/
/************************************************************************/

void L1BDataset::UpdateCorners(GDAL_GCP *psGCP)
{
    double tldist, trdist, bldist, brdist;

    // Will cycle through all GCPs to find ones closest to corners
    tldist = psGCP->dfGCPPixel * psGCP->dfGCPPixel +
    psGCP->dfGCPLine * psGCP->dfGCPLine;
    if (tldist < dfTLDist)
    {
        memcpy(&pasCorners[0], psGCP, sizeof(GDAL_GCP));
	dfTLDist = tldist;
    }
    else
    {
	trdist = (GetRasterXSize() - psGCP->dfGCPPixel) *
	    (GetRasterXSize() - psGCP->dfGCPPixel) + psGCP->dfGCPLine * psGCP->dfGCPLine;
	if (trdist < dfTRDist)
	{
	    memcpy(&pasCorners[1], psGCP, sizeof(GDAL_GCP));
	    dfTRDist = trdist;
	}
	else
	{
	    bldist=psGCP->dfGCPPixel*psGCP->dfGCPPixel+
	        (GetRasterYSize() - psGCP->dfGCPLine) * (GetRasterYSize() - psGCP->dfGCPLine);
	    if (bldist < dfBLDist)
	    {
	        memcpy(&pasCorners[2], psGCP, sizeof(GDAL_GCP));
	        dfBLDist = bldist;
	    }
	    else
	    {
	        brdist = (GetRasterXSize() - psGCP->dfGCPPixel) *
	            (GetRasterXSize() - psGCP->dfGCPPixel) +
		    (GetRasterYSize() - psGCP->dfGCPLine) *
		    (GetRasterYSize() - psGCP->dfGCPLine);
		if (brdist < dfBRDist)
	        {
		    memcpy(&pasCorners[3], psGCP, sizeof(GDAL_GCP));
		    dfBRDist = brdist;
	        }
	    }
	 }
    }
}

/************************************************************************/
/* Fetch the GCP from the individual scanlines (NOAA9-NOAA14 version)	*/
/************************************************************************/

void L1BDataset::FetchNOAA9GCPs(GDAL_GCP *pasGCPList, GInt16 *piRecordHeader, int iLine)
{
    int		nGoodGCPs, iPixel;
    
    nGoodGCPs =(piRecordHeader[iGCPCodeOffset] <= nGCPPerLine)?
	    piRecordHeader[iGCPCodeOffset]:nGCPPerLine;
    iPixel = nGCPStart;
    int j = iGCPOffset / (int)sizeof(piRecordHeader[0]);
    while ( j < iGCPOffset / (int)sizeof(piRecordHeader[0]) + 2 * nGoodGCPs )
    {
#ifdef CPL_LSB
        pasGCPList[nGCPCount].dfGCPY = CPL_SWAP16(piRecordHeader[j]) / 128.0; j++;
        pasGCPList[nGCPCount].dfGCPX = CPL_SWAP16(piRecordHeader[j]) / 128.0; j++;
#else
        pasGCPList[nGCPCount].dfGCPY = piRecordHeader[j++] / 128.0;
        pasGCPList[nGCPCount].dfGCPX = piRecordHeader[j++] / 128.0;
#endif
//	pasGCPList[nGCPCount].pszId;
	pasGCPList[nGCPCount].dfGCPZ = 0.0;
	pasGCPList[nGCPCount].dfGCPPixel = (double)iPixel;
	iPixel += nGCPStep;
	pasGCPList[nGCPCount].dfGCPLine = (double)iLine;
        UpdateCorners(&pasGCPList[nGCPCount]);
        nGCPCount++;
    }
}

/************************************************************************/
/* Fetch the GCP from the individual scanlines (NOAA15-NOAA17 version)	*/
/************************************************************************/

void L1BDataset::FetchNOAA15GCPs(GDAL_GCP *pasGCPList, GInt32 *piRecordHeader, int iLine)
{
    int		nGoodGCPs, iPixel;
    
    nGoodGCPs = nGCPPerLine;
    iPixel = nGCPStart;
    int j = iGCPOffset / (int)sizeof(piRecordHeader[0]);
    while ( j < iGCPOffset / (int)sizeof(piRecordHeader[0]) + 2 * nGoodGCPs )
    {
#ifdef CPL_LSB
        pasGCPList[nGCPCount].dfGCPY = CPL_SWAP32(piRecordHeader[j]) / 10000.0; j++;
        pasGCPList[nGCPCount].dfGCPX = CPL_SWAP32(piRecordHeader[j]) / 10000.0; j++;
#else
        pasGCPList[nGCPCount].dfGCPY = piRecordHeader[j++] / 10000.0;
        pasGCPList[nGCPCount].dfGCPX = piRecordHeader[j++] / 10000.0;
#endif
//	pasGCPList[nGCPCount].pszId;
	pasGCPList[nGCPCount].dfGCPZ = 0.0;
	pasGCPList[nGCPCount].dfGCPPixel = (double)iPixel;
	iPixel += nGCPStep;
	pasGCPList[nGCPCount].dfGCPLine = (double)iLine;
        UpdateCorners(&pasGCPList[nGCPCount]);
        nGCPCount++;
    }
}

/************************************************************************/
/*			ProcessHeader()					*/
/************************************************************************/

void L1BDataset::ProcessHeader()
{
    int		iLine;
    void	*piRecordHeader;

/* -------------------------------------------------------------------- */
/*      Fetch the GCP from the individual scanlines                     */
/* -------------------------------------------------------------------- */
    piRecordHeader = CPLMalloc(nRecordDataStart);
    pasGCPList = (GDAL_GCP *) CPLCalloc( GetRasterYSize() * nGCPPerLine, sizeof(GDAL_GCP) );
    GDALInitGCPs( GetRasterYSize() * nGCPPerLine, pasGCPList );
    dfTLDist = dfTRDist = dfBLDist = dfBRDist = GetRasterXSize() * GetRasterXSize() +
	    				GetRasterYSize() * GetRasterYSize();
    
    for ( iLine = 0; iLine < GetRasterYSize(); iLine++ )
    {
	VSIFSeek(fp, nDataStartOffset + iLine * nRecordSize, SEEK_SET);
	VSIFRead(piRecordHeader, 1, nRecordDataStart, fp);
	if (iSpacecraftID <= NOAA14)
	    FetchNOAA9GCPs(pasGCPList, (GInt16 *)piRecordHeader, iLine);
	else
	    FetchNOAA15GCPs(pasGCPList, (GInt32 *)piRecordHeader, iLine);
    }
    ComputeGeoref();
    int bApproxOK = TRUE;
    GDALGCPsToGeoTransform( 4, pasCorners, adfGeoTransform, bApproxOK );
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
    else if ( EQUALN((const char *)poDS->pabyTBMHeader + 39, "NL", 2) )
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
	    poDS->nGCPPerLine = 51;
	    if (poDS->iSpacecraftID <= NOAA14)
	    {
                poDS->nDataStartOffset = 14922;
                poDS->nRecordSize = 14800;
	        poDS->nRecordDataStart = 448;
	        poDS->nRecordDataEnd = 14104;
		poDS->iGCPCodeOffset = 53;
		poDS->iGCPOffset = 104;
	    }
	    else if (poDS->iSpacecraftID <= NOAA17)
	    {
		poDS->nDataStartOffset = 16384;
                poDS->nRecordSize = 15872;
	        poDS->nRecordDataStart = 1264;
	        poDS->nRecordDataEnd = 14920;
		poDS->iGCPCodeOffset = 0; // XXX: not exist for NOAA15?
                poDS->iGCPOffset = 640;
	    }
	    else
		goto bad;
        break;
	case GAC:
    	    poDS->nRasterXSize = 409;
	    poDS->nBufferSize = 4092;
	    poDS->nGCPStart = 5;
	    poDS->nGCPStep = 8;
	    poDS->nGCPPerLine = 51;
	    if (poDS->iSpacecraftID <= NOAA14)
	    {
	        poDS->nDataStartOffset = 6562;
                poDS->nRecordSize = 3220;
	        poDS->nRecordDataStart = 448;
	        poDS->nRecordDataEnd = 3176;
		poDS->iGCPCodeOffset = 53;
		poDS->iGCPOffset = 104;
	    }
	    else if (poDS->iSpacecraftID <= NOAA17)
	    {
		poDS->nDataStartOffset = 9728;
                poDS->nRecordSize = 4608;
	        poDS->nRecordDataStart = 1264;
	        poDS->nRecordDataEnd = 3992; //4016;
		poDS->iGCPCodeOffset = 0; // XXX: not exist for NOAA15?
                poDS->iGCPOffset = 640;
	    }
	    else
		goto bad;
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
    {
	poDS->ProcessHeader();
    }

/* -------------------------------------------------------------------- */
/*      Get and set other important information	                        */
/* -------------------------------------------------------------------- */
    char *pszText;
    switch(poDS->iSpacecraftID)
    {
	case NOAA9:
	    pszText = "NOAA-9(F)";
	break;
	case NOAA10:
	    pszText = "NOAA-10(G)";
	break;
	case NOAA11:
	    pszText = "NOAA-11(H)";
	break;
	case NOAA12:
	    pszText = "NOAA-12(D)";
	break;
	case NOAA13:
	    pszText = "NOAA-13(I)";
	break;
	case NOAA14:
	    pszText = "NOAA-14(J)";
	break;
	case NOAA15:
	    pszText = "NOAA-15(K)";
	break;
	case NOAA16:
	    pszText = "NOAA-16(L)";
	break;
	case NOAA17:
	    pszText = "NOAA-17(M)";
	break;
	default:
	    pszText = "Unknown";
    }
    poDS->SetMetadataItem( "SATELLITE",  pszText );
    switch(poDS->iDataType)
    {
        case LAC:
	    pszText = "LAC";
	break;
        case HRPT:
	    pszText = "HRPT";
	break;
        case GAC:
	    pszText = "GAC";
	break;
	default:
	    pszText = "Unknown";
    }
    poDS->SetMetadataItem( "DATATYPE",  pszText );
    
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

