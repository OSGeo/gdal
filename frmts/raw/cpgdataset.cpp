/******************************************************************************
 * $Id$
 *
 * Project:  Polarimetric Workstation
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
 * Revision 1.5  2004/11/11 00:16:01  gwalter
 * Polarmetric->Polarimetric.
 *
 * Revision 1.4  2004/10/21 18:15:25  gwalter
 * Added gcp id's- the lack of them
 * was causing weird export problems.
 *
 * Revision 1.3  2004/10/20 23:27:45  gwalter
 * Added geocoding.
 *
 * Revision 1.2  2004/09/07 15:36:58  gwalter
 * Updated to recognize more convair
 * file naming conventions; change
 * band ordering from hh,hv,vh,vv to
 * hh,hv,vv,vh.
 *
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

    int nGCPCount;
    GDAL_GCP *pasGCPList;
    char *pszGCPProjection;

    static int  AdjustFilename( char *, const char *, const char * );

  public:
    		CPGDataset();
    	        ~CPGDataset();
    
    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();
    
    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/*                            CPGDataset()                             */
/************************************************************************/

CPGDataset::CPGDataset()
{
    int iBand;

    nGCPCount = 0;
    pasGCPList = NULL;
    pszGCPProjection = CPLStrdup("");

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

    if( nGCPCount > 0 )
    {
        for( int i = 0; i < nGCPCount; i++ )
            CPLFree( pasGCPList[i].pszId );

        CPLFree( pasGCPList );
    }

    CPLFree( pszGCPProjection );

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
    if ( EQUAL(pszFilename+nNameLen-7,"sso.hdr") || 
         EQUAL(pszFilename+nNameLen-7,"sso.img"))
        strncpy( pszFilename + nNameLen - 9, pszPolarization, 2 );
    else
        strncpy( pszFilename + nNameLen - 13, pszPolarization, 2 );

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

    if(( nNameLen < 9 
        || (!EQUAL(poOpenInfo->pszFilename+nNameLen-7,"sso.hdr")
            && !EQUAL(poOpenInfo->pszFilename+nNameLen-7,"sso.img")) ) &&
       ( nNameLen < 13 
        || (!EQUAL(poOpenInfo->pszFilename+nNameLen-11,"polgasp.hdr")
            && !EQUAL(poOpenInfo->pszFilename+nNameLen-11,"polgasp.img")) ))
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
    
    /* Parameters required for pseudo-geocoding.  GCPs map */
    /* slant range to ground range at 16 points.           */
    int iGeoParamsFound = 0, itransposed = 0;
    double dfaltitude = 0.0, dfnear_srd = 0.0;
    double dfsample_size = 0.0, dfsample_size_az = 0.0;

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
                     && atoi(papszTokens[1]) != 1))
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Keyword %s has value %s which does not match CPG driver expectation.",
                      papszTokens[0], papszTokens[1] );
            nError = 1;
        }
        else if( EQUAL(papszTokens[0],"altitude") )
        {
            dfaltitude = atof(papszTokens[1]);
            iGeoParamsFound++;
        }
        else if( EQUAL(papszTokens[0],"near_srd") )
        {
            dfnear_srd = atof(papszTokens[1]);
            iGeoParamsFound++;
        }

        else if( EQUAL(papszTokens[0],"sample_size") )
        {
            dfsample_size = atof(papszTokens[1]);
            iGeoParamsFound++;
        }
        else if( EQUAL(papszTokens[0],"sample_size_az") )
        {
            dfsample_size_az = atof(papszTokens[1]);
            iGeoParamsFound++;
        }
        else if( EQUAL(papszTokens[0],"transposed") )
        {
            itransposed = atoi(papszTokens[1]);
            iGeoParamsFound++;
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
    char *apszPolarizations[4] = { "hh", "hv", "vv", "vh" };

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

        poBand->SetMetadataItem( "POLARIMETRIC_INTERP", 
                                 apszPolarizations[iBand] );
    }

/* -------------------------------------------------------------------- */
/*      Add pseudo-geocoding, if enough information found.              */
/* -------------------------------------------------------------------- */
    if (iGeoParamsFound == 5)
    {
        int ngcp;
        double dfgcpLine, dfgcpPixel, dfgcpX, dfgcpY, dftemp;

        poDS->nGCPCount = 16;
        poDS->pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),16);

        for( ngcp = 0; ngcp < 16; ngcp ++ )
        {
            char szID[32];

            sprintf(szID,"%d",ngcp+1);
            if (itransposed == 1)
            {
                if (ngcp < 4)
                    dfgcpPixel = 0.0;
                else if (ngcp < 8)
                    dfgcpPixel = nSamples/3.0;
                else if (ngcp < 12)
                    dfgcpPixel = 2.0*nSamples/3.0;
                else
                    dfgcpPixel = nSamples;

                dfgcpLine = nLines*( ngcp % 4 )/3.0;

                dftemp = dfnear_srd + (dfsample_size*dfgcpLine);
                /* -1 so that 0,0 maps to largest Y */
                dfgcpY = -1*sqrt( dftemp*dftemp - dfaltitude*dfaltitude );
                dfgcpX = dfgcpPixel*dfsample_size_az;

            }
            else
            {
                if (ngcp < 4)
                    dfgcpLine = 0.0;
                else if (ngcp < 8)
                    dfgcpLine = nLines/3.0;
                else if (ngcp < 12)
                    dfgcpLine = 2.0*nLines/3.0;
                else
                    dfgcpLine = nLines;

                dfgcpPixel = nSamples*( ngcp % 4 )/3.0;

                dftemp = dfnear_srd + (dfsample_size*dfgcpPixel);
                dfgcpX = sqrt( dftemp*dftemp - dfaltitude*dfaltitude );
                dfgcpY = (nLines - dfgcpLine)*dfsample_size_az;

            }
            poDS->pasGCPList[ngcp].dfGCPX = dfgcpX;
            poDS->pasGCPList[ngcp].dfGCPY = dfgcpY;
            poDS->pasGCPList[ngcp].dfGCPZ = 0.0;

            poDS->pasGCPList[ngcp].dfGCPPixel = dfgcpPixel;
            poDS->pasGCPList[ngcp].dfGCPLine = dfgcpLine;

            poDS->pasGCPList[ngcp].pszId = CPLStrdup( szID );
            poDS->pasGCPList[ngcp].pszInfo = "";

        }
        poDS->pszGCPProjection = (char *) CPLStrdup("LOCAL_CS[\"Ground range view / unreferenced meters\",UNIT[\"Meter\",1.0]]"); 

    }

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    // Need to think about this. 
    // poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int CPGDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *CPGDataset::GetGCPProjection()

{
  return pszGCPProjection;
}

/************************************************************************/
/*                               GetGCPs()                               */
/************************************************************************/

const GDAL_GCP *CPGDataset::GetGCPs()

{
    return pasGCPList;
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

