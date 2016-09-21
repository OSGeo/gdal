/******************************************************************************
 * $Id:  $
 *
 * Project:  Erdas EIR Raw Driver
 * Purpose:  Implementation of EIRDataset
 * Author:   Adam Milling, amilling@alumni.uwaterloo.ca
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2010, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "ogr_spatialref.h"
#include "rawdataset.h"

CPL_CVSID("$Id:  $");

/************************************************************************/
/* ==================================================================== */
/*              EIRDataset                                              */
/* ==================================================================== */
/************************************************************************/

class EIRDataset : public RawDataset
{
    friend class RawRasterBand;

    VSILFILE  *fpImage; // image data file
    bool   bGotTransform;
    double adfGeoTransform[6];
    bool   bHDRDirty;
    char **papszHDR;
    char **papszExtraFiles;

    void        ResetKeyValue( const char *pszKey, const char *pszValue );
    const char *GetKeyValue( const char *pszKey, const char *pszDefault = "" );

  public:
    EIRDataset();
    virtual ~EIRDataset();

    virtual CPLErr GetGeoTransform( double * padfTransform );

    virtual char **GetFileList();

    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );
};


/************************************************************************/
/* ==================================================================== */
/*                            EIRDataset                                */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            EIRDataset()                             */
/************************************************************************/

EIRDataset::EIRDataset() :
    fpImage(NULL),
    bGotTransform(false),
    bHDRDirty(false),
    papszHDR(NULL),
    papszExtraFiles(NULL)
{}

/************************************************************************/
/*                            ~EIRDataset()                            */
/************************************************************************/

EIRDataset::~EIRDataset()

{
    FlushCache();

    if( nBands > 0 && GetAccess() == GA_Update )
    {
        RawRasterBand *poBand
            = reinterpret_cast<RawRasterBand *>( GetRasterBand( 1 ) );

        int bNoDataSet = FALSE;
        const double dfNoData = poBand->GetNoDataValue(&bNoDataSet);
        if( bNoDataSet )
        {
            ResetKeyValue( "NODATA",
                           CPLString().Printf( "%.8g", dfNoData ) );
        }
    }

    if( fpImage != NULL )
        CPL_IGNORE_RET_VAL(VSIFCloseL( fpImage ));

    CSLDestroy( papszHDR );
    CSLDestroy( papszExtraFiles );
}

/************************************************************************/
/*                            GetKeyValue()                             */
/************************************************************************/

const char *EIRDataset::GetKeyValue( const char *pszKey,
                                     const char *pszDefault )

{
    for( int i = 0; papszHDR[i] != NULL; i++ )
    {
        if( EQUALN(pszKey,papszHDR[i],strlen(pszKey))
            && isspace((unsigned char)papszHDR[i][strlen(pszKey)]) )
        {
            const char *pszValue = papszHDR[i] + strlen(pszKey);
            while( isspace(static_cast<unsigned char>(*pszValue)) )
                pszValue++;

            return pszValue;
        }
    }

    return pszDefault;
}

/************************************************************************/
/*                           ResetKeyValue()                            */
/*                                                                      */
/*      Replace or add the keyword with the indicated value in the      */
/*      papszHDR list.                                                  */
/************************************************************************/

void EIRDataset::ResetKeyValue( const char *pszKey, const char *pszValue )

{
    if( strlen(pszValue) > 65 )
    {
        CPLAssert( strlen(pszValue) <= 65 );
        return;
    }

    char szNewLine[82] = { '\0' };
    snprintf( szNewLine, sizeof(szNewLine), "%-15s%s", pszKey, pszValue );

    for( int i = CSLCount(papszHDR)-1; i >= 0; i-- )
    {
        if( EQUALN(papszHDR[i],szNewLine,strlen(pszKey)+1 ) )
        {
            if( strcmp(papszHDR[i],szNewLine) != 0 )
            {
                CPLFree( papszHDR[i] );
                papszHDR[i] = CPLStrdup( szNewLine );
                bHDRDirty = true;
            }
            return;
        }
    }

    bHDRDirty = true;
    papszHDR = CSLAddString( papszHDR, szNewLine );
}


/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr EIRDataset::GetGeoTransform( double * padfTransform )

{
    if( bGotTransform )
    {
        memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
        return CE_None;
    }

    return GDALPamDataset::GetGeoTransform( padfTransform );
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **EIRDataset::GetFileList()

{
    const CPLString osPath = CPLGetPath( GetDescription() );
    const CPLString osName = CPLGetBasename( GetDescription() );

    // Main data file, etc.
    char **papszFileList = GDALPamDataset::GetFileList();

    // Header file.
    papszFileList = CSLInsertStrings( papszFileList, -1,
                                      papszExtraFiles );

    return papszFileList;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int EIRDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes < 100 )
        return FALSE;

    if( strstr((const char *) poOpenInfo->pabyHeader,
               "IMAGINE_RAW_FILE" ) == NULL )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *EIRDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify( poOpenInfo ) )
        return NULL;

    VSILFILE *fp = VSIFOpenL( poOpenInfo->pszFilename, "r" );
    if( fp == NULL )
        return NULL;

    /* header example and description

    IMAGINE_RAW_FILE // must be on first line, by itself
    WIDTH 581        // number of columns in the image
    HEIGHT 695       // number of rows in the image
    NUM_LAYERS 3     // number of spectral bands in the image; default 1
    PIXEL_FILES raw8_3n_ui_sanjack.bl // raster file
                                      // default: same name with no extension
    FORMAT BIL       // BIL BIP BSQ; default BIL
    DATATYPE U8      // U1 U2 U4 U8 U16 U32 S16 S32 F32 F64; default U8
    BYTE_ORDER       // LSB MSB; required for U16 U32 S16 S32 F32 F64
    DATA_OFFSET      // start of image data in raster file; default 0 bytes
    END_RAW_FILE     // end RAW file - stop reading

    For a true color image with three bands (R, G, B) stored using 8 bits
    for each pixel in each band, DATA_TYPE equals U8 and NUM_LAYERS equals
    3 for a total of 24 bits per pixel.

    Note that the current version of ERDAS Raw Raster Reader/Writer does
    not support the LAYER_SKIP_BYTES, RECORD_SKIP_BYTES, TILE_WIDTH and
    TILE_HEIGHT directives. Since the reader does not read the PIXEL_FILES
    directive, the reader always assumes that the raw binary file is the
    dataset, and the name of this file is the name of the header without the
    extension. Currently, the reader does not support multiple raw binary
    files in one dataset or a single file with both the header and the raw
    binary data at the same time.
    */

    int nRows = -1;
    int nCols = -1;
    int nBands = 1;
    int nSkipBytes = 0;
    int nLineCount = 0;
    GDALDataType eDataType = GDT_Byte;
    int nBits = 8;
    char chByteOrder = 'M';
    char szLayout[10] = "BIL";
    char **papszHDR = NULL;

    // default raster file: same name with no extension
    const CPLString osPath = CPLGetPath( poOpenInfo->pszFilename );
    const CPLString osName = CPLGetBasename( poOpenInfo->pszFilename );
    CPLString osRasterFilename = CPLFormCIFilename( osPath, osName, "" );

    // parse the header file
    const char *pszLine = NULL;
    while( (pszLine = CPLReadLineL( fp )) != NULL )
    {
        nLineCount++;

        if ( (nLineCount == 1) && !EQUAL(pszLine, "IMAGINE_RAW_FILE") ) {
            return NULL;
        }

        if ( (nLineCount > 50) || EQUAL(pszLine, "END_RAW_FILE") ) {
            break;
        }

        if( strlen(pszLine) > 1000 )
            break;

        papszHDR = CSLAddString( papszHDR, pszLine );

        char **papszTokens
            = CSLTokenizeStringComplex( pszLine, " \t", TRUE, FALSE );
        if( CSLCount( papszTokens ) < 2 )
        {
            CSLDestroy( papszTokens );
            continue;
        }

        if( EQUAL(papszTokens[0], "WIDTH") )
        {
            nCols = atoi(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0], "HEIGHT") )
        {
            nRows = atoi(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0], "NUM_LAYERS") )
        {
            nBands = atoi(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0], "PIXEL_FILES") )
        {
            osRasterFilename = CPLFormCIFilename( osPath, papszTokens[1], "" );
        }
        else if( EQUAL(papszTokens[0], "FORMAT") )
        {
            strncpy( szLayout, papszTokens[1], sizeof(szLayout) );
            szLayout[sizeof(szLayout)-1] = '\0';
        }
        else if( EQUAL(papszTokens[0], "DATATYPE")
                 || EQUAL(papszTokens[0], "DATA_TYPE") )
        {
            if ( EQUAL(papszTokens[1], "U1")
                 || EQUAL(papszTokens[1], "U2")
                 || EQUAL(papszTokens[1], "U4")
                 || EQUAL(papszTokens[1], "U8") ) {
                nBits = 8;
                eDataType = GDT_Byte;
            }
            else if( EQUAL(papszTokens[1], "U16") ) {
                nBits = 16;
                eDataType = GDT_UInt16;
            }
            else if( EQUAL(papszTokens[1], "U32") ) {
                nBits = 32;
                eDataType = GDT_UInt32;
            }
            else if( EQUAL(papszTokens[1], "S16") ) {
                nBits = 16;
                eDataType = GDT_Int16;
            }
            else if( EQUAL(papszTokens[1], "S32") ) {
                nBits = 32;
                eDataType = GDT_Int32;
            }
            else if( EQUAL(papszTokens[1], "F32") ) {
                nBits = 32;
                eDataType = GDT_Float32;
            }
            else if( EQUAL(papszTokens[1], "F64") ) {
                nBits = 64;
                eDataType = GDT_Float64;
            }
            else {
                CPLError(
                    CE_Failure, CPLE_NotSupported,
                    "EIR driver does not support DATATYPE %s.",
                    papszTokens[1] );
                CSLDestroy( papszTokens );
                CSLDestroy( papszHDR );
                CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));
                return NULL;
            }
        }
        else if( EQUAL(papszTokens[0], "BYTE_ORDER") )
        {
            // M for MSB, L for LSB
            chByteOrder = static_cast<char>( toupper(papszTokens[1][0]) );
        }
        else if( EQUAL(papszTokens[0],"DATA_OFFSET") )
        {
            nSkipBytes = atoi(papszTokens[1]); // TBD: is this mapping right?
        }

        CSLDestroy( papszTokens );
    }

    CPL_IGNORE_RET_VAL(VSIFCloseL( fp ));

/* -------------------------------------------------------------------- */
/*      Did we get the required keywords?  If not we return with        */
/*      this never having been considered to be a match. This isn't     */
/*      an error!                                                       */
/* -------------------------------------------------------------------- */
    if( nRows == -1 || nCols == -1 )
    {
        CSLDestroy( papszHDR );
        return NULL;
    }

    if (!GDALCheckDatasetDimensions(nCols, nRows) ||
        !GDALCheckBandCount(nBands, FALSE))
    {
        CSLDestroy( papszHDR );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CSLDestroy( papszHDR );
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The EIR driver does not support update access to existing"
                  " datasets." );
        return NULL;
    }
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    EIRDataset *poDS = new EIRDataset();

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = nCols;
    poDS->nRasterYSize = nRows;
    poDS->papszHDR = papszHDR;

/* -------------------------------------------------------------------- */
/*      Open target binary file.                                        */
/* -------------------------------------------------------------------- */
    poDS->fpImage = VSIFOpenL( osRasterFilename.c_str(), "rb" );
    if( poDS->fpImage == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to open %s: %s",
                  osRasterFilename.c_str(), VSIStrerror( errno ) );
        delete poDS;
        return NULL;
    }
    poDS->papszExtraFiles =
            CSLAddString( poDS->papszExtraFiles,
                          osRasterFilename );

    poDS->eAccess = poOpenInfo->eAccess;

/* -------------------------------------------------------------------- */
/*      Compute the line offset.                                        */
/* -------------------------------------------------------------------- */
    const int nItemSize = GDALGetDataTypeSizeBytes(eDataType);
    int nPixelOffset = 0;
    int nLineOffset = 0;
    vsi_l_offset nBandOffset = 0;

    if( EQUAL(szLayout, "BIP") )
    {
        nPixelOffset = nItemSize * nBands;
        nLineOffset = nPixelOffset * nCols;
        nBandOffset = static_cast<vsi_l_offset>(nItemSize);
    }
    else if( EQUAL(szLayout, "BSQ") )
    {
        nPixelOffset = nItemSize;
        nLineOffset = nPixelOffset * nCols;
        nBandOffset = static_cast<vsi_l_offset>(nLineOffset) * nRows;
    }
    else /* assume BIL */
    {
        nPixelOffset = nItemSize;
        nLineOffset = nItemSize * nBands * nCols;
        nBandOffset = static_cast<vsi_l_offset>(nItemSize) * nCols;
    }

    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->PamInitialize();

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->nBands = nBands;
    for( int i = 0; i < poDS->nBands; i++ )
    {
        RawRasterBand *poBand
            = new RawRasterBand( poDS, i+1, poDS->fpImage,
                                nSkipBytes + nBandOffset * i,
                                nPixelOffset, nLineOffset, eDataType,
#ifdef CPL_LSB
                                chByteOrder == 'I' || chByteOrder == 'L',
#else
                                chByteOrder == 'M',
#endif
                                nBits);

        poDS->SetBand( i+1, poBand );
    }

/* -------------------------------------------------------------------- */
/*      look for a worldfile                                            */
/* -------------------------------------------------------------------- */

    if( !poDS->bGotTransform )
        poDS->bGotTransform = CPL_TO_BOOL(
            GDALReadWorldFile( poOpenInfo->pszFilename, NULL,
                               poDS->adfGeoTransform ) );

    if( !poDS->bGotTransform )
        poDS->bGotTransform = CPL_TO_BOOL(
            GDALReadWorldFile( poOpenInfo->pszFilename, "wld",
                               poDS->adfGeoTransform ) );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return poDS;
}


/************************************************************************/
/*                         GDALRegister_EIR()                           */
/************************************************************************/

void GDALRegister_EIR()

{
    if( GDALGetDriverByName( "EIR" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "EIR" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Erdas Imagine Raw" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_various.html#EIR" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = EIRDataset::Open;
    poDriver->pfnIdentify = EIRDataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
