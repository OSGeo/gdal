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
 * Revision 1.6  2004/12/17 22:45:55  gwalter
 * Added support for sirc-style convair flat
 * binary datasets.
 *
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

class SIRC_QSLCRasterBand;

class CPGDataset : public RawDataset
{
    friend class SIRC_QSLCRasterBand;

    FILE	*afpImage[4];

    int nGCPCount;
    GDAL_GCP *pasGCPList;
    char *pszGCPProjection;

    double adfGeoTransform[6];
    char *pszProjection;

    static int  AdjustFilename( char *, const char *, const char * );

  public:
    		CPGDataset();
    	        ~CPGDataset();
    
    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

    virtual const char *GetProjectionRef(void);
    virtual CPLErr GetGeoTransform( double * );
    
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
    pszProjection = CPLStrdup("");
    pszGCPProjection = CPLStrdup("");
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

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

    CPLFree( pszProjection );
    CPLFree( pszGCPProjection );

}

/************************************************************************/
/* ==================================================================== */
/*                          SIRC_QSLCPRasterBand                        */
/* ==================================================================== */
/************************************************************************/

class SIRC_QSLCRasterBand : public GDALRasterBand
{
    friend class CPGDataset;

  public:
                   SIRC_QSLCRasterBand( CPGDataset *, int, GDALDataType );

    virtual CPLErr IReadBlock( int, int, void * );
};

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
    else if( EQUAL(pszFilename+nNameLen-7,"asp.hdr") || 
         EQUAL(pszFilename+nNameLen-7,"asp.img"))
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
/*      one of:                                                         */
/*               1) sso.img or sso.hdr.                                 */
/*               2) polgasp.img or polgasp.hdr                          */
/*               3) SIRC.hdr or SIRC.img                                */
/* -------------------------------------------------------------------- */
    int nNameLen = strlen(poOpenInfo->pszFilename);

    if(( nNameLen < 9 
        || (!EQUAL(poOpenInfo->pszFilename+nNameLen-7,"sso.hdr")
            && !EQUAL(poOpenInfo->pszFilename+nNameLen-7,"sso.img")) ) &&
       ( nNameLen < 13 
        || (!EQUAL(poOpenInfo->pszFilename+nNameLen-11,"polgasp.hdr")
            && !EQUAL(poOpenInfo->pszFilename+nNameLen-11,"polgasp.img")) ) &&
       ( nNameLen < 8
        || (!EQUAL(poOpenInfo->pszFilename+nNameLen-8,"SIRC.hdr")
            && !EQUAL(poOpenInfo->pszFilename+nNameLen-8,"SIRC.img")) ))
        return NULL;

/* -------------------------------------------------------------------- */
/*      OK, we believe we have a valid polgasp dataset.  Prepare a      */
/*      modifiable local name we can fiddle with.                       */
/* -------------------------------------------------------------------- */
    char *pszWorkName = CPLStrdup(poOpenInfo->pszFilename);

/* -------------------------------------------------------------------- */
/*      Verify we have our various files.                               */
/* -------------------------------------------------------------------- */
    
    if( ( EQUAL(pszWorkName+nNameLen-7,"sso.hdr") || 
          EQUAL(pszWorkName+nNameLen-7,"sso.img")  || 
          EQUAL(pszWorkName+nNameLen-7,"asp.img") ||
          EQUAL(pszWorkName+nNameLen-7,"asp.hdr") ) &&
        (!AdjustFilename( pszWorkName, "hh", "img" ) 
        || !AdjustFilename( pszWorkName, "hh", "hdr" ) 
        || !AdjustFilename( pszWorkName, "hv", "img" ) 
        || !AdjustFilename( pszWorkName, "hv", "hdr" ) 
        || !AdjustFilename( pszWorkName, "vh", "img" ) 
        || !AdjustFilename( pszWorkName, "vh", "hdr" ) 
        || !AdjustFilename( pszWorkName, "vv", "img" ) 
         || !AdjustFilename( pszWorkName, "vv", "hdr" )) )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Apparent attempt to open Convair PolGASP data failed as\n"
                  "one or more of the eight required files is missing." );
        CPLFree( pszWorkName );
        return NULL;
    }
    else if( ( EQUAL(pszWorkName+nNameLen-7,"SIRC.hdr") || 
               EQUAL(pszWorkName+nNameLen-7,"SIRC.img") ) &&
             !AdjustFilename( pszWorkName, "", "img" ) )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Apparent attempt to open SIRC Convair PolGASP data\n"
                  "failed as the image file is missing." );
        CPLFree( pszWorkName );
        return NULL;
    } 
    else if( ( EQUAL(pszWorkName+nNameLen-7,"SIRC.hdr") || 
               EQUAL(pszWorkName+nNameLen-7,"SIRC.img") ) &&
             !AdjustFilename( pszWorkName, "", "hdr" ))
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Apparent attempt to open SIRC Convair PolGASP data\n"
                  "failed as the header file is missing." );
        CPLFree( pszWorkName );
        return NULL;
    } 

/* -------------------------------------------------------------------- */
/*      Read the .hdr file (the hh one for the .sso and polgasp cases)  */
/*      and parse it.                                                   */
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

    /* Parameters in geogratis geocoded images */
    int iUTMParamsFound = 0, iUTMZone=0, iCorner=0;
    double dfnorth = 0.0, dfeast = 0.0;

    AdjustFilename( pszWorkName, "hh", "hdr" );
    papszHdrLines = CSLLoad( pszWorkName );

    for( iLine = 0; papszHdrLines && papszHdrLines[iLine] != NULL; iLine++ )
    {
        char **papszTokens = CSLTokenizeString( papszHdrLines[iLine] );

        /* Note: some cv580 file seem to have comments with #, hence the >=
         *       instead of = for token checking, and the equalN for the corner.
         */

        if( CSLCount( papszTokens ) < 2 )
        {
          /* ignore */;
        }
        else if ( ( CSLCount( papszTokens ) >= 3 ) &&
                 EQUAL(papszTokens[0],"reference") &&
                 EQUAL(papszTokens[1],"north") )
        {
            dfnorth = atof(papszTokens[2]);
            iUTMParamsFound++;
        }
        else if ( ( CSLCount( papszTokens ) >= 3 ) &&
               EQUAL(papszTokens[0],"reference") &&
               EQUAL(papszTokens[1],"east") )
        {
            dfeast = atof(papszTokens[2]);
            iUTMParamsFound++;
        }  
        else if ( ( CSLCount( papszTokens ) >= 5 ) &&
               EQUAL(papszTokens[0],"reference") &&
               EQUAL(papszTokens[1],"projection") &&
               EQUAL(papszTokens[2],"UTM") &&
               EQUAL(papszTokens[3],"zone") )
        {
            iUTMZone = atoi(papszTokens[4]);
            iUTMParamsFound++;
        } 
        else if ( ( CSLCount( papszTokens ) >= 3 ) &&
               EQUAL(papszTokens[0],"reference") &&
               EQUAL(papszTokens[1],"corner") &&
               EQUALN(papszTokens[2],"Upper_Left",10) )
        {
            iCorner = 0;
            iUTMParamsFound++;
        }  
        else if( EQUAL(papszTokens[0],"number_lines") )
            nLines = atoi(papszTokens[1]);
        
        else if( EQUAL(papszTokens[0],"number_samples") )
            nSamples = atoi(papszTokens[1]);

        else if( (EQUAL(papszTokens[0],"header_offset") 
                  && atoi(papszTokens[1]) != 0) 
                 || (EQUAL(papszTokens[0],"number_channels") 
                     && (atoi(papszTokens[1]) != 1)
                     && (atoi(papszTokens[1]) != 10)) 
                 || (EQUAL(papszTokens[0],"datatype") 
                     && atoi(papszTokens[1]) != 1) 
                 || (EQUAL(papszTokens[0],"number_format") 
                     && !EQUAL(papszTokens[1],"float32")
                     && !EQUAL(papszTokens[1],"int8")))
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
            iUTMParamsFound++;
        }
        else if( EQUAL(papszTokens[0],"sample_size_az") )
        {
            dfsample_size_az = atof(papszTokens[1]);
            iGeoParamsFound++;
            iUTMParamsFound++;
        }
        else if( EQUAL(papszTokens[0],"transposed") )
        {
            itransposed = atoi(papszTokens[1]);
            iGeoParamsFound++;
            iUTMParamsFound++;
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
    int iBand=0;
    CPGDataset     *poDS;

    poDS = new CPGDataset();

    poDS->nRasterXSize = nSamples;
    poDS->nRasterYSize = nLines;

/* -------------------------------------------------------------------- */
/*      Open the four bands.                                            */
/* -------------------------------------------------------------------- */
    char *apszPolarizations[4] = { "hh", "hv", "vv", "vh" };

    if ( EQUAL(pszWorkName+nNameLen-7,"IRC.hdr") ||
         EQUAL(pszWorkName+nNameLen-7,"IRC.img") )
    {

        AdjustFilename( pszWorkName, "" , "img" );
        poDS->afpImage[0] = VSIFOpen( pszWorkName, "rb" );
        if( poDS->afpImage[0] == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Failed to open .img file: %s", 
                      pszWorkName );
            return NULL;
        }
        for( iBand = 0; iBand < 4; iBand++ )
        {
            SIRC_QSLCRasterBand	*poBand;

            poBand = new SIRC_QSLCRasterBand( poDS, iBand+1, GDT_CFloat32 );
            poDS->SetBand( iBand+1, poBand );
            poBand->SetMetadataItem( "POLARIMETRIC_INTERP", 
                                 apszPolarizations[iBand] );
        }
    }
    else
    {
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
    }
/* -------------------------------------------------------------------------------------- */
/*      Add georeferencing or pseudo-geocoding, if enough information found.              */
/* -------------------------------------------------------------------------------------- */
    if (iUTMParamsFound == 7)
    {
        OGRSpatialReference oUTM;
        double dfnorth_center;

        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[4] = 0.0;
 
        if (itransposed == 1)
        {
            printf("Warning- did not have a convair SIRC-style test dataset\n"
                 "with transposed=1 for testing.  Georefencing may be wrong.\n");
            dfnorth_center = dfnorth + nLines*dfsample_size/2.0;
            poDS->adfGeoTransform[0] = dfnorth;
            poDS->adfGeoTransform[1] = -1*dfsample_size;
            poDS->adfGeoTransform[3] = dfeast;
            poDS->adfGeoTransform[5] = dfsample_size_az;
        }
        else
        {
            dfnorth_center = dfnorth + nLines*dfsample_size_az/2.0;
            poDS->adfGeoTransform[0] = dfeast;
            poDS->adfGeoTransform[1] = dfsample_size_az;
            poDS->adfGeoTransform[3] = dfnorth;
            poDS->adfGeoTransform[5] = -1*dfsample_size;
        }
        if (dfnorth_center < 0)
            oUTM.SetUTM(iUTMZone, 0);
        else
            oUTM.SetUTM(iUTMZone, 1);

        /* Assuming WGS84 */
        oUTM.SetWellKnownGeogCS( "WGS84" );
        CPLFree( poDS->pszProjection );
        poDS->pszProjection = NULL;
        oUTM.exportToWkt( &(poDS->pszProjection) );



    }
    else if (iGeoParamsFound == 5)
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
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *CPGDataset::GetProjectionRef()

{
    return( pszProjection );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr CPGDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform,  adfGeoTransform, sizeof(double) * 6 ); 
    return( CE_None );
}

/************************************************************************/
/*                           SIRC_QSLCRasterBand()                      */
/************************************************************************/

SIRC_QSLCRasterBand::SIRC_QSLCRasterBand( CPGDataset *poGDS, int nBand,
                                          GDALDataType eType )

{
    this->poDS = poGDS;
    this->nBand = nBand;

    eDataType = eType;

    nBlockXSize = poGDS->nRasterXSize;
    nBlockYSize = 1;

    if( nBand == 1 )
        SetMetadataItem( "POLARIMETRIC_INTERP", "HH" );
    else if( nBand == 2 )
        SetMetadataItem( "POLARIMETRIC_INTERP", "HV" );
    else if( nBand == 3 )
        SetMetadataItem( "POLARIMETRIC_INTERP", "VH" );
    else if( nBand == 4 )
        SetMetadataItem( "POLARIMETRIC_INTERP", "VV" );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

/* From: http://southport.jpl.nasa.gov/software/dcomp/dcomp.html

ysca = sqrt{ [ (Byte(2) / 254 ) + 1.5] 2Byte(1) }

Re(SHH) = byte(3) ysca/127

Im(SHH) = byte(4) ysca/127

Re(SHV) = byte(5) ysca/127

Im(SHV) = byte(6) ysca/127

Re(SVH) = byte(7) ysca/127

Im(SVH) = byte(8) ysca/127

Re(SVV) = byte(9) ysca/127

Im(SVV) = byte(10) ysca/127

*/

CPLErr SIRC_QSLCRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    int	   offset, nBytesPerSample=10;
    GByte  *pabyRecord;
    CPGDataset *poGDS = (CPGDataset *) poDS;
    static float afPowTable[256];
    static int bPowTableInitialized = FALSE;

    offset = nBlockXSize* nBlockYOff*nBytesPerSample;

/* -------------------------------------------------------------------- */
/*      Load all the pixel data associated with this scanline.          */
/* -------------------------------------------------------------------- */
    int	        nBytesToRead = nBytesPerSample * nBlockXSize;

    pabyRecord = (GByte *) CPLMalloc( nBytesToRead );

    if( VSIFSeek( poGDS->afpImage[0], offset, SEEK_SET ) != 0 
        || (int) VSIFRead( pabyRecord, 1, nBytesToRead, 
                           poGDS->afpImage[0] ) != nBytesToRead )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Error reading %d bytes of SIRC Convair at offset %d.\n"
                  "Reading file %s failed.", 
                  nBytesToRead, offset, poGDS->GetDescription() );
        CPLFree( pabyRecord );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Initialize our power table if this is our first time through.   */
/* -------------------------------------------------------------------- */
    if( !bPowTableInitialized )
    {
        int i;

        bPowTableInitialized = TRUE;

        for( i = 0; i < 256; i++ )
        {
            afPowTable[i] = pow( 2.0, i-128 );
        }
    }

/* -------------------------------------------------------------------- */
/*      Copy the desired band out based on the size of the type, and    */
/*      the interleaving mode.                                          */
/* -------------------------------------------------------------------- */
    int iX;

    for( iX = 0; iX < nBlockXSize; iX++ )
    {
        unsigned char *pabyGroup = pabyRecord + iX * nBytesPerSample;
        signed char *Byte = (signed char*)pabyGroup-1; /* A ones based alias */
        double dfReSHH, dfImSHH, dfReSHV, dfImSHV, 
            dfReSVH, dfImSVH, dfReSVV, dfImSVV, dfScale;

        dfScale = sqrt( (Byte[2] / 254 + 1.5) * afPowTable[Byte[1] + 128] );
        
        if( nBand == 1 )
        {
            dfReSHH = Byte[3] * dfScale / 127.0;
            dfImSHH = Byte[4] * dfScale / 127.0;

            ((float *) pImage)[iX*2  ] = dfReSHH;
            ((float *) pImage)[iX*2+1] = dfImSHH;
        }        
        else if( nBand == 2 )
        {
            dfReSHV = Byte[5] * dfScale / 127.0;
            dfImSHV = Byte[6] * dfScale / 127.0;

            ((float *) pImage)[iX*2  ] = dfReSHV;
            ((float *) pImage)[iX*2+1] = dfImSHV;
        }
        else if( nBand == 3 )
        {
            dfReSVH = Byte[7] * dfScale / 127.0;
            dfImSVH = Byte[8] * dfScale / 127.0;

            ((float *) pImage)[iX*2  ] = dfReSVH;
            ((float *) pImage)[iX*2+1] = dfImSVH;
        }
        else if( nBand == 4 )
        {
            dfReSVV = Byte[9] * dfScale / 127.0;
            dfImSVV = Byte[10]* dfScale / 127.0;

            ((float *) pImage)[iX*2  ] = dfReSVV;
            ((float *) pImage)[iX*2+1] = dfImSVV;
        }
    }

    CPLFree( pabyRecord );

    return CE_None;
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

