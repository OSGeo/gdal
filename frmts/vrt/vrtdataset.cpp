/******************************************************************************
 * $Id$
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of VRTDataset
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.2  2001/11/18 15:46:45  warmerda
 * added SRS and GeoTransform
 *
 * Revision 1.1  2001/11/16 21:14:31  warmerda
 * New
 *
 */

#include "vrtdataset.h"
#include "cpl_string.h"
#include "cpl_minixml.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

static GDALDriver	*poVRTDriver = NULL;

/************************************************************************/
/*                            VRTDataset()                             */
/************************************************************************/

VRTDataset::VRTDataset( int nXSize, int nYSize )

{
    nRasterXSize = nXSize;
    nRasterYSize = nYSize;
    pszProjection = NULL;

    bGeoTransformSet = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~VRTDataset()                            */
/************************************************************************/

VRTDataset::~VRTDataset()

{
    FlushCache();
    CPLFree( pszProjection );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *VRTDataset::GetProjectionRef()

{
    return pszProjection;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr VRTDataset::GetGeoTransform( double * padfGeoTransform )

{
    memcpy( padfGeoTransform, adfGeoTransform, sizeof(double) * 6 );
    
    if( bGeoTransformSet )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *VRTDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Does this appear to be a virtual dataset definition XML         */
/*      file?                                                           */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 20 
        || !EQUALN((const char *)poOpenInfo->pabyHeader,"<VRTDataset",11) )
        return NULL;

/* -------------------------------------------------------------------- */
/*	Try to read the whole file into memory.				*/
/* -------------------------------------------------------------------- */
    unsigned int nLength;
    char        *pszXML;

    VSIFSeek( poOpenInfo->fp, 0, SEEK_END );
    nLength = VSIFTell( poOpenInfo->fp );
    VSIFSeek( poOpenInfo->fp, 0, SEEK_SET );

    nLength = MAX(0,nLength);
    pszXML = (char *) VSIMalloc(nLength+1);

    if( pszXML == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, 
                  "Failed to allocate %d byte buffer to hold VRT xml file.",
                  nLength );
        return NULL;
    }
    
    if( VSIFRead( pszXML, 1, nLength, poOpenInfo->fp ) != nLength )
    {
        CPLFree( pszXML );
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to read %d bytes from VRT xml file.",
                  nLength );
        return NULL;
    }
    
    pszXML[nLength] = '\0';

/* -------------------------------------------------------------------- */
/*      Parse the XML.                                                  */
/* -------------------------------------------------------------------- */
    CPLXMLNode	*psTree;

    psTree = CPLParseXMLString( pszXML );
    CPLFree( pszXML );

    if( psTree == NULL )
        return NULL;

    if( CPLGetXMLNode( psTree, "rasterXSize" ) == NULL
        || CPLGetXMLNode( psTree, "rasterYSize" ) == NULL
        || CPLGetXMLNode( psTree, "VRTRasterBand" ) == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Missing one of rasterXSize, rasterYSize or bands on"
                  " VRTDataset." );
        CPLDestroyXMLNode( psTree );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the new virtual dataset object.                          */
/* -------------------------------------------------------------------- */
    VRTDataset *poDS;

    poDS = new VRTDataset(atoi(CPLGetXMLValue(psTree,"rasterXSize","0")),
                          atoi(CPLGetXMLValue(psTree,"rasterYSize","0")));
    poDS->poDriver = poVRTDriver;

    poDS->eAccess = GA_ReadOnly;

/* -------------------------------------------------------------------- */
/*	Check for an SRS node.						*/
/* -------------------------------------------------------------------- */
    if( strlen(CPLGetXMLValue(psTree, "SRS", "")) > 0 )
        poDS->pszProjection = CPLStrdup(CPLGetXMLValue(psTree, "SRS", ""));

/* -------------------------------------------------------------------- */
/*      Check for a GeoTransform node.                                  */
/* -------------------------------------------------------------------- */
    if( strlen(CPLGetXMLValue(psTree, "GeoTransform", "")) > 0 )
    {
        const char *pszGT = CPLGetXMLValue(psTree, "GeoTransform", "");
        char	**papszTokens;

        papszTokens = CSLTokenizeStringComplex( pszGT, ",", FALSE, FALSE );
        if( CSLCount(papszTokens) != 6 )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "GeoTransform node does not have expected six values.");
        }
        else
        {
            for( int iTA = 0; iTA < 6; iTA++ )
                poDS->adfGeoTransform[iTA] = atof(papszTokens[iTA]);
            poDS->bGeoTransformSet = TRUE;
        }

        CSLDestroy( papszTokens );
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int		nBands = 0;
    CPLXMLNode *psChild;

    for( psChild=psTree->psChild; psChild != NULL; psChild=psChild->psNext )
    {
        if( psChild->eType == CXT_Element
            && EQUAL(psChild->pszValue,"VRTRasterBand") )
        {
            VRTRasterBand  *poBand;

            poBand = new VRTRasterBand( poDS, nBands+1 );
            if( poBand->XMLInit( psChild ) == CE_None )
            {
                poDS->SetBand( ++nBands, poBand );
            }
            else
            {
                delete poBand; 
                break;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to return a regular handle on the file.                     */
/* -------------------------------------------------------------------- */
    CPLDestroyXMLNode( psTree );

    return poDS;
}

/************************************************************************/
/*                          GDALRegister_VRT()                          */
/************************************************************************/

void GDALRegister_VRT()

{
    GDALDriver	*poDriver;

    if( poVRTDriver == NULL )
    {
        poVRTDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "VRT";
        poDriver->pszLongName = "Virtual Raster";
        
        poDriver->pfnOpen = VRTDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

