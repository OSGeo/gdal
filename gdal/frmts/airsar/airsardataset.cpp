/******************************************************************************
 *
 * Project:  AirSAR Reader
 * Purpose:  Implements read support for AirSAR Polarimetric data.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2009, Even Rouault <even dot rouault at spatialys.com>
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
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                              AirSARDataset                           */
/* ==================================================================== */
/************************************************************************/

class AirSARRasterBand;

class AirSARDataset final: public GDALPamDataset
{
    friend class AirSARRasterBand;

    VSILFILE    *fp;

    int         nLoadedLine;
    GByte       *pabyCompressedLine;
    double      *padfMatrix;

    int         nDataStart;
    int         nRecordLength;

    CPLErr      LoadLine(int iLine);

    static char  **ReadHeader( VSILFILE * fp, int nFileOffset,
                               const char *pszPrefix, int nMaxLines );

  public:
                AirSARDataset();
                ~AirSARDataset();

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            AirSARRasterBand                          */
/* ==================================================================== */
/************************************************************************/

class AirSARRasterBand final: public GDALPamRasterBand
{
  public:
    AirSARRasterBand( AirSARDataset *, int );
    ~AirSARRasterBand() override;

    CPLErr IReadBlock( int, int, void * ) override;
};

/* locations of stokes matrix values within padfMatrix ... same order as they
   are computed in the document. */

constexpr int M11 = 0;
constexpr int M12 = 1;
constexpr int M13 = 2;
constexpr int M14 = 3;
constexpr int M23 = 4;
constexpr int M24 = 5;
constexpr int M33 = 6;
constexpr int M34 = 7;
constexpr int M44 = 8;
constexpr int M22 = 9;

/************************************************************************/
/*                          AirSARRasterBand()                          */
/************************************************************************/

AirSARRasterBand::AirSARRasterBand( AirSARDataset *poDSIn,
                                    int nBandIn )

{
    poDS = poDSIn;
    nBand = nBandIn;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

    if( this->nBand == 2 || this->nBand == 3 || this->nBand == 5 )
        eDataType = GDT_CFloat32;
    else
        eDataType = GDT_Float32;

    switch( nBand )
    {
      case 1:
        SetMetadataItem( "POLARIMETRIC_INTERP", "Covariance_11" );
        SetDescription( "Covariance_11" );
        eDataType = GDT_CFloat32;
        break;

      case 2:
        SetMetadataItem( "POLARIMETRIC_INTERP", "Covariance_12" );
        SetDescription( "Covariance_12" );
        eDataType = GDT_CFloat32;
        break;

      case 3:
        SetMetadataItem( "POLARIMETRIC_INTERP", "Covariance_13" );
        SetDescription( "Covariance_13" );
        eDataType = GDT_CFloat32;
        break;

      case 4:
        SetMetadataItem( "POLARIMETRIC_INTERP", "Covariance_22" );
        SetDescription( "Covariance_22" );
        eDataType = GDT_CFloat32;
        break;

      case 5:
        SetMetadataItem( "POLARIMETRIC_INTERP", "Covariance_23" );
        SetDescription( "Covariance_23" );
        eDataType = GDT_CFloat32;
        break;

      case 6:
        SetMetadataItem( "POLARIMETRIC_INTERP", "Covariance_33" );
        SetDescription( "Covariance_33" );
        eDataType = GDT_CFloat32;
        break;
    }
}

/************************************************************************/
/*                         ~AirSARRasterBand()                          */
/************************************************************************/

AirSARRasterBand::~AirSARRasterBand()

{
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr AirSARRasterBand::IReadBlock( int /* nBlockXOff */,
                                     int nBlockYOff,
                                     void * pImage )
{
    float *pafLine = (float *) pImage;
    const double SQRT_2 = 1.4142135623730951;

    CPLErr eErr = ((AirSARDataset *)poDS)->LoadLine( nBlockYOff );
    if( eErr != CE_None )
        return eErr;

    double *padfMatrix = ((AirSARDataset *) poDS)->padfMatrix;

    if( nBand == 1 ) /* C11 */
    {
        for( int iPixel = 0; iPixel < nRasterXSize; iPixel++ )
        {
            double *m = padfMatrix + 10 * iPixel;

            pafLine[iPixel*2+0] = (float)(m[M11] + m[M22] + 2 * m[M12]);
            pafLine[iPixel*2+1] = 0.0;
        }
    }
    else if( nBand == 2 ) /* C12 */
    {
        for( int iPixel = 0; iPixel < nRasterXSize; iPixel++ )
        {
            double *m = padfMatrix + 10 * iPixel;

            // real
            pafLine[iPixel*2 + 0] = (float)(SQRT_2 * (m[M13] + m[M23]));

            // imaginary
            pafLine[iPixel*2 + 1] = (float)(- SQRT_2 * (m[M24] + m[M14]));
        }
    }
    else if( nBand == 3 ) /* C13 */
    {
        for( int iPixel = 0; iPixel < nRasterXSize; iPixel++ )
        {
            double *m = padfMatrix + 10 * iPixel;

            // real
            pafLine[iPixel*2 + 0] = (float)(2*m[M33] + m[M22] - m[M11]);

            // imaginary
            pafLine[iPixel*2 + 1] = (float)(-2 * m[M34]);
        }
    }
    else if( nBand == 4 ) /* C22 */
    {
        for( int iPixel = 0; iPixel < nRasterXSize; iPixel++ )
        {
            double *m = padfMatrix + 10 * iPixel;

            pafLine[iPixel*2+0] = (float)(2 * (m[M11] - m[M22]));
            pafLine[iPixel*2+1] = 0.0;
        }
    }
    else if( nBand == 5 ) /* C23 */
    {
        for( int iPixel = 0; iPixel < nRasterXSize; iPixel++ )
        {
            double *m = padfMatrix + 10 * iPixel;

            // real
            pafLine[iPixel*2 + 0] = (float)(SQRT_2 * (m[M13] - m[M23]));

            // imaginary
            pafLine[iPixel*2 + 1] = (float)(SQRT_2 * (m[M24] - m[M14]));
        }
    }
    else if( nBand == 6 ) /* C33 */
    {
        for( int iPixel = 0; iPixel < nRasterXSize; iPixel++ )
        {
            double *m = padfMatrix + 10 * iPixel;

            pafLine[iPixel*2+0] = (float)(m[M11] + m[M22] - 2 * m[M12]);
            pafLine[iPixel*2+1] = 0.0;
        }
    }

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                              AirSARDataset                           */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           AirSARDataset()                            */
/************************************************************************/

AirSARDataset::AirSARDataset() :
    fp(nullptr),
    nLoadedLine(-1),
    pabyCompressedLine(nullptr),
    padfMatrix(nullptr),
    nDataStart(0),
    nRecordLength(0)
{}

/************************************************************************/
/*                           ~AirSARDataset()                           */
/************************************************************************/

AirSARDataset::~AirSARDataset()

{
    FlushCache(true);
    CPLFree( pabyCompressedLine );
    CPLFree( padfMatrix );

    if( fp != nullptr )
    {
        VSIFCloseL( fp );
        fp = nullptr;
    }
}

/************************************************************************/
/*                              LoadLine()                              */
/************************************************************************/

CPLErr AirSARDataset::LoadLine( int iLine )

{
    if( iLine == nLoadedLine )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      allocate working buffers if we don't have them already.         */
/* -------------------------------------------------------------------- */
    if( pabyCompressedLine == nullptr )
    {
        pabyCompressedLine = (GByte *) VSI_MALLOC2_VERBOSE(nRasterXSize, 10);

        padfMatrix = (double *) VSI_MALLOC2_VERBOSE(10* sizeof(double), nRasterXSize);
        if (pabyCompressedLine == nullptr ||
            padfMatrix == nullptr)
        {
            CPLFree (pabyCompressedLine);
            CPLFree (padfMatrix);
            return CE_Failure;
        }
    }

    CPLAssert( nRecordLength == nRasterXSize * 10 );

/* -------------------------------------------------------------------- */
/*      Load raw compressed data.                                       */
/* -------------------------------------------------------------------- */
    if( VSIFSeekL( fp, nDataStart + iLine * nRecordLength, SEEK_SET ) != 0
        || ((int) VSIFReadL( pabyCompressedLine, 10, nRasterXSize, fp ))
                 != nRasterXSize )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Error reading %d bytes for line %d at offset %d.\n%s",
                  nRasterXSize * 10, iLine, nDataStart + iLine * nRecordLength,
                  VSIStrerror( errno ) );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Build stokes matrix                                             */
/* -------------------------------------------------------------------- */
    for( int iPixel = 0; iPixel < nRasterXSize; iPixel++ )
    {
        double *M = padfMatrix + 10 * iPixel;
        signed char *byte = (signed char *) pabyCompressedLine + 10*iPixel - 1;
        const double gen_fac = 1.0; // should we have a general scale factor?

        M[M11] = (byte[2] / 254.0 + 1.5) * pow(2.0,byte[1]) * gen_fac;
        M[M12] = byte[3] * M[M11] / 127.0;
        M[M13] = byte[4] * fabs((double) byte[4]) * M[M11] / (127*127);
        M[M14] = byte[5] * fabs((double) byte[5]) * M[M11] / (127*127);
        M[M23] = byte[6] * fabs((double) byte[6]) * M[M11] / (127*127);
        M[M24] = byte[7] * fabs((double) byte[7]) * M[M11] / (127*127);
        M[M33] = byte[8] * M[M11] / 127;
        M[M34] = byte[9] * M[M11] / 127;
        M[M44] = byte[10] * M[M11] / 127;
        M[M22] = M[M11] - M[M33] - M[M44];
    }

    return CE_None;
}

/************************************************************************/
/*                             ReadHeader()                             */
/*                                                                      */
/*      Read the AirSAR header.  We assume an equal sign separates      */
/*      the keyword name from the value.  If not, assume the last       */
/*      "blank delimited" word is the value and everything else is a    */
/*      keyword.                                                        */
/*                                                                      */
/*      The records are 50 characters each.  Read till we get an all    */
/*      blank record or some zero bytes.                                */
/************************************************************************/

char ** AirSARDataset::ReadHeader( VSILFILE * fp, int nFileOffset,
                                   const char *pszPrefix, int nMaxLines )

{
    char **papszHeadInfo = nullptr;
    char szLine[51];

    VSIFSeekL( fp, nFileOffset, SEEK_SET );

/* ==================================================================== */
/*      Loop collecting one line at a time.                             */
/* ==================================================================== */
    for( int iLine = 0; iLine < nMaxLines; iLine++ )
    {
/* -------------------------------------------------------------------- */
/*      Read a 50 byte header record.                                   */
/* -------------------------------------------------------------------- */
        if( VSIFReadL( szLine, 1, 50, fp ) != 50 )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Read error collecting AirSAR header." );
            CSLDestroy( papszHeadInfo );
            return nullptr;
        }

        szLine[50] = '\0';

/* -------------------------------------------------------------------- */
/*      Is it all spaces, or does it have a zero byte?                  */
/* -------------------------------------------------------------------- */
        bool bAllSpaces = true;
        bool bHasIllegalChars = false;

        for( int i = 0; i < 50; i++ )
        {
            if( szLine[i] == '\0' )
                break;

            if( szLine[i] != ' ' )
                bAllSpaces = false;

            if( ((unsigned char *) szLine)[i] > 127
                || ((unsigned char *) szLine)[i] < 10 )
                bHasIllegalChars = true;
        }

        if( bAllSpaces || bHasIllegalChars )
            break;

/* -------------------------------------------------------------------- */
/*      Find the pivot between the keyword name and value.              */
/* -------------------------------------------------------------------- */
        int iPivot = -1;

        for( int i = 0; i < 50; i++ )
        {
            if( szLine[i] == '=' )
            {
                iPivot = i;
                break;
            }
        }

        // If no "=" found, split on first double white space
        if( iPivot == -1 )
        {
            for( int i = 48; i >= 0; i-- )
            {
                if( szLine[i] == ' ' && szLine[i+1] == ' ' )
                {
                    iPivot = i;
                    break;
                }
            }
        }

        if( iPivot == -1 ) // Yikes!
        {
            CPLDebug( "AIRSAR", "No pivot in line `%s'.",
                      szLine );
            CPLAssert( iPivot != -1 );
            break;
        }

/* -------------------------------------------------------------------- */
/*      Trace ahead to the first non-white space value character.       */
/* -------------------------------------------------------------------- */
        int iValue = iPivot + 1;

        while( iValue < 50 && szLine[iValue] == ' ' )
            iValue++;

/* -------------------------------------------------------------------- */
/*      Strip any white space off the keyword.                          */
/* -------------------------------------------------------------------- */
        int iKeyEnd = iPivot - 1;

        while( iKeyEnd > 0 && szLine[iKeyEnd] == ' ' )
            iKeyEnd--;

        szLine[iKeyEnd+1] = '\0';

/* -------------------------------------------------------------------- */
/*      Convert spaces or colons into underscores in the key name.      */
/* -------------------------------------------------------------------- */
        for( int i = 0; szLine[i] != '\0'; i++ )
        {
            if( szLine[i] == ' ' || szLine[i] == ':' || szLine[i] == ',' )
                szLine[i] = '_';
        }

/* -------------------------------------------------------------------- */
/*      Prefix key name with provided prefix string.                    */
/* -------------------------------------------------------------------- */
        char szPrefixedKeyName[55];

        snprintf( szPrefixedKeyName, sizeof(szPrefixedKeyName), "%s_%s", pszPrefix, szLine );

        papszHeadInfo =
            CSLSetNameValue( papszHeadInfo, szPrefixedKeyName, szLine+iValue );
    }

    return papszHeadInfo;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *AirSARDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->fpL == nullptr || poOpenInfo->nHeaderBytes < 800 )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Check for AirSAR/ keyword.                                      */
/* -------------------------------------------------------------------- */
    if( !STARTS_WITH_CI((char *) poOpenInfo->pabyHeader, "RECORD LENGTH IN BYTES") )
        return nullptr;

    if( strstr((char *) poOpenInfo->pabyHeader, "COMPRESSED") == nullptr
        || strstr((char *) poOpenInfo->pabyHeader, "JPL AIRCRAFT") == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Parse the header fields.  We turn all the transform the         */
/*      keywords by converting spaces to underscores so they will be    */
/*      "well behaved" as metadata keywords.                            */
/* -------------------------------------------------------------------- */
    char **papszMD = ReadHeader( poOpenInfo->fpL, 0, "MH", 20 );

    if( papszMD == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The AIRSAR driver does not support update access to existing"
                  " datasets.\n" );
        CSLDestroy( papszMD );
        return nullptr;
    }
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    AirSARDataset *poDS = new AirSARDataset();

/* -------------------------------------------------------------------- */
/*      Extract some key information.                                   */
/* -------------------------------------------------------------------- */

    poDS->nRasterXSize =
        atoi(CSLFetchNameValue(papszMD,"MH_NUMBER_OF_SAMPLES_PER_RECORD"));
    poDS->nRasterYSize =
        atoi(CSLFetchNameValue(papszMD,"MH_NUMBER_OF_LINES_IN_IMAGE"));

    poDS->nRecordLength = atoi(
        CSLFetchNameValue( papszMD, "MH_RECORD_LENGTH_IN_BYTES" ) );

    poDS->nDataStart = atoi(
        CSLFetchNameValue( papszMD, "MH_BYTE_OFFSET_OF_FIRST_DATA_RECORD" ));

/* -------------------------------------------------------------------- */
/*      Adopt the openinfo file pointer.                                */
/* -------------------------------------------------------------------- */
    poDS->fp = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

/* -------------------------------------------------------------------- */
/*      Read and merge parameter header into metadata.  Prefix          */
/*      parameter header values with PH_.                               */
/* -------------------------------------------------------------------- */
    int nPHOffset = 0;

    if( CSLFetchNameValue( papszMD,
                           "MH_BYTE_OFFSET_OF_PARAMETER_HEADER" ) != nullptr )
    {
        nPHOffset = atoi(CSLFetchNameValue(
                        papszMD, "MH_BYTE_OFFSET_OF_PARAMETER_HEADER"));
        char **papszPHInfo = ReadHeader( poDS->fp, nPHOffset, "PH", 100 );

        papszMD = CSLInsertStrings( papszMD, CSLCount(papszMD), papszPHInfo );

        CSLDestroy( papszPHInfo );
    }

/* -------------------------------------------------------------------- */
/*      Read and merge calibration header into metadata.  Prefix        */
/*      parameter header values with CH_.                               */
/* -------------------------------------------------------------------- */
    if( nPHOffset != 0 )
    {
        char **papszCHInfo = ReadHeader( poDS->fp,
                                         nPHOffset+poDS->nRecordLength,
                                         "CH", 18 );

        papszMD = CSLInsertStrings( papszMD, CSLCount(papszMD), papszCHInfo );

        CSLDestroy( papszCHInfo );
    }

/* -------------------------------------------------------------------- */
/*      Assign metadata to dataset.                                     */
/* -------------------------------------------------------------------- */
    poDS->SetMetadata( papszMD );
    CSLDestroy( papszMD );

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->SetBand( 1, new AirSARRasterBand( poDS, 1 ));
    poDS->SetBand( 2, new AirSARRasterBand( poDS, 2 ));
    poDS->SetBand( 3, new AirSARRasterBand( poDS, 3 ));
    poDS->SetBand( 4, new AirSARRasterBand( poDS, 4 ));
    poDS->SetBand( 5, new AirSARRasterBand( poDS, 5 ));
    poDS->SetBand( 6, new AirSARRasterBand( poDS, 6 ));

    poDS->SetMetadataItem( "MATRIX_REPRESENTATION", "SYMMETRIZED_COVARIANCE" );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return poDS;
}

/************************************************************************/
/*                        GDALRegister_AirSAR()                         */
/************************************************************************/

void GDALRegister_AirSAR()

{
    if( GDALGetDriverByName( "AirSAR" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "AirSAR" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "AirSAR Polarimetric Image" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/airsar.html" );

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = AirSARDataset::Open;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
