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
 * Revision 1.2  2000/06/19 14:42:27  warmerda
 * Don't close old overviews till after we have identified which already
 * exist, otherwise multiple copies of overviews may be created.
 *
 * Revision 1.1  2000/04/21 21:54:05  warmerda
 * New
 *
 */

#include "gdal_priv.h"

/************************************************************************/
/*                        GDALDefaultOverviews()                        */
/************************************************************************/

GDALDefaultOverviews::GDALDefaultOverviews()

{
    poDS = NULL;
    poODS = NULL;
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
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

void GDALDefaultOverviews::Initialize( GDALDataset *poDS,
                                       const char * pszBasename )

{
    char * pszTIFFFilename;
    VSIStatBuf sStatBuf;

/* -------------------------------------------------------------------- */
/*      If we were already initialized, destroy the old overview        */
/*      file handle.                                                    */
/* -------------------------------------------------------------------- */
    if( this->poODS != NULL )
    {
        delete poODS;
    }

/* -------------------------------------------------------------------- */
/*      Open overview dataset if it exists.                             */
/* -------------------------------------------------------------------- */
    this->poDS = poDS;
    
    if( pszBasename == NULL )
        pszBasename = poDS->GetDescription();

    pszTIFFFilename = (char *) CPLMalloc(strlen(pszBasename)+5);
    sprintf( pszTIFFFilename, "%s.ovr", pszBasename );

    if( VSIStat( pszTIFFFilename, &sStatBuf ) == 0 )
        poODS = (GDALDataset *) GDALOpen( pszTIFFFilename, poDS->GetAccess() );

    CPLFree( pszTIFFFilename );
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
    char * pszTIFFFilename;
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
                  " support when operating on all bands.\n" 
                  "Operation failed.\n" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      By default we assume that the dataset name is a suitable        */
/*      filesystem object to associate with if nothing is provided.     */
/* -------------------------------------------------------------------- */
    if( pszBasename == NULL )
        pszBasename = poDS->GetDescription();

/* -------------------------------------------------------------------- */
/*      Generate the TIFF filename.  This approach may not work well    */
/*      on Windows.                                                     */
/* -------------------------------------------------------------------- */
    pszTIFFFilename = (char *) CPLMalloc(strlen(pszBasename)+5);
    sprintf( pszTIFFFilename, "%s.ovr", pszBasename );

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

            if( nOvFactor == panOverviewList[i] )
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

    eErr = GTIFFBuildOverviews( pszTIFFFilename, nBands, pahBands, 
                                nNewOverviews, panNewOverviewList, 
                                pszResampling, pfnProgress, pProgressData );

/* -------------------------------------------------------------------- */
/*      Refresh old overviews that were listed.                         */
/* -------------------------------------------------------------------- */
    GDALRasterBand **papoOverviewBands;

    if( eErr == CE_None )
    {
        poODS = (GDALDataset *) GDALOpen( pszTIFFFilename, GA_Update );
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

                if( nOvFactor == -1 * panOverviewList[i] )
                {
                    panOverviewList[i] *= -1;
                    papoOverviewBands[nNewOverviews++] = poBand;
                }
            }
        }

        if( nNewOverviews > 0 )
        {
            eErr = GDALRegenerateOverviews( poBand, 
                                            nNewOverviews, papoOverviewBands,
                                            pszResampling, NULL, NULL );
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree( papoOverviewBands );
    CPLFree( panNewOverviewList );
    CPLFree( pahBands );
    CPLFree( pszTIFFFilename );

    return eErr;
}
