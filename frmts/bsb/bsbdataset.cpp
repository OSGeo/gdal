/******************************************************************************
 * $Id$
 *
 * Project:  BSB Reader
 * Purpose:  BSBDataset implementation for BSB format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.2  2001/12/08 21:58:27  warmerda
 * added gcps
 *
 * Revision 1.1  2001/12/08 04:35:16  warmerda
 * New
 *
 */

#include "gdal_priv.h"
#include "bsb_read.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

static GDALDriver	*poBSBDriver = NULL;

CPL_C_START
void	GDALRegister_BSB(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				BSBDataset				*/
/* ==================================================================== */
/************************************************************************/

class BSBRasterBand;

class BSBDataset : public GDALDataset
{
    int         nGCPCount;
    GDAL_GCP    *pasGCPList;
    char        *pszGCPProjection;

    void        ScanForGCPs();
  public:
                BSBDataset();
		~BSBDataset();
    
    BSBInfo     *psInfo;

    static GDALDataset *Open( GDALOpenInfo * );

    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

#ifdef notdef
    CPLErr 	GetGeoTransform( double * padfTransform );
    const char *GetProjectionRef();
#endif
};

/************************************************************************/
/* ==================================================================== */
/*                            BSBRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class BSBRasterBand : public GDALRasterBand
{
    GDALColorTable	oCT;

  public:
    		BSBRasterBand( BSBDataset * );
    
    virtual CPLErr IReadBlock( int, int, void * );
    virtual GDALColorTable *GetColorTable();
    virtual GDALColorInterp GetColorInterpretation();
};


/************************************************************************/
/*                           BSBRasterBand()                            */
/************************************************************************/

BSBRasterBand::BSBRasterBand( BSBDataset *poDS )

{
    this->poDS = poDS;
    this->nBand = 1;

    eDataType = GDT_Byte;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

    for( int i = 0; i < poDS->psInfo->nPCTSize; i++ )
    {
        GDALColorEntry  oColor;

        oColor.c1 = poDS->psInfo->pabyPCT[i*3+0];
        oColor.c2 = poDS->psInfo->pabyPCT[i*3+1];
        oColor.c3 = poDS->psInfo->pabyPCT[i*3+2];
        oColor.c4 = 255;

        oCT.SetColorEntry( i, &oColor );
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr BSBRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                      void * pImage )

{
    BSBDataset *poGDS = (BSBDataset *) poDS;

    if( BSBReadScanline( poGDS->psInfo, nBlockYOff, (unsigned char *)pImage ) )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *BSBRasterBand::GetColorTable()

{
    return &oCT;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp BSBRasterBand::GetColorInterpretation()

{
    return GCI_PaletteIndex;
}

/************************************************************************/
/* ==================================================================== */
/*				BSBDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           BSBDataset()                               */
/************************************************************************/

BSBDataset::BSBDataset()

{
    psInfo = NULL;

    nGCPCount = 0;
    pasGCPList = NULL;
    pszGCPProjection = "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",7030]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",6326]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",8901]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",9108]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST],AUTHORITY[\"EPSG\",4326]]";
}

/************************************************************************/
/*                            ~BSBDataset()                             */
/************************************************************************/

BSBDataset::~BSBDataset()

{
    GDALDeinitGCPs( nGCPCount, pasGCPList );
    CPLFree( pasGCPList );

    if( psInfo != NULL )
        BSBClose( psInfo );
}


#ifdef notdef
/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr BSBDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *BSBDataset::GetProjectionRef()

{
    return pszProjection;
}
#endif

/************************************************************************/
/*                            ScanForGCPs()                             */
/************************************************************************/

void BSBDataset::ScanForGCPs()

{
#define MAX_GCP		256

    nGCPCount = 0;
    pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),MAX_GCP);

/* -------------------------------------------------------------------- */
/*      Collect standalone GCPs.  They look like:                       */
/*                                                                      */
/*      REF/1,115,2727,32.346666666667,-60.881666666667			*/
/*      REF/n,pixel,line,lat,long                                       */
/* -------------------------------------------------------------------- */
    int i;

    for( i = 0; psInfo->papszHeader[i] != NULL; i++ )
    {
        char	**papszTokens;
        char	szName[50];

        if( !EQUALN(psInfo->papszHeader[i],"REF/",4) )
            continue;

        papszTokens = 
            CSLTokenizeStringComplex( psInfo->papszHeader[i]+4, ",", 
                                      FALSE, FALSE );

        if( CSLCount(papszTokens) >= 4 )
        {
            GDALInitGCPs( 1, pasGCPList + nGCPCount );

            pasGCPList[nGCPCount].dfGCPX = atof(papszTokens[4]);
            pasGCPList[nGCPCount].dfGCPY = atof(papszTokens[3]);
            pasGCPList[nGCPCount].dfGCPPixel = atof(papszTokens[1]);
            pasGCPList[nGCPCount].dfGCPLine = atof(papszTokens[2]);

            CPLFree( pasGCPList[nGCPCount].pszId );
            if( CSLCount(papszTokens) > 5 )
            {
                pasGCPList[nGCPCount].pszId = papszTokens[5];
            }
            else
            {
                sprintf( szName, "GCP_%d", nGCPCount+1 );
                pasGCPList[nGCPCount].pszId = CPLStrdup( szName );
            }

            nGCPCount++;
        }
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *BSBDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Check for BSB/ keyword.                                         */
/* -------------------------------------------------------------------- */
    int		i;

    if( poOpenInfo->fp == NULL || poOpenInfo->nHeaderBytes < 1000 )
        return NULL;

    for( i = 0; i < poOpenInfo->nHeaderBytes - 4; i++ )
    {
        if( poOpenInfo->pabyHeader[i+0] == 'B' 
            && poOpenInfo->pabyHeader[i+1] == 'S' 
            && poOpenInfo->pabyHeader[i+2] == 'B' 
            && poOpenInfo->pabyHeader[i+3] == '/' )
            break;
    }

    if( i == poOpenInfo->nHeaderBytes - 4 )
        return NULL;


/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    BSBDataset 	*poDS;

    poDS = new BSBDataset();

    poDS->poDriver = poBSBDriver;
    
/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    poDS->psInfo = BSBOpen( poOpenInfo->pszFilename );
    if( poDS->psInfo == NULL )
    {
        delete poDS;
        return NULL;
    }

    poDS->nRasterXSize = poDS->psInfo->nXSize;
    poDS->nRasterYSize = poDS->psInfo->nYSize;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->SetBand( 1, new BSBRasterBand( poDS ));

    poDS->ScanForGCPs();
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int BSBDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *BSBDataset::GetGCPProjection()

{
    if( nGCPCount > 0 && pszGCPProjection != NULL )
        return pszGCPProjection;
    else
        return "";
}

/************************************************************************/
/*                               GetGCP()                               */
/************************************************************************/

const GDAL_GCP *BSBDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                        GDALRegister_BSB()                            */
/************************************************************************/

void GDALRegister_BSB()

{
    GDALDriver	*poDriver;

    if( poBSBDriver == NULL )
    {
        poBSBDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "BSB";
        poDriver->pszLongName = "Maptech BSB Nautical Charts";
        poDriver->pszHelpTopic = "frmt_various.html#BSB";
        
        poDriver->pfnOpen = BSBDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
