/******************************************************************************
 * $Id$
 *
 * Project:  Polarmetric Workstation
 * Purpose:  Convair PolGASP data (.img/.hdr format). 
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.1  2004/09/03 19:07:25  warmerda
 * New
 *
 */

#include "rawdataset.h"
#include "ogr_spatialref.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_CPG(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				CPGDataset				*/
/* ==================================================================== */
/************************************************************************/

class CPGDataset : public RawDataset
{
    FILE	*afpImage[4];

    static int  AdjustFilename( char *, const char *, const char * );

  public:
    		CPGDataset();
    	        ~CPGDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/*                            CPGDataset()                             */
/************************************************************************/

CPGDataset::CPGDataset()
{
    int iBand;

    for( iBand = 0; iBand < 4; iBand++ )
        afpImage[iBand] = NULL;
}

/************************************************************************/
/*                            ~CPGDataset()                            */
/************************************************************************/

CPGDataset::~CPGDataset()

{
    int iBand;

    FlushCache();

    for( iBand = 0; iBand < 4; iBand++ )
    {
        if( afpImage[iBand] != NULL )
            VSIFClose( afpImage[iBand] );
    }
}

/************************************************************************/
/*                           AdjustFilename()                           */
/*                                                                      */
/*      Try to find the file with the request polarization and          */
/*      extention and update the passed filename accordingly.           */
/*                                                                      */
/*      Return TRUE if file found otherwise FALSE.                      */
/************************************************************************/

int CPGDataset::AdjustFilename( char *pszFilename, 
                                const char *pszPolarization,
                                const char *pszExtension )

{
    int nNameLen = strlen(pszFilename);
    VSIStatBuf  sStatBuf;

    /* eventually we should handle upper/lower case ... */

    strncpy( pszFilename + nNameLen - 3, pszExtension, 3 );
    strncpy( pszFilename + nNameLen - 9, pszPolarization, 2 );

    return VSIStat( pszFilename, &sStatBuf ) == 0;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *CPGDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Is this a PolGASP .img/.hdr file?  We expect it to end with     */
/*      sso.img or sso.hdr.                                             */
/* -------------------------------------------------------------------- */
    int nNameLen = strlen(poOpenInfo->pszFilename);

    if( nNameLen < 9 
        || (!EQUAL(poOpenInfo->pszFilename+nNameLen-7,"sso.hdr")
            && !EQUAL(poOpenInfo->pszFilename+nNameLen-7,"sso.img")) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      OK, we believe we have a valid polgasp dataset.  Prepare a      */
/*      modifiable local name we can fiddle with.                       */
/* -------------------------------------------------------------------- */
    char *pszWorkName = CPLStrdup(poOpenInfo->pszFilename);

/* -------------------------------------------------------------------- */
/*      Verify we have our various files.                               */
/* -------------------------------------------------------------------- */
    
    if( !AdjustFilename( pszWorkName, "hh", "img" ) 
        || !AdjustFilename( pszWorkName, "hh", "hdr" ) 
        || !AdjustFilename( pszWorkName, "hv", "img" ) 
        || !AdjustFilename( pszWorkName, "hv", "hdr" ) 
        || !AdjustFilename( pszWorkName, "vh", "img" ) 
        || !AdjustFilename( pszWorkName, "vh", "hdr" ) 
        || !AdjustFilename( pszWorkName, "vv", "img" ) 
        || !AdjustFilename( pszWorkName, "vv", "hdr" ) )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Apparent attempt to open Convair PolGASP data failed as\n"
                  "one or more of the eight required files is missing." );
        CPLFree( pszWorkName );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the hh .hdr file and parse it.                             */
/* -------------------------------------------------------------------- */
    char **papszHdrLines;
    int iLine;
    int nLines = 0, nSamples = 0;
    int nError = 0;

    AdjustFilename( pszWorkName, "hh", "hdr" );
    papszHdrLines = CSLLoad( pszWorkName );

    
    for( iLine = 0; papszHdrLines && papszHdrLines[iLine] != NULL; iLine++ )
    {
        char **papszTokens = CSLTokenizeString( papszHdrLines[iLine] );

        if( CSLCount( papszTokens ) != 2 )
            /* ignore */;

        else if( EQUAL(papszTokens[0],"number_lines") )
            nLines = atoi(papszTokens[1]);
        
        else if( EQUAL(papszTokens[0],"number_samples") )
            nSamples = atoi(papszTokens[1]);

        else if( (EQUAL(papszTokens[0],"header_offset") 
                  && atoi(papszTokens[1]) != 0) 
                 || (EQUAL(papszTokens[0],"number_of_channels") 
                     && atoi(papszTokens[1]) != 1) 
                 || (EQUAL(papszTokens[0],"datatype") 
                     && atoi(papszTokens[1]) != 1) 
                 || (EQUAL(papszTokens[0],"number_format") 
                     && !EQUAL(papszTokens[1],"float32"))
                 || (EQUAL(papszTokens[0],"complex_flag") 
                     && atoi(papszTokens[1]) != 1) 
                 || (EQUAL(papszTokens[0],"transposed") 
                     && atoi(papszTokens[1]) != 0) )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Keyword %s has value %s which does not match CPG driver expectation.",
                      papszTokens[0], papszTokens[1] );
            nError = 1;
        }

        CSLDestroy( papszTokens );
    }

    CSLDestroy( papszHdrLines );

/* -------------------------------------------------------------------- */
/*      Check for successful completion.                                */
/* -------------------------------------------------------------------- */
    if( nError )
        return NULL;

    if( nLines == 0 || nSamples == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Did not find valid number_lines or number_samples keywords in %s.",
                  pszWorkName );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Initialize dataset.                                             */
/* -------------------------------------------------------------------- */
    int iBand;
    CPGDataset     *poDS;

    poDS = new CPGDataset();

    poDS->nRasterXSize = nSamples;
    poDS->nRasterYSize = nLines;

/* -------------------------------------------------------------------- */
/*      Open the four bands.                                            */
/* -------------------------------------------------------------------- */
    char *apszPolarizations[4] = { "hh", "hv", "vh", "vv" };

    for( iBand = 0; iBand < 4; iBand++ )
    {
        RawRasterBand	*poBand;
        
        AdjustFilename( pszWorkName, apszPolarizations[iBand], "img" );
        
        poDS->afpImage[iBand] = VSIFOpen( pszWorkName, "rb" );
        if( poDS->afpImage[iBand] == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Failed to open .img file: %s", 
                      pszWorkName );
            return NULL;
        }

        poBand = 
            new RawRasterBand( poDS, iBand+1, poDS->afpImage[iBand], 
                               0, 8, 8*nSamples, 
                               GDT_CFloat32, !CPL_IS_LSB, FALSE );
        poDS->SetBand( iBand+1, poBand );

        poBand->SetMetadataItem( "POLARMETRIC_INTERP", 
                                 apszPolarizations[iBand] );
    }

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    // Need to think about this. 
    // poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                         GDALRegister_CPG()                          */
/************************************************************************/

void GDALRegister_CPG()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "CPG" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "CPG" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Convair PolGASP" );

        poDriver->pfnOpen = CPGDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

