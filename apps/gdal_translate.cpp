/* ****************************************************************************
 * $Id$
 *
 * Project:  GDAL Utilities
 * Purpose:  GDAL Image Translator Program
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 * ****************************************************************************
 * Copyright (c) 1998, 2002, Frank Warmerdam
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
 * ****************************************************************************
 *
 * $Log$
 * Revision 1.6  2002/10/04 20:47:31  warmerda
 * fixed type override going through virtual db
 *
 * Revision 1.5  2002/09/11 14:22:33  warmerda
 * make an effort to preserve band descriptions
 *
 * Revision 1.4  2002/08/27 16:49:28  dron
 * Check for subdatasets in input file.
 *
 * Revision 1.3  2002/08/07 02:17:10  warmerda
 * Fixed -projwin messages.
 *
 * Revision 1.2  2002/08/06 17:47:54  warmerda
 * added the -projwin switch
 *
 * Revision 1.1  2002/06/24 19:59:57  warmerda
 * New
 *
 * Revision 1.27  2002/06/24 19:36:25  warmerda
 * carry over colortable
 *
 * Revision 1.26  2002/06/12 21:07:55  warmerda
 * test create and createcopy capability
 *
 * Revision 1.25  2002/04/16 17:17:18  warmerda
 * Ensure variables initialized.
 *
 * Revision 1.24  2002/04/16 14:00:25  warmerda
 * added GDALVersionInfo
 *
 * Revision 1.23  2002/02/07 20:24:18  warmerda
 * added -scale option
 *
 * Revision 1.22  2001/12/12 21:01:01  warmerda
 * fixed bug in windowing logic
 *
 * Revision 1.21  2001/09/24 15:55:11  warmerda
 * added cleanup percent done
 *
 * Revision 1.20  2001/08/23 03:23:46  warmerda
 * added the -not_strict switch
 *
 * Revision 1.19  2001/07/18 05:05:12  warmerda
 * added CPL_CSVID
 *
 * Revision 1.18  2001/03/21 15:00:49  warmerda
 * Escape % signs in printf() format.
 *
 * Revision 1.17  2001/03/20 20:20:14  warmerda
 * Added % handling, fixed completion, fixed adGeoTransform (Mark Salazar)
 *
 * Revision 1.16  2001/02/19 15:12:17  warmerda
 * fixed windowing problem with -srcwin
 */

#include "cpl_vsi.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdal_priv.h"
#include "vrt/vrtdataset.h"

CPL_CVSID("$Id$");

static int ArgIsNumeric( const char * );

/*  ******************************************************************* */
/*                               Usage()                                */
/* ******************************************************************** */

static void Usage()

{
    int	iDr;
        
    printf( "Usage: gdal_translate [--version]\n"
            "       [-ot {Byte/Int16/UInt16/UInt32/Int32/Float32/Float64/\n"
            "             CInt16/CInt32/CFloat32/CFloat64}] [-not_strict]\n"
            "       [-of format] [-b band] [-outsize xsize[%%] ysize[%%]]\n"
            "       [-scale [src_min src_max [dst_min dst_max]]]\n"
            "       [-srcwin xoff yoff xsize ysize]\n"
            "       [-projwin ulx uly lrx lry] [-co \"NAME=VALUE\"]*\n"
            "       src_dataset dst_dataset\n\n" );

    printf( "%s\n\n", GDALVersionInfo( "--version" ) );
    printf( "The following format drivers are configured and support output:\n" );
    for( iDr = 0; iDr < GDALGetDriverCount(); iDr++ )
    {
        GDALDriverH hDriver = GDALGetDriver(iDr);
        
        if( GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, NULL ) != NULL
            || GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATECOPY,
                                    NULL ) != NULL )
        {
            printf( "  %s: %s\n",
                    GDALGetDriverShortName( hDriver ),
                    GDALGetDriverLongName( hDriver ) );
        }
    }
}

/* ******************************************************************** */
/*                                main()                                */
/* ******************************************************************** */

int main( int argc, char ** argv )

{
    GDALDatasetH	hDataset, hOutDS;
    GDALRasterBandH	hBand;
    int			i, nSrcBand = -1;
    int			nRasterXSize, nRasterYSize;
    const char		*pszSource=NULL, *pszDest=NULL, *pszFormat = "GTiff";
    GDALDriverH		hDriver;
    int			*panBandList, nBandCount;
    double		adfGeoTransform[6];
    GDALDataType	eOutputType = GDT_Unknown;
    int			nOXSize = 0, nOYSize = 0;
    char		*pszOXSize=NULL, *pszOYSize=NULL;
    char                **papszCreateOptions = NULL;
    int                 anSrcWin[4], bStrict = TRUE;
    const char          *pszProjection;
    int                 bScale = FALSE, bHaveScaleSrc = FALSE;
    double	        dfScaleSrcMin=0.0, dfScaleSrcMax=255.0;
    double              dfScaleDstMin=0.0, dfScaleDstMax=255.0;
    double              dfULX, dfULY, dfLRX, dfLRY;

    anSrcWin[0] = 0;
    anSrcWin[1] = 0;
    anSrcWin[2] = 0;
    anSrcWin[3] = 0;

    dfULX = dfULY = dfLRX = dfLRY = 0.0;

/* -------------------------------------------------------------------- */
/*      Register standard GDAL drivers, and identify output driver.     */
/* -------------------------------------------------------------------- */
    GDALAllRegister();

/* -------------------------------------------------------------------- */
/*      Handle command line arguments.                                  */
/* -------------------------------------------------------------------- */
    for( i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i],"-of") && i < argc-1 )
            pszFormat = argv[++i];

        else if( EQUAL(argv[i],"--version") )
        {
            printf( "%s\n", GDALVersionInfo( "--version" ) );
        }

        else if( EQUAL(argv[i],"-ot") && i < argc-1 )
        {
            int	iType;
            
            for( iType = 1; iType < GDT_TypeCount; iType++ )
            {
                if( GDALGetDataTypeName((GDALDataType)iType) != NULL
                    && EQUAL(GDALGetDataTypeName((GDALDataType)iType),
                             argv[i+1]) )
                {
                    eOutputType = (GDALDataType) iType;
                }
            }

            if( eOutputType == GDT_Unknown )
            {
                printf( "Unknown output pixel type: %s\n", argv[i+1] );
                Usage();
                exit( 2 );
            }
            i++;
        }
        else if( EQUAL(argv[i],"-b") && i < argc-1 )
            nSrcBand = atoi(argv[++i]);
            
        else if( EQUAL(argv[i],"-not_strict")  )
            bStrict = FALSE;
            
        else if( EQUAL(argv[i],"-co") && i < argc-1 )
        {
            papszCreateOptions = CSLAddString( papszCreateOptions, argv[++i] );
        }   

        else if( EQUAL(argv[i],"-scale") )
        {
            bScale = TRUE;
            if( i < argc-2 && ArgIsNumeric(argv[i+1]) )
            {
                bHaveScaleSrc = TRUE;
                dfScaleSrcMin = atof(argv[i+1]);
                dfScaleSrcMax = atof(argv[i+2]);
                i += 2;
            }
            if( i < argc-2 && bHaveScaleSrc && ArgIsNumeric(argv[i+1]) )
            {
                dfScaleDstMin = atof(argv[i+1]);
                dfScaleDstMax = atof(argv[i+2]);
                i += 2;
            }
            else
            {
                dfScaleDstMin = 0.0;
                dfScaleDstMax = 255.999;
            }
        }   

        else if( EQUAL(argv[i],"-outsize") && i < argc-2 )
        {
            pszOXSize = argv[++i];
            pszOYSize = argv[++i];
        }   

        else if( EQUAL(argv[i],"-srcwin") && i < argc-4 )
        {
            anSrcWin[0] = atoi(argv[++i]);
            anSrcWin[1] = atoi(argv[++i]);
            anSrcWin[2] = atoi(argv[++i]);
            anSrcWin[3] = atoi(argv[++i]);
        }   

        else if( EQUAL(argv[i],"-projwin") && i < argc-4 )
        {
            dfULX = atof(argv[++i]);
            dfULY = atof(argv[++i]);
            dfLRX = atof(argv[++i]);
            dfLRY = atof(argv[++i]);
        }   

        else if( argv[i][0] == '-' )
        {
            printf( "Option %s incomplete, or not recognised.\n\n", 
                    argv[i] );
            Usage();
            exit( 2 );
        }
        else if( pszSource == NULL )
            pszSource = argv[i];

        else if( pszDest == NULL )
            pszDest = argv[i];

        else
        {
            printf( "Too many command options.\n\n" );
            Usage();
            exit( 2 );
        }
    }

    if( pszDest == NULL )
    {
        Usage();
        exit( 10 );
    }

/* -------------------------------------------------------------------- */
/*      Attempt to open source file.                                    */
/* -------------------------------------------------------------------- */

    hDataset = GDALOpen( pszSource, GA_ReadOnly );
    
    if( hDataset == NULL )
    {
        fprintf( stderr,
                 "GDALOpen failed - %d\n%s\n",
                 CPLGetLastErrorNo(), CPLGetLastErrorMsg() );
        exit( 1 );
    }

    if( CSLCount(GDALGetMetadata( hDataset, "SUBDATASETS" )) > 0 )
    {
        fprintf( stderr,
            "Input file contains subdatasets. Please, select one of them for reading.\n" );
        exit( 1 );
    }

    nRasterXSize = GDALGetRasterXSize( hDataset );
    nRasterYSize = GDALGetRasterYSize( hDataset );
    
    printf( "Size is %d, %d\n", nRasterXSize, nRasterYSize );

    if( anSrcWin[2] == 0 && anSrcWin[3] == 0 )
    {
        anSrcWin[2] = nRasterXSize;
        anSrcWin[3] = nRasterYSize;
    }

/* -------------------------------------------------------------------- */
/*	Build band list to translate					*/
/* -------------------------------------------------------------------- */
    if( nSrcBand != -1 )
    {
        nBandCount = 1;
        panBandList = (int *) CPLMalloc(sizeof(int)*nBandCount);
        panBandList[0] = nSrcBand;
    }
    else
    {
        nBandCount = GDALGetRasterCount( hDataset );
        panBandList = (int *) CPLMalloc(sizeof(int)*nBandCount);
        for( i = 0; i < nBandCount; i++ )
            panBandList[i] = i+1;
    }

/* -------------------------------------------------------------------- */
/*      Compute the source window from the projected source window      */
/*      if the projected coordinates were provided.  Note that the      */
/*      projected coordinates are in ulx, uly, lrx, lry format,         */
/*      while the anSrcWin is xoff, yoff, xsize, ysize with the         */
/*      xoff,yoff being the ulx, uly in pixel/line.                     */
/* -------------------------------------------------------------------- */
    if( dfULX != 0.0 || dfULY != 0.0 
        || dfLRX != 0.0 || dfLRY != 0.0 )
    {
        double	adfGeoTransform[6];

        GDALGetGeoTransform( hDataset, adfGeoTransform );

        if( adfGeoTransform[2] != 0.0 || adfGeoTransform[4] != 0.0 )
        {
            fprintf( stderr, 
                     "The -projwin option was used, but the geotransform is\n"
                     "rotated.  This configuration is not supported.\n" );
            exit( 1 );
        }

        anSrcWin[0] = (int) 
            ((dfULX - adfGeoTransform[0]) / adfGeoTransform[1] + 0.001);
        anSrcWin[1] = (int) 
            ((dfULY - adfGeoTransform[3]) / adfGeoTransform[5] + 0.001);

        anSrcWin[2] = (int) ((dfLRX - dfULX) / adfGeoTransform[1] + 0.5);
        anSrcWin[3] = (int) ((dfLRY - dfULY) / adfGeoTransform[5] + 0.5);

        fprintf( stdout, 
                 "Computed -srcwin %d %d %d %d from projected window.\n",
                 anSrcWin[0], 
                 anSrcWin[1], 
                 anSrcWin[2], 
                 anSrcWin[3] );

        if( anSrcWin[0] < 0 || anSrcWin[1] < 0 
            || anSrcWin[0] + anSrcWin[2] > GDALGetRasterXSize(hDataset) 
            || anSrcWin[1] + anSrcWin[3] > GDALGetRasterYSize(hDataset) )
        {
            fprintf( stderr, 
                     "Computed -srcwin falls outside raster size of %dx%d.\n",
                     GDALGetRasterXSize(hDataset), 
                     GDALGetRasterYSize(hDataset) );
        }
    }

/* -------------------------------------------------------------------- */
/*      Find the output driver.                                         */
/* -------------------------------------------------------------------- */
    hDriver = GDALGetDriverByName( pszFormat );
    if( hDriver == NULL )
    {
        int	iDr;
        
        printf( "Output driver `%s' not recognised.\n", pszFormat );
        printf( "The following format drivers are configured and support output:\n" );
        for( iDr = 0; iDr < GDALGetDriverCount(); iDr++ )
        {
            GDALDriverH hDriver = GDALGetDriver(iDr);

            if( GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, NULL ) != NULL
              || GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATECOPY,
                                      NULL ) != NULL )
            {
                printf( "  %s: %s\n",
                        GDALGetDriverShortName( hDriver  ),
                        GDALGetDriverLongName( hDriver ) );
            }
        }
        printf( "\n" );
        Usage();
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      The short form is to CreateCopy().  We use this if the input    */
/*      matches the whole dataset.  Eventually we should rewrite        */
/*      this entire program to use virtual datasets to construct a      */
/*      virtual input source to copy from.                              */
/* -------------------------------------------------------------------- */
    if( eOutputType == GDT_Unknown 
        && !bScale
        && nBandCount == GDALGetRasterCount(hDataset)
        && anSrcWin[0] == 0 && anSrcWin[1] == 0 
        && anSrcWin[2] == GDALGetRasterXSize(hDataset)
        && anSrcWin[3] == GDALGetRasterYSize(hDataset) 
        && pszOXSize == NULL && pszOYSize == NULL )
    {
        
        hOutDS = GDALCreateCopy( hDriver, pszDest, hDataset, 
                                 bStrict, papszCreateOptions, 
                                 GDALTermProgress, NULL );
        if( hOutDS != NULL )
            GDALClose( hOutDS );
        
        GDALClose( hDataset );

        exit( hOutDS == NULL );
    }

/* -------------------------------------------------------------------- */
/*      Establish some parameters.                                      */
/* -------------------------------------------------------------------- */
    if( pszOXSize == NULL )
    {
        nOXSize = anSrcWin[2];
        nOYSize = anSrcWin[3];
    }
    else
    {
        nOXSize = (int) ((pszOXSize[strlen(pszOXSize)-1]=='%' 
                   ? atof(pszOXSize)/100*anSrcWin[2] : atoi(pszOXSize)));
        nOYSize = (int) ((pszOYSize[strlen(pszOYSize)-1]=='%' 
                   ? atof(pszOYSize)/100*anSrcWin[3] : atoi(pszOYSize)));
    }
    
/* ==================================================================== */
/*      Create a virtual dataset as long as no scaling is being         */
/*      applied.                                                        */
/* ==================================================================== */
    if( !bScale )
    {
        VRTDataset *poVDS;
        
/* -------------------------------------------------------------------- */
/*      Make a virtual clone.                                           */
/* -------------------------------------------------------------------- */
        poVDS = new VRTDataset( nOXSize, nOYSize );

        pszProjection = GDALGetProjectionRef( hDataset );
        if( pszProjection != NULL && strlen(pszProjection) > 0 )
            poVDS->SetProjection( pszProjection );

        if( GDALGetGeoTransform( hDataset, adfGeoTransform ) == CE_None )
        {
            adfGeoTransform[0] += anSrcWin[0] * adfGeoTransform[1]
                + anSrcWin[1] * adfGeoTransform[2];
            adfGeoTransform[3] += anSrcWin[0] * adfGeoTransform[4]
                + anSrcWin[1] * adfGeoTransform[5];

            adfGeoTransform[1] *= anSrcWin[2] / (double) nOXSize;
            adfGeoTransform[2] *= anSrcWin[3] / (double) nOYSize;
            adfGeoTransform[4] *= anSrcWin[2] / (double) nOXSize;
            adfGeoTransform[5] *= anSrcWin[3] / (double) nOYSize;
            
            poVDS->SetGeoTransform( adfGeoTransform );
        }
        
        poVDS->SetMetadata( ((GDALDataset*)hDataset)->GetMetadata() );

        for( i = 0; i < nBandCount; i++ )
        {
            VRTRasterBand *poVRTBand;
            GDALRasterBand *poSrcBand;
            GDALDataType  eBandType;

            poSrcBand = ((GDALDataset *) 
                         hDataset)->GetRasterBand(panBandList[i]);

            if( eOutputType == GDT_Unknown )
                eBandType = poSrcBand->GetRasterDataType();
            else
                eBandType = eOutputType;

            poVDS->AddBand( eBandType, NULL );
            poVRTBand = (VRTRasterBand *) poVDS->GetRasterBand( i+1 );
            
            poVRTBand->AddSimpleSource( poSrcBand,
                                        anSrcWin[0], anSrcWin[1], 
                                        anSrcWin[2], anSrcWin[3], 
                                        0, 0, nOXSize, nOYSize );

            poVRTBand->SetMetadata( poSrcBand->GetMetadata() );
            poVRTBand->SetColorTable( poSrcBand->GetColorTable() );
            poVRTBand->SetColorInterpretation(
                poSrcBand->GetColorInterpretation());
            if( strlen(poSrcBand->GetDescription()) > 0 )
                poVRTBand->SetDescription( poSrcBand->GetDescription() );
        }

/* -------------------------------------------------------------------- */
/*      Write to the output file using CopyCreate().                    */
/* -------------------------------------------------------------------- */
        hOutDS = GDALCreateCopy( hDriver, pszDest, (GDALDatasetH) poVDS,
                                 bStrict, papszCreateOptions, 
                                 GDALTermProgress, NULL );
        if( hOutDS != NULL )
            GDALClose( hOutDS );

        GDALClose( (GDALDatasetH) poVDS );
        
        GDALClose( hDataset );

        exit( hOutDS == NULL );
    }

/* -------------------------------------------------------------------- */
/*      Set the band type if not previously set.                        */
/* -------------------------------------------------------------------- */
    if( eOutputType == GDT_Unknown )
    {
        eOutputType = GDALGetRasterDataType( 
            GDALGetRasterBand( hDataset, panBandList[0] ) );
    }

/* -------------------------------------------------------------------- */
/*      Create the output database.                                     */
/* -------------------------------------------------------------------- */
    GDALTermProgress( 0.0, NULL, NULL );
    hOutDS = GDALCreate( hDriver, pszDest, nOXSize, nOYSize, 
                         nBandCount, eOutputType, papszCreateOptions );
    if( hOutDS == NULL )
    {
        printf( "GDALCreate() failed.\n" );
        exit( 10 );
    }
    
/* -------------------------------------------------------------------- */
/*	Copy over projection, and geotransform information.		*/
/* -------------------------------------------------------------------- */
    pszProjection = GDALGetProjectionRef( hDataset );
    if( pszProjection != NULL && strlen(pszProjection) > 0 )
        GDALSetProjection( hOutDS, pszProjection );

    if( GDALGetGeoTransform( hDataset, adfGeoTransform ) == CE_None )
    {
        adfGeoTransform[0] += anSrcWin[0] * adfGeoTransform[1]
                            + anSrcWin[1] * adfGeoTransform[2];
        adfGeoTransform[3] += anSrcWin[0] * adfGeoTransform[4]
                            + anSrcWin[1] * adfGeoTransform[5];

        adfGeoTransform[1] *= anSrcWin[2] / (double) nOXSize;
        adfGeoTransform[2] *= anSrcWin[3] / (double) nOYSize;
        adfGeoTransform[4] *= anSrcWin[2] / (double) nOXSize;
        adfGeoTransform[5] *= anSrcWin[3] / (double) nOYSize;

        GDALSetGeoTransform( hOutDS, adfGeoTransform );
    }

/* -------------------------------------------------------------------- */
/*      Loop copying bands.                                             */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nBandCount; i++ )
    {
        GByte	*pabyBlock;
        int     iBlockY;
        GDALRasterBandH hDstBand;
        double  dfScale = 1.0, dfOffset = 0.0;
        GDALColorTableH hCT;

        hBand = GDALGetRasterBand( hDataset, panBandList[i] );
        hDstBand = GDALGetRasterBand( hOutDS, i+1 );
        
        printf( "Band %d Type = %d\n",
                panBandList[i], GDALGetRasterDataType( hBand ) );

        if( strlen(GDALGetDescription(hBand)) > 0 )
            GDALSetDescription( hDstBand, GDALGetDescription(hBand) );

/* -------------------------------------------------------------------- */
/*      Do we need to copy a colortable?                                */
/* -------------------------------------------------------------------- */
        hCT = GDALGetRasterColorTable( hBand );
        if( hCT != NULL )
            GDALSetRasterColorTable( hBand, hCT );

/* -------------------------------------------------------------------- */
/*      Do we need to collect scaling information?                      */
/* -------------------------------------------------------------------- */
        if( bScale && !bHaveScaleSrc )
        {
            double	adfCMinMax[2];
            GDALComputeRasterMinMax( hBand, TRUE, adfCMinMax );
            dfScaleSrcMin = adfCMinMax[0];
            dfScaleSrcMax = adfCMinMax[1];
        }

        if( bScale )
        {
            if( dfScaleSrcMax == dfScaleSrcMin )
                dfScaleSrcMax += 0.1;
            if( dfScaleDstMax == dfScaleDstMin )
                dfScaleDstMax += 0.1;

            dfScale = (dfScaleDstMax - dfScaleDstMin) 
                    / (dfScaleSrcMax - dfScaleSrcMin);
            dfOffset = -1 * dfScaleSrcMin * dfScale + dfScaleDstMin;
        }

/* -------------------------------------------------------------------- */
/*      Write out the raw raster data.                                  */
/* -------------------------------------------------------------------- */
        pabyBlock = (GByte *) CPLMalloc(sizeof(double)*2*nOXSize);

        for( iBlockY = 0; iBlockY < nOYSize; iBlockY++ )
        {
            int		iSrcYOff;
            double      dfComplete;

            if( nOYSize == anSrcWin[3] )
                iSrcYOff = iBlockY + anSrcWin[1];
            else
            {
                iSrcYOff = (int) ((iBlockY / (double) nOYSize) * anSrcWin[3]
                                  + anSrcWin[1]);
                iSrcYOff = MAX(0,MIN(anSrcWin[1]+anSrcWin[3]-1,iSrcYOff));
            }

            if( !bScale )
            {
                GDALRasterIO( hBand, GF_Read,
                              anSrcWin[0], iSrcYOff, anSrcWin[2], 1,
                              pabyBlock, nOXSize, 1,
                              GDALGetRasterDataType(hBand),
                              0, 0 );
                
                GDALRasterIO( hDstBand, GF_Write,
                              0, iBlockY, nOXSize, 1,
                              pabyBlock, nOXSize, 1,
                              GDALGetRasterDataType(hBand),
                              0, 0 );
            }
            else
            {
                int   iPixel;

                GDALRasterIO( hBand, GF_Read,
                              anSrcWin[0], iSrcYOff, anSrcWin[2], 1,
                              pabyBlock, nOXSize, 1, GDT_Float64,
                              0, 0 );

                for( iPixel = 0; iPixel < nOXSize; iPixel++ )
                {
                    ((double *)pabyBlock)[iPixel] = 
                        ((double *)pabyBlock)[iPixel] * dfScale + dfOffset;
                }
                
                
                GDALRasterIO( hDstBand, GF_Write,
                              0, iBlockY, nOXSize, 1,
                              pabyBlock, nOXSize, 1, GDT_Float64,
                              0, 0 );
            }

            dfComplete = (i / (double) nBandCount)
                + ((iBlockY+1) / ((double) nOYSize*nBandCount));
            
            GDALTermProgress( dfComplete, NULL, NULL );
        }

        CPLFree( pabyBlock );
    }

    GDALTermProgress( 1.0000001, NULL, NULL );
    
    GDALClose( hOutDS );
    GDALClose( hDataset );
    
    exit( 0 );
}


/************************************************************************/
/*                            ArgIsNumeric()                            */
/************************************************************************/

int ArgIsNumeric( const char *pszArg )

{
    if( pszArg[0] == '-' )
        pszArg++;

    if( *pszArg == '\0' )
        return FALSE;

    while( *pszArg != '\0' )
    {
        if( (*pszArg < '0' || *pszArg > '9') && *pszArg != '.' )
            return FALSE;
        pszArg++;
    }
        
    return TRUE;
}
