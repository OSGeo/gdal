/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implements the Golden Software ASCII Grid Format.
 * Author:   Kevin Locke, kwl7@cornell.edu
 *           (Based largely on aaigriddataset.cpp by Frank Warmerdam)
 *
 ******************************************************************************
 * Copyright (c) 2006, Kevin Locke <kwl7@cornell.edu>
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#include <sstream>
#include <float.h>
#include <limits.h>
#include <assert.h>

#include "gdal_frmts.h"
#include "gdal_pam.h"

CPL_CVSID("$Id$")

#ifndef DBL_MAX
# ifdef __DBL_MAX__
#  define DBL_MAX __DBL_MAX__
# else
#  define DBL_MAX 1.7976931348623157E+308
# endif /* __DBL_MAX__ */
#endif /* DBL_MAX */

#ifndef INT_MAX
# define INT_MAX 2147483647
#endif /* INT_MAX */

/************************************************************************/
/* ==================================================================== */
/*                              GSAGDataset                             */
/* ==================================================================== */
/************************************************************************/

class GSAGRasterBand;

class GSAGDataset : public GDALPamDataset
{
    friend class GSAGRasterBand;

    static const double dfNODATA_VALUE;
    static const int nFIELD_PRECISION;
    static const size_t nMAX_HEADER_SIZE;

    static CPLErr ShiftFileContents( VSILFILE *, vsi_l_offset, int, const char * );

    VSILFILE    *fp;
    size_t       nMinMaxZOffset;
    char         szEOL[3];

    CPLErr UpdateHeader();

  public:
    explicit     GSAGDataset( const char *pszEOL = "\x0D\x0A" );
                ~GSAGDataset();

    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *CreateCopy( const char *pszFilename,
                                    GDALDataset *poSrcDS,
                                    int bStrict, char **papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void *pProgressData );

    CPLErr GetGeoTransform( double *padfGeoTransform ) override;
    CPLErr SetGeoTransform( double *padfGeoTransform ) override;
};

/* NOTE:  This is not mentioned in the spec, but Surfer 8 uses this value */
const double GSAGDataset::dfNODATA_VALUE = 1.70141E+38;

const int GSAGDataset::nFIELD_PRECISION = 14;
const size_t GSAGDataset::nMAX_HEADER_SIZE = 200;

/************************************************************************/
/* ==================================================================== */
/*                            GSAGRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class GSAGRasterBand : public GDALPamRasterBand
{
    friend class GSAGDataset;

    double dfMinX;
    double dfMaxX;
    double dfMinY;
    double dfMaxY;
    double dfMinZ;
    double dfMaxZ;

    vsi_l_offset *panLineOffset;
    int nLastReadLine;
    size_t nMaxLineSize;

    double *padfRowMinZ;
    double *padfRowMaxZ;
    int nMinZRow;
    int nMaxZRow;

    CPLErr ScanForMinMaxZ();

  public:

                GSAGRasterBand( GSAGDataset *, int, vsi_l_offset );
                ~GSAGRasterBand();

    CPLErr IReadBlock( int, int, void * ) override;
    CPLErr IWriteBlock( int, int, void * ) override;

    double GetNoDataValue( int *pbSuccess = NULL ) override;
    double GetMinimum( int *pbSuccess = NULL ) override;
    double GetMaximum( int *pbSuccess = NULL ) override;
};

/************************************************************************/
/*                            AlmostEqual()                             */
/* This function is needed because in release mode "1.70141E+38" is not */
/* parsed as 1.70141E+38 in the last bit of the mantissa.               */
/* See http://gcc.gnu.org/ml/gcc/2003-08/msg01195.html for some         */
/* explanation.                                                         */
/************************************************************************/
    
static bool AlmostEqual( double dfVal1, double dfVal2 )

{
    const double dfTOLERANCE = 0.0000000001;
    if( dfVal1 == 0.0 || dfVal2 == 0.0 )
        return fabs(dfVal1 - dfVal2) < dfTOLERANCE;
    return fabs((dfVal1 - dfVal2)/dfVal1) < dfTOLERANCE;
}

/************************************************************************/
/*                           GSAGRasterBand()                           */
/************************************************************************/

GSAGRasterBand::GSAGRasterBand( GSAGDataset *poDSIn, int nBandIn,
                                vsi_l_offset nDataStart ) :
    dfMinX(0.0),
    dfMaxX(0.0),
    dfMinY(0.0),
    dfMaxY(0.0),
    dfMinZ(0.0),
    dfMaxZ(0.0),
    panLineOffset(NULL),
    nLastReadLine(poDSIn->nRasterYSize),
    nMaxLineSize(128),
    padfRowMinZ(NULL),
    padfRowMaxZ(NULL),
    nMinZRow(-1),
    nMaxZRow(-1)
{
    poDS = poDSIn;
    nBand = nBandIn;

    eDataType = GDT_Float64;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

    if( poDSIn->nRasterYSize > 1000000 )
    {
        // Sanity check to avoid excessive memory allocations
        VSIFSeekL( poDSIn->fp, 0, SEEK_END );
        vsi_l_offset nFileSize = VSIFTellL(poDSIn->fp);
        if( static_cast<vsi_l_offset>(poDSIn->nRasterYSize) > nFileSize )
        {
            CPLError(CE_Failure, CPLE_FileIO, "Truncated file");
            return;
        }
    }
    panLineOffset = static_cast<vsi_l_offset *>(
        VSI_CALLOC_VERBOSE( poDSIn->nRasterYSize+1, sizeof(vsi_l_offset) ));
    if( panLineOffset == NULL )
    {
        return;
    }

    panLineOffset[poDSIn->nRasterYSize - 1] = nDataStart;
}

/************************************************************************/
/*                          ~GSAGRasterBand()                           */
/************************************************************************/

GSAGRasterBand::~GSAGRasterBand()
{
    CPLFree( panLineOffset );
    CPLFree( padfRowMinZ );
    CPLFree( padfRowMaxZ );
}

/************************************************************************/
/*                           ScanForMinMaxZ()                           */
/************************************************************************/

CPLErr GSAGRasterBand::ScanForMinMaxZ()

{
    double *padfRowValues = (double *)VSI_MALLOC2_VERBOSE( nBlockXSize, sizeof(double) );
    if( padfRowValues == NULL )
    {
        return CE_Failure;
    }

    double dfNewMinZ = DBL_MAX;
    double dfNewMaxZ = -DBL_MAX;
    int nNewMinZRow = 0;
    int nNewMaxZRow = 0;

    /* Since we have to scan, lets calc. statistics too */
    double dfSum = 0.0;
    double dfSum2 = 0.0;
    unsigned long nValuesRead = 0;
    for( int iRow=0; iRow<nRasterYSize; iRow++ )
    {
        CPLErr eErr = IReadBlock( 0, iRow, padfRowValues );
        if( eErr != CE_None )
        {
            VSIFree( padfRowValues );
            return eErr;
        }

        padfRowMinZ[iRow] = DBL_MAX;
        padfRowMaxZ[iRow] = -DBL_MAX;
        for( int iCell=0; iCell<nRasterXSize; iCell++ )
        {
            if( AlmostEqual(padfRowValues[iCell], GSAGDataset::dfNODATA_VALUE) )
                continue;

            if( padfRowValues[iCell] < padfRowMinZ[iRow] )
                padfRowMinZ[iRow] = padfRowValues[iCell];

            if( padfRowValues[iCell] > padfRowMaxZ[iRow] )
                padfRowMaxZ[iRow] = padfRowValues[iCell];

            dfSum += padfRowValues[iCell];
            dfSum2 += padfRowValues[iCell] * padfRowValues[iCell];
            nValuesRead++;
        }

        if( padfRowMinZ[iRow] < dfNewMinZ )
        {
            dfNewMinZ = padfRowMinZ[iRow];
            nNewMinZRow = iRow;
        }

        if( padfRowMaxZ[iRow] > dfNewMaxZ )
        {
            dfNewMaxZ = padfRowMaxZ[iRow];
            nNewMaxZRow = iRow;
        }
    }

    VSIFree( padfRowValues );

    if( nValuesRead == 0 )
    {
        dfMinZ = 0.0;
        dfMaxZ = 0.0;
        nMinZRow = 0;
        nMaxZRow = 0;
        return CE_None;
    }

    dfMinZ = dfNewMinZ;
    dfMaxZ = dfNewMaxZ;
    nMinZRow = nNewMinZRow;
    nMaxZRow = nNewMaxZRow;

    double dfMean = dfSum / nValuesRead;
    double dfStdDev = sqrt((dfSum2 / nValuesRead) - (dfMean * dfMean));
    SetStatistics( dfMinZ, dfMaxZ, dfMean, dfStdDev );

    return CE_None;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GSAGRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                   void * pImage )
{
    GSAGDataset *poGDS = (GSAGDataset *)poDS;
    assert( poGDS != NULL );

    double *pdfImage = (double *)pImage;

    if( nBlockYOff < 0 || nBlockYOff > nRasterYSize - 1 || nBlockXOff != 0 )
        return CE_Failure;

    if( panLineOffset[nBlockYOff] == 0 )
    {
        // Discover the last read block
        for ( int iFoundLine = nLastReadLine - 1; iFoundLine > nBlockYOff; iFoundLine--)
        {
            if( IReadBlock( nBlockXOff, iFoundLine, NULL) != CE_None )
                return CE_Failure;
        }
    }

    if( panLineOffset[nBlockYOff] == 0 )
        return CE_Failure;
    if( VSIFSeekL( poGDS->fp, panLineOffset[nBlockYOff], SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Can't seek to offset %ld to read grid row %d.",
                  (long) panLineOffset[nBlockYOff], nBlockYOff );
        return CE_Failure;
    }

    size_t nLineBufSize = nMaxLineSize;
    /* If we know the offsets, we can just read line directly */
    if( (nBlockYOff > 0) && ( panLineOffset[nBlockYOff-1] != 0 ) )
    {
        assert(panLineOffset[nBlockYOff-1] > panLineOffset[nBlockYOff]);
        nLineBufSize = (size_t) (panLineOffset[nBlockYOff-1]
                                 - panLineOffset[nBlockYOff] + 1);
    }

    char *szLineBuf = (char *)VSI_MALLOC_VERBOSE( nLineBufSize );
    if( szLineBuf == NULL )
    {
        return CE_Failure;
    }

    size_t nCharsRead = VSIFReadL( szLineBuf, 1, nLineBufSize - 1, poGDS->fp );
    if( nCharsRead == 0 )
    {
        VSIFree( szLineBuf );
        CPLError( CE_Failure, CPLE_FileIO,
                  "Can't read grid row %d at offset %ld.\n",
                  nBlockYOff, (long) panLineOffset[nBlockYOff] );
        return CE_Failure;
    }
    szLineBuf[nCharsRead] = '\0';

    size_t nCharsExamined = 0;
    char *szStart = szLineBuf;
    char *szEnd = szStart;
    for( int iCell=0; iCell<nBlockXSize; szStart = szEnd )
    {
        while( isspace( (unsigned char)*szStart ) )
            szStart++;

        double dfValue = CPLStrtod( szStart, &szEnd );
        if( szStart == szEnd )
        {
            /* No number found */
            if( *szStart == '.' )
            {
                CPLError( CE_Warning, CPLE_FileIO,
                          "Unexpected value in grid row %d (expected floating "
                          "point value, found \"%s\").\n",
                          nBlockYOff, szStart );
                VSIFree( szLineBuf );
                return CE_Failure;
            }

            /* Check if this was an expected failure */

            /* Found sign at end of input, seek back to re-read it */
            bool bOnlySign = false;
            if ( (*szStart == '-' || *szStart == '+') && *(szStart+1) == '\0' )
            {
                if( VSIFSeekL( poGDS->fp,
                               VSIFTellL( poGDS->fp)-1,
                               SEEK_SET ) != 0 )
                {
                    VSIFree( szLineBuf );
                    CPLError( CE_Failure, CPLE_FileIO,
                              "Unable to seek in grid row %d "
                              "(offset %ld, seek %d).\n",
                              nBlockYOff,
                              (long) VSIFTellL(poGDS->fp),
                              -1 );

                    return CE_Failure;
                }
                bOnlySign = true;
            }
            else if( *szStart != '\0' )
            {
                szEnd = szStart;
                while( !isspace( (unsigned char)*szEnd ) && *szEnd != '\0' )
                    szEnd++;
                char cOldEnd = *szEnd;
                *szEnd = '\0';

                CPLError( CE_Warning, CPLE_FileIO,
                          "Unexpected value in grid row %d (expected floating "
                          "point value, found \"%s\").\n",
                          nBlockYOff, szStart );

                *szEnd = cOldEnd;

                szEnd = szStart;
                while( !isdigit( *szEnd ) && *szEnd != '.' && *szEnd != '\0' )
                    szEnd++;

                continue;
            }
            else if( static_cast<size_t>(szStart - szLineBuf) != nCharsRead )
            {
                CPLError( CE_Warning, CPLE_FileIO,
                          "Unexpected ASCII null-character in grid row %d at "
                          "offset %ld.\n",
                          nBlockYOff,
                          (long) (szStart - szLineBuf) );

                while( *szStart == '\0' &&
                       static_cast<size_t>(szStart - szLineBuf) < nCharsRead )
                    szStart++;

                szEnd = szStart;
                continue;
            }

            nCharsExamined += szStart - szLineBuf;
            nCharsRead = VSIFReadL( szLineBuf, 1, nLineBufSize - 1, poGDS->fp );
            if( nCharsRead == 0 || (bOnlySign && nCharsRead == 1) )
            {
                VSIFree( szLineBuf );
                CPLError( CE_Failure, CPLE_FileIO,
                          "Can't read portion of grid row %d at offset %ld.",
                          nBlockYOff, (long) panLineOffset[nBlockYOff] );
                return CE_Failure;
            }
            szLineBuf[nCharsRead] = '\0';
            szEnd = szLineBuf;
            continue;
        }
        else if( *szEnd == '\0'
                 || (*szEnd == '.' && *(szEnd+1) == '\0')
                 || (*szEnd == '-' && *(szEnd+1) == '\0')
                 || (*szEnd == '+' && *(szEnd+1) == '\0')
                 || (*szEnd == 'E' && *(szEnd+1) == '\0')
                 || (*szEnd == 'E' && *(szEnd+1) == '-' && *(szEnd+2) == '\0')
                 || (*szEnd == 'E' && *(szEnd+1) == '+' && *(szEnd+2) == '\0')
                 || (*szEnd == 'e' && *(szEnd+1) == '\0')
                 || (*szEnd == 'e' && *(szEnd+1) == '-' && *(szEnd+2) == '\0')
                 || (*szEnd == 'e' && *(szEnd+1) == '+' && *(szEnd+2) == '\0'))
        {
            /* Number was interrupted by a nul character */
            while( *szEnd != '\0' )
                szEnd++;

            if( static_cast<size_t>(szEnd - szLineBuf) != nCharsRead )
            {
                CPLError( CE_Warning, CPLE_FileIO,
                          "Unexpected ASCII null-character in grid row %d at "
                          "offset %ld.\n",
                          nBlockYOff,
                          (long) (szStart - szLineBuf) );

                while( *szEnd == '\0' &&
                       static_cast<size_t>(szEnd - szLineBuf) < nCharsRead )
                    szEnd++;

                continue;
            }

            /* End of buffer, could be interrupting a number */
            if( VSIFSeekL( poGDS->fp, VSIFTellL(poGDS->fp) + szStart - szEnd, SEEK_SET ) != 0 )
            {
                VSIFree( szLineBuf );
                CPLError( CE_Failure, CPLE_FileIO,
                          "Unable to seek in grid row %d (offset %ld, seek %d)"
                          ".\n", nBlockYOff,
                          (long) VSIFTellL(poGDS->fp),
                          (int) (szStart - szEnd) );

                return CE_Failure;
            }
            nCharsExamined += szStart - szLineBuf;
            nCharsRead = VSIFReadL( szLineBuf, 1, nLineBufSize - 1, poGDS->fp );
            szLineBuf[nCharsRead] = '\0';

            if( nCharsRead == 0 )
            {
                VSIFree( szLineBuf );
                CPLError( CE_Failure, CPLE_FileIO,
                          "Can't read portion of grid row %d at offset %ld.",
                          nBlockYOff, (long) panLineOffset[nBlockYOff] );
                return CE_Failure;
            }
            else if( nCharsRead > static_cast<size_t>(szEnd - szStart) )
            {
                /* Read new data, this was not really the end */
                szEnd = szLineBuf;
                continue;
            }

            /* This is really the last value and has no tailing newline */
            szEnd = szLineBuf + nCharsRead;
        }

        if( pdfImage != NULL )
        {
            *(pdfImage+iCell) = dfValue;
        }

        iCell++;
    }

    while( *szEnd == ' ' )
        szEnd++;

    if( *szEnd != '\0' && *szEnd != poGDS->szEOL[0] )
        CPLDebug( "GSAG", "Grid row %d does not end with a newline.  "
                  "Possible skew.\n", nBlockYOff );

    while( isspace( (unsigned char)*szEnd ) )
        szEnd++;

    nCharsExamined += szEnd - szLineBuf;

    if( nCharsExamined >= nMaxLineSize )
        nMaxLineSize = nCharsExamined + 1;

    if( nBlockYOff > 0 )
        panLineOffset[nBlockYOff - 1] =
            panLineOffset[nBlockYOff] + nCharsExamined;

    nLastReadLine = nBlockYOff;

    VSIFree( szLineBuf );

    return CE_None;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GSAGRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                    void * pImage )

{
    if( eAccess == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Unable to write block, dataset opened read only.\n" );
        return CE_Failure;
    }

    if( nBlockYOff < 0 || nBlockYOff > nRasterYSize - 1 || nBlockXOff != 0 )
        return CE_Failure;

    GSAGDataset *poGDS = (GSAGDataset *)poDS;
    assert( poGDS != NULL );

    if( padfRowMinZ == NULL || padfRowMaxZ == NULL
        || nMinZRow < 0 || nMaxZRow < 0 )
    {
        padfRowMinZ = (double *)VSI_MALLOC2_VERBOSE( nRasterYSize,sizeof(double) );
        if( padfRowMinZ == NULL )
        {
            return CE_Failure;
        }

        padfRowMaxZ = (double *)VSI_MALLOC2_VERBOSE( nRasterYSize,sizeof(double) );
        if( padfRowMaxZ == NULL )
        {
            VSIFree( padfRowMinZ );
            padfRowMinZ = NULL;
            return CE_Failure;
        }

        CPLErr eErr = ScanForMinMaxZ();
        if( eErr != CE_None )
            return eErr;
    }

    if( panLineOffset[nBlockYOff+1] == 0 )
        IReadBlock( nBlockXOff, nBlockYOff, NULL );

    if( panLineOffset[nBlockYOff+1] == 0 || panLineOffset[nBlockYOff] == 0 )
        return CE_Failure;

    std::ostringstream ssOutBuf;
    ssOutBuf.precision( GSAGDataset::nFIELD_PRECISION );
    ssOutBuf.setf( std::ios::uppercase );

    double *pdfImage = (double *)pImage;
    padfRowMinZ[nBlockYOff] = DBL_MAX;
    padfRowMaxZ[nBlockYOff] = -DBL_MAX;
    for( int iCell=0; iCell<nBlockXSize; )
    {
        for( int iCol=0; iCol<10 && iCell<nBlockXSize; iCol++, iCell++ )
        {
            if( AlmostEqual( pdfImage[iCell], GSAGDataset::dfNODATA_VALUE ) )
            {
                if( pdfImage[iCell] < padfRowMinZ[nBlockYOff] )
                    padfRowMinZ[nBlockYOff] = pdfImage[iCell];

                if( pdfImage[iCell] > padfRowMaxZ[nBlockYOff] )
                    padfRowMaxZ[nBlockYOff] = pdfImage[iCell];
            }

            ssOutBuf << pdfImage[iCell] << " ";
        }
        ssOutBuf << poGDS->szEOL;
    }
    ssOutBuf << poGDS->szEOL;

    CPLString sOut = ssOutBuf.str();
    if( sOut.length() != panLineOffset[nBlockYOff+1]-panLineOffset[nBlockYOff] )
    {
        int nShiftSize = (int) (sOut.length() - (panLineOffset[nBlockYOff+1]
                                                 - panLineOffset[nBlockYOff]));
        if( nBlockYOff != poGDS->nRasterYSize
            && GSAGDataset::ShiftFileContents( poGDS->fp,
                                               panLineOffset[nBlockYOff+1],
                                               nShiftSize,
                                               poGDS->szEOL ) != CE_None )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failure writing block, "
                      "unable to shift file contents.\n" );
            return CE_Failure;
        }

        for( size_t iLine=nBlockYOff+1;
             iLine < static_cast<unsigned>(poGDS->nRasterYSize+1)
                && panLineOffset[iLine] != 0; iLine++ )
            panLineOffset[iLine] += nShiftSize;
    }

    if( VSIFSeekL( poGDS->fp, panLineOffset[nBlockYOff], SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO, "Unable to seek to grid line.\n" );
        return CE_Failure;
    }

    if( VSIFWriteL( sOut.c_str(), 1, sOut.length(),
                    poGDS->fp ) != sOut.length() )
    {
        CPLError( CE_Failure, CPLE_FileIO, "Unable to write grid block.\n" );
        return CE_Failure;
    }

    /* Update header as needed */
    bool bHeaderNeedsUpdate = false;
    if( nMinZRow == nBlockYOff && padfRowMinZ[nBlockYOff] > dfMinZ )
    {
        double dfNewMinZ = -DBL_MAX;
        for( int iRow=0; iRow<nRasterYSize; iRow++ )
        {
            if( padfRowMinZ[iRow] < dfNewMinZ )
            {
                dfNewMinZ = padfRowMinZ[iRow];
                nMinZRow = iRow;
            }
        }

        if( dfNewMinZ != dfMinZ )
        {
            dfMinZ = dfNewMinZ;
            bHeaderNeedsUpdate = true;
        }
    }

    if( nMaxZRow == nBlockYOff && padfRowMaxZ[nBlockYOff] < dfMaxZ )
    {
        double dfNewMaxZ = -DBL_MAX;
        for( int iRow=0; iRow<nRasterYSize; iRow++ )
        {
            if( padfRowMaxZ[iRow] > dfNewMaxZ )
            {
                dfNewMaxZ = padfRowMaxZ[iRow];
                nMaxZRow = iRow;
            }
        }

        if( dfNewMaxZ != dfMaxZ )
        {
            dfMaxZ = dfNewMaxZ;
            bHeaderNeedsUpdate = true;
        }
    }

    if( padfRowMinZ[nBlockYOff] < dfMinZ || padfRowMaxZ[nBlockYOff] > dfMaxZ )
    {
        if( padfRowMinZ[nBlockYOff] < dfMinZ )
        {
            dfMinZ = padfRowMinZ[nBlockYOff];
            nMinZRow = nBlockYOff;
        }

        if( padfRowMaxZ[nBlockYOff] > dfMaxZ )
        {
            dfMaxZ = padfRowMaxZ[nBlockYOff];
            nMaxZRow = nBlockYOff;
        }

        bHeaderNeedsUpdate = true;
    }

    if( bHeaderNeedsUpdate && dfMaxZ > dfMinZ )
    {
        CPLErr eErr = poGDS->UpdateHeader();
        return eErr;
    }

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double GSAGRasterBand::GetNoDataValue( int * pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = TRUE;

    return GSAGDataset::dfNODATA_VALUE;
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double GSAGRasterBand::GetMinimum( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = TRUE;

    return dfMinZ;
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double GSAGRasterBand::GetMaximum( int *pbSuccess )
{
    if( pbSuccess )
        *pbSuccess = TRUE;

    return dfMaxZ;
}

/************************************************************************/
/* ==================================================================== */
/*                              GSAGDataset                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             GSAGDataset()                            */
/************************************************************************/

GSAGDataset::GSAGDataset( const char *pszEOL ) :
    fp(NULL),
    nMinMaxZOffset(0)
{
    if( pszEOL == NULL || EQUAL(pszEOL, "") )
    {
        CPLDebug( "GSAG", "GSAGDataset() created with invalid EOL string.\n" );
        szEOL[0] = '\x0D';
        szEOL[1] = '\x0A';
        szEOL[2] = '\0';
        return;
    }

    snprintf(szEOL, sizeof(szEOL), "%s", pszEOL);
}

/************************************************************************/
/*                            ~GSAGDataset()                            */
/************************************************************************/

GSAGDataset::~GSAGDataset()

{
    FlushCache();
    if( fp != NULL )
        VSIFCloseL( fp );
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int GSAGDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    /* Check for signature */
    if( poOpenInfo->nHeaderBytes < 5
        || !STARTS_WITH_CI((const char *) poOpenInfo->pabyHeader, "DSAA")
        || ( poOpenInfo->pabyHeader[4] != '\x0D'
            && poOpenInfo->pabyHeader[4] != '\x0A' ))
    {
        return FALSE;
    }
    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GSAGDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify(poOpenInfo) )
    {
        return NULL;
    }

    /* Identify the end of line marker (should be \x0D\x0A, but try some others)
     * (note that '\x0D' == '\r' and '\x0A' == '\n' on most systems) */
    char szEOL[3];
    szEOL[0] = poOpenInfo->pabyHeader[4];
    szEOL[1] = poOpenInfo->pabyHeader[5];
    szEOL[2] = '\0';
    if( szEOL[1] != '\x0D' && szEOL[1] != '\x0A' )
        szEOL[1] = '\0';

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GSAGDataset *poDS = new GSAGDataset( szEOL );

/* -------------------------------------------------------------------- */
/*      Open file with large file API.                                  */
/* -------------------------------------------------------------------- */

    poDS->eAccess = poOpenInfo->eAccess;
    if( poOpenInfo->eAccess == GA_ReadOnly )
        poDS->fp = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    else
        poDS->fp = VSIFOpenL( poOpenInfo->pszFilename, "r+b" );

    if( poDS->fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "VSIFOpenL(%s) failed unexpectedly.",
                  poOpenInfo->pszFilename );
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    char *pabyHeader = NULL;
    bool bMustFreeHeader = false;
    if( poOpenInfo->nHeaderBytes >= static_cast<int>(nMAX_HEADER_SIZE) )
    {
        pabyHeader = (char *)poOpenInfo->pabyHeader;
    }
    else
    {
        bMustFreeHeader = true;
        pabyHeader = (char *)VSI_MALLOC_VERBOSE( nMAX_HEADER_SIZE );
        if( pabyHeader == NULL )
        {
            delete poDS;
            return NULL;
        }

        size_t nRead = VSIFReadL( pabyHeader, 1, nMAX_HEADER_SIZE-1, poDS->fp );
        pabyHeader[nRead] = '\0';
    }

    const char *szErrorMsg = NULL;
    const char *szStart = pabyHeader + 5;
    char *szEnd = NULL;
    double dfTemp;

    /* Parse number of X axis grid rows */
    long nTemp = strtol( szStart, &szEnd, 10 );
    if( szStart == szEnd || nTemp < 0l )
    {
        szErrorMsg = "Unable to parse the number of X axis grid columns.\n";
        goto error;
    }
    else if( nTemp > INT_MAX )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Number of X axis grid columns not representable.\n" );
        poDS->nRasterXSize = INT_MAX;
    }
    else if ( nTemp == 0 )
    {
        szErrorMsg = "Number of X axis grid columns is zero, which is invalid.\n";
        goto error;
    }
    else
    {
        poDS->nRasterXSize = static_cast<int>(nTemp);
    }
    szStart = szEnd;

    /* Parse number of Y axis grid rows */
    nTemp = strtol( szStart, &szEnd, 10 );
    if( szStart == szEnd || nTemp < 0l )
    {
        szErrorMsg = "Unable to parse the number of Y axis grid rows.\n";
        goto error;
    }
    else if( nTemp > INT_MAX - 1 )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Number of Y axis grid rows not representable.\n" );
        poDS->nRasterYSize = INT_MAX - 1;
    }
    else if ( nTemp == 0)
    {
        szErrorMsg = "Number of Y axis grid rows is zero, which is invalid.\n";
        goto error;
    }
    else
    {
        poDS->nRasterYSize = static_cast<int>(nTemp);
    }
    szStart = szEnd;

    /* Parse the minimum X value of the grid */
    double dfMinX;
    dfTemp = CPLStrtod( szStart, &szEnd );
    if( szStart == szEnd )
    {
        szErrorMsg = "Unable to parse the minimum X value.\n";
        goto error;
    }
    else
    {
        dfMinX = dfTemp;
    }
    szStart = szEnd;

    /* Parse the maximum X value of the grid */
    double dfMaxX;
    dfTemp = CPLStrtod( szStart, &szEnd );
    if( szStart == szEnd )
    {
        szErrorMsg = "Unable to parse the maximum X value.\n";
        goto error;
    }
    else
    {
        dfMaxX = dfTemp;
    }
    szStart = szEnd;

    /* Parse the minimum Y value of the grid */
    double dfMinY;
    dfTemp = CPLStrtod( szStart, &szEnd );
    if( szStart == szEnd )
    {
        szErrorMsg = "Unable to parse the minimum Y value.\n";
        goto error;
    }
    else
    {
        dfMinY = dfTemp;
    }
    szStart = szEnd;

    /* Parse the maximum Y value of the grid */
    double dfMaxY;
    dfTemp = CPLStrtod( szStart, &szEnd );
    if( szStart == szEnd )
    {
        szErrorMsg = "Unable to parse the maximum Y value.\n";
        goto error;
    }
    else
    {
        dfMaxY = dfTemp;
    }
    szStart = szEnd;

    /* Parse the minimum Z value of the grid */
    while( isspace( (unsigned char)*szStart ) )
        szStart++;
    poDS->nMinMaxZOffset = szStart - pabyHeader;

    double dfMinZ;
    dfTemp = CPLStrtod( szStart, &szEnd );
    if( szStart == szEnd )
    {
        szErrorMsg = "Unable to parse the minimum Z value.\n";
        goto error;
    }
    else
    {
        dfMinZ = dfTemp;
    }
    szStart = szEnd;

    /* Parse the maximum Z value of the grid */
    double dfMaxZ;
    dfTemp = CPLStrtod( szStart, &szEnd );
    if( szStart == szEnd )
    {
        szErrorMsg = "Unable to parse the maximum Z value.\n";
        goto error;
    }
    else
    {
        dfMaxZ = dfTemp;
    }

    while( isspace((unsigned char)*szEnd) )
            szEnd++;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    {
    GSAGRasterBand *poBand = new GSAGRasterBand( poDS, 1, szEnd-pabyHeader );
    if( poBand->panLineOffset == NULL )
    {
        delete poBand;
        goto error;
    }

    poBand->dfMinX = dfMinX;
    poBand->dfMaxX = dfMaxX;
    poBand->dfMinY = dfMinY;
    poBand->dfMaxY = dfMaxY;
    poBand->dfMinZ = dfMinZ;
    poBand->dfMaxZ = dfMaxZ;

    poDS->SetBand( 1, poBand );
    }

    if( bMustFreeHeader )
    {
        CPLFree( pabyHeader );
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for external overviews.                                   */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename, poOpenInfo->GetSiblingFiles() );

    return poDS;

error:
    if ( bMustFreeHeader )
    {
        CPLFree( pabyHeader );
    }

    delete poDS;

    if (szErrorMsg)
        CPLError( CE_Failure, CPLE_AppDefined, "%s", szErrorMsg );
    return NULL;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GSAGDataset::GetGeoTransform( double *padfGeoTransform )
{
    padfGeoTransform[0] = 0;
    padfGeoTransform[1] = 1;
    padfGeoTransform[2] = 0;
    padfGeoTransform[3] = 0;
    padfGeoTransform[4] = 0;
    padfGeoTransform[5] = 1;

    GSAGRasterBand *poGRB = (GSAGRasterBand *)GetRasterBand( 1 );

    if( poGRB == NULL )
    {
        return CE_Failure;
    }

    /* check if we have a PAM GeoTransform stored */
    CPLPushErrorHandler( CPLQuietErrorHandler );
    CPLErr eErr = GDALPamDataset::GetGeoTransform( padfGeoTransform );
    CPLPopErrorHandler();

    if( eErr == CE_None )
        return CE_None;

    if( nRasterXSize == 1 || nRasterYSize == 1 )
        return CE_Failure;

    /* calculate pixel size first */
    padfGeoTransform[1] = (poGRB->dfMaxX - poGRB->dfMinX)/(nRasterXSize - 1);
    padfGeoTransform[5] = (poGRB->dfMinY - poGRB->dfMaxY)/(nRasterYSize - 1);

    /* then calculate image origin */
    padfGeoTransform[0] = poGRB->dfMinX - padfGeoTransform[1] / 2;
    padfGeoTransform[3] = poGRB->dfMaxY - padfGeoTransform[5] / 2;

    /* tilt/rotation does not supported by the GS grids */
    padfGeoTransform[4] = 0.0;
    padfGeoTransform[2] = 0.0;

    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr GSAGDataset::SetGeoTransform( double *padfGeoTransform )
{
    if( eAccess == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Unable to set GeoTransform, dataset opened read only.\n" );
        return CE_Failure;
    }

    GSAGRasterBand *poGRB = (GSAGRasterBand *)GetRasterBand( 1 );

    if( poGRB == NULL || padfGeoTransform == NULL)
        return CE_Failure;

    /* non-zero transform 2 or 4 or negative 1 or 5 not supported natively */
    /*if( padfGeoTransform[2] != 0.0 || padfGeoTransform[4] != 0.0
        || padfGeoTransform[1] < 0.0 || padfGeoTransform[5] < 0.0 )
        eErr = GDALPamDataset::SetGeoTransform( padfGeoTransform );*/
    // if( eErr != CE_None )
    //     return eErr;

    const double dfOldMinX = poGRB->dfMinX;
    const double dfOldMaxX = poGRB->dfMaxX;
    const double dfOldMinY = poGRB->dfMinY;
    const double dfOldMaxY = poGRB->dfMaxY;

    poGRB->dfMinX = padfGeoTransform[0] + padfGeoTransform[1] / 2;
    poGRB->dfMaxX =
        padfGeoTransform[1] * (nRasterXSize - 0.5) + padfGeoTransform[0];
    poGRB->dfMinY =
        padfGeoTransform[5] * (nRasterYSize - 0.5) + padfGeoTransform[3];
    poGRB->dfMaxY = padfGeoTransform[3] + padfGeoTransform[5] / 2;

    CPLErr eErr = UpdateHeader();

    if( eErr != CE_None )
    {
        poGRB->dfMinX = dfOldMinX;
        poGRB->dfMaxX = dfOldMaxX;
        poGRB->dfMinY = dfOldMinY;
        poGRB->dfMaxY = dfOldMaxY;
    }

    return eErr;
}

/************************************************************************/
/*                         ShiftFileContents()                          */
/************************************************************************/
CPLErr GSAGDataset::ShiftFileContents( VSILFILE *fp, vsi_l_offset nShiftStart,
                                       int nShiftSize, const char *pszEOL )
{
    /* nothing to do for zero-shift */
    if( nShiftSize == 0 )
        return CE_None;

    /* make sure start location is sane */
/* Tautology is always false.  nShiftStart is unsigned. */
    if( /* nShiftStart < 0
           || */ (nShiftSize < 0
            && nShiftStart < static_cast<vsi_l_offset>(-nShiftSize)) )
        nShiftStart = /*(nShiftSize > 0) ? 0 :*/  -nShiftSize;

    /* get offset at end of file */
    if( VSIFSeekL( fp, 0, SEEK_END ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to seek to end of grid file.\n" );
        return CE_Failure;
    }

    vsi_l_offset nOldEnd = VSIFTellL( fp );

    /* If shifting past end, just zero-pad as necessary */
    if( nShiftStart >= nOldEnd )
    {
        if( nShiftSize < 0 )
        {
            if( nShiftStart + nShiftSize >= nOldEnd )
                return CE_None;

            VSIFTruncateL( fp, nShiftStart + nShiftSize );

            return CE_None;
        }
        else
        {
            for( vsi_l_offset nPos = nOldEnd;
                 nPos < nShiftStart + nShiftSize; nPos++ )
            {
                if( VSIFWriteL( (void *)" ", 1, 1, fp ) != 1 )
                {
                    CPLError( CE_Failure, CPLE_FileIO,
                              "Unable to write padding to grid file "
                              "(Out of space?).\n" );
                    return CE_Failure;
                }
            }
            return CE_None;
        }
    }

    /* prepare buffer for real shifting */
    size_t nBufferSize = (1024 >= abs(nShiftSize)*2) ? 1024 : abs(nShiftSize)*2;
    char *pabyBuffer = (char *)VSI_MALLOC_VERBOSE( nBufferSize );
    if( pabyBuffer == NULL)
    {
        return CE_Failure;
    }

    if( VSIFSeekL( fp, nShiftStart, SEEK_SET ) != 0 )
    {
        VSIFree( pabyBuffer );
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to seek to start of shift in grid file.\n" );
        return CE_Failure;
    }

    size_t nRead;
    size_t nOverlap = (nShiftSize > 0) ? nShiftSize : 0;
    /* If there is overlap, fill buffer with the overlap to start */
    if( nOverlap > 0)
    {
        nRead = VSIFReadL( (void *)pabyBuffer, 1, nOverlap, fp );
        if( nRead < nOverlap && !VSIFEofL( fp ) )
        {
            VSIFree( pabyBuffer );
            CPLError( CE_Failure, CPLE_FileIO,
                      "Error reading grid file.\n" );
            return CE_Failure;
        }

        /* overwrite the new space with ' ' */
        if( VSIFSeekL( fp, nShiftStart, SEEK_SET ) != 0 )
        {
            VSIFree( pabyBuffer );
            CPLError( CE_Failure, CPLE_FileIO,
                      "Unable to seek to start of shift in grid file.\n" );
            return CE_Failure;
        }

        for( int iFill=0; iFill<nShiftSize; iFill++ )
        {
            if( VSIFWriteL( (void *)" ", 1, 1, fp ) != 1 )
            {
                VSIFree( pabyBuffer );
                CPLError( CE_Failure, CPLE_FileIO,
                          "Unable to write padding to grid file "
                          "(Out of space?).\n" );
                return CE_Failure;
            }
        }

        /* if we have already read the entire file, finish it off */
        if( VSIFTellL( fp ) >= nOldEnd )
        {
            if( VSIFWriteL( (void *)pabyBuffer, 1, nRead, fp ) != nRead )
            {
                VSIFree( pabyBuffer );
                CPLError( CE_Failure, CPLE_FileIO,
                          "Unable to write to grid file (Out of space?).\n" );
                return CE_Failure;
            }

            VSIFree( pabyBuffer );
            return CE_None;
        }
    }

    /* iterate over the remainder of the file and shift as requested */
    bool bEOF = false;
    while( !bEOF )
    {
        nRead = VSIFReadL( (void *)(pabyBuffer+nOverlap), 1,
                           nBufferSize - nOverlap, fp );

        if( VSIFEofL( fp ) )
            bEOF = true;
        else
            bEOF = false;

        if( nRead == 0 && !bEOF )
        {
            VSIFree( pabyBuffer );
            CPLError( CE_Failure, CPLE_FileIO,
                      "Unable to read from grid file (possible corruption).\n");
            return CE_Failure;
        }

        /* FIXME:  Should use SEEK_CUR, review integer promotions... */
        vsi_l_offset nNewPos = (nShiftSize >= 0 ) ?
            VSIFTellL(fp)+nShiftSize-nRead-nOverlap :
            VSIFTellL(fp) - (-nShiftSize) -nRead-nOverlap;
        if( VSIFSeekL( fp, nNewPos, SEEK_SET ) != 0 )
        {
            VSIFree( pabyBuffer );
            CPLError( CE_Failure, CPLE_FileIO,
                      "Unable to seek in grid file (possible corruption).\n" );
            return CE_Failure;
        }

        size_t nWritten = VSIFWriteL( (void *)pabyBuffer, 1, nRead, fp );
        if( nWritten != nRead )
        {
            VSIFree( pabyBuffer );
            CPLError( CE_Failure, CPLE_FileIO,
                      "Unable to write to grid file (out of space?).\n" );
            return CE_Failure;
        }

        /* shift overlapped contents to the front of the buffer if necessary */
        if( nOverlap > 0)
            memmove(pabyBuffer, pabyBuffer+nRead, nOverlap);
    }

    /* write the remainder of the buffer or overwrite leftovers and finish */
    if( nShiftSize > 0 )
    {
        size_t nTailSize = nOverlap;
        while( nTailSize > 0 && isspace( (unsigned char)pabyBuffer[nTailSize-1] ) )
            nTailSize--;

        if( VSIFWriteL( (void *)pabyBuffer, 1, nTailSize, fp ) != nTailSize )
        {
            VSIFree( pabyBuffer );
            CPLError( CE_Failure, CPLE_FileIO,
                      "Unable to write to grid file (out of space?).\n" );
            return CE_Failure;
        }

        if( VSIFWriteL( (void *)pszEOL, 1, strlen(pszEOL), fp )
            != strlen(pszEOL) )
        {
            VSIFree( pabyBuffer );
            CPLError( CE_Failure, CPLE_FileIO,
                      "Unable to write to grid file (out of space?).\n" );
            return CE_Failure;
        }
    }
    else
    {
        /* FIXME: ftruncate()? */
        /* FIXME:  Should use SEEK_CUR, review integer promotions... */
        if( VSIFSeekL( fp, VSIFTellL(fp)-strlen(pszEOL), SEEK_SET ) != 0 )
        {
            VSIFree( pabyBuffer );
            CPLError( CE_Failure, CPLE_FileIO,
                      "Unable to seek in grid file.\n" );
            return CE_Failure;
        }

        for( int iPadding=0; iPadding<-nShiftSize; iPadding++ )
        {
            if( VSIFWriteL( (void *)" ", 1, 1, fp ) != 1 )
            {
                VSIFree( pabyBuffer );
                CPLError( CE_Failure, CPLE_FileIO,
                          "Error writing to grid file.\n" );
                return CE_Failure;
            }
        }

        if( VSIFWriteL( (void *)pszEOL, 1, strlen(pszEOL), fp )
            != strlen(pszEOL) )
        {
            VSIFree( pabyBuffer );
            CPLError( CE_Failure, CPLE_FileIO,
                      "Unable to write to grid file (out of space?).\n" );
            return CE_Failure;
        }
    }

    VSIFree( pabyBuffer );
    return CE_None;
}

/************************************************************************/
/*                             UpdateHeader()                           */
/************************************************************************/

CPLErr GSAGDataset::UpdateHeader()

{
    GSAGRasterBand *poBand = (GSAGRasterBand *)GetRasterBand( 1 );
    if( poBand == NULL )
    {
        CPLError( CE_Failure, CPLE_FileIO, "Unable to open raster band.\n" );
        return CE_Failure;
    }

    std::ostringstream ssOutBuf;
    ssOutBuf.precision( nFIELD_PRECISION );
    ssOutBuf.setf( std::ios::uppercase );

    /* signature */
    ssOutBuf << "DSAA" << szEOL;

    /* columns rows */
    ssOutBuf << nRasterXSize << " " << nRasterYSize << szEOL;

    /* x range */
    ssOutBuf << poBand->dfMinX << " " << poBand->dfMaxX << szEOL;

    /* y range */
    ssOutBuf << poBand->dfMinY << " " << poBand->dfMaxY << szEOL;

    /* z range */
    ssOutBuf << poBand->dfMinZ << " " << poBand->dfMaxZ << szEOL;

    CPLString sOut = ssOutBuf.str();
    if( sOut.length() != poBand->panLineOffset[0] )
    {
        int nShiftSize = (int) (sOut.length() - poBand->panLineOffset[0]);
        if( ShiftFileContents( fp, poBand->panLineOffset[0], nShiftSize,
                               szEOL ) != CE_None )
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Unable to update grid header, "
                      "failure shifting file contents.\n" );
            return CE_Failure;
        }

        for( size_t iLine=0;
             iLine < static_cast<unsigned>(nRasterYSize+1)
                && poBand->panLineOffset[iLine] != 0;
             iLine++ )
            poBand->panLineOffset[iLine] += nShiftSize;
    }

    if( VSIFSeekL( fp, 0, SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to seek to start of grid file.\n" );
        return CE_Failure;
    }

    if( VSIFWriteL( sOut.c_str(), 1, sOut.length(), fp ) != sOut.length() )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to update file header.  Disk full?\n" );
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *GSAGDataset::CreateCopy( const char *pszFilename,
                                      GDALDataset *poSrcDS,
                                      int bStrict,
                                      CPL_UNUSED char **papszOptions,
                                      GDALProgressFunc pfnProgress,
                                      void *pProgressData )
{
    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

    int nBands = poSrcDS->GetRasterCount();
    if (nBands == 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "GSAG driver does not support source dataset with zero band.\n");
        return NULL;
    }
    else if (nBands > 1)
    {
        if( bStrict )
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "Unable to create copy, Golden Software ASCII Grid "
                      "format only supports one raster band.\n" );
            return NULL;
        }
        else
            CPLError( CE_Warning, CPLE_NotSupported,
                      "Golden Software ASCII Grid format only supports one "
                      "raster band, first band will be copied.\n" );
    }

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated\n" );
        return NULL;
    }

    VSILFILE *fp = VSIFOpenL( pszFilename, "w+b" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Attempt to create file '%s' failed.\n",
                  pszFilename );
        return NULL;
    }

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();
    double adfGeoTransform[6];

    poSrcDS->GetGeoTransform( adfGeoTransform );

    std::ostringstream ssHeader;
    ssHeader.precision( nFIELD_PRECISION );
    ssHeader.setf( std::ios::uppercase );

    ssHeader << "DSAA\x0D\x0A";

    ssHeader << nXSize << " " << nYSize << "\x0D\x0A";

    ssHeader << adfGeoTransform[0] + adfGeoTransform[1] / 2 << " "
             << adfGeoTransform[1] * (nXSize - 0.5) + adfGeoTransform[0]
             << "\x0D\x0A";

    ssHeader << adfGeoTransform[5] * (nYSize - 0.5) + adfGeoTransform[3] << " "
             << adfGeoTransform[3] + adfGeoTransform[5] / 2
             << "\x0D\x0A";

    if( VSIFWriteL( (void *)ssHeader.str().c_str(), 1, ssHeader.str().length(),
                    fp ) != ssHeader.str().length() )
    {
        VSIFCloseL( fp );
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to create copy, writing header failed.\n" );
        return NULL;
    }

    /* Save the location and write placeholders for the min/max Z value */
    vsi_l_offset nRangeStart = VSIFTellL( fp );
    const char *szDummyRange = "0.0000000000001 0.0000000000001\x0D\x0A";
    size_t nDummyRangeLen = strlen( szDummyRange );
    if( VSIFWriteL( (void *)szDummyRange, 1, nDummyRangeLen,
                    fp ) != nDummyRangeLen )
    {
        VSIFCloseL( fp );
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to create copy, writing header failed.\n" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Copy band data.                                                 */
/* -------------------------------------------------------------------- */
    double *pdfData = (double *)VSI_MALLOC2_VERBOSE( nXSize, sizeof( double ) );
    if( pdfData == NULL )
    {
        VSIFCloseL( fp );
        return NULL;
    }

    GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand(1);
    int bSrcHasNDValue;
    double dfSrcNoDataValue = poSrcBand->GetNoDataValue( &bSrcHasNDValue );
    double dfMin = DBL_MAX;
    double dfMax = -DBL_MAX;
    for( int iRow=0; iRow<nYSize; iRow++ )
    {
        CPLErr eErr = poSrcBand->RasterIO( GF_Read, 0, nYSize-iRow-1,
                                           nXSize, 1, pdfData,
                                           nXSize, 1, GDT_Float64, 0, 0, NULL );

        if( eErr != CE_None )
        {
            VSIFCloseL( fp );
            VSIFree( pdfData );
            return NULL;
        }

        for( int iCol=0; iCol<nXSize; )
        {
            for( int iCount=0;
                 iCount<10 && iCol<nXSize;
                 iCount++, iCol++ )
            {
                double dfValue = pdfData[iCol];

                if( bSrcHasNDValue && AlmostEqual( dfValue, dfSrcNoDataValue ) )
                {
                    dfValue = dfNODATA_VALUE;
                }
                else
                {
                    if( dfValue > dfMax )
                        dfMax = dfValue;

                    if( dfValue < dfMin )
                        dfMin = dfValue;
                }

                std::ostringstream ssOut;
                ssOut.precision(nFIELD_PRECISION);
                ssOut.setf( std::ios::uppercase );
                ssOut << dfValue << " ";
                CPLString sOut = ssOut.str();

                if( VSIFWriteL( sOut.c_str(), 1, sOut.length(), fp )
                    != sOut.length() )
                {
                    VSIFCloseL( fp );
                    VSIFree( pdfData );
                    CPLError( CE_Failure, CPLE_FileIO,
                              "Unable to write grid cell.  Disk full?\n" );
                    return NULL;
                }
            }

            if( VSIFWriteL( (void *)"\x0D\x0A", 1, 2, fp ) != 2 )
            {
                VSIFCloseL( fp );
                VSIFree( pdfData );
                CPLError( CE_Failure, CPLE_FileIO,
                          "Unable to finish write of grid line. Disk full?\n" );
                return NULL;
            }
        }

        if( VSIFWriteL( (void *)"\x0D\x0A", 1, 2, fp ) != 2 )
        {
            VSIFCloseL( fp );
            VSIFree( pdfData );
            CPLError( CE_Failure, CPLE_FileIO,
                      "Unable to finish write of grid row. Disk full?\n" );
            return NULL;
        }

        if( !pfnProgress( static_cast<double>(iRow + 1)/nYSize,
                          NULL, pProgressData ) )
        {
            VSIFCloseL( fp );
            VSIFree( pdfData );
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            return NULL;
        }
    }

    VSIFree( pdfData );

    /* write out the min and max values */
    std::ostringstream ssRange;
    ssRange.precision( nFIELD_PRECISION );
    ssRange.setf( std::ios::uppercase );
    ssRange << dfMin << " " << dfMax << "\x0D\x0A";
    if( ssRange.str().length() != nDummyRangeLen )
    {
        int nShiftSize = static_cast<int>(ssRange.str().length()) -
                         static_cast<int>(nDummyRangeLen);
        if( ShiftFileContents( fp, nRangeStart + nDummyRangeLen,
                               nShiftSize, "\x0D\x0A" ) != CE_None )
        {
            VSIFCloseL( fp );
            CPLError( CE_Failure, CPLE_FileIO,
                      "Unable to shift file contents.\n" );
            return NULL;
        }
    }

    if( VSIFSeekL( fp, nRangeStart, SEEK_SET ) != 0 )
    {
        VSIFCloseL( fp );
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to seek to start of grid file copy.\n" );
        return NULL;
    }

    if( VSIFWriteL( (void *)ssRange.str().c_str(), 1, ssRange.str().length(),
                    fp ) != ssRange.str().length() )
    {
        VSIFCloseL( fp );
        CPLError( CE_Failure, CPLE_FileIO,
                  "Unable to write range information.\n" );
        return NULL;
    }

    VSIFCloseL( fp );

    GDALPamDataset *poDS = (GDALPamDataset *)GDALOpen( pszFilename,
                                                GA_Update );
    if (poDS)
    {
        poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );
    }
    return poDS;
}

/************************************************************************/
/*                          GDALRegister_GSAG()                         */
/************************************************************************/

void GDALRegister_GSAG()

{
    if( GDALGetDriverByName( "GSAG" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "GSAG" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Golden Software ASCII Grid (.grd)" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_various.html#GSAG" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "grd" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte Int16 UInt16 Int32 UInt32 "
                               "Float32 Float64" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnIdentify = GSAGDataset::Identify;
    poDriver->pfnOpen = GSAGDataset::Open;
    poDriver->pfnCreateCopy = GSAGDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
