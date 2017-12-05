/******************************************************************************
 *
 * Project:  OziExplorer .MAP Driver
 * Purpose:  GDALDataset driver for OziExplorer .MAP files
 * Author:   Jean-Claude Repetto, <jrepetto at @free dot fr>
 *
 ******************************************************************************
 * Copyright (c) 2012, Jean-Claude Repetto
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_proxy.h"
#include "ogr_geometry.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                                MAPDataset                            */
/* ==================================================================== */
/************************************************************************/

class CPL_DLL MAPDataset : public GDALDataset
{
    GDALDataset *poImageDS;

    char        *pszWKT;
    int         bGeoTransformValid;
    double      adfGeoTransform[6];
    int         nGCPCount;
    GDAL_GCP    *pasGCPList;
    OGRPolygon  *poNeatLine;
    CPLString   osImgFilename;

  public:
    MAPDataset();
    virtual ~MAPDataset();

    virtual const char* GetProjectionRef() override;
    virtual CPLErr      GetGeoTransform( double * ) override;
    virtual int GetGCPCount() override;
    virtual const char *GetGCPProjection() override;
    virtual const GDAL_GCP *GetGCPs() override;
    virtual char **GetFileList() override;

    virtual int         CloseDependentDatasets() override;

    static GDALDataset *Open( GDALOpenInfo * );
    static int Identify( GDALOpenInfo *poOpenInfo );
};

/************************************************************************/
/* ==================================================================== */
/*                         MAPWrapperRasterBand                         */
/* ==================================================================== */
/************************************************************************/
class MAPWrapperRasterBand : public GDALProxyRasterBand
{
  GDALRasterBand* poBaseBand;

  protected:
    virtual GDALRasterBand* RefUnderlyingRasterBand() override { return poBaseBand; }

  public:
    explicit MAPWrapperRasterBand( GDALRasterBand* poBaseBandIn )
        {
            this->poBaseBand = poBaseBandIn;
            eDataType = poBaseBand->GetRasterDataType();
            poBaseBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
        }
    ~MAPWrapperRasterBand() {}
};

/************************************************************************/
/* ==================================================================== */
/*                             MAPDataset                               */
/* ==================================================================== */
/************************************************************************/

MAPDataset::MAPDataset() :
    poImageDS(NULL),
    pszWKT(NULL),
    bGeoTransformValid(false),
    nGCPCount(0),
    pasGCPList(NULL),
    poNeatLine(NULL)
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~MAPDataset()                             */
/************************************************************************/

MAPDataset::~MAPDataset()

{
    if (poImageDS != NULL)
    {
        GDALClose( poImageDS );
        poImageDS = NULL;
    }

    CPLFree(pszWKT);

    if (nGCPCount)
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree(pasGCPList);
    }

    if ( poNeatLine != NULL )
    {
        delete poNeatLine;
        poNeatLine = NULL;
    }
}

/************************************************************************/
/*                       CloseDependentDatasets()                       */
/************************************************************************/

int MAPDataset::CloseDependentDatasets()
{
    int bRet = GDALDataset::CloseDependentDatasets();
    if (poImageDS != NULL)
    {
        GDALClose( poImageDS );
        poImageDS = NULL;
        bRet = TRUE;
    }
    return bRet;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int MAPDataset::Identify( GDALOpenInfo *poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes < 200
        || !EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"MAP") )
        return FALSE;

    if( strstr(reinterpret_cast<const char *>( poOpenInfo->pabyHeader ),
               "OziExplorer Map Data File") == NULL )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *MAPDataset::Open( GDALOpenInfo * poOpenInfo )
{
    if( !Identify( poOpenInfo ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The MAP driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */

    MAPDataset *poDS = new MAPDataset();

/* -------------------------------------------------------------------- */
/*      Try to load and parse the .MAP file.                            */
/* -------------------------------------------------------------------- */

    bool bOziFileOK =
         CPL_TO_BOOL(GDALLoadOziMapFile( poOpenInfo->pszFilename,
                             poDS->adfGeoTransform,
                             &poDS->pszWKT,
                             &poDS->nGCPCount, &poDS->pasGCPList ));

    if ( bOziFileOK && poDS->nGCPCount == 0 )
         poDS->bGeoTransformValid = TRUE;

    /* We need to read again the .map file because the GDALLoadOziMapFile function
       does not returns all required data . An API change is necessary : maybe in GDAL 2.0 ? */

    char **papszLines = CSLLoad2( poOpenInfo->pszFilename, 200, 200, NULL );

    if ( !papszLines )
    {
        delete poDS;
        return NULL;
    }

    const int nLines = CSLCount( papszLines );
    if( nLines < 2 )
    {
        delete poDS;
        CSLDestroy(papszLines);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      We need to open the image in order to establish                 */
/*      details like the band count and types.                          */
/* -------------------------------------------------------------------- */
    poDS->osImgFilename = papszLines[2];

    const CPLString osPath = CPLGetPath(poOpenInfo->pszFilename);
    if (CPLIsFilenameRelative(poDS->osImgFilename))
    {
        poDS->osImgFilename = CPLFormCIFilename(osPath, poDS->osImgFilename, NULL);
    }
    else
    {
        VSIStatBufL sStat;
        if (VSIStatL(poDS->osImgFilename, &sStat) != 0)
        {
            poDS->osImgFilename = CPLGetFilename(poDS->osImgFilename);
            poDS->osImgFilename = CPLFormCIFilename(osPath, poDS->osImgFilename, NULL);
        }
    }

/* -------------------------------------------------------------------- */
/*      Try and open the file.                                          */
/* -------------------------------------------------------------------- */
    poDS->poImageDS = reinterpret_cast<GDALDataset *>(
        GDALOpen(poDS->osImgFilename, GA_ReadOnly ) );
    if( poDS->poImageDS == NULL || poDS->poImageDS->GetRasterCount() == 0)
    {
        CSLDestroy(papszLines);
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Attach the bands.                                               */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = poDS->poImageDS->GetRasterXSize();
    poDS->nRasterYSize = poDS->poImageDS->GetRasterYSize();
    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        GDALClose( poDS->poImageDS );
        delete poDS;
        return NULL;
    }

    for( int iBand = 1; iBand <= poDS->poImageDS->GetRasterCount(); iBand++ )
        poDS->SetBand( iBand,
                       new MAPWrapperRasterBand( poDS->poImageDS->GetRasterBand( iBand )) );

/* -------------------------------------------------------------------- */
/*      Add the neatline/cutline, if required                           */
/* -------------------------------------------------------------------- */

    /* First, we need to check if it is necessary to define a neatline */
    bool bNeatLine = false;
    for ( int iLine = 10; iLine < nLines; iLine++ )
    {
        if ( STARTS_WITH_CI(papszLines[iLine], "MMPXY,") )
        {
            char **papszTok
                = CSLTokenizeString2( papszLines[iLine], ",",
                                      CSLT_STRIPLEADSPACES
                                      | CSLT_STRIPENDSPACES );

            if ( CSLCount(papszTok) != 4 )
            {
                CSLDestroy(papszTok);
                continue;
            }

            const int x = atoi(papszTok[2]);
            const int y = atoi(papszTok[3]);
            if (( x != 0 && x != poDS->nRasterXSize) || (y != 0 && y != poDS->nRasterYSize) )
            {
                bNeatLine = true;
                CSLDestroy(papszTok);
                break;
            }
            CSLDestroy(papszTok);
        }
    }

    /* Create and fill the neatline polygon */
    if (bNeatLine)
    {
        poDS->poNeatLine = new OGRPolygon();   /* Create a polygon to store the neatline */
        OGRLinearRing* poRing = new OGRLinearRing();

        if ( poDS->bGeoTransformValid )        /* Compute the projected coordinates of the corners */
        {
            for ( int iLine = 10; iLine < nLines; iLine++ )
            {
                if ( STARTS_WITH_CI(papszLines[iLine], "MMPXY,") )
                {
                    char **papszTok
                        = CSLTokenizeString2( papszLines[iLine], ",",
                                              CSLT_STRIPLEADSPACES
                                              | CSLT_STRIPENDSPACES );

                    if ( CSLCount(papszTok) != 4 )
                    {
                        CSLDestroy(papszTok);
                        continue;
                    }

                    const double x = CPLAtofM(papszTok[2]);
                    const double y = CPLAtofM(papszTok[3]);
                    const double X = poDS->adfGeoTransform[0] + x * poDS->adfGeoTransform[1] +
                        y * poDS->adfGeoTransform[2];
                    const double Y = poDS->adfGeoTransform[3] + x * poDS->adfGeoTransform[4] +
                        y * poDS->adfGeoTransform[5];
                    poRing->addPoint(X, Y);
                    CPLDebug( "CORNER MMPXY", "%f, %f, %f, %f", x, y, X, Y);
                    CSLDestroy(papszTok);
                }
            }
        }
        else /* Convert the geographic coordinates to projected coordinates */
        {
            OGRCoordinateTransformation *poTransform = NULL;
            char *pszWKT = poDS->pszWKT;

            if ( pszWKT != NULL )
            {
                OGRSpatialReference oSRS;
                OGRSpatialReference *poLatLong = NULL;
                if ( OGRERR_NONE == oSRS.importFromWkt ( &pszWKT ))
                    poLatLong = oSRS.CloneGeogCS();
                if ( poLatLong )
                    poTransform = OGRCreateCoordinateTransformation( poLatLong, &oSRS );
                if ( poLatLong )
                    delete poLatLong;
            }

            for ( int iLine = 10; iLine < nLines; iLine++ )
            {
                if ( STARTS_WITH_CI(papszLines[iLine], "MMPLL,") )
                {
                    CPLDebug( "MMPLL", "%s", papszLines[iLine] );
                    char **papszTok = NULL;

                    papszTok = CSLTokenizeString2( papszLines[iLine], ",",
                                                   CSLT_STRIPLEADSPACES
                                                   | CSLT_STRIPENDSPACES );

                    if ( CSLCount(papszTok) != 4 )
                    {
                         CSLDestroy(papszTok);
                         continue;
                    }

                    double dfLon = CPLAtofM(papszTok[2]);
                    double dfLat = CPLAtofM(papszTok[3]);

                    if ( poTransform )
                        poTransform->Transform( 1, &dfLon, &dfLat );
                    poRing->addPoint(dfLon, dfLat);
                    CPLDebug( "CORNER MMPLL", "%f, %f", dfLon, dfLat);
                    CSLDestroy(papszTok);
                }
            }
            if ( poTransform )
                delete poTransform;
        }

        poRing->closeRings();
        poDS->poNeatLine->addRingDirectly(poRing);

        char* pszNeatLineWkt = NULL;
        poDS->poNeatLine->exportToWkt(&pszNeatLineWkt);
        CPLDebug( "NEATLINE", "%s", pszNeatLineWkt);
        poDS->SetMetadataItem("NEATLINE", pszNeatLineWkt);
        CPLFree(pszNeatLineWkt);
    }

    CSLDestroy(papszLines);

    return poDS;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char* MAPDataset::GetProjectionRef()
{
    return (pszWKT && nGCPCount == 0) ? pszWKT : "";
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr MAPDataset::GetGeoTransform( double * padfTransform )

{
    memcpy(padfTransform, adfGeoTransform, 6 * sizeof(double));

    return (nGCPCount == 0) ? CE_None : CE_Failure;
}

/************************************************************************/
/*                           GetGCPCount()                              */
/************************************************************************/

int MAPDataset::GetGCPCount()
{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char * MAPDataset::GetGCPProjection()
{
    return (pszWKT && nGCPCount != 0) ? pszWKT : "";
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP * MAPDataset::GetGCPs()
{
    return pasGCPList;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char** MAPDataset::GetFileList()
{
    char **papszFileList = GDALDataset::GetFileList();

    papszFileList = CSLAddString( papszFileList, osImgFilename );

    return papszFileList;
}

/************************************************************************/
/*                          GDALRegister_MAP()                          */
/************************************************************************/

void GDALRegister_MAP()

{
    if( GDALGetDriverByName( "MAP" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "MAP" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "OziExplorer .MAP" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_map.html" );

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = MAPDataset::Open;
    poDriver->pfnIdentify = MAPDataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
