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
 * Revision 1.17  2005/11/17 22:02:32  fwarmerdam
 * avoid overwriting existing .aux file, overview filename now CPLString
 *
 * Revision 1.16  2005/10/14 21:11:28  fwarmerdam
 * avoid all-bands test for imagine files
 *
 * Revision 1.15  2005/10/12 18:21:57  fwarmerdam
 * use GDALClose instead of delete in case dataset opened shared
 *
 * Revision 1.14  2005/09/26 15:52:03  fwarmerdam
 * centralized .aux opening logic
 *
 * Revision 1.13  2005/09/17 03:46:18  fwarmerdam
 * added USE_RRD support to create overviews
 *
 * Revision 1.12  2005/09/16 20:32:44  fwarmerdam
 * added preliminary .aux support (read only)
 *
 * Revision 1.11  2005/05/10 04:48:42  fwarmerdam
 * GDALOvLevelAdjust now public
 *
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
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        GDALDefaultOverviews()                        */
/************************************************************************/

GDALDefaultOverviews::GDALDefaultOverviews()

{
    poDS = NULL;
    poODS = NULL;
    bOvrIsAux = FALSE;
}

/************************************************************************/
/*                       ~GDALDefaultOverviews()                        */
/************************************************************************/

GDALDefaultOverviews::~GDALDefaultOverviews()

{
    if( poODS != NULL )
    {
        poODS->FlushCache();
        GDALClose( poODS );
        poODS = NULL;
    }
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

void GDALDefaultOverviews::Initialize( GDALDataset *poDSIn,
                                       const char * pszBasename,
                                       int bNameIsOVR )

{
    VSIStatBufL sStatBuf;

/* -------------------------------------------------------------------- */
/*      If we were already initialized, destroy the old overview        */
/*      file handle.                                                    */
/* -------------------------------------------------------------------- */
    if( poODS != NULL )
    {
        GDALClose( poODS );
        poODS = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open overview dataset if it exists.                             */
/* -------------------------------------------------------------------- */
    int bExists;

    poDS = poDSIn;
    
    if( pszBasename == NULL )
        pszBasename = poDS->GetDescription();

    if( bNameIsOVR )
        osOvrFilename = pszBasename;
    else
        osOvrFilename.Printf( "%s.ovr", pszBasename );

    bExists = VSIStatL( osOvrFilename, &sStatBuf ) == 0;

#if !defined(WIN32)
    if( !bNameIsOVR && !bExists )
    {
        osOvrFilename.Printf( "%s.OVR", pszBasename );
        bExists = VSIStatL( osOvrFilename, &sStatBuf ) == 0;
        if( !bExists )
            osOvrFilename.Printf( "%s.ovr", pszBasename );
    }
#endif

    if( bExists )
    {
        poODS = (GDALDataset *) GDALOpen( osOvrFilename, poDS->GetAccess() );
    }

/* -------------------------------------------------------------------- */
/*      We didn't find that, so try and find a corresponding aux        */
/*      file.  Check that we are the dependent file of the aux          */
/*      file.                                                           */
/* -------------------------------------------------------------------- */
    if( !poODS )
    {
        poODS = GDALFindAssociatedAuxFile( pszBasename, poDS->GetAccess() );

        if( poODS )
        {
            bOvrIsAux = TRUE;
            osOvrFilename = poODS->GetDescription();
        }
    }
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
    {
        if( bOvrIsAux )
            return poBand->GetOverviewCount();
        else
            return poBand->GetOverviewCount() + 1;
    }
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

    if( bOvrIsAux )
        return poBand->GetOverview( iOverview );

    else // TIFF case, base is overview 0.
    {
        if( iOverview == 0 )
            return poBand;
        else if( iOverview-1 >= poBand->GetOverviewCount() )
            return NULL;
        else
            return poBand->GetOverview( iOverview-1 );
    }
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

int GDALOvLevelAdjust( int nOvLevel, int nXSize )

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
/*      If we don't already have an overview file, we need to decide    */
/*      what format to use.                                             */
/* -------------------------------------------------------------------- */
    if( poODS == NULL )
    {
        bOvrIsAux = CSLTestBoolean(CPLGetConfigOption( "USE_RRD", "NO" ));
        if( bOvrIsAux )
        {
            VSIStatBufL sStatBuf;

            osOvrFilename = CPLResetExtension(poDS->GetDescription(),"aux");

            if( VSIStatL( osOvrFilename, &sStatBuf ) == 0 )
                osOvrFilename.Printf( "%s.aux", poDS->GetDescription() );
        }
    }

/* -------------------------------------------------------------------- */
/*      Our TIFF overview support currently only works safely if all    */
/*      bands are handled at the same time.                             */
/* -------------------------------------------------------------------- */
    if( !bOvrIsAux && nBands != poDS->GetRasterCount() )
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
    if( pszBasename == NULL && osOvrFilename.length() == 0  )
        pszBasename = poDS->GetDescription();

    if( pszBasename != NULL )
    {
        if( bOvrIsAux )
            osOvrFilename.Printf( "%s.aux", pszBasename );
        else
            osOvrFilename.Printf( "%s.ovr", pszBasename );
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
/*      Build band list.                                                */
/* -------------------------------------------------------------------- */
    pahBands = (GDALRasterBand **) CPLCalloc(sizeof(GDALRasterBand *),nBands);
    for( i = 0; i < nBands; i++ )
        pahBands[i] = poDS->GetRasterBand( panBandList[i] );

/* -------------------------------------------------------------------- */
/*      Build new overviews - Imagine.  Keep existing file open if      */
/*      we have it.  But mark all overviews as in need of               */
/*      regeneration, since HFAAuxBuildOverviews() doesn't actually     */
/*      produce the imagery.                                            */
/* -------------------------------------------------------------------- */
    if( bOvrIsAux )
    {
        eErr = HFAAuxBuildOverviews( osOvrFilename, poDS, &poODS,
                                     nBands, panBandList,
                                     nNewOverviews, panNewOverviewList, 
                                     pszResampling, 
                                     pfnProgress, pProgressData );

        int j;
        
        for( j = 0; j < nOverviews; j++ )
        {
            if( panOverviewList[j] > 0 )
                panOverviewList[j] *= -1;
        }
    }

/* -------------------------------------------------------------------- */
/*      Build new overviews - TIFF.  Close TIFF files while we          */
/*      operate on it.                                                  */
/* -------------------------------------------------------------------- */
    else
    {
        if( poODS != NULL )
        {
            delete poODS;
            poODS = NULL;
        }

        eErr = GTIFFBuildOverviews( osOvrFilename, nBands, pahBands, 
                                    nNewOverviews, panNewOverviewList, 
                                    pszResampling, pfnProgress, pProgressData );

        if( eErr == CE_None )
        {
            poODS = (GDALDataset *) GDALOpen( osOvrFilename, GA_Update );
            if( poODS == NULL )
                eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Refresh old overviews that were listed.                         */
/* -------------------------------------------------------------------- */
    GDALRasterBand **papoOverviewBands;

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
                    //panOverviewList[i] *= -1;
                    papoOverviewBands[nNewOverviews++] = poOverview;
                }
            }
        }

        if( nNewOverviews > 0 )
        {
            eErr = GDALRegenerateOverviews( poBand, 
                                            nNewOverviews, papoOverviewBands,
                                            pszResampling, 
                                            pfnProgress, pProgressData );
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

