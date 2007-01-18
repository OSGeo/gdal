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
 ****************************************************************************/

#include "gdal_pam.h"
#include "bsb_read.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_BSB(void);
CPL_C_END

// Define for BSB support, disable now since it doesn't really work.
#undef BSB_CREATE

/************************************************************************/
/* ==================================================================== */
/*				BSBDataset				*/
/* ==================================================================== */
/************************************************************************/

class BSBRasterBand;

class BSBDataset : public GDALPamDataset
{
    int         nGCPCount;
    GDAL_GCP    *pasGCPList;
    char        *pszGCPProjection;

    double      adfGeoTransform[6];
    int         bGeoTransformSet;

    void        ScanForGCPs( bool isNos, const char *pszFilename );
    void        ScanForGCPsNos( const char *pszFilename );
    void        ScanForGCPsBSB();
  public:
                BSBDataset();
		~BSBDataset();
    
    BSBInfo     *psInfo;

    static GDALDataset *Open( GDALOpenInfo * );

    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

    CPLErr 	GetGeoTransform( double * padfTransform );
    const char *GetProjectionRef();
};

/************************************************************************/
/* ==================================================================== */
/*                            BSBRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class BSBRasterBand : public GDALPamRasterBand
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

    // Note that the first color table entry is dropped, everything is
    // shifted down.
    for( int i = 0; i < poDS->psInfo->nPCTSize-1; i++ )
    {
        GDALColorEntry  oColor;

        oColor.c1 = poDS->psInfo->pabyPCT[i*3+0+3];
        oColor.c2 = poDS->psInfo->pabyPCT[i*3+1+3];
        oColor.c3 = poDS->psInfo->pabyPCT[i*3+2+3];
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
    GByte *pabyScanline = (GByte*) pImage;

    if( BSBReadScanline( poGDS->psInfo, nBlockYOff, pabyScanline ) )
    {
        for( int i = 0; i < nBlockXSize; i++ )
            pabyScanline[i] -= 1;

        return CE_None;
    }
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

    bGeoTransformSet = FALSE;

    nGCPCount = 0;
    pasGCPList = NULL;
    pszGCPProjection = "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",7030]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",6326]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",8901]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",9108]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST],AUTHORITY[\"EPSG\",4326]]";

    adfGeoTransform[0] = 0.0;     /* X Origin (top left corner) */
    adfGeoTransform[1] = 1.0;     /* X Pixel size */
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;     /* Y Origin (top left corner) */
    adfGeoTransform[4] = 0.0;     
    adfGeoTransform[5] = 1.0;     /* Y Pixel Size */

}

/************************************************************************/
/*                            ~BSBDataset()                             */
/************************************************************************/

BSBDataset::~BSBDataset()

{
    FlushCache();

    GDALDeinitGCPs( nGCPCount, pasGCPList );
    CPLFree( pasGCPList );

    if( psInfo != NULL )
        BSBClose( psInfo );
}


/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr BSBDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
    
    if( bGeoTransformSet )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *BSBDataset::GetProjectionRef()

{
    if( bGeoTransformSet )
        return pszGCPProjection;
    else
        return "";
}

/************************************************************************/
/*                            ScanForGCPs( isNos, *pszFilename )        */
/************************************************************************/

void BSBDataset::ScanForGCPs( bool isNos, const char *pszFilename )

{
#define MAX_GCP		256

    nGCPCount = 0;
    pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),MAX_GCP);

    if ( isNos )
    {
        ScanForGCPsNos(pszFilename);
    } else {
        ScanForGCPsBSB();
    }

/* -------------------------------------------------------------------- */
/*      Attempt to prepare a geotransform from the GCPs.                */
/* -------------------------------------------------------------------- */
    if( GDALGCPsToGeoTransform( nGCPCount, pasGCPList, adfGeoTransform, 
                                FALSE ) )
    {
        bGeoTransformSet = TRUE;
    }
}

/************************************************************************/
/*                            ScanForGCPsNos( pszFilename )             */
/* Nos files have an accompanying .geo file, that contains some of the  */
/* information normally contained in the header section with BSB files. */
/* we try and open a file with the same name, but a .geo extension, and */
/* look for lines like...                                               */
/*   PointX=long lat line pixel    (using the same naming system as BSB)*/
/*   Point1=-22.0000 64.250000 197 744                                  */
/************************************************************************/

void BSBDataset::ScanForGCPsNos( const char *pszFilename )
{
    char **Tokens;
    const char *geofile;
    const char *extension;

    extension = CPLGetExtension(pszFilename);

    // pseudointelligently try and guess whether we want a .geo or a .GEO
    if (extension[1] == 'O')
    {
        geofile = CPLResetExtension( pszFilename, "GEO");
    } else {
        geofile = CPLResetExtension( pszFilename, "geo");
    }

    FILE *gfp = VSIFOpen( geofile, "r" );  // Text files
    if( gfp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Couldn't find a matching .GEO file: %s", geofile );
        return;
    }

    char *thisLine = (char *) CPLMalloc( 80 ); // FIXME
    while (fgets(thisLine, 80, gfp))
    {
        if( EQUALN(thisLine, "Point", 5) )
        {
            // got a point line, turn it into a gcp
            Tokens = CSLTokenizeStringComplex(thisLine, "=", FALSE, FALSE);
            Tokens = CSLTokenizeStringComplex(Tokens[1], " ", FALSE, FALSE);

            GDALInitGCPs( 1, pasGCPList + nGCPCount );
            pasGCPList[nGCPCount].dfGCPX = atof(Tokens[0]);
            pasGCPList[nGCPCount].dfGCPY = atof(Tokens[1]);
            pasGCPList[nGCPCount].dfGCPPixel = atof(Tokens[3]);
            pasGCPList[nGCPCount].dfGCPLine = atof(Tokens[2]);

            CPLFree( pasGCPList[nGCPCount].pszId );
            char	szName[50];
            sprintf( szName, "GCP_%d", nGCPCount+1 );
            pasGCPList[nGCPCount].pszId = CPLStrdup( szName );

            nGCPCount++;
        }
    }

    CPLFree(thisLine);
    VSIFClose(gfp);
}


/************************************************************************/
/*                            ScanForGCPsBSB()                          */
/************************************************************************/

void BSBDataset::ScanForGCPsBSB()
{
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
        CSLDestroy( papszTokens );
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
    bool        isNos = false;

    if( poOpenInfo->nHeaderBytes < 1000 )
        return NULL;

    for( i = 0; i < poOpenInfo->nHeaderBytes - 4; i++ )
    {
        if( poOpenInfo->pabyHeader[i+0] == 'B' 
            && poOpenInfo->pabyHeader[i+1] == 'S' 
            && poOpenInfo->pabyHeader[i+2] == 'B' 
            && poOpenInfo->pabyHeader[i+3] == '/' )
            break;
        if( poOpenInfo->pabyHeader[i+0] == 'N' 
            && poOpenInfo->pabyHeader[i+1] == 'O' 
            && poOpenInfo->pabyHeader[i+2] == 'S' 
            && poOpenInfo->pabyHeader[i+3] == '/' )
        {
            isNos = true;
            break;
        }
        if( poOpenInfo->pabyHeader[i+0] == 'W' 
            && poOpenInfo->pabyHeader[i+1] == 'X' 
            && poOpenInfo->pabyHeader[i+2] == '\\' 
            && poOpenInfo->pabyHeader[i+3] == '8' )
            break;
    }

    if( i == poOpenInfo->nHeaderBytes - 4 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    BSBDataset 	*poDS;

    poDS = new BSBDataset();

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

    poDS->ScanForGCPs( isNos, poOpenInfo->pszFilename );
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

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

#ifdef BSB_CREATE
/************************************************************************/
/*                           BSBCreateCopy()                            */
/************************************************************************/

static GDALDataset *
BSBCreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
               int bStrict, char ** papszOptions, 
               GDALProgressFunc pfnProgress, void * pProgressData )

{
    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();

/* -------------------------------------------------------------------- */
/*      Some some rudimentary checks                                    */
/* -------------------------------------------------------------------- */
    if( nBands != 1 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "BSB driver only supports one band images.\n" );

        return NULL;
    }

    if( poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte 
        && bStrict )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "BSB driver doesn't support data type %s. "
                  "Only eight bit bands supported.\n", 
                  GDALGetDataTypeName( 
                      poSrcDS->GetRasterBand(1)->GetRasterDataType()) );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open the output file.                                           */
/* -------------------------------------------------------------------- */
    BSBInfo *psBSB;

    psBSB = BSBCreate( pszFilename, 0, 200, nXSize, nYSize );
    if( psBSB == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Prepare initial color table.colortable.                         */
/* -------------------------------------------------------------------- */
    GDALRasterBand	*poBand = poSrcDS->GetRasterBand(1);
    int			iColor;
    unsigned char       abyPCT[771];
    int                 nPCTSize;
    int                 anRemap[256];

    abyPCT[0] = 0;
    abyPCT[1] = 0;
    abyPCT[2] = 0;

    if( poBand->GetColorTable() == NULL )
    {
        /* map greyscale down to 63 grey levels. */
        for( iColor = 0; iColor < 256; iColor++ )
        {
            int nOutValue = (int) (iColor / 4.1) + 1;

            anRemap[iColor] = nOutValue;
            abyPCT[nOutValue*3 + 0] = (unsigned char) iColor;
            abyPCT[nOutValue*3 + 1] = (unsigned char) iColor;
            abyPCT[nOutValue*3 + 2] = (unsigned char) iColor;
        }
        nPCTSize = 64;
    }
    else
    {
        GDALColorTable	*poCT = poBand->GetColorTable();

        nPCTSize = poCT->GetColorEntryCount();
        for( iColor = 0; iColor < nPCTSize; iColor++ )
        {
            GDALColorEntry	sEntry;

            poCT->GetColorEntryAsRGB( iColor, &sEntry );
            
            anRemap[iColor] = (unsigned char) (iColor + 1);
            abyPCT[(iColor+1)*3 + 0] = (unsigned char) sEntry.c1;
            abyPCT[(iColor+1)*3 + 1] = (unsigned char) sEntry.c2;
            abyPCT[(iColor+1)*3 + 2] = (unsigned char) sEntry.c3;
        }

        // Add entries for pixel values which apparently will not occur.
        for( iColor = nPCTSize; iColor < 256; iColor++ )
            anRemap[iColor] = 1;
    }

/* -------------------------------------------------------------------- */
/*      Boil out all duplicate entries.                                 */
/* -------------------------------------------------------------------- */
    int  i;

    for( i = 1; i < nPCTSize-1; i++ )
    {
        int  j;

        for( j = i+1; j < nPCTSize; j++ )
        {
            if( abyPCT[i*3+0] == abyPCT[j*3+0] 
                && abyPCT[i*3+1] == abyPCT[j*3+1] 
                && abyPCT[i*3+2] == abyPCT[j*3+2] )
            {
                int   k;

                nPCTSize--;
                abyPCT[j*3+0] = abyPCT[nPCTSize*3+0];
                abyPCT[j*3+1] = abyPCT[nPCTSize*3+1];
                abyPCT[j*3+2] = abyPCT[nPCTSize*3+2];

                for( k = 0; k < 256; k++ )
                {
                    // merge matching entries.
                    if( anRemap[k] == j )
                        anRemap[k] = i;

                    // shift the last PCT entry into the new hole.
                    if( anRemap[k] == nPCTSize )
                        anRemap[k] = j;
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Boil out all duplicate entries.                                 */
/* -------------------------------------------------------------------- */
    if( nPCTSize > 128 )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Having to merge color table entries to reduce %d real\n"
                  "color table entries down to 127 values.", 
                  nPCTSize );
    }

    while( nPCTSize > 127 )
    {
        int nBestRange = 768;
        int iBestMatch1=-1, iBestMatch2=-1;

        // Find the closest pair of color table entries.

        for( i = 1; i < nPCTSize-1; i++ )
        {
            int  j;
            
            for( j = i+1; j < nPCTSize; j++ )
            {
                int nRange = ABS(abyPCT[i*3+0] - abyPCT[j*3+0])
                    + ABS(abyPCT[i*3+1] - abyPCT[j*3+1])
                    + ABS(abyPCT[i*3+2] - abyPCT[j*3+2]);

                if( nRange < nBestRange )
                {
                    iBestMatch1 = i;
                    iBestMatch2 = j;
                    nBestRange = nRange;
                }
            }
        }

        // Merge the second entry into the first. 
        nPCTSize--;
        abyPCT[iBestMatch2*3+0] = abyPCT[nPCTSize*3+0];
        abyPCT[iBestMatch2*3+1] = abyPCT[nPCTSize*3+1];
        abyPCT[iBestMatch2*3+2] = abyPCT[nPCTSize*3+2];

        for( i = 0; i < 256; i++ )
        {
            // merge matching entries.
            if( anRemap[i] == iBestMatch2 )
                anRemap[i] = iBestMatch1;
            
            // shift the last PCT entry into the new hole.
            if( anRemap[i] == nPCTSize )
                anRemap[i] = iBestMatch2;
        }
    }

/* -------------------------------------------------------------------- */
/*      Write the PCT.                                                  */
/* -------------------------------------------------------------------- */
    if( !BSBWritePCT( psBSB, nPCTSize, abyPCT ) )
    {
        BSBClose( psBSB );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Loop over image, copying image data.                            */
/* -------------------------------------------------------------------- */
    GByte 	*pabyScanline;
    CPLErr      eErr = CE_None;

    pabyScanline = (GByte *) CPLMalloc( nXSize );

    for( int iLine = 0; iLine < nYSize && eErr == CE_None; iLine++ )
    {
        eErr = poBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                 pabyScanline, nXSize, 1, GDT_Byte,
                                 nBands, nBands * nXSize );
        if( eErr == CE_None )
        {
            for( i = 0; i < nXSize; i++ )
                pabyScanline[i] = (GByte) anRemap[pabyScanline[i]];

            if( !BSBWriteScanline( psBSB, pabyScanline ) )
                eErr = CE_Failure;
        }
    }

    CPLFree( pabyScanline );

/* -------------------------------------------------------------------- */
/*      cleanup                                                         */
/* -------------------------------------------------------------------- */
    BSBClose( psBSB );

    if( eErr != CE_None )
    {
        VSIUnlink( pszFilename );
        return NULL;
    }
    else
        return (GDALDataset *) GDALOpen( pszFilename, GA_ReadOnly );
}
#endif

/************************************************************************/
/*                        GDALRegister_BSB()                            */
/************************************************************************/

void GDALRegister_BSB()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "BSB" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "BSB" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Maptech BSB Nautical Charts" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#BSB" );
#ifdef BSB_CREATE
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, 
"<CreationOptionList>\n"
"   <Option name='NA' type='string'/>\n"
"</CreationOptionList>\n" );
#endif
        poDriver->pfnOpen = BSBDataset::Open;
#ifdef BSB_CREATE
        poDriver->pfnCreateCopy = BSBCreateCopy;
#endif

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
