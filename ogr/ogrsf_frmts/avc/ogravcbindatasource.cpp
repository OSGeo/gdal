/******************************************************************************
 * $Id$
 *
 * Project:  OGR
 * Purpose:  Implements OGRAVCBinDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.5  2005/09/21 01:00:55  fwarmerdam
 * fixup OGRFeatureDefn and OGRSpatialReference refcount handling
 *
 * Revision 1.4  2002/03/18 19:56:13  warmerda
 * added an error reset
 *
 * Revision 1.3  2002/02/18 20:38:42  warmerda
 * added TXT and TX6 support
 *
 * Revision 1.2  2002/02/14 23:01:04  warmerda
 * added region and attribute support
 *
 * Revision 1.1  2002/02/13 20:48:18  warmerda
 * New
 *
 */

#include "ogr_avc.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRAVCBinDataSource()                         */
/************************************************************************/

OGRAVCBinDataSource::OGRAVCBinDataSource()

{
    pszName = NULL;
    papoLayers = NULL;
    nLayers = 0;
    poSRS = NULL;
}

/************************************************************************/
/*                        ~OGRAVCBinDataSource()                        */
/************************************************************************/

OGRAVCBinDataSource::~OGRAVCBinDataSource()

{
    if( psAVC )
    {
        AVCE00ReadClose( psAVC );
        psAVC = NULL;
    }

    CPLFree( pszName );

    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    
    CPLFree( papoLayers );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRAVCBinDataSource::Open( const char * pszNewName, int bTestOpen )

{
/* -------------------------------------------------------------------- */
/*      Open the source file.  Supress error reporting if we are in     */
/*      TestOpen mode.                                                  */
/* -------------------------------------------------------------------- */
    if( bTestOpen )
        CPLPushErrorHandler( CPLQuietErrorHandler );

    psAVC = AVCE00ReadOpen( pszNewName );

    if( bTestOpen )
    {
        CPLPopErrorHandler();
        CPLErrorReset();
    }

    if( psAVC == NULL )
        return FALSE;

    pszName = CPLStrdup( pszNewName );
    pszCoverageName = CPLStrdup( psAVC->pszCoverName );

/* -------------------------------------------------------------------- */
/*      Create layers for the "interesting" sections of the coverage.   */
/* -------------------------------------------------------------------- */
    int		iSection;

    papoLayers = (OGRLayer **)
        CPLCalloc( sizeof(OGRLayer *), psAVC->numSections );
    nLayers = 0;

    for( iSection = 0; iSection < psAVC->numSections; iSection++ )
    {
        AVCE00Section *psSec = psAVC->pasSections + iSection;

        switch( psSec->eType )
        {
          case AVCFileARC:
          case AVCFilePAL:
          case AVCFileCNT:
          case AVCFileLAB:
          case AVCFileRPL:
          case AVCFileTXT:
          case AVCFileTX6:
            papoLayers[nLayers++] = new OGRAVCBinLayer( this, psSec );
            break;

          case AVCFilePRJ:
          {
              char 	**papszPRJ;
              AVCBinFile *hFile;
              
              hFile = AVCBinReadOpen(psAVC->pszCoverPath, 
                                     psSec->pszFilename, 
                                     psAVC->eCoverType, 
                                     psSec->eType,
                                     psAVC->psDBCSInfo);
              if( hFile && poSRS == NULL )
              {
                  papszPRJ = AVCBinReadNextPrj( hFile );

                  poSRS = new OGRSpatialReference();
                  if( poSRS->importFromESRI( papszPRJ ) != OGRERR_NONE )
                  {
                      CPLError( CE_Warning, CPLE_AppDefined, 
                                "Failed to parse PRJ section, ignoring." );
                      delete poSRS;
                      poSRS = NULL;
                  }
                  AVCBinReadClose( hFile );
              }
          }
          break;

          default:
            ;
        }
    }
    
    return nLayers > 0;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRAVCBinDataSource::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRAVCBinDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}
