/******************************************************************************
 * $Id$
 *
 * Project:  OGR
 * Purpose:  Implements OGRAVCE00DataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *           James Flemer <jflemer@alum.rpi.edu>
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2006, James Flemer <jflemer@alum.rpi.edu>
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

#include "ogr_avc.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRAVCE00DataSource()                         */
/************************************************************************/

OGRAVCE00DataSource::OGRAVCE00DataSource()
    : nLayers(0), pszName(NULL), psE00(NULL), papoLayers(NULL)
{
}

/************************************************************************/
/*                        ~OGRAVCE00DataSource()                        */
/************************************************************************/

OGRAVCE00DataSource::~OGRAVCE00DataSource()

{
    if( psE00 )
    {
        AVCE00ReadCloseE00( psE00 );
        psE00 = NULL;
    }

    CPLFree( pszName );

    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    
    CPLFree( papoLayers );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRAVCE00DataSource::Open( const char * pszNewName, int bTestOpen )

{
/* -------------------------------------------------------------------- */
/*      Open the source file.  Suppress error reporting if we are in    */
/*      TestOpen mode.                                                  */
/* -------------------------------------------------------------------- */
    int bCompressed = FALSE;

    if( bTestOpen )
        CPLPushErrorHandler( CPLQuietErrorHandler );

    psE00 = AVCE00ReadOpenE00(pszNewName);

    if( CPLGetLastErrorNo() == CPLE_OpenFailed
        && strstr(CPLGetLastErrorMsg(),"compressed E00") != NULL ) 
    {
        bCompressed = TRUE;
    }

    if( bTestOpen )
    {
        CPLPopErrorHandler();
        CPLErrorReset();
    }

    if( psE00 == NULL )
    {
        if( bCompressed ) 
        {
            CPLError(CE_Failure, CPLE_OpenFailed, 
                     "This looks like a compressed E00 file and cannot be "
                     "processed directly. You may need to uncompress it "
                     "first using the E00compr library or the e00conv "
                     "program." );
        }
        return FALSE;
    }

    pszName = CPLStrdup( pszNewName );
    /* pszCoverageName = CPLStrdup( psE00->pszCoverName ); */
    pszCoverageName = CPLStrdup( pszNewName );

/* -------------------------------------------------------------------- */
/*      Create layers for the "interesting" sections of the coverage.   */
/* -------------------------------------------------------------------- */
    int iSection;

    papoLayers = (OGRAVCE00Layer **)
        CPLCalloc( sizeof(OGRAVCE00Layer *), psE00->numSections );
    nLayers = 0;

    for( iSection = 0; iSection < psE00->numSections; iSection++ )
    {
        AVCE00Section *psSec = psE00->pasSections + iSection;

        switch( psSec->eType )
        {
          case AVCFileARC:
          case AVCFilePAL:
          case AVCFileCNT:
          case AVCFileLAB:
          case AVCFileRPL:
          case AVCFileTXT:
            papoLayers[nLayers++] = new OGRAVCE00Layer( this, psSec );
            break;

          case AVCFileTX6:
            break;

          case AVCFileTABLE:
            CheckAddTable(psSec);
            break;

          case AVCFilePRJ:
          {
#if 0
              poSRS = new OGRSpatialReference();
              char  **papszPRJ;
              AVCE00File *hFile;
              
              hFile = AVCE00ReadOpen(psE00->pszCoverPath, 
                                     psSec->pszFilename, 
                                     psE00->eCoverType, 
                                     psSec->eType,
                                     psE00->psDBCSInfo);
              if( hFile && poSRS == NULL )
              {
                  papszPRJ = AVCE00ReadNextPrj( hFile );

                  poSRS = new OGRSpatialReference();
                  if( poSRS->importFromESRI( papszPRJ ) != OGRERR_NONE )
                  {
                      CPLError( CE_Warning, CPLE_AppDefined, 
                                "Failed to parse PRJ section, ignoring." );
                      delete poSRS;
                      poSRS = NULL;
                  }
                  AVCE00ReadClose( hFile );
              }
#endif
          }
          break;

          default:
            ;
        }
    }
    
    return nLayers > 0;
}

int OGRAVCE00DataSource::CheckAddTable( AVCE00Section *psTblSection )
{
    int i, nCount = 0;
    for (i = 0; i < nLayers; ++i)
    {
        if (papoLayers[i]->CheckSetupTable(psTblSection))
            ++nCount;
    }
    return nCount;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRAVCE00DataSource::TestCapability( CPL_UNUSED const char * pszCap )
{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRAVCE00DataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/
OGRSpatialReference *OGRAVCE00DataSource::GetSpatialRef()
{
    if (NULL == poSRS && psE00 != NULL)
    /* if (psE00 != NULL) */
    {
        int iSection;
        AVCE00Section *psSec;
        char **pszPRJ;

        for( iSection = 0; iSection < psE00->numSections; iSection++ )
        {
            psSec = psE00->pasSections + iSection;
            if (psSec->eType == AVCFilePRJ)
            {
		        AVCE00ReadGotoSectionE00(psE00, psSec, 0);
	            pszPRJ = (char **)AVCE00ReadNextObjectE00(psE00);
                poSRS = new OGRSpatialReference();
                if( poSRS->importFromESRI( pszPRJ ) != OGRERR_NONE )
                {
                    CPLError( CE_Warning, CPLE_AppDefined, 
                              "Failed to parse PRJ section, ignoring." );
                    delete poSRS;
                    poSRS = NULL;
                }
                break;
            }
        }
    }
    return poSRS;
}
