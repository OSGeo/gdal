/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Grid Coverages Client
 * Purpose:  Main code file for client.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 * Revision 1.1  1999/07/25 01:57:17  warmerda
 * New
 *
 */

#define INITGUID
#define DBINITCONSTANTS

#import "CoverageIdl.tlb"

#include "com_util.h"

static void CInfo( const char * pszFactory, const char * pszFile,int bVerbose);
static void Usage();

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
    const char *pszFactory = "OGRCoverage.GridCoverageFactoryImpl";
    const char *pszFile = "F:\\opengis\\coverage\\client\\utm11-27.tif";
    int        bVerbose = TRUE;

/* -------------------------------------------------------------------- */
/*      Process command line switches.                                  */
/* -------------------------------------------------------------------- */
    for( int iArg=1; iArg < argc; iArg++ )
    {
        if( strcmp( argv[iArg], "-bmp" ) == 0 )
        {
            pszFactory = "BmpCoverage.GridCoverageFactoryImpl";
        }
        else if( strcmp( argv[iArg], "-v" ) == 0 )
        {
            bVerbose = 1;
        }
        else if( strcmp( argv[iArg], "-nv" ) == 0 )
        {
            bVerbose = 0;
        }
        else if( strcmp( argv[iArg], "-ft" ) == 0 && iArg < argc-1 )
        {
            pszFactory = argv[++iArg];
        }
        else if( strncmp( argv[iArg], "-h", 2 ) == 0 
                 || argv[iArg][0] == '-' )
        {
            Usage();
        }
        else
        {
            pszFile = argv[iArg];
        }
    }

    if( pszFile == NULL )
        Usage();

/* -------------------------------------------------------------------- */
/*      Initialize OLE                                                  */
/* -------------------------------------------------------------------- */
    OleSupInitialize();

/* -------------------------------------------------------------------- */
/*      Get information on the file, if possible.                       */
/* -------------------------------------------------------------------- */
    try 
    {
        CInfo( pszFactory, pszFile, bVerbose );
    }
    catch( ... )
    {
        printf( "Caught exception in CInfo() ... aborting.\n" );
    }

/* -------------------------------------------------------------------- */
/*      Deinitialize OLE                                                */
/* -------------------------------------------------------------------- */
    OleSupUninitialize();

    return 0;
}

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( "Usage: cinfo [-bmp] [-v/-nv] [-ft factory_name] [-h] filename\n");
    printf( "       -bmp: Use Cadcorp BMP Driver\n" );
    printf( "       -ft factory_name: Usage a different named factory.\n" );
    printf( "       -h: Get this usage message.\n" );
    printf( "       -v: turn verbose flag on.\n" );
    printf( "       -nv: turn verbose flag off.\n" );
    printf( "       filename: the raster file to get information on.\n" );

    exit( 0 );
}

/************************************************************************/
/*                           CInfoGridInfo()                            */
/************************************************************************/

static void CInfoGridInfo( Coverage::IGridInfoPtr pIGridInfo, int bVerbose )

{
    const char      *pszByteOrdering, *pszPixelOrdering = "";
    const char      *pszValueInByte = "";
    char            szValueSequence[128];
        
    printf( "Optimal Block Size: %dp x %dl\n",
            pIGridInfo->OptimalRowSize, 
            pIGridInfo->OptimalColumnSize );

    if( pIGridInfo->ByteOrdering == Coverage::wkbNDR )
        pszByteOrdering = "NDR (LSB)";
    else
        pszByteOrdering = "XDR (MSB)";

    if( pIGridInfo->PixelOrdering == Coverage::PixelInterleaved )
        pszPixelOrdering = "PixelInterleaved";
    else if( pIGridInfo->PixelOrdering == Coverage::LineInterleaved )
        pszPixelOrdering = "LineInterleaved";
    else if( pIGridInfo->PixelOrdering == Coverage::BandSequencial  )
        pszPixelOrdering = "BandSequencial";

    if( pIGridInfo->ValueInBytePacking == Coverage::LoBitFirst )
        pszValueInByte = "LoBitFirst";
    else
        pszValueInByte = "HiBitFirst";

    szValueSequence[0] = '\0';
    if( ((int) pIGridInfo->ValueSequence) 
           & ((int) Coverage::RowSequenceMaxtoMin) )
        strcat( szValueSequence, "RowSequenceMaxToMin" );
    else
        strcat( szValueSequence, "RowSequenceMinToMax" );
                
    if( ((int) pIGridInfo->ValueSequence) 
           & ((int) Coverage::ColumnSequenceMaxToMin) )
        strcat( szValueSequence, ",ColumnSequenceMaxToMin" );
    else
        strcat( szValueSequence, ",ColumnSequenceMinToMax" );
                
    if( ((int) pIGridInfo->ValueSequence) 
           & ((int) Coverage::ColumnDominant) )
        strcat( szValueSequence, ",ColumnDominant" );
    else
        strcat( szValueSequence, ",RowDominant" );
        
    printf( "Grid Organization: %s, %s, %s\n", 
            pszPixelOrdering, pszByteOrdering, pszValueInByte );
    printf( "                   %s\n", szValueSequence );

    if( pIGridInfo->HasArbitraryOverview )
        printf( "Grid has arbitrary overviews.\n" );

    for( int iOvr = 0; iOvr < pIGridInfo->NumOverview; iOvr++ )
    {
        Coverage::IGridGeometryPtr  pIOG = 
                              pIGridInfo->OverviewGridGeometry[iOvr];

        printf( "Overview %d: %dp x %dl\n", 
                iOvr + 1, 
                pIOG->MaxColumn - pIOG->MinColumn, 
                pIOG->MaxRow - pIOG->MinRow );
    }
}

/************************************************************************/
/*                          CInfoColorTable()                           */
/************************************************************************/

static void CInfoColorTable( Coverage::IColorTablePtr pIColorTable )

{
    const char      *pszCEI = "";
    Coverage::ColorEntryInterpretation eInterp = pIColorTable->Interpretation;

    switch( eInterp )
    {
        case Coverage::Gray:
            pszCEI = "Gray";
            break;

        case Coverage::RGB:
            pszCEI = "RGB";
            break;

        case Coverage::CMYK:
            pszCEI = "CMYK";
            break;

        case Coverage::HLS:
            pszCEI = "HLS";
            break;
    }

    printf( "  %d %s color entries:\n",
            pIColorTable->NumColor, pszCEI );

    for( int iColor = 0; iColor < pIColorTable->NumColor; iColor++ )
    {
        Coverage::ColorEntry      sColor, sColorRGB;

        sColor = pIColorTable->Color( iColor );
        sColorRGB = pIColorTable->ColorAsRGB( iColor );

        printf( "    %3d: Color(%d,%d,%d,%d) RGB(%d,%d,%d,%d)\n", 
                iColor, 
                sColor.C1, sColor.C2, sColor.C3, sColor.C4,
                sColorRGB.C1, sColorRGB.C2, sColorRGB.C3, sColorRGB.C4 );
    }
}

/************************************************************************/
/*                           CInfoDimension()                           */
/************************************************************************/

static void CInfoDimension( Coverage::IDimension *pIDimension, int bVerbose )

{
    const char      *pszDTN = "", *pszCI = "";
    Coverage::DimensionType eDimType = pIDimension->DimensionType;
    Coverage::ColorInterpretation eCI = pIDimension->ColorInterpretation;
    
    switch( eCI )
    {
        case Coverage::Undefined:
            pszCI = "Undefined";
            break;

        case Coverage::GrayIndex:
            pszCI = "GrayIndex";
            break;

        case Coverage::ColorIndex:
            pszCI = "ColorIndex";
            break;

        case Coverage::RedBand:
            pszCI = "RedBand";
            break;

        case Coverage::GreenBand:
            pszCI = "GreenBand";
            break;

        case Coverage::BlueBand:
            pszCI = "BlueBand";
            break;

        case Coverage::AlphaBand:
            pszCI = "AlphaBand";
            break;

        case Coverage::HueBand:
            pszCI = "HueBand";
            break;

        case Coverage::SaturationBand:
            pszCI = "SaturationBand";
            break;

        case Coverage::LightnessBand:
            pszCI = "LightnessBand";
            break;

        case Coverage::CyanBand:
            pszCI = "CyanBand";
            break;

        case Coverage::MagentaBand:
            pszCI = "MagentaBand";
            break;

        case Coverage::YellowBand:
            pszCI = "YellowBand";
            break;

        case Coverage::BlackBand:
            pszCI = "BlackBand";
            break;
    }

    switch( eDimType )
    {
        case Coverage::DT_1BIT:
            pszDTN = "1BIT";
            break;

        case Coverage::DT_2BIT:
            pszDTN = "2BIT";
            break;

        case Coverage::DT_4BIT:
            pszDTN = "4BIT";
            break;

        case Coverage::DT_8BIT_U:
            pszDTN = "8BIT_U";
            break;

        case Coverage::DT_8BIT_S:
            pszDTN = "8BIT_S";
            break;

        case Coverage::DT_16BIT_U:
            pszDTN = "16BIT_U";
            break;

        case Coverage::DT_16BIT_S:
            pszDTN = "16BIT_S";
            break;

        case Coverage::DT_32BIT_U:
            pszDTN = "32BIT_U";
            break;

        case Coverage::DT_32BIT_S:
            pszDTN = "32BIT_S";
            break;

        case Coverage::DT_32BIT_REAL:
            pszDTN = "32BIT_REAL";
            break;

        case Coverage::DT_64BIT_REAL:
            pszDTN = "64BIT_REAL";
            break;
    }

    printf( "  Type: %s, Color:%s, Min:%g, Max:%g, Nodata:%g\n",
            pszDTN, pszCI,
            (double) pIDimension->MinimumValue,
            (double) pIDimension->MaximumValue,
            (double) pIDimension->NodataValue );

    printf( "  Description: %S\n", pIDimension->Description );

    if( !bVerbose )    
        return;

/* -------------------------------------------------------------------- */
/*      Report the categories                                           */
/* -------------------------------------------------------------------- */
    SAFEARRAY *pCategoriesArray;
    LONG      nCategories = 0;

    pCategoriesArray = pIDimension->Categories;
    if( pCategoriesArray != NULL )
    {
        SafeArrayGetUBound( pCategoriesArray, 1, &nCategories );
        nCategories++;
    }
 
    if( nCategories > 0 )
    {
        printf( "  Categories:\n" );
        for( LONG iCat = 0; iCat < nCategories; iCat++ )
        {
            BSTR      *pCatName;

            SafeArrayGetElement( pCategoriesArray, &iCat, (void *) &pCatName );
            printf( "    %3d: %S\n", iCat, pCatName );
        }
    }

    if( pCategoriesArray != NULL )
        SafeArrayDestroy( pCategoriesArray );

/* -------------------------------------------------------------------- */
/*      Report on the color table.                                      */
/* -------------------------------------------------------------------- */
    if( pIDimension->ColorTable != NULL )
        CInfoColorTable( pIDimension->ColorTable );
}

/************************************************************************/
/*                               CInfo()                                */
/************************************************************************/

static void CInfo( const char * pszFactory, const char * pszFile, 
                   int bVerbose )

{
    Coverage::IGridCoverageFactoryPtr      pIGridCoverageFactory;
    HRESULT    hr;

/* -------------------------------------------------------------------- */
/*      Create factory.                                                 */
/* -------------------------------------------------------------------- */
    try 
    {
        Coverage::IGridCoverageFactoryPtr p( pszFactory );
        pIGridCoverageFactory = p;
    }
    catch( ... ) 
    {
        printf( "Attempt to instantiate IGridCoverageFactory %s failed.\n"
                "Giving up.\n",  pszFactory );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Open file.                                                      */
/* -------------------------------------------------------------------- */
    BSTR      pFilename;
    Coverage::IGridCoveragePtr pIGridCoverage;

    AnsiToBSTR( pszFile, &pFilename );

    try 
    {
        pIGridCoverage = pIGridCoverageFactory->CreateFromName( pFilename );
    }
    catch( ... )
    {
        printf( "Failed to create a coverage from file %s\n"
                "using factory %s.\n", 
                pszFile, pszFactory );
        return;
    }

    CoTaskMemFree( pFilename );

/* -------------------------------------------------------------------- */
/*      GridInfo.                                                       */
/* -------------------------------------------------------------------- */
    Coverage::IGridInfoPtr pIGridInfo = pIGridCoverage->GridInfo;

/* -------------------------------------------------------------------- */
/*      GridGeometry                                                    */
/* -------------------------------------------------------------------- */
    SAFEARRAY *pDimensions = pIGridCoverage->Codomain;
    LONG nDims;

    SafeArrayGetUBound( pDimensions, 1, &nDims );
    nDims++;

    printf( "Size = %ldp x %ldl x %ldb, Origin = (%ld,%ld)\n", 
            pIGridCoverage->GridGeometry->MaxColumn
               - pIGridCoverage->GridGeometry->MinColumn, 
            pIGridCoverage->GridGeometry->MaxRow
               - pIGridCoverage->GridGeometry->MinRow, 
            nDims,
            pIGridCoverage->GridGeometry->MinColumn, 
            pIGridCoverage->GridGeometry->MinRow );

/* -------------------------------------------------------------------- */
/*      Report on editibility and interpolation type.                   */
/* -------------------------------------------------------------------- */
    const char      *pszInterp = "unknown";
    Coverage::Interpolation eInterp = pIGridCoverage->InterpolationType;

    switch( eInterp )
    {
        case Coverage::NearestNeighbor:
            pszInterp = "NearestNeighbor";
            break;

        case Coverage::Bilinear:
            pszInterp = "Bilinear";
            break;

        case Coverage::Bicubic:
            pszInterp = "Bicubic";
            break;

        case Coverage::Optimal:
            pszInterp = "Optimal";
            break;
    }

    if( pIGridCoverage->DataEditable )
        printf( "Grid is editable, interpolation type is %s.\n",  pszInterp );
    else
        printf( "Grid is readonly, interpolation type is %s.\n", pszInterp );

/* -------------------------------------------------------------------- */
/*      Try to translate grid coordinates to SRS.                       */
/* -------------------------------------------------------------------- */
    double            adfSrcX[5], adfSrcY[5];

    adfSrcX[0] = pIGridCoverage->GridGeometry->MinRow;
    adfSrcY[0] = pIGridCoverage->GridGeometry->MinColumn;
    adfSrcX[1] = pIGridCoverage->GridGeometry->MinRow;
    adfSrcY[1] = pIGridCoverage->GridGeometry->MaxColumn;
    adfSrcX[2] = pIGridCoverage->GridGeometry->MaxRow;
    adfSrcY[2] = pIGridCoverage->GridGeometry->MinColumn;
    adfSrcX[3] = pIGridCoverage->GridGeometry->MaxRow;
    adfSrcY[3] = pIGridCoverage->GridGeometry->MaxColumn;
    adfSrcX[4] = 0.5 * (adfSrcX[0] + adfSrcX[2]);
    adfSrcY[4] = 0.5 * (adfSrcY[0] + adfSrcY[2]);

    try 
    {
        for( int iPnt = 0; iPnt < 5; iPnt++ )
        {
            Coverage::WKSPoint      sGridPoint;

            sGridPoint.x = adfSrcX[iPnt];
            sGridPoint.y = adfSrcY[iPnt];

            Coverage::IPointPtr pSRS = 
                pIGridCoverage->GridGeometry->GridToPoint( &sGridPoint );

            printf( "Grid (%lg,%lg) <--> SRS (%lg,%lg)\n", 
                    adfSrcX[iPnt], adfSrcY[iPnt], 
                    pSRS->x, pSRS->y );
        }
    } 
    catch( ... )
    {
        printf( "GridToPoint() threw an exception.\n" );
    }

/* -------------------------------------------------------------------- */
/*      Report on gridinfo.                                             */
/* -------------------------------------------------------------------- */
    CInfoGridInfo( pIGridCoverage->GridInfo, bVerbose );

/* -------------------------------------------------------------------- */
/*      Report on dimensions                                            */
/* -------------------------------------------------------------------- */
    for( LONG iDim = 0; iDim < nDims; iDim++ )
    {
        Coverage::IDimension *pIDimension;
        
        SafeArrayGetElement( pDimensions, &iDim, &pIDimension );
        
        printf( "Dimension/Band: %d\n", iDim+1 );
        CInfoDimension( pIDimension, bVerbose );
    }
    
    SafeArrayDestroy( pDimensions );

/* -------------------------------------------------------------------- */
/*      Number of sources.                                              */
/* -------------------------------------------------------------------- */
    if( bVerbose )
        printf( "Number of sources: %d\n", pIGridCoverage->NumSource );

/* -------------------------------------------------------------------- */
/*      Read the top left 10x10 chunk.                                  */
/* -------------------------------------------------------------------- */

    SAFEARRAY      *pBlock 
        = pIGridCoverage->GetDataBlockAsByte( 0, 0, 15, 10 );

    assert( pBlock != NULL );
    assert( SafeArrayGetDim(pBlock) == 3 );

    LONG    n = 120;

    SafeArrayGetLBound( pBlock, 1, &n );
    assert( n == 0 );

    SafeArrayGetLBound( pBlock, 2, &n );
    assert( n == 0 );

    SafeArrayGetLBound( pBlock, 3, &n );
    assert( n == 0 );

    SafeArrayGetUBound( pBlock, 2, &n );
    printf( "n = %ld\n", n );
    assert( n == 14 );

    SafeArrayGetUBound( pBlock, 3, &n );
    assert( n == 9 );

    SafeArrayGetUBound( pBlock, 1, &n );
    n++;
    printf( "nBands = %d\n", n );

    unsigned char      *pData;
    hr = SafeArrayAccessData( pBlock, (void **) &pData );
    assert( !FAILED(hr) );

    for( int iBand = 0; iBand < n; iBand++ )
    {
        printf( "Band %d\n", iBand+1 );
        for( int iY = 0; iY < 10; iY++ )
        {
            printf( "%2d: ", iY );
            for( int iX = 0; iX < 15; iX++ )
            {
                printf( "%3d ", pData[iBand + iX*n + iY*n*15] );
            }
            printf( "\n" );
        }
    }
        
    LONG      anIndices[3];
    unsigned char byValue;

    anIndices[0] = 0;
    anIndices[1] = 4;
    anIndices[2] = 2;
    SafeArrayGetElement( pBlock, anIndices, (void *) &byValue );
    printf( "byValue[4,2,0] = %d\n", byValue );

    assert( byValue == pData[0 + 4*n + 2*n*15] );

    printf( "\n" );
    SafeArrayUnaccessData( pBlock );

    SafeArrayDestroy( pBlock );
}

