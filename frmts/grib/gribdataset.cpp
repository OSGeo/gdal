/******************************************************************************
 * $Id$
 *
 * Project:  GRIB Driver
 * Purpose:  GDALDataset driver for GRIB translator for read support
 * Author:   Bas Retsios, retsios@itc.nl
 *
 ******************************************************************************
 * Copyright (c) 2007, ITC
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
 */

#include "gdal_pam.h"
#include "cpl_multiproc.h"

#include "degrib18/degrib/degrib2.h"
#include "degrib18/degrib/inventory.h"
#include "degrib18/degrib/myerror.h"
#include "degrib18/degrib/filedatasource.h"
#include "degrib18/degrib/memorydatasource.h"

#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_GRIB(void);
CPL_C_END

static void *hGRIBMutex = NULL;

/************************************************************************/
/* ==================================================================== */
/*				GRIBDataset				*/
/* ==================================================================== */
/************************************************************************/

class GRIBRasterBand;

class GRIBDataset : public GDALPamDataset
{
    friend class GRIBRasterBand;

  public:
		GRIBDataset();
		~GRIBDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );

    CPLErr 	GetGeoTransform( double * padfTransform );
    const char *GetProjectionRef();
    
	private:
		void SetGribMetaData(grib_MetaData* meta);
    VSILFILE	*fp;
    char  *pszProjection;
    OGRCoordinateTransformation *poTransform;
    double adfGeoTransform[6]; // Calculate and store once as GetGeoTransform may be called multiple times

    GIntBig  nCachedBytes;
    GIntBig  nCachedBytesThreshold;
    int      bCacheOnlyOneBand;
    GRIBRasterBand* poLastUsedBand;
};

/************************************************************************/
/* ==================================================================== */
/*                            GRIBRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class GRIBRasterBand : public GDALPamRasterBand
{
    friend class GRIBDataset;
    
public:
    GRIBRasterBand( GRIBDataset*, int, inventoryType* );
    virtual ~GRIBRasterBand();
    virtual CPLErr IReadBlock( int, int, void * );
    virtual const char *GetDescription() const;

    virtual double GetNoDataValue( int *pbSuccess = NULL );

    void    FindPDSTemplate();

    void    UncacheData();

private:

    CPLErr       LoadData();

    static void ReadGribData( DataSource &, sInt4, int, double**, grib_MetaData**);
    sInt4 start;
    int subgNum;
    char *longFstLevel;

    double * m_Grib_Data;
    grib_MetaData* m_Grib_MetaData;

    int      nGribDataXSize;
    int      nGribDataYSize;
};


/************************************************************************/
/*                           GRIBRasterBand()                            */
/************************************************************************/

GRIBRasterBand::GRIBRasterBand( GRIBDataset *poDS, int nBand, 
                                inventoryType *psInv )
  : m_Grib_Data(NULL), m_Grib_MetaData(NULL)
{
    this->poDS = poDS;
    this->nBand = nBand;
    this->start = psInv->start;
    this->subgNum = psInv->subgNum;
    this->longFstLevel = CPLStrdup(psInv->longFstLevel);

    eDataType = GDT_Float64; // let user do -ot Float32 if needed for saving space, GRIB contains Float64 (though not fully utilized most of the time)

    nBlockXSize = poDS->nRasterXSize;
    nBlockYSize = 1;

    nGribDataXSize = poDS->nRasterXSize;
    nGribDataYSize = poDS->nRasterYSize;

    SetMetadataItem( "GRIB_UNIT", psInv->unitName );
    SetMetadataItem( "GRIB_COMMENT", psInv->comment );
    SetMetadataItem( "GRIB_ELEMENT", psInv->element );
    SetMetadataItem( "GRIB_SHORT_NAME", psInv->shortFstLevel );
    SetMetadataItem( "GRIB_REF_TIME", 
                     CPLString().Printf("%12.0f sec UTC", psInv->refTime ) );
    SetMetadataItem( "GRIB_VALID_TIME", 
                     CPLString().Printf("%12.0f sec UTC", psInv->validTime ) );
    SetMetadataItem( "GRIB_FORECAST_SECONDS", 
                     CPLString().Printf("%.0f sec", psInv->foreSec ) );
}

/************************************************************************/
/*                          FindPDSTemplate()                           */
/*                                                                      */
/*      Scan the file for the PDS template info and represent it as     */
/*      metadata.                                                       */
/************************************************************************/

void GRIBRasterBand::FindPDSTemplate()

{
    GRIBDataset *poGDS = (GRIBDataset *) poDS;

/* -------------------------------------------------------------------- */
/*      Collect section 4 octet information ... we read the file        */
/*      ourselves since the GRIB API does not appear to preserve all    */
/*      this for us.                                                    */
/* -------------------------------------------------------------------- */
    GIntBig nOffset = VSIFTellL( poGDS->fp );
    GByte abyHead[5];
    GUInt32 nSectSize;

    VSIFSeekL( poGDS->fp, start+16, SEEK_SET );
    VSIFReadL( abyHead, 5, 1, poGDS->fp );

    while( abyHead[4] != 4 )
    {
        memcpy( &nSectSize, abyHead, 4 );
        CPL_MSBPTR32( &nSectSize );

        if( VSIFSeekL( poGDS->fp, nSectSize-5, SEEK_CUR ) != 0
            || VSIFReadL( abyHead, 5, 1, poGDS->fp ) != 1 )
            break;
    }
        
    if( abyHead[4] == 4 )
    {
        GUInt16 nCoordCount;
        GUInt16 nPDTN;
        CPLString osOctet;
        int i;
        GByte *pabyBody;

        memcpy( &nSectSize, abyHead, 4 );
        CPL_MSBPTR32( &nSectSize );

        pabyBody = (GByte *) CPLMalloc(nSectSize-5);
        VSIFReadL( pabyBody, 1, nSectSize-5, poGDS->fp );

        memcpy( &nCoordCount, pabyBody + 5 - 5, 2 );
        CPL_MSBPTR16( &nCoordCount );

        memcpy( &nPDTN, pabyBody + 7 - 5, 2 );
        CPL_MSBPTR16( &nPDTN );

        SetMetadataItem( "GRIB_PDS_PDTN",
                         CPLString().Printf( "%d", nPDTN ) );

        for( i = 9; i < (int) nSectSize; i++ )
        {
            char szByte[10];

            if( i == 9 )
                sprintf( szByte, "%d", pabyBody[i-5] );
            else
                sprintf( szByte, " %d", pabyBody[i-5] );
            osOctet += szByte;
        }
        
        SetMetadataItem( "GRIB_PDS_TEMPLATE_NUMBERS", osOctet );

        CPLFree( pabyBody );
    }

    VSIFSeekL( poGDS->fp, nOffset, SEEK_SET );
}

/************************************************************************/
/*                         GetDescription()                             */
/************************************************************************/

const char * GRIBRasterBand::GetDescription() const
{
    if( longFstLevel == NULL )
        return GDALPamRasterBand::GetDescription();
    else
        return longFstLevel;
}
 
/************************************************************************/
/*                             LoadData()                               */
/************************************************************************/

CPLErr GRIBRasterBand::LoadData()

{
    if( !m_Grib_Data )
    {
        GRIBDataset *poGDS = (GRIBDataset *) poDS;

        if (poGDS->bCacheOnlyOneBand)
        {
            /* In "one-band-at-a-time" strategy, if the last recently used */
            /* band is not that one, uncache it. We could use a smarter strategy */
            /* based on a LRU, but that's a bit overkill for now. */
            poGDS->poLastUsedBand->UncacheData();
            poGDS->nCachedBytes = 0;
        }
        else
        {
            /* Once we have cached more than nCachedBytesThreshold bytes, we will switch */
            /* to "one-band-at-a-time" strategy, instead of caching all bands that have */
            /* been accessed */
            if (poGDS->nCachedBytes > poGDS->nCachedBytesThreshold)
            {
                CPLDebug("GRIB", "Maximum band cache size reached for this dataset. "
                         "Caching only one band at a time from now");
                for(int i=0;i<poGDS->nBands;i++)
                {
                    ((GRIBRasterBand*) poGDS->GetRasterBand(i+1))->UncacheData();
                }
                poGDS->nCachedBytes = 0;
                poGDS->bCacheOnlyOneBand = TRUE;
            }
        }

        FileDataSource grib_fp (poGDS->fp);

        // we don't seem to have any way to detect errors in this!
        ReadGribData(grib_fp, start, subgNum, &m_Grib_Data, &m_Grib_MetaData);
        if( !m_Grib_Data )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Out of memory." );
            return CE_Failure;
        }

/* -------------------------------------------------------------------- */
/*      Check that this band matches the dataset as a whole, size       */
/*      wise. (#3246)                                                   */
/* -------------------------------------------------------------------- */
        nGribDataXSize = m_Grib_MetaData->gds.Nx;
        nGribDataYSize = m_Grib_MetaData->gds.Ny;

        poGDS->nCachedBytes += nGribDataXSize * nGribDataYSize * sizeof(double);
        poGDS->poLastUsedBand = this;

        if( nGribDataXSize != nRasterXSize 
            || nGribDataYSize != nRasterYSize )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Band %d of GRIB dataset is %dx%d, while the first band and dataset is %dx%d.  Georeferencing of band %d may be incorrect, and data access may be incomplete.", 
                      nBand, 
                      nGribDataXSize, nGribDataYSize, 
                      nRasterXSize, nRasterYSize, 
                      nBand );
        }
    }

    return CE_None;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GRIBRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                   void * pImage )

{
    CPLErr eErr = LoadData();
    if (eErr != CE_None)
        return eErr;

/* -------------------------------------------------------------------- */
/*      The image as read is always upside down to our normal           */
/*      orientation so we need to effectively flip it at this           */
/*      point.  We also need to deal with bands that are a different    */
/*      size than the dataset as a whole.                               */
/* -------------------------------------------------------------------- */
    if( nGribDataXSize == nRasterXSize
        && nGribDataYSize == nRasterYSize )
    {
        // Simple 1:1 case.
        memcpy(pImage, 
               m_Grib_Data + nRasterXSize * (nRasterYSize - nBlockYOff - 1), 
               nRasterXSize * sizeof(double));
        
        return CE_None;
    }
    else
    {
        memset( pImage, 0, sizeof(double)*nRasterXSize );

        if( nBlockYOff >= nGribDataYSize ) // off image?
            return CE_None;

        int nCopyWords = MIN(nRasterXSize,nGribDataXSize);

        memcpy( pImage, 
                m_Grib_Data + nGribDataXSize*(nGribDataYSize-nBlockYOff-1),
                nCopyWords * sizeof(double) );
        
        return CE_None;
    }
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double GRIBRasterBand::GetNoDataValue( int *pbSuccess )
{
    CPLErr eErr = LoadData();
    if (eErr != CE_None ||
        m_Grib_MetaData == NULL ||
        m_Grib_MetaData->gridAttrib.f_miss == 0)
    {
        if (pbSuccess)
            *pbSuccess = FALSE;
        return 0;
    }

    if (m_Grib_MetaData->gridAttrib.f_miss == 2)
    {
        /* what TODO ? */
        CPLDebug("GRIB", "Secondary missing value also set for band %d : %f",
                 nBand, m_Grib_MetaData->gridAttrib.missSec);
    }

    if (pbSuccess)
        *pbSuccess = TRUE;
    return m_Grib_MetaData->gridAttrib.missPri;
}

/************************************************************************/
/*                            ReadGribData()                            */
/************************************************************************/

void GRIBRasterBand::ReadGribData( DataSource & fp, sInt4 start, int subgNum, double** data, grib_MetaData** metaData)
{
    /* Initialisation, for calling the ReadGrib2Record function */
    sInt4 f_endMsg = 1;  /* 1 if we read the last grid in a GRIB message, or we haven't read any messages. */
    // int subgNum = 0;     /* The subgrid in the message that we are interested in. */
    sChar f_unit = 2;        /* None = 0, English = 1, Metric = 2 */
    double majEarth = 0;     /* -radEarth if < 6000 ignore, otherwise use this to
                              * override the radEarth in the GRIB1 or GRIB2
                              * message.  Needed because NCEP uses 6371.2 but GRIB1 could only state 6367.47. */
    double minEarth = 0;     /* -minEarth if < 6000 ignore, otherwise use this to
                              * override the minEarth in the GRIB1 or GRIB2 message. */
    sChar f_SimpleVer = 4;   /* Which version of the simple NDFD Weather table to
                              * use. (1 is 6/2003) (2 is 1/2004) (3 is 2/2004)
                              * (4 is 11/2004) (default 4) */
    LatLon lwlf;         /* lower left corner (cookie slicing) -lwlf */
    LatLon uprt;         /* upper right corner (cookie slicing) -uprt */
    IS_dataType is;      /* Un-parsed meta data for this GRIB2 message. As well as some memory used by the unpacker. */

    lwlf.lat = -100; // lat == -100 instructs the GRIB decoder that we don't want a subgrid

    IS_Init (&is);

    const char* pszGribNormalizeUnits = CPLGetConfigOption("GRIB_NORMALIZE_UNITS", NULL);
    if ( pszGribNormalizeUnits != NULL && ( STRCASECMP(pszGribNormalizeUnits,"NO")==0 ) )
        f_unit = 0; /* do not normalize units to metric */

    /* Read GRIB message from file position "start". */
    fp.DataSourceFseek(start, SEEK_SET);
    uInt4 grib_DataLen = 0;  /* Size of Grib_Data. */
    *metaData = new grib_MetaData();
    MetaInit (*metaData);
    ReadGrib2Record (fp, f_unit, data, &grib_DataLen, *metaData, &is, subgNum,
                     majEarth, minEarth, f_SimpleVer, &f_endMsg, &lwlf, &uprt);

    char * errMsg = errSprintf(NULL); // no intention to show errors, just swallow it and free the memory
    if( errMsg != NULL )
        CPLDebug( "GRIB", "%s", errMsg );
    free(errMsg);
    IS_Free(&is);
}

/************************************************************************/
/*                            UncacheData()                             */
/************************************************************************/

void GRIBRasterBand::UncacheData()
{
    if (m_Grib_Data)
        free (m_Grib_Data);
    m_Grib_Data = NULL;
    if (m_Grib_MetaData)
    {
        MetaFree( m_Grib_MetaData );
        delete m_Grib_MetaData;
    }
    m_Grib_MetaData = NULL;
}

/************************************************************************/
/*                           ~GRIBRasterBand()                          */
/************************************************************************/

GRIBRasterBand::~GRIBRasterBand()
{
    CPLFree(longFstLevel);
    UncacheData();
}

/************************************************************************/
/* ==================================================================== */
/*				GRIBDataset				*/
/* ==================================================================== */
/************************************************************************/

GRIBDataset::GRIBDataset()

{
  poTransform = NULL;
  pszProjection = CPLStrdup("");
  adfGeoTransform[0] = 0.0;
  adfGeoTransform[1] = 1.0;
  adfGeoTransform[2] = 0.0;
  adfGeoTransform[3] = 0.0;
  adfGeoTransform[4] = 0.0;
  adfGeoTransform[5] = 1.0;

  nCachedBytes = 0;
  /* Switch caching strategy once 100 MB threshold is reached */
  /* Why 100 MB ? --> why not ! */
  nCachedBytesThreshold = ((GIntBig)atoi(CPLGetConfigOption("GRIB_CACHEMAX", "100"))) * 1024 * 1024;
  bCacheOnlyOneBand = FALSE;
  poLastUsedBand = NULL;
}

/************************************************************************/
/*                            ~GRIBDataset()                             */
/************************************************************************/

GRIBDataset::~GRIBDataset()

{
    FlushCache();
    if( fp != NULL )
        VSIFCloseL( fp );
		
    CPLFree( pszProjection );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GRIBDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform,  adfGeoTransform, sizeof(double) * 6 );
    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *GRIBDataset::GetProjectionRef()

{
    return pszProjection;
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int GRIBDataset::Identify( GDALOpenInfo * poOpenInfo )
{
    if (poOpenInfo->nHeaderBytes < 8)
        return FALSE;
        
/* -------------------------------------------------------------------- */
/*      Does a part of what ReadSECT0() but in a thread-safe way.       */
/* -------------------------------------------------------------------- */
    int i;
    for(i=0;i<poOpenInfo->nHeaderBytes-3;i++)
    {
        if (EQUALN((const char*)poOpenInfo->pabyHeader + i, "GRIB", 4) ||
            EQUALN((const char*)poOpenInfo->pabyHeader + i, "TDLP", 4))
            return TRUE;
    }
    
    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *GRIBDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify(poOpenInfo) )
        return NULL;
        
/* -------------------------------------------------------------------- */
/*      A fast "probe" on the header that is partially read in memory.  */
/* -------------------------------------------------------------------- */
    char *buff = NULL;
    uInt4 buffLen = 0;
    sInt4 sect0[SECT0LEN_WORD];
    uInt4 gribLen;
    int version;
// grib is not thread safe, make sure not to cause problems
// for other thread safe formats

    CPLMutexHolderD(&hGRIBMutex);
    MemoryDataSource mds (poOpenInfo->pabyHeader, poOpenInfo->nHeaderBytes);
    if (ReadSECT0 (mds, &buff, &buffLen, -1, sect0, &gribLen, &version) < 0) {
        free (buff);
        char * errMsg = errSprintf(NULL);
        if( errMsg != NULL && strstr(errMsg,"Ran out of file") == NULL )
            CPLDebug( "GRIB", "%s", errMsg );
        free(errMsg);
        return NULL;
    }
    free(buff);
    
/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The GRIB driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    GRIBDataset 	*poDS;

    poDS = new GRIBDataset();

    poDS->fp = VSIFOpenL( poOpenInfo->pszFilename, "r" );

	/* Check the return values */    
	if (!poDS->fp) {
        // we have no FP, so we don't have anywhere to read from
        char * errMsg = errSprintf(NULL);
        if( errMsg != NULL )
            CPLDebug( "GRIB", "%s", errMsg );
        free(errMsg);
		
		CPLError( CE_Failure, CPLE_OpenFailed, "Error (%d) opening file %s", errno, poOpenInfo->pszFilename);
        CPLReleaseMutex(hGRIBMutex); // Release hGRIBMutex otherwise we'll deadlock with GDALDataset own hGRIBMutex
        delete poDS;
        CPLAcquireMutex(hGRIBMutex, 1000.0);
        return NULL;
	}
    
/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- */
/*      Make an inventory of the GRIB file.                             */
/* The inventory does not contain all the information needed for        */
/* creating the RasterBands (especially the x and y size), therefore    */
/* the first GRIB band is also read for some additional metadata.       */
/* The band-data that is read is stored into the first RasterBand,      */
/* simply so that the same portion of the file is not read twice.       */
/* -------------------------------------------------------------------- */
    
    VSIFSeekL( poDS->fp, 0, SEEK_SET );

    FileDataSource grib_fp (poDS->fp);

    inventoryType *Inv = NULL;  /* Contains an GRIB2 message inventory of the file */
    uInt4 LenInv = 0;        /* size of Inv (also # of GRIB2 messages) */
    int msgNum =0;          /* The messageNumber during the inventory. */

    if (GRIB2Inventory (grib_fp, &Inv, &LenInv, 0, &msgNum) <= 0 )
    {
        char * errMsg = errSprintf(NULL);
        if( errMsg != NULL )
            CPLDebug( "GRIB", "%s", errMsg );
        free(errMsg);

        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "%s is a grib file, but no raster dataset was successfully identified.",
                  poOpenInfo->pszFilename );
        CPLReleaseMutex(hGRIBMutex); // Release hGRIBMutex otherwise we'll deadlock with GDALDataset own hGRIBMutex
        delete poDS;
        CPLAcquireMutex(hGRIBMutex, 1000.0);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create band objects.                                            */
/* -------------------------------------------------------------------- */
    for (uInt4 i = 0; i < LenInv; ++i)
    {
        uInt4 bandNr = i+1;
        if (bandNr == 1)
        {
            // important: set DataSet extents before creating first RasterBand in it
            double * data = NULL;
            grib_MetaData* metaData;
            GRIBRasterBand::ReadGribData(grib_fp, 0, Inv[i].subgNum, &data, &metaData);
            if (data == 0 || metaData->gds.Nx < 1 || metaData->gds.Ny < 1)
            {
                CPLError( CE_Failure, CPLE_OpenFailed, 
                          "%s is a grib file, but no raster dataset was successfully identified.",
                          poOpenInfo->pszFilename );
                CPLReleaseMutex(hGRIBMutex); // Release hGRIBMutex otherwise we'll deadlock with GDALDataset own hGRIBMutex
                delete poDS;
                CPLAcquireMutex(hGRIBMutex, 1000.0);
                return NULL;
            }

            poDS->SetGribMetaData(metaData); // set the DataSet's x,y size, georeference and projection from the first GRIB band
            GRIBRasterBand* gribBand = new GRIBRasterBand( poDS, bandNr, Inv+i);

            if( Inv->GribVersion == 2 )
                gribBand->FindPDSTemplate();

            gribBand->m_Grib_Data = data;
            gribBand->m_Grib_MetaData = metaData;
            poDS->SetBand( bandNr, gribBand);
        }
        else
            poDS->SetBand( bandNr, new GRIBRasterBand( poDS, bandNr, Inv+i ));
        GRIB2InventoryFree (Inv + i);
    }
    free (Inv);

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    
    CPLReleaseMutex(hGRIBMutex); // Release hGRIBMutex otherwise we'll deadlock with GDALDataset own hGRIBMutex
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for external overviews.                                   */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename, poOpenInfo->papszSiblingFiles );
    CPLAcquireMutex(hGRIBMutex, 1000.0);

    return( poDS );
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

void GRIBDataset::SetGribMetaData(grib_MetaData* meta)
{
    nRasterXSize = meta->gds.Nx;
    nRasterYSize = meta->gds.Ny;

/* -------------------------------------------------------------------- */
/*      Image projection.                                               */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;

    switch(meta->gds.projType)
    {
      case GS3_LATLON:
      case GS3_GAUSSIAN_LATLON:
          // No projection, only latlon system (geographic)
          break;
      case GS3_MERCATOR:
        oSRS.SetMercator(meta->gds.meshLat, meta->gds.orientLon,
                         1.0, 0.0, 0.0);
        break;
      case GS3_POLAR:
        oSRS.SetPS(meta->gds.meshLat, meta->gds.orientLon,
                   meta->gds.scaleLat1,
                   0.0, 0.0);
        break;
      case GS3_LAMBERT:
        oSRS.SetLCC(meta->gds.scaleLat1, meta->gds.scaleLat2,
                    meta->gds.meshLat, meta->gds.orientLon,
                    0.0, 0.0); // set projection
        break;
			

      case GS3_ORTHOGRAPHIC:

        //oSRS.SetOrthographic(0.0, meta->gds.orientLon,
        //											meta->gds.lon2, meta->gds.lat2);
        //oSRS.SetGEOS(meta->gds.orientLon, meta->gds.stretchFactor, meta->gds.lon2, meta->gds.lat2);
        oSRS.SetGEOS(  0, 35785831, 0, 0 ); // hardcoded for now, I don't know yet how to parse the meta->gds section
        break;
      case GS3_EQUATOR_EQUIDIST:
        break;
      case GS3_AZIMUTH_RANGE:
        break;
    }

/* -------------------------------------------------------------------- */
/*      Earth model                                                     */
/* -------------------------------------------------------------------- */
    double a = meta->gds.majEarth * 1000.0; // in meters
    double b = meta->gds.minEarth * 1000.0;
    if( a == 0 && b == 0 )
    {
        a = 6377563.396;
        b = 6356256.910;
    }

    if (meta->gds.f_sphere)
    {
        oSRS.SetGeogCS( "Coordinate System imported from GRIB file",
                        NULL,
                        "Sphere",
                        a, 0.0 );
    }
    else
    {
        double fInv = a/(a-b);
        oSRS.SetGeogCS( "Coordinate System imported from GRIB file",
                        NULL,
                        "Spheroid imported from GRIB file",
                        a, fInv );
    }

    OGRSpatialReference oLL; // construct the "geographic" part of oSRS
    oLL.CopyGeogCSFrom( &oSRS );

    double rMinX;
    double rMaxY;
    double rPixelSizeX;
    double rPixelSizeY;
    if (meta->gds.projType == GS3_ORTHOGRAPHIC)
    {
        //rMinX = -meta->gds.Dx * (meta->gds.Nx / 2); // This is what should work, but it doesn't .. Dx seems to have an inverse relation with pixel size
        //rMaxY = meta->gds.Dy * (meta->gds.Ny / 2);
        const double geosExtentInMeters = 11137496.552; // hardcoded for now, assumption: GEOS projection, full disc (like MSG)
        rMinX = -(geosExtentInMeters / 2);
        rMaxY = geosExtentInMeters / 2;
        rPixelSizeX = geosExtentInMeters / meta->gds.Nx;
        rPixelSizeY = geosExtentInMeters / meta->gds.Ny;
    }
    else if( oSRS.IsProjected() )
    {
        rMinX = meta->gds.lon1; // longitude in degrees, to be transformed to meters (or degrees in case of latlon)
        rMaxY = meta->gds.lat1; // latitude in degrees, to be transformed to meters 
        OGRCoordinateTransformation *poTransformLLtoSRS = OGRCreateCoordinateTransformation( &(oLL), &(oSRS) );
        if ((poTransformLLtoSRS != NULL) && poTransformLLtoSRS->Transform( 1, &rMinX, &rMaxY )) // transform it to meters
        {
            if (meta->gds.scan == GRIB2BIT_2) // Y is minY, GDAL wants maxY
                rMaxY += (meta->gds.Ny - 1) * meta->gds.Dy; // -1 because we GDAL needs the coordinates of the centre of the pixel
            rPixelSizeX = meta->gds.Dx;
            rPixelSizeY = meta->gds.Dy;
        }
        else
        {
            rMinX = 0.0;
            rMaxY = 0.0;
            
            rPixelSizeX = 1.0;
            rPixelSizeY = -1.0;
            
            oSRS.Clear();

            CPLError( CE_Warning, CPLE_AppDefined,
                      "Unable to perform coordinate transformations, so the correct\n"
                      "projected geotransform could not be deduced from the lat/long\n"
                      "control points.  Defaulting to ungeoreferenced." );
        }
        delete poTransformLLtoSRS;
    }
    else
    {
        rMinX = meta->gds.lon1; // longitude in degrees, to be transformed to meters (or degrees in case of latlon)
        rMaxY = meta->gds.lat1; // latitude in degrees, to be transformed to meters 

        double rMinY = meta->gds.lat2;
        if (meta->gds.lat2 > rMaxY)
        {
          rMaxY = meta->gds.lat2;
          rMinY = meta->gds.lat1;
        }

        if (meta->gds.lon1 > meta->gds.lon2)
          rPixelSizeX = (360.0 - (meta->gds.lon1 - meta->gds.lon2)) / (meta->gds.Nx - 1);
        else
          rPixelSizeX = (meta->gds.lon2 - meta->gds.lon1) / (meta->gds.Nx - 1);

        rPixelSizeY = (rMaxY - rMinY) / (meta->gds.Ny - 1);

        // Do some sanity checks for cases that can't be handled by the above
        // pixel size corrections. GRIB1 has a minimum precision of 0.001
        // for latitudes and longitudes, so we'll allow a bit higher than that.
        if (rPixelSizeX < 0 || fabs(rPixelSizeX - meta->gds.Dx) > 0.002)
          rPixelSizeX = meta->gds.Dx;

        if (rPixelSizeY < 0 || fabs(rPixelSizeY - meta->gds.Dy) > 0.002)
          rPixelSizeY = meta->gds.Dy;
    }

    // http://gdal.org/gdal_datamodel.html :
    //   we need the top left corner of the top left pixel.
    //   At the moment we have the center of the pixel.
    rMinX-=rPixelSizeX/2;
    rMaxY+=rPixelSizeY/2;

    adfGeoTransform[0] = rMinX;
    adfGeoTransform[3] = rMaxY;
    adfGeoTransform[1] = rPixelSizeX;
    adfGeoTransform[5] = -rPixelSizeY;

    CPLFree( pszProjection );
    pszProjection = NULL;
    oSRS.exportToWkt( &(pszProjection) );
}

/************************************************************************/
/*                       GDALDeregister_GRIB()                          */
/************************************************************************/

static void GDALDeregister_GRIB(GDALDriver* )
{
    if( hGRIBMutex != NULL )
    {
        CPLDestroyMutex(hGRIBMutex);
        hGRIBMutex = NULL;
    }
}

/************************************************************************/
/*                         GDALRegister_GRIB()                          */
/************************************************************************/

void GDALRegister_GRIB()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "GRIB" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "GRIB" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "GRIdded Binary (.grb)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_grib.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "grb" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = GRIBDataset::Open;
        poDriver->pfnIdentify = GRIBDataset::Identify;
        poDriver->pfnUnloadDriver = GDALDeregister_GRIB;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
