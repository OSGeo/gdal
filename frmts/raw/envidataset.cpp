/******************************************************************************
 * $Id$
 *
 * Project:  ENVI .hdr Driver
 * Purpose:  Implementation of ENVI .hdr labelled raw raster support.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
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
 * Revision 1.1  2002/03/04 21:52:48  warmerda
 * New
 *
 */

#include "rawdataset.h"
#include "ogr_spatialref.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

static GDALDriver	*poENVIDriver = NULL;

CPL_C_START
void	GDALRegister_ENVI(void);
CPL_C_END

static int anUsgsEsriZones[] =
{
  101, 3101,
  102, 3126,
  201, 3151,
  202, 3176,
  203, 3201,
  301, 3226,
  302, 3251,
  401, 3276,
  402, 3301,
  403, 3326,
  404, 3351,
  405, 3376,
  406, 3401,
  407, 3426,
  501, 3451,
  502, 3476,
  503, 3501,
  600, 3526,
  700, 3551,
  901, 3601,
  902, 3626,
  903, 3576,
 1001, 3651,
 1002, 3676,
 1101, 3701,
 1102, 3726,
 1103, 3751,
 1201, 3776,
 1202, 3801,
 1301, 3826,
 1302, 3851,
 1401, 3876,
 1402, 3901,
 1501, 3926,
 1502, 3951,
 1601, 3976,
 1602, 4001,
 1701, 4026,
 1702, 4051,
 1703, 6426,
 1801, 4076,
 1802, 4101,
 1900, 4126,
 2001, 4151,
 2002, 4176,
 2101, 4201,
 2102, 4226,
 2103, 4251,
 2111, 6351,
 2112, 6376,
 2113, 6401,
 2201, 4276,
 2202, 4301,
 2203, 4326,
 2301, 4351,
 2302, 4376,
 2401, 4401,
 2402, 4426,
 2403, 4451,
 2500,    0,
 2501, 4476,
 2502, 4501,
 2503, 4526,
 2600,    0,
 2601, 4551,
 2602, 4576,
 2701, 4601,
 2702, 4626,
 2703, 4651,
 2800, 4676,
 2900, 4701,
 3001, 4726,
 3002, 4751,
 3003, 4776,
 3101, 4801,
 3102, 4826,
 3103, 4851,
 3104, 4876,
 3200, 4901,
 3301, 4926,
 3302, 4951,
 3401, 4976,
 3402, 5001,
 3501, 5026,
 3502, 5051,
 3601, 5076,
 3602, 5101,
 3701, 5126,
 3702, 5151,
 3800, 5176,
 3900,    0,
 3901, 5201,
 3902, 5226,
 4001, 5251,
 4002, 5276,
 4100, 5301,
 4201, 5326,
 4202, 5351,
 4203, 5376,
 4204, 5401,
 4205, 5426,
 4301, 5451,
 4302, 5476,
 4303, 5501,
 4400, 5526,
 4501, 5551,
 4502, 5576,
 4601, 5601,
 4602, 5626,
 4701, 5651,
 4702, 5676,
 4801, 5701,
 4802, 5726,
 4803, 5751,
 4901, 5776,
 4902, 5801,
 4903, 5826,
 4904, 5851,
 5001, 6101,
 5002, 6126,
 5003, 6151,
 5004, 6176,
 5005, 6201,
 5006, 6226,
 5007, 6251,
 5008, 6276,
 5009, 6301,
 5010, 6326,
 5101, 5876,
 5102, 5901,
 5103, 5926,
 5104, 5951,
 5105, 5976,
 5201, 6001,
 5200, 6026,
 5200, 6076,
 5201, 6051,
 5202, 6051,
 5300,    0, 
 5400,    0
};

/************************************************************************/
/*                           ESRIToUSGSZone()                           */
/*                                                                      */
/*      Convert ESRI style state plane zones to USGS style state        */
/*      plane zones.                                                    */
/************************************************************************/

static int ESRIToUSGSZone( int nESRIZone )

{
    int		nPairs = sizeof(anUsgsEsriZones) / (2*sizeof(int));
    int		i;
    
    for( i = 0; i < nPairs; i++ )
    {
        if( anUsgsEsriZones[i*2+1] == nESRIZone )
            return anUsgsEsriZones[i*2];
    }

    return 0;
}

/************************************************************************/
/* ==================================================================== */
/*				ENVIDataset				*/
/* ==================================================================== */
/************************************************************************/

class ENVIDataset : public RawDataset
{
    FILE	*fpImage;	// image data file.

    int		bFoundMapinfo;

    double      adfGeoTransform[6];

    char	*pszProjection;

    char        **papszHeader;

    int         ReadHeader( FILE * );
    int         ProcessMapinfo( const char * );
    
    char        **SplitList( const char * );

  public:
    		ENVIDataset();
    	        ~ENVIDataset();

    virtual CPLErr GetGeoTransform( double * padfTransform );
    virtual const char *GetProjectionRef(void);

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/*                            ENVIDataset()                             */
/************************************************************************/

ENVIDataset::ENVIDataset()
{
    fpImage = NULL;
    pszProjection = CPLStrdup("");

    papszHeader = NULL;

    bFoundMapinfo = FALSE;

    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~ENVIDataset()                            */
/************************************************************************/

ENVIDataset::~ENVIDataset()

{
    if( fpImage != NULL )
        VSIFClose( fpImage );
    CPLFree( pszProjection );
    CSLDestroy( papszHeader );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *ENVIDataset::GetProjectionRef()

{
    return pszProjection;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr ENVIDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
    
    if( bFoundMapinfo )
        return CE_None;
    else
        return CE_Failure;
}


/************************************************************************/
/*                             SplitList()                              */
/*                                                                      */
/*      Split an ENVI value list into component fields, and strip       */
/*      white space.                                                    */
/************************************************************************/

char **ENVIDataset::SplitList( const char *pszCleanInput )

{
    char	**papszReturn = NULL;
    char	*pszInput = CPLStrdup(pszCleanInput);

    if( pszInput[0] != '{' )
        return NULL;

    int iChar=1;


    while( pszInput[iChar] != '}' && pszInput[iChar] != '\0' )
    {
        int iFStart=-1, iFEnd=-1;

        // Find start of token.
        iFStart = iChar;
        while( pszInput[iFStart] == ' ' )
            iFStart++;

        iFEnd = iFStart;
        while( pszInput[iFEnd] != ',' 
               && pszInput[iFEnd] != '}'
               && pszInput[iFEnd] != '\0' )
            iFEnd++;

        if( pszInput[iFEnd] == '\0' )
            break;

        iChar = iFEnd+1;
        iFEnd = iFEnd-1;

        while( iFEnd > iFStart && pszInput[iFEnd] == ' ' )
            iFEnd--;

        pszInput[iFEnd+1] = '\0';
        papszReturn = CSLAddString( papszReturn, pszInput + iFStart );
    }

    CPLFree( pszInput );

    return papszReturn;
}

/************************************************************************/
/*                           ProcessMapinfo()                           */
/*                                                                      */
/*      Extract projection, and geotransform from a mapinfo value in    */
/*      the header.                                                     */
/************************************************************************/

int ENVIDataset::ProcessMapinfo( const char *pszMapinfo )

{
    char	**papszFields;		
    int         nCount;
    OGRSpatialReference oSRS;

    papszFields = SplitList( pszMapinfo );
    nCount = CSLCount(papszFields);

    if( nCount < 7 )
    {
        CSLDestroy( papszFields );
        return FALSE;
    }

    adfGeoTransform[0] = atof(papszFields[3]);
    adfGeoTransform[1] = atof(papszFields[5]);
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = atof(papszFields[4]);
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = -atof(papszFields[6]);

    if( EQUALN(papszFields[0],"UTM",3) && nCount >= 9 )
    {
        oSRS.SetUTM( atoi(papszFields[7]), 
                     EQUAL(papszFields[8],"South") );
        oSRS.SetWellKnownGeogCS( "WGS84" );
    }
    else if( EQUALN(papszFields[0],"State Plane (NAD 27)",19)
             && nCount >= 8 )
    {
        oSRS.SetStatePlane( ESRIToUSGSZone(atoi(papszFields[7])), FALSE );
    }
    else if( EQUALN(papszFields[0],"State Plane (NAD 83)",19) 
             && nCount >= 8 )
    {
        oSRS.SetStatePlane( ESRIToUSGSZone(atoi(papszFields[7])), TRUE );
    }

    if( oSRS.GetRoot() == NULL )
        oSRS.SetLocalCS( papszFields[0] );

    if( EQUAL(papszFields[nCount-1],"units=Feet") )
    {
        oSRS.SetLinearUnits( SRS_UL_US_FOOT, atof(SRS_UL_US_FOOT_CONV) );
    }

    if( oSRS.GetRoot() != NULL )
    {
        CPLFree( pszProjection );
        pszProjection = NULL;
        oSRS.exportToWkt( &pszProjection );
    }

    return TRUE;
}

/************************************************************************/
/*                             ReadHeader()                             */
/************************************************************************/

int ENVIDataset::ReadHeader( FILE * fpHdr )

{
    char	szTestHdr[4];
    
/* -------------------------------------------------------------------- */
/*      Check that the first line says "ENVI".                          */
/* -------------------------------------------------------------------- */
    if( VSIFRead( szTestHdr, 4, 1, fpHdr ) != 1 )
        return FALSE;

    if( strncmp(szTestHdr,"ENVI",4) != 0 )
        return FALSE;

    CPLReadLine( fpHdr );

/* -------------------------------------------------------------------- */
/*      Now start forming sets of name/value pairs.                     */
/* -------------------------------------------------------------------- */
    while( TRUE )
    {
        const char *pszNewLine;
        char       *pszWorkingLine;

        pszNewLine = CPLReadLine( fpHdr );
        if( pszNewLine == NULL )
            break;

        if( strstr(pszNewLine,"=") == NULL )
            continue;

        pszWorkingLine = CPLStrdup(pszNewLine);

        // Collect additional lines if we have open sqiggly bracket.
        if( strstr(pszWorkingLine,"{") != NULL 
            && strstr(pszWorkingLine,"}") == NULL )
        {
            do { 
                pszNewLine = CPLReadLine( fpHdr );
                if( pszNewLine )
                {
                    pszWorkingLine = (char *) 
                        CPLRealloc(pszWorkingLine, 
                                 strlen(pszWorkingLine)+strlen(pszNewLine)+1);
                    strcat( pszWorkingLine, pszNewLine );
                }
            } while( pszNewLine != NULL && strstr(pszNewLine,"}") == NULL );
        }

        // Try to break input into name and value portions.  Trim whitespace.
        const char *pszValue;
        int         iEqual;

        for( iEqual = 0; 
             pszWorkingLine[iEqual] != '\0' && pszWorkingLine[iEqual] != '=';
             iEqual++ ) {}

        if( pszWorkingLine[iEqual] == '=' )
        {
            int		i;

            pszValue = pszWorkingLine + iEqual + 1;
            while( *pszValue == ' ' )
                pszValue++;
            
            pszWorkingLine[iEqual--] = '\0';
            while( iEqual > 0 && pszWorkingLine[iEqual] == ' ' )
                pszWorkingLine[iEqual--] = '\0';

            // Convert spaces in the name to underscores.
            for( i = 0; pszWorkingLine[i] != '\0'; i++ )
            {
                if( pszWorkingLine[i] == ' ' )
                    pszWorkingLine[i] = '_';
            }

            papszHeader = CSLSetNameValue( papszHeader, 
                                           pszWorkingLine, pszValue );
        }

        CPLFree( pszWorkingLine );
    }

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ENVIDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		i, bSelectedHDR;
    char	*pszHDRFilename;
    
/* -------------------------------------------------------------------- */
/*	We assume the user is pointing to the binary (ie. .bil) file.	*/
/* -------------------------------------------------------------------- */
    if( poOpenInfo->fp == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Now we need to tear apart the filename to form a .HDR           */
/*      filename.                                                       */
/* -------------------------------------------------------------------- */
    pszHDRFilename = (char *) CPLMalloc(strlen(poOpenInfo->pszFilename)+5);
    strcpy( pszHDRFilename, poOpenInfo->pszFilename );;

    for( i = strlen(pszHDRFilename)-1; i > 0; i-- )
    {
        if( pszHDRFilename[i] == '.' )
        {
            pszHDRFilename[i] = '\0';
            break;
        }
    }

    strcat( pszHDRFilename, ".hdr" );

    bSelectedHDR = EQUAL(pszHDRFilename,poOpenInfo->pszFilename);

/* -------------------------------------------------------------------- */
/*      Do we have a .hdr file?                                         */
/* -------------------------------------------------------------------- */
    FILE	*fp;

    fp = VSIFOpen( pszHDRFilename, "r" );
    if( fp == NULL )
    {
        strcpy( pszHDRFilename + strlen(pszHDRFilename)-4, ".HDR" );
        fp = VSIFOpen( pszHDRFilename, "r" );
    }

    CPLFree( pszHDRFilename );
    
    if( fp == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    ENVIDataset 	*poDS;

    poDS = new ENVIDataset();

    poDS->poDriver = poENVIDriver;

/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    if( !poDS->ReadHeader( fp ) )
    {
        delete poDS;
        VSIFClose( fp );
        return FALSE;
    }

    VSIFClose( fp );

/* -------------------------------------------------------------------- */
/*      Has the user selected the .hdr file to open?                    */
/* -------------------------------------------------------------------- */
    if( bSelectedHDR )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "The selected file is an ENVI header file, but to\n"
                  "open ENVI datasets, the data file should be selected\n"
                  "instead of the .hdr file.  Please try again selecting\n"
                  "the data file corresponding to the header file:\n"
                  "  %s\n", 
                  poOpenInfo->pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Extract required values from the .hdr.                          */
/* -------------------------------------------------------------------- */
    int	nLines = 0, nSamples = 0, nBands = 0, nHeaderSize = 0;
    const char   *pszInterleave = NULL;

    if( CSLFetchNameValue(poDS->papszHeader,"lines") )
        nLines = atoi(CSLFetchNameValue(poDS->papszHeader,"lines"));

    if( CSLFetchNameValue(poDS->papszHeader,"samples") )
        nSamples = atoi(CSLFetchNameValue(poDS->papszHeader,"samples"));

    if( CSLFetchNameValue(poDS->papszHeader,"bands") )
        nBands = atoi(CSLFetchNameValue(poDS->papszHeader,"bands"));

    pszInterleave = CSLFetchNameValue(poDS->papszHeader,"interleave");

    if( nLines == 0 || nSamples == 0 || nBands == 0 || pszInterleave == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "The file appears to have an associated ENVI header, but\n"
                  "one or more of the samples, lines, bands and interleave\n"
                  "keywords appears to be missing." );
        return NULL;
    }

    if( CSLFetchNameValue(poDS->papszHeader,"header_offset") )
        nHeaderSize = atoi(CSLFetchNameValue(poDS->papszHeader,"header_offset"));

/* -------------------------------------------------------------------- */
/*      Translate the datatype.                                         */
/* -------------------------------------------------------------------- */
    GDALDataType	eType = GDT_Byte;

    if( CSLFetchNameValue(poDS->papszHeader,"data_type" ) != NULL )
    {
        switch( atoi(CSLFetchNameValue(poDS->papszHeader,"data_type" )) )
        {
          case 1:
            eType = GDT_Byte;
            break;

          case 2:
            eType = GDT_Int16;
            break;

          case 4:
            eType = GDT_Float32;
            break;

          case 12:
            eType = GDT_UInt16;
            break;

          default:
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "The file has a 'data type' value of '%s'.  This value\n"
                      "isn't recognised by the GDAL ENVI driver.",
                      CSLFetchNameValue(poDS->papszHeader,"data_type" ) );
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Translate the byte order.					*/
/* -------------------------------------------------------------------- */
    int		bNativeOrder = TRUE;

    if( CSLFetchNameValue(poDS->papszHeader,"data_type" ) != NULL )
    {
#ifdef CPL_LSB                               
        bNativeOrder = atoi(CSLFetchNameValue(poDS->papszHeader,
                                              "data_type" )) == 1;
#else
        bNativeOrder = atoi(CSLFetchNameValue(poDS->papszHeader,
                                              "data_type" )) != 1;
#endif
    }

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = nSamples;
    poDS->nRasterYSize = nLines;

/* -------------------------------------------------------------------- */
/*      Assume ownership of the file handled from the GDALOpenInfo.     */
/* -------------------------------------------------------------------- */
    poDS->fpImage = poOpenInfo->fp;
    poOpenInfo->fp = NULL;

/* -------------------------------------------------------------------- */
/*      Compute the line offset.                                        */
/* -------------------------------------------------------------------- */
    int	nDataSize = GDALGetDataTypeSize(eType)/8;
    int nPixelOffset, nLineOffset;
    vsi_l_offset nBandOffset;
    
    if( EQUAL(pszInterleave,"bsq") )
    {
        nLineOffset = nDataSize * nSamples;
        nPixelOffset = nDataSize;
        nBandOffset = nLineOffset * nLines;
    }
    else if( EQUAL(pszInterleave,"bil") )
    {
        nLineOffset = nDataSize * nSamples * nBands;
        nPixelOffset = nDataSize;
        nBandOffset = nDataSize * nSamples;
    }
    else if( EQUAL(pszInterleave,"bip") )
    {
        nLineOffset = nDataSize * nSamples * nBands;
        nPixelOffset = nDataSize * nBands;
        nBandOffset = nDataSize;
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "The interleaving type of the file (%s) is not supported.",
                  pszInterleave );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->nBands = nBands;
    for( i = 0; i < poDS->nBands; i++ )
    {
        RawRasterBand	*poBand;

        poBand = 
            new RawRasterBand( poDS, i+1, poDS->fpImage,
                               nHeaderSize + nBandOffset * i,
                               nPixelOffset, nLineOffset, eType, bNativeOrder);

        poDS->SetBand( i+1, poBand );
    }

/* -------------------------------------------------------------------- */
/*      Apply band names if we have them.                               */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue( poDS->papszHeader, "band_names" ) != NULL )
    {
        char	**papszBandNames = 
            poDS->SplitList( CSLFetchNameValue( poDS->papszHeader, 
                                                "band_names" ) );

        for( i = 0; i < MIN(CSLCount(papszBandNames),nBands); i++ )
            poDS->GetRasterBand(i+1)->SetDescription( papszBandNames[i] );
    }
    
    
/* -------------------------------------------------------------------- */
/*      Look for mapinfo						*/
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue( poDS->papszHeader, "map_info" ) != NULL )
    {
        poDS->bFoundMapinfo = 
            poDS->ProcessMapinfo( 
                CSLFetchNameValue(poDS->papszHeader,"map_info") );
    }
    
/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                         GDALRegister_ENVI()                          */
/************************************************************************/

void GDALRegister_ENVI()

{
    GDALDriver	*poDriver;

    if( poENVIDriver == NULL )
    {
        poENVIDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "ENVI";
        poDriver->pszLongName = "ENVI .hdr Labelled";
        poDriver->pszHelpTopic = "frmt_various.html#ENVI";
        
        poDriver->pfnOpen = ENVIDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

