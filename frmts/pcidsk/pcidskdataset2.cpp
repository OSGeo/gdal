/******************************************************************************
 * $Id: pcidskdataset.cpp 17097 2009-05-21 19:59:35Z warmerdam $
 *
 * Project:  PCIDSK Database File
 * Purpose:  Read/write PCIDSK Database File used by the PCI software, using
 *           the external PCIDSK library.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
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

#include "pcidsk.h"
#include "gdal_pam.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id: pcidskdataset.cpp 17097 2009-05-21 19:59:35Z warmerdam $");

using namespace PCIDSK;

CPL_C_START
void    GDALRegister_PCIDSK2(void);
CPL_C_END

/************************************************************************/
/*                              PCIDSK2Dataset                           */
/************************************************************************/

class PCIDSK2Dataset : public GDALPamDataset
{
    friend class PCIDSK2Band;

    CPLString   osSRS;

    PCIDSKFile  *poFile;

    GDALDataType PCIDSKTypeToGDAL( eChanType eType );

  public:
                PCIDSK2Dataset();
                ~PCIDSK2Dataset();

    static int           Identify( GDALOpenInfo * );
    static GDALDataset  *Open( GDALOpenInfo * );

    CPLErr              GetGeoTransform( double * padfTransform );
    const char          *GetProjectionRef();
};

/************************************************************************/
/*                             PCIDSK2Band                              */
/************************************************************************/

class PCIDSK2Band : public GDALPamRasterBand
{
    friend class PCIDSK2Dataset;

    PCIDSK2Dataset     *poPDS;
    PCIDSKFile *poFile;
    PCIDSKChannel *poChannel;
    
  public:
                PCIDSK2Band( PCIDSK2Dataset *, PCIDSKFile *, int );
                ~PCIDSK2Band();

    virtual CPLErr IReadBlock( int, int, void * );
};

/************************************************************************/
/* ==================================================================== */
/*                            PCIDSK2Band                               */
/* ==================================================================== */
/************************************************************************/

PCIDSK2Band::PCIDSK2Band( PCIDSK2Dataset *poDS, 
                          PCIDSKFile *poFile,
                          int nBand )                        

{
    poPDS = poDS;
    this->poDS = poDS;

    this->nBand = nBand;

    poChannel = poFile->GetChannel( nBand );

    nBlockXSize = (int) poChannel->GetBlockWidth();
    nBlockYSize = (int) poChannel->GetBlockHeight();
    
    eDataType = poPDS->PCIDSKTypeToGDAL( poChannel->GetType() );
}

/************************************************************************/
/*                            ~PCIDSK2Band()                            */
/************************************************************************/

PCIDSK2Band::~PCIDSK2Band()

{
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr PCIDSK2Band::IReadBlock( int iBlockX, int iBlockY, void *pData )

{
    try 
    {
        poChannel->ReadBlock( iBlockX + iBlockY * nBlocksPerRow,
                              pData );
        return CE_None;
    }
    catch( PCIDSKException ex )
    {
        return CE_Failure;
    }
}

/************************************************************************/
/* ==================================================================== */
/*                            PCIDSK2Dataset                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           PCIDSK2Dataset()                            */
/************************************************************************/

PCIDSK2Dataset::PCIDSK2Dataset()
{
    poFile = NULL;
}

/************************************************************************/
/*                            ~PCIDSK2Dataset()                          */
/************************************************************************/

PCIDSK2Dataset::~PCIDSK2Dataset()
{
    FlushCache();
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr PCIDSK2Dataset::GetGeoTransform( double * padfTransform )
{
    PCIDSKGeoref *poGeoref = NULL;
    try
    {
        PCIDSKSegment *poGeoSeg = poFile->GetSegment(1);
        poGeoref = dynamic_cast<PCIDSKGeoref*>( poGeoSeg );
    }
    catch( PCIDSKException ex )
    {
        // I should really check whether this is an expected issue.
    }
        
    if( poGeoref == NULL )
        return GDALPamDataset::GetGeoTransform( padfTransform );
    else
    {
        poGeoref->GetTransform( padfTransform[0], 
                                padfTransform[1],
                                padfTransform[2],
                                padfTransform[3],
                                padfTransform[4],
                                padfTransform[5] );
        return CE_None;
    }
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *PCIDSK2Dataset::GetProjectionRef()
{
    if( osSRS != "" )
        return osSRS.c_str();

    PCIDSKGeoref *poGeoref = NULL;
    try
    {
        PCIDSKSegment *poGeoSeg = poFile->GetSegment(1);
        poGeoref = dynamic_cast<PCIDSKGeoref*>( poGeoSeg );
    }
    catch( PCIDSKException ex )
    {
        // I should really check whether this is an expected issue.
    }
        
    if( poGeoref == NULL )
    {
        osSRS = GDALPamDataset::GetProjectionRef();
    }
    else
    {
        OGRSpatialReference oSRS;
        char *pszWKT = NULL;

        if( oSRS.importFromPCI( poGeoref->GetGeosys().c_str() ) == OGRERR_NONE )
        {
            oSRS.exportToWkt( &pszWKT );
            osSRS = pszWKT;
            CPLFree( pszWKT );
        }
        else
        {
            osSRS = GDALPamDataset::GetProjectionRef();
        }
    }

    return osSRS.c_str();
}

/************************************************************************/
/*                         PCIDSKTypeToGDAL()                           */
/************************************************************************/

GDALDataType PCIDSK2Dataset::PCIDSKTypeToGDAL( eChanType eType )
{
    switch( eType )
    {
      case CHN_8U:
        return GDT_Byte;
        
      case CHN_16U:
        return GDT_UInt16;
        
      case CHN_16S:
        return GDT_Int16;
        
      case CHN_32R:
        return GDT_Float32;
        
      default:
        return GDT_Unknown;
    }
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int PCIDSK2Dataset::Identify( GDALOpenInfo * poOpenInfo )
{
    if( poOpenInfo->nHeaderBytes < 512 
        || !EQUALN((const char *) poOpenInfo->pabyHeader, "PCIDSK  ", 8) )
        return FALSE;
    else
        return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *PCIDSK2Dataset::Open( GDALOpenInfo * poOpenInfo )
{
    if( !Identify( poOpenInfo ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Try opening the file.                                           */
/* -------------------------------------------------------------------- */
    try {

        PCIDSKFile *poFile = 
            PCIDSK::Open( poOpenInfo->pszFilename, 
                          poOpenInfo->eAccess == GA_ReadOnly ? "r" : "r+",
                          NULL /* &oIOInterfaces */ );

        if( poFile == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Failed to re-open %s within PCIDSK driver.\n",
                      poOpenInfo->pszFilename );
            return NULL;
        }
                               
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
        PCIDSK2Dataset   *poDS = NULL;

        poDS = new PCIDSK2Dataset();

        poDS->poFile = poFile;
        poDS->eAccess = poOpenInfo->eAccess;
        poDS->nRasterXSize = poFile->GetWidth();
        poDS->nRasterYSize = poFile->GetHeight();

/* -------------------------------------------------------------------- */
/*      Create band objects.                                            */
/* -------------------------------------------------------------------- */
        int iBand;

        for( iBand = 0; iBand < poFile->GetChannels(); iBand++ )
        {
            poDS->SetBand( iBand+1, new PCIDSK2Band( poDS, poFile, iBand+1 ));
        }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
        poDS->SetDescription( poOpenInfo->pszFilename );
        poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Open overviews.                                                 */
/* -------------------------------------------------------------------- */
        poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );
        
        return( poDS );
    }

/* -------------------------------------------------------------------- */
/*      Trap exceptions.                                                */
/* -------------------------------------------------------------------- */
    catch( PCIDSKException ex )
    {
    }
    catch( ... )
    {
    }

    return NULL;
}

/************************************************************************/
/*                        GDALRegister_PCIDSK()                         */
/************************************************************************/

void GDALRegister_PCIDSK2()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "PCIDSK2" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "PCIDSK2" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "PCIDSK Database File" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_pcidsk.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "pix" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte UInt16 Int16 Float32" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"   <Option name='FILEDESC1' type='string' description='The first line of descriptive text'/>"
"   <Option name='FILEDESC2' type='string' description='The second line of descriptive text'/>"
"   <Option name='BANDDESCn' type='string' description='Text describing contents of the specified band'/>"
"</CreationOptionList>" ); 

        poDriver->pfnIdentify = PCIDSK2Dataset::Identify;
        poDriver->pfnOpen = PCIDSK2Dataset::Open;
//        poDriver->pfnCreate = PCIDSK2Dataset::Create;
//        poDriver->pfnCreateCopy = PCIDSK2Dataset::CreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}


