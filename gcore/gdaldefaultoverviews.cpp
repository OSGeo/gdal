/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Helper code to implement overview support in different drivers.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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
 * Revision 1.10  2004/07/26 22:32:30  warmerda
 * Support .OVR files as well as .ovr
 *
 * Revision 1.9  2002/07/09 20:33:12  warmerda
 * expand tabs
 *
 * Revision 1.8  2001/10/18 14:35:22  warmerda
 * avoid conflicts between parameters and member data
 *
 * Revision 1.7  2001/07/18 04:04:30  warmerda
 * added CPL_CVSID
 *
 * Revision 1.6  2001/06/22 21:00:38  warmerda
 * fixed several problems with regenerating existing overviews
 *
 * Revision 1.5  2001/06/22 13:52:03  warmerda
 * fixed bug when refreshing overviews during build
 *
 * Revision 1.4  2001/06/20 16:08:54  warmerda
 * GDALDefaultOverviews now remembers ovr filename, and allows explicit setting
 *
 * Revision 1.3  2000/06/19 18:48:49  warmerda
 * fixed message
 *
 * Revision 1.2  2000/06/19 14:42:27  warmerda
 * Don't close old overviews till after we have identified which already
 * exist, otherwise multiple copies of overviews may be created.
 *
 * Revision 1.1  2000/04/21 21:54:05  warmerda
 * New
 *
 */

#include "gdal_priv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        GDALDefaultOverviews()                        */
/************************************************************************/

GDALDefaultOverviews::GDALDefaultOverviews()

{
    poDS = NULL;
    poODS = NULL;
    pszOvrFilename = NULL;
}

/************************************************************************/
/*                       ~GDALDefaultOverviews()                        */
/************************************************************************/

GDALDefaultOverviews::~GDALDefaultOverviews()

{
    if( poODS != NULL )
    {
        poODS->FlushCache();
        delete poODS;
    }
    CPLFree( pszOvrFilename );
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

void GDALDefaultOverviews::Initialize( GDALDataset *poDSIn,
                                       const char * pszBasename,
                                       int bNameIsOVR )

{
    VSIStatBuf sStatBuf;

/* -------------------------------------------------------------------- */
/*      If we were already initialized, destroy the old overview        */
/*      file handle.                                                    */
/* -------------------------------------------------------------------- */
    if( poODS != NULL )
    {
        delete poODS;
    }

/* -------------------------------------------------------------------- */
/*      Open overview dataset if it exists.                             */
/* -------------------------------------------------------------------- */
    int bExists;

    poDS = poDSIn;
    
    if( pszBasename == NULL )
        pszBasename = poDS->GetDescription();

    CPLFree( pszOvrFilename );
    pszOvrFilename = (char *) CPLMalloc(strlen(pszBasename)+5);
    if( bNameIsOVR )
        strcpy( pszOvrFilename, pszBasename );
    else
        sprintf( pszOvrFilename, "%s.ovr", pszBasename );

    bExists = VSIStat( pszOvrFilename, &sStatBuf ) == 0;

#if !defined(WIN32)
    if( !bNameIsOVR && !bExists )
    {
        sprintf( pszOvrFilename, "%s.OVR", pszBasename );
        bExists = VSIStat( pszOvrFilename, &sStatBuf ) == 0;
        if( !bExists )
            sprintf( pszOvrFilename, "%s.ovr", pszBasename );
    }
#endif

    if( bExists )
        poODS = (GDALDataset *) GDALOpen( pszOvrFilename, poDS->GetAccess() );
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int GDALDefaultOverviews::GetOverviewCount( int nBand )

{
    GDALRasterBand * poBand;

    if( poODS == NULL || nBand < 1 || nBand > poODS->GetRasterCount() )
        return 0;

    poBand = poODS->GetRasterBand( nBand );
    if( poBand == NULL )
        return 0;
    else
        return poBand->GetOverviewCount() + 1;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *
GDALDefaultOverviews::GetOverview( int nBand, int iOverview )

{
    GDALRasterBand * poBand;

    if( poODS == NULL || nBand < 1 || nBand > poODS->GetRasterCount() )
        return NULL;

    poBand = poODS->GetRasterBand( nBand );
    if( poBand == NULL )
        return NULL;

    if( iOverview == 0 )
        return poBand;
    else if( iOverview-1 >= poBand->GetOverviewCount() )
        return NULL;
    else
        return poBand->GetOverview( iOverview-1 );
}

/************************************************************************/
/*                         GDALOvLevelAdjust()                          */
/*                                                                      */
/*      Some overview levels cannot be achieved closely enough to be    */
/*      recognised as the desired overview level.  This function        */
/*      will adjust an overview level to one that is achievable on      */
/*      the given raster size.                                          */
/*                                                                      */
/*      For instance a 1200x1200 image on which a 256 level overview    */
/*      is request will end up generating a 5x5 overview.  However,     */
/*      this will appear to the system be a level 240 overview.         */
/*      This function will adjust 256 to 240 based on knowledge of      */
/*      the image size.                                                 */
/************************************************************************/

static int GDALOvLevelAdjust( int nOvLevel, int nXSize )

{
    int nOXSize = (nXSize + nOvLevel - 1) / nOvLevel;
    
    return (int) (0.5 + nXSize / (double) nOXSize);
}
    
/************************************************************************/
/*                     GDALDefaultBuildOverviews()                      */
/************************************************************************/

CPLErr
GDALDefaultOverviews::BuildOverviews( 
    const char * pszBasename,
    const char * pszResampling, 
    int nOverviews, int * panOverviewList,
    int nBands, int * panBandList,
    GDALProgressFunc pfnProgress, void * pProgressData)

{
    GDALRasterBand **pahBands;
    CPLErr       eErr;
    int          i;

/* -------------------------------------------------------------------- */
/*      Our TIFF overview support currently only works safely if all    */
/*      bands are handled at the same time.                             */
/* -------------------------------------------------------------------- */
    if( nBands != poDS->GetRasterCount() )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Generation of overviews in external TIFF currently only"
                  " supported when operating on all bands.\n" 
                  "Operation failed.\n" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      If a basename is provided, use it to override the internal      */
/*      overview filename.                                              */
/* -------------------------------------------------------------------- */
    if( pszBasename == NULL && pszOvrFilename == NULL )
        pszBasename = poDS->GetDescription();

    if( pszBasename != NULL )
    {
        CPLFree( pszOvrFilename );
        pszOvrFilename = (char *) CPLMalloc(strlen(pszBasename)+5);
        sprintf( pszOvrFilename, "%s.ovr", pszBasename );
    }

/* -------------------------------------------------------------------- */
/*      Establish which of the overview levels we already have, and     */
/*      which are new.  We assume that band 1 of the file is            */
/*      representative.                                                 */
/* -------------------------------------------------------------------- */
    int   nNewOverviews, *panNewOverviewList = NULL;
    GDALRasterBand *poBand = poDS->GetRasterBand( 1 );

    nNewOverviews = 0;
    panNewOverviewList = (int *) CPLCalloc(sizeof(int),nOverviews);
    for( i = 0; i < nOverviews && poBand != NULL; i++ )
    {
        int   j;

        for( j = 0; j < poBand->GetOverviewCount(); j++ )
        {
            int    nOvFactor;
            GDALRasterBand * poOverview = poBand->GetOverview( j );
 
            nOvFactor = (int) 
                (0.5 + poBand->GetXSize() / (double) poOverview->GetXSize());

            if( nOvFactor == panOverviewList[i] 
                || nOvFactor == GDALOvLevelAdjust( panOverviewList[i], 
                                                   poBand->GetXSize() ) )
                panOverviewList[i] *= -1;
        }

        if( panOverviewList[i] > 0 )
            panNewOverviewList[nNewOverviews++] = panOverviewList[i];
    }

/* -------------------------------------------------------------------- */
/*      If we have an existing overview file open, close it now.        */
/* -------------------------------------------------------------------- */
    if( poODS != NULL )
    {
        delete poODS;
        poODS = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Build band list.                                                */
/* -------------------------------------------------------------------- */
    pahBands = (GDALRasterBand **) CPLCalloc(sizeof(GDALRasterBand *),nBands);
    for( i = 0; i < nBands; i++ )
        pahBands[i] = poDS->GetRasterBand( panBandList[i] );

/* -------------------------------------------------------------------- */
/*      Build new overviews.                                            */
/* -------------------------------------------------------------------- */

    eErr = GTIFFBuildOverviews( pszOvrFilename, nBands, pahBands, 
                                nNewOverviews, panNewOverviewList, 
                                pszResampling, pfnProgress, pProgressData );

/* -------------------------------------------------------------------- */
/*      Refresh old overviews that were listed.                         */
/* -------------------------------------------------------------------- */
    GDALRasterBand **papoOverviewBands;

    if( eErr == CE_None )
    {
        poODS = (GDALDataset *) GDALOpen( pszOvrFilename, GA_Update );
        if( poODS == NULL )
            eErr = CE_Failure;
    }

    papoOverviewBands = (GDALRasterBand **) 
        CPLCalloc(sizeof(void*),nOverviews);

    for( int iBand = 0; iBand < nBands && eErr == CE_None; iBand++ )
    {
        poBand = poDS->GetRasterBand( panBandList[iBand] );

        nNewOverviews = 0;
        for( i = 0; i < nOverviews && poBand != NULL; i++ )
        {
            int   j;
            
            for( j = 0; j < poBand->GetOverviewCount(); j++ )
            {
                int    nOvFactor;
                GDALRasterBand * poOverview = poBand->GetOverview( j );

                nOvFactor = (int) 
                    (0.5 + poBand->GetXSize() / (double) poOverview->GetXSize());

                if( nOvFactor == - panOverviewList[i] 
                    || nOvFactor == GDALOvLevelAdjust( -panOverviewList[i], 
                                                       poBand->GetXSize() ) )
                {
                    panOverviewList[i] *= -1;
                    papoOverviewBands[nNewOverviews++] = poOverview;
                }
            }
        }

        if( nNewOverviews > 0 )
        {
            eErr = GDALRegenerateOverviews( poBand, 
                                            nNewOverviews, papoOverviewBands,
                                            pszResampling, 
                                            GDALDummyProgress, NULL );
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree( papoOverviewBands );
    CPLFree( panNewOverviewList );
    CPLFree( pahBands );

    return eErr;
}

