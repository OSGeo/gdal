/******************************************************************************
 *
 * Project:  NTF Translator
 * Purpose:  NTFFileReader class implementation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
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

#include <stdarg.h>
#include "ntf.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_api.h"

#include <algorithm>

CPL_CVSID("$Id$")

#define DIGIT_ZERO '0'

static int DefaultNTFRecordGrouper( NTFFileReader *, NTFRecord **,
                                    NTFRecord * );

/************************************************************************/
/*                            NTFFileReader                             */
/************************************************************************/

NTFFileReader::NTFFileReader( OGRNTFDataSource * poDataSource ) :
    pszFilename(nullptr),
    poDS(poDataSource),
    fp(nullptr),
    nFCCount(0),
    papszFCNum(nullptr),
    papszFCName(nullptr),
    nAttCount(0),
    pasAttDesc(nullptr),
    pszTileName(nullptr),
    nCoordWidth(6),
    nZWidth(6),
    nNTFLevel(0),
    dfXYMult(1.0),
    dfZMult(1.0),
    dfXOrigin(0),
    dfYOrigin(0),
    dfTileXSize(0),
    dfTileYSize(0),
    dfScale(0.0),
    dfPaperToGround(0.0),
    nStartPos(0),
    nPreSavedPos(0),
    nPostSavedPos(0),
    poSavedRecord(nullptr),
    nSavedFeatureId(1),
    nBaseFeatureId(1),
    nFeatureCount(-1),
    pszProduct(nullptr),
    pszPVName(nullptr),
    nProduct(NPC_UNKNOWN),
    pfnRecordGrouper(DefaultNTFRecordGrouper),
    bIndexBuilt(FALSE),
    bIndexNeeded(FALSE),
    nRasterXSize(1),
    nRasterYSize(1),
    nRasterDataType(1),
    poRasterLayer(nullptr),
    panColumnOffset(nullptr),
    bCacheLines(TRUE),
    nLineCacheSize(0),
    papoLineCache(nullptr)
{
    apoCGroup[0] = nullptr;
    apoCGroup[1] = nullptr;
    apoCGroup[MAX_REC_GROUP] = nullptr;
    memset( adfGeoTransform, 0, sizeof(adfGeoTransform) );
    memset( apoTypeTranslation, 0, sizeof(apoTypeTranslation) );
    for( int i = 0; i < 100; i++ )
    {
        anIndexSize[i] = 0;
        apapoRecordIndex[i] = nullptr;
    }
    if( poDS->GetOption("CACHE_LINES") != nullptr
        && EQUAL(poDS->GetOption("CACHE_LINES"),"OFF") )
        bCacheLines = FALSE;
}

/************************************************************************/
/*                           ~NTFFileReader()                           */
/************************************************************************/

NTFFileReader::~NTFFileReader()

{
    CacheClean();
    DestroyIndex();
    ClearDefs();
    CPLFree( pszFilename );
    CPLFree( panColumnOffset );
}

/************************************************************************/
/*                             SetBaseFID()                             */
/************************************************************************/

void NTFFileReader::SetBaseFID( long nNewBase )

{
    CPLAssert( nSavedFeatureId == 1 );

    nBaseFeatureId = nNewBase;
    nSavedFeatureId = nBaseFeatureId;
}

/************************************************************************/
/*                             ClearDefs()                              */
/*                                                                      */
/*      Clear attribute definitions and feature classes.  All the       */
/*      stuff that would have to be cleaned up by Open(), and the       */
/*      destructor.                                                     */
/************************************************************************/

void NTFFileReader::ClearDefs()

{
    Close();

    ClearCGroup();

    CSLDestroy( papszFCNum );
    papszFCNum = nullptr;
    CSLDestroy( papszFCName );
    papszFCName = nullptr;
    nFCCount = 0;

    for( int i = 0; i < nAttCount; i++ )
    {
        if( pasAttDesc[i].poCodeList != nullptr )
            delete pasAttDesc[i].poCodeList;
    }

    CPLFree( pasAttDesc );
    nAttCount = 0;
    pasAttDesc = nullptr;

    CPLFree( pszProduct );
    pszProduct = nullptr;

    CPLFree( pszPVName );
    pszPVName = nullptr;

    CPLFree( pszTileName );
    pszTileName = nullptr;
}

/************************************************************************/
/*                               Close()                                */
/*                                                                      */
/*      Close the file, but don't wipe out our knowledge about this     */
/*      file.                                                           */
/************************************************************************/

void NTFFileReader::Close()

{
    if( poSavedRecord != nullptr )
        delete poSavedRecord;
    poSavedRecord = nullptr;

    nPreSavedPos = nPostSavedPos = 0;
    nSavedFeatureId = nBaseFeatureId;
    if( fp != nullptr )
    {
        VSIFCloseL( fp );
        fp = nullptr;
    }

    CacheClean();
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int NTFFileReader::Open( const char * pszFilenameIn )

{
    if( pszFilenameIn != nullptr )
    {
        ClearDefs();

        CPLFree( pszFilename );
        pszFilename = CPLStrdup( pszFilenameIn );
    }
    else
        Close();

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    fp = VSIFOpenL( pszFilename, "rb" );

    // notdef: we should likely issue a proper CPL error message based
    // based on errno here.
    if( fp == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to open file `%s' for read access.\n",
                  pszFilename );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      If we are just reopening an existing file we will just scan     */
/*      past the section header ... no need to reform all the definitions.*/
/* -------------------------------------------------------------------- */
    if( pszFilenameIn == nullptr )
    {
        NTFRecord *poRecord = nullptr;

        for( poRecord = new NTFRecord( fp );
             poRecord->GetType() != NRT_VTR && poRecord->GetType() != NRT_SHR;
             poRecord = new NTFRecord( fp ) )
        {
            delete poRecord;
        }

        delete poRecord;

        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Read the first record, and verify it is a proper volume header. */
/* -------------------------------------------------------------------- */
    NTFRecord      oVHR( fp );

    if( oVHR.GetType() != NRT_VHR )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "File `%s' appears to not be a UK NTF file.\n",
                  pszFilename );
        return FALSE;
    }

    nNTFLevel = atoi(oVHR.GetField( 57, 57 ));
    if( !( nNTFLevel >= 1 && nNTFLevel <= 5 ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid value : nNTFLevel = %d", nNTFLevel );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Read records till we get the section header.                    */
/* -------------------------------------------------------------------- */
    NTFRecord *poRecord = nullptr;

    for( poRecord = new NTFRecord( fp );
         poRecord->GetType() != NRT_VTR && poRecord->GetType() != NRT_SHR;
         poRecord = new NTFRecord( fp ) )
    {
/* -------------------------------------------------------------------- */
/*      Handle feature class name records.                              */
/* -------------------------------------------------------------------- */
        if( poRecord->GetType() == NRT_FCR && poRecord->GetLength() >= 37 )
        {
            nFCCount++;

            papszFCNum = CSLAddString( papszFCNum, poRecord->GetField(3,6) );

            CPLString osFCName;
            const char *pszData = poRecord->GetData();

            // CODE_COM
            int iChar = 15;
            for( ; pszData[iChar] == ' ' && iChar > 5; iChar-- ) {}

            if( iChar > 6 )
                osFCName += poRecord->GetField(7,iChar+1);

            // STCLASS
            for( iChar = 35; pszData[iChar] == ' ' && iChar > 15; iChar-- ) {}

            if( iChar > 15 )
            {
                if( !osFCName.empty() )
                    osFCName += " : " ;
                osFCName += poRecord->GetField(17,iChar+1);
            }

            // FEATDES
            for( iChar = 36;
                 pszData[iChar] != '\0' && pszData[iChar] != '\\';
                 iChar++ ) {}

            if( iChar > 37 )
            {
                if( !osFCName.empty() )
                    osFCName += " : " ;
                osFCName += poRecord->GetField(37,iChar);
            }

            papszFCName = CSLAddString(papszFCName, osFCName );
        }

/* -------------------------------------------------------------------- */
/*      Handle attribute description records.                           */
/* -------------------------------------------------------------------- */
        else if( poRecord->GetType() == NRT_ADR )
        {
            nAttCount++;

            pasAttDesc = static_cast<NTFAttDesc *>(
                CPLRealloc( pasAttDesc, sizeof(NTFAttDesc) * nAttCount ));
            memset( &pasAttDesc[nAttCount-1], 0, sizeof(NTFAttDesc) );

            if( !ProcessAttDesc( poRecord, pasAttDesc + nAttCount - 1 ) )
                nAttCount --;
        }

/* -------------------------------------------------------------------- */
/*      Handle attribute description records.                           */
/* -------------------------------------------------------------------- */
        else if( poRecord->GetType() == NRT_CODELIST )
        {
            NTFCodeList *poCodeList = new NTFCodeList( poRecord );
            NTFAttDesc  *psAttDesc = GetAttDesc( poCodeList->szValType );
            if( psAttDesc == nullptr )
            {
                CPLDebug( "NTF", "Got CODELIST for %s without ATTDESC.",
                          poCodeList->szValType );
                delete poCodeList;
            }
            else if( psAttDesc->poCodeList != nullptr )
            {
                // Should not happen on sane files.
                delete poCodeList;
            }
            else
            {
                psAttDesc->poCodeList = poCodeList;
            }
        }

/* -------------------------------------------------------------------- */
/*      Handle database header record.                                  */
/* -------------------------------------------------------------------- */
        else if( poRecord->GetType() == NRT_DHR && pszProduct == nullptr )
        {
            pszProduct = CPLStrdup(poRecord->GetField(3,22));
            for( int iChar = static_cast<int>(strlen(pszProduct))-1;
                 iChar > 0 && pszProduct[iChar] == ' ';
                 pszProduct[iChar--] = '\0' ) {}

            pszPVName = CPLStrdup(poRecord->GetField(76+3,76+22));
            for( int iChar = static_cast<int>(strlen(pszPVName))-1;
                 iChar > 0 && pszPVName[iChar] == ' ';
                 pszPVName[iChar--] = '\0' ) {}
        }

        delete poRecord;
    }

/* -------------------------------------------------------------------- */
/*      Did we fall off the end without finding what we were looking    */
/*      for?                                                            */
/* -------------------------------------------------------------------- */
    if( poRecord->GetType() == NRT_VTR )
    {
        delete poRecord;
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Could not find section header record in %s.\n",
                  pszFilename );
        return FALSE;
    }

    if( pszProduct == nullptr )
    {
        delete poRecord;
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Could not find product type in %s.\n",
                  pszFilename );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Classify the product type.                                      */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszProduct, "LAND-LINE") && strlen(pszPVName) > 5 &&
        CPLAtof(pszPVName+5) < 1.3 )
        nProduct = NPC_LANDLINE;
    else if( STARTS_WITH_CI(pszProduct, "LAND-LINE") )
        nProduct = NPC_LANDLINE99;
    else if( EQUAL(pszProduct,"OS_LANDRANGER_CONT") ) // Panorama
        nProduct = NPC_LANDRANGER_CONT;
    else if( EQUAL(pszProduct,"L-F_PROFILE_CON") ) // Panorama
        nProduct = NPC_LANDFORM_PROFILE_CONT;
    else if( STARTS_WITH_CI(pszProduct, "Strategi") )
        nProduct = NPC_STRATEGI;
    else if( STARTS_WITH_CI(pszProduct, "Meridian_02") )
        nProduct = NPC_MERIDIAN2;
    else if( STARTS_WITH_CI(pszProduct, "Meridian_01") )
        nProduct = NPC_MERIDIAN;
    else if( EQUAL(pszProduct,NTF_BOUNDARYLINE)
             && STARTS_WITH_CI(pszPVName, "A10N_FC") )
        nProduct = NPC_BOUNDARYLINE;
    else if( EQUAL(pszProduct,NTF_BOUNDARYLINE)
             && STARTS_WITH_CI(pszPVName, "A20N_FC") )
        nProduct = NPC_BL2000;
    else if( STARTS_WITH_CI(pszProduct, "BaseData.GB") )
        nProduct = NPC_BASEDATA;
    else if( STARTS_WITH_CI(pszProduct, "OSCAR_ASSET") )
        nProduct = NPC_OSCAR_ASSET;
    else if( STARTS_WITH_CI(pszProduct, "OSCAR_TRAFF") )
        nProduct = NPC_OSCAR_TRAFFIC;
    else if( STARTS_WITH_CI(pszProduct, "OSCAR_ROUTE") )
        nProduct = NPC_OSCAR_ROUTE;
    else if( STARTS_WITH_CI(pszProduct, "OSCAR_NETWO") )
        nProduct = NPC_OSCAR_NETWORK;
    else if( STARTS_WITH_CI(pszProduct, "ADDRESS_POI") )
        nProduct = NPC_ADDRESS_POINT;
    else if( STARTS_WITH_CI(pszProduct, "CODE_POINT") )
    {
        if( GetAttDesc( "RH" ) == nullptr )
            nProduct = NPC_CODE_POINT;
        else
            nProduct = NPC_CODE_POINT_PLUS;
    }
    else if( STARTS_WITH_CI(pszProduct, "OS_LANDRANGER_DTM") )
        nProduct = NPC_LANDRANGER_DTM;
    else if( STARTS_WITH_CI(pszProduct, "L-F_PROFILE_DTM") )
        nProduct = NPC_LANDFORM_PROFILE_DTM;
    else if( STARTS_WITH_CI(pszProduct, "NEXTMap Britain DTM") )
        nProduct = NPC_LANDFORM_PROFILE_DTM; // Treat as landform

    if( poDS->GetOption("FORCE_GENERIC") != nullptr
        && !EQUAL(poDS->GetOption("FORCE_GENERIC"),"OFF") )
        nProduct = NPC_UNKNOWN;

    // No point in caching lines if there are no polygons.
    if( nProduct != NPC_BOUNDARYLINE && nProduct != NPC_BL2000 )
        bCacheLines = FALSE;

/* -------------------------------------------------------------------- */
/*      Handle the section header record.                               */
/* -------------------------------------------------------------------- */
    nSavedFeatureId = nBaseFeatureId;
    nStartPos = VSIFTellL(fp);

    pszTileName = CPLStrdup(poRecord->GetField(3,12));        // SECT_REF
    size_t nTileNameLen = strlen(pszTileName);
    while( nTileNameLen > 0 && pszTileName[nTileNameLen-1] == ' ' )
    {
        pszTileName[nTileNameLen-1] = '\0';
        nTileNameLen --;
    }

    nCoordWidth = atoi(poRecord->GetField(15,19));            // XYLEN
    if( nCoordWidth <= 0 )
        nCoordWidth = 10;

    nZWidth = atoi(poRecord->GetField(31,35));                // ZLEN
    if( nZWidth <= 0 )
        nZWidth = 10;

    dfXYMult = atoi(poRecord->GetField(21,30)) / 1000.0;      // XY_MULT
    dfXOrigin = atoi(poRecord->GetField(47,56));
    dfYOrigin = atoi(poRecord->GetField(57,66));
    dfTileXSize = atoi(poRecord->GetField(23+74,32+74));
    dfTileYSize = atoi(poRecord->GetField(33+74,42+74));
    dfZMult = atoi(poRecord->GetField(37,46)) / 1000.0;

/* -------------------------------------------------------------------- */
/*      Setup scale and transformation factor for text height.          */
/* -------------------------------------------------------------------- */
    if( poRecord->GetLength() >= 187 )
        dfScale = atoi(poRecord->GetField(148+31,148+39));
    else if( nProduct == NPC_STRATEGI )
        dfScale = 250000;
    else if( nProduct == NPC_MERIDIAN || nProduct == NPC_MERIDIAN2 )
        dfScale = 100000;
    else if( nProduct == NPC_LANDFORM_PROFILE_CONT )
        dfScale = 10000;
    else if( nProduct == NPC_LANDRANGER_CONT )
        dfScale = 50000;
    else if( nProduct == NPC_OSCAR_ASSET
             || nProduct == NPC_OSCAR_TRAFFIC
             || nProduct == NPC_OSCAR_NETWORK
             || nProduct == NPC_OSCAR_ROUTE )
        dfScale = 10000;
    else if( nProduct == NPC_BASEDATA )
        dfScale = 625000;
    else /*if( nProduct == NPC_BOUNDARYLINE ) or default case */
        dfScale = 10000;

    if( dfScale != 0.0 )
        dfPaperToGround = dfScale / 1000.0;
    else
        dfPaperToGround = 0.0;

    delete poRecord;

/* -------------------------------------------------------------------- */
/*      Ensure we have appropriate layers defined.                      */
/* -------------------------------------------------------------------- */
    CPLErrorReset();

    if( !IsRasterProduct() )
        EstablishLayers();
    else
        EstablishRasterAccess();

    return CPLGetLastErrorType() != CE_Failure;
}

/************************************************************************/
/*                            DumpReadable()                            */
/************************************************************************/

void NTFFileReader::DumpReadable( FILE *fpLog )

{
    fprintf( fpLog, "Tile Name = %s\n", pszTileName );
    fprintf( fpLog, "Product = %s\n", pszProduct );
    fprintf( fpLog, "NTFLevel = %d\n", nNTFLevel );
    fprintf( fpLog, "XYLEN = %d\n", nCoordWidth );
    fprintf( fpLog, "XY_MULT = %g\n", dfXYMult );
    fprintf( fpLog, "X_ORIG = %g\n", dfXOrigin );
    fprintf( fpLog, "Y_ORIG = %g\n", dfYOrigin );
    fprintf( fpLog, "XMAX = %g\n", dfTileXSize );
    fprintf( fpLog, "YMAX = %g\n", dfTileYSize );
}

/************************************************************************/
/*                          ProcessGeometry()                           */
/*                                                                      */
/*      Drop duplicate vertices from line strings ... they mess up      */
/*      FME's polygon handling sometimes.                               */
/************************************************************************/

OGRGeometry *NTFFileReader::ProcessGeometry( NTFRecord * poRecord,
                                             int * pnGeomId )

{
    if( poRecord->GetType() == NRT_GEOMETRY3D )
        return ProcessGeometry3D( poRecord, pnGeomId );

    else if( poRecord->GetType() != NRT_GEOMETRY )
        return nullptr;

    const int nGType = atoi(poRecord->GetField(9,9));            // GTYPE
    const int nNumCoord = atoi(poRecord->GetField(10,13));       // NUM_COORD
    if( nNumCoord < 0 )
        return nullptr;
    if( pnGeomId != nullptr )
        *pnGeomId = atoi(poRecord->GetField(3,8));     // GEOM_ID

/* -------------------------------------------------------------------- */
/*      Point                                                           */
/* -------------------------------------------------------------------- */
    OGRGeometry *poGeometry = nullptr;
    if( nGType == 1 )
    {
        const double dfX =
            atoi(poRecord->GetField(14,14+GetXYLen()-1)) * GetXYMult()
            + GetXOrigin();
        const double dfY =
            atoi(poRecord->GetField(14+GetXYLen(),14+GetXYLen()*2-1))
            * GetXYMult() + GetYOrigin();

        poGeometry = new OGRPoint( dfX, dfY );
    }

/* -------------------------------------------------------------------- */
/*      Line (or arc)                                                   */
/* -------------------------------------------------------------------- */
    else if( nGType == 2 || nGType == 3 || nGType == 4 )
    {

        if( nNumCoord > 0 &&
            poRecord->GetLength() <
                14 + (nNumCoord-1) * (GetXYLen()*2+1) + GetXYLen()*2-1 )
        {
            return nullptr;
        }

        OGRLineString      *poLine = new OGRLineString;
        double dfXLast = 0.0;
        double dfYLast = 0.0;
        int nOutCount = 0;

        poGeometry = poLine;
        poLine->setNumPoints( nNumCoord );
        for( int iCoord = 0; iCoord < nNumCoord; iCoord++ )
        {
            const int iStart = 14 + iCoord * (GetXYLen()*2+1);

            const double dfX = atoi(poRecord->GetField(iStart+0,
                                          iStart+GetXYLen()-1))
                * GetXYMult() + GetXOrigin();
            const double dfY = atoi(poRecord->GetField(iStart+GetXYLen(),
                                          iStart+GetXYLen()*2-1))
                * GetXYMult() + GetYOrigin();

            if( iCoord == 0 )
            {
                dfXLast = dfX;
                dfYLast = dfY;
                poLine->setPoint( nOutCount++, dfX, dfY );
            }
            else if( dfXLast != dfX || dfYLast != dfY )
            {
                dfXLast = dfX;
                dfYLast = dfY;
                poLine->setPoint( nOutCount++, dfX, dfY );
            }
        }
        poLine->setNumPoints( nOutCount );

        CacheAddByGeomId( atoi(poRecord->GetField(3,8)), poLine );
    }

/* -------------------------------------------------------------------- */
/*      Arc defined by three points on the arc.                         */
/* -------------------------------------------------------------------- */
    else if( nGType == 5 && nNumCoord == 3 )
    {
        double adfX[3] = { 0.0, 0.0, 0.0 };
        double adfY[3] = { 0.0, 0.0, 0.0 };

        for( int iCoord = 0; iCoord < nNumCoord; iCoord++ )
        {
            const int iStart = 14 + iCoord * (GetXYLen()*2+1);

            adfX[iCoord] = atoi(poRecord->GetField(iStart+0,
                                                  iStart+GetXYLen()-1))
                * GetXYMult() + GetXOrigin();
            adfY[iCoord] = atoi(poRecord->GetField(iStart+GetXYLen(),
                                                  iStart+GetXYLen()*2-1))
                * GetXYMult() + GetYOrigin();
        }

        poGeometry = NTFStrokeArcToOGRGeometry_Points( adfX[0], adfY[0],
                                                       adfX[1], adfY[1],
                                                       adfX[2], adfY[2], 72 );
    }

/* -------------------------------------------------------------------- */
/*      Circle                                                          */
/* -------------------------------------------------------------------- */
    else if( nGType == 7 )
    {
        const int iCenterStart = 14;
        const int iArcStart = 14 + 2 * GetXYLen() + 1;

        const double dfCenterX =
            atoi(poRecord->GetField(iCenterStart, iCenterStart+GetXYLen()-1))
            * GetXYMult() + GetXOrigin();
        const double dfCenterY =
            atoi(poRecord->GetField(iCenterStart+GetXYLen(),
                                    iCenterStart+GetXYLen()*2-1))
            * GetXYMult() + GetYOrigin();

        const double dfArcX =
            atoi(poRecord->GetField(iArcStart, iArcStart+GetXYLen()-1))
            * GetXYMult() + GetXOrigin();
        const double dfArcY =
            atoi(poRecord->GetField(iArcStart+GetXYLen(),
                                    iArcStart+GetXYLen()*2-1))
            * GetXYMult() + GetYOrigin();

        const double dfRadius =
            sqrt( (dfCenterX - dfArcX) * (dfCenterX - dfArcX)
                  + (dfCenterY - dfArcY) * (dfCenterY - dfArcY) );

        poGeometry = NTFStrokeArcToOGRGeometry_Angles( dfCenterX, dfCenterY,
                                                       dfRadius,
                                                       0.0, 360.0,
                                                       72 );
    }

    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unhandled GType = %d", nGType );
    }

    if( poGeometry != nullptr )
        poGeometry->assignSpatialReference( poDS->DSGetSpatialRef() );

    return poGeometry;
}

/************************************************************************/
/*                         ProcessGeometry3D()                          */
/************************************************************************/

OGRGeometry *NTFFileReader::ProcessGeometry3D( NTFRecord * poRecord,
                                               int * pnGeomId )

{
    OGRGeometry *poGeometry = nullptr;

    if( poRecord->GetType() != NRT_GEOMETRY3D )
        return nullptr;

    const int nGType = atoi(poRecord->GetField(9,9));  // GTYPE
    const int nNumCoord = atoi(poRecord->GetField(10,13));       // NUM_COORD
    if( pnGeomId != nullptr )
        *pnGeomId = atoi(poRecord->GetField(3,8));     // GEOM_ID

    if( nGType == 1 )
    {
        if( 14+1+2*static_cast<GIntBig>(GetXYLen())+nZWidth-1 > INT_MAX )
        {
            return nullptr;
        }
        const double dfX =
            atoi(poRecord->GetField(14,14+GetXYLen()-1)) * GetXYMult()
            + GetXOrigin();
        const double dfY =
            atoi(poRecord->GetField(14+GetXYLen(),14+GetXYLen()*2-1))
            * GetXYMult() + GetYOrigin();
        const double dfZ =
            atoi(poRecord->GetField(14+1+2*GetXYLen(),
                                    14+1+2*GetXYLen()+nZWidth-1)) * dfZMult;

        poGeometry = new OGRPoint( dfX, dfY, dfZ );
    }

    else if( nGType == 2 )
    {
        if( 14 + static_cast<GIntBig>(nNumCoord-1) *
                (GetXYLen()*2+nZWidth+2) +1+2*GetXYLen()+nZWidth-1 > INT_MAX )
        {
            return nullptr;
        }

        OGRLineString *poLine = new OGRLineString;
        double dfXLast = 0.0;
        double dfYLast = 0.0;
        int nOutCount = 0;

        poGeometry = poLine;
        poLine->setNumPoints( nNumCoord );
        const GUInt32 nErrorsBefore = CPLGetErrorCounter();
        for( int iCoord = 0; iCoord < nNumCoord; iCoord++ )
        {
            const int iStart = 14 + iCoord * (GetXYLen()*2+nZWidth+2);

            const char* pszX = poRecord->GetField(iStart+0,
                                          iStart+GetXYLen()-1);
            bool bSpace = pszX[0] == ' ';
            const double dfX = atoi(pszX)
                * GetXYMult() + GetXOrigin();
            const char* pszY = poRecord->GetField(iStart+GetXYLen(),
                                          iStart+GetXYLen()*2-1);
            bSpace |= pszY[0] == ' ';
            const double dfY = atoi(pszY)
                * GetXYMult() + GetYOrigin();

            const char* pszZ = poRecord->GetField(iStart+1+2*GetXYLen(),
                                          iStart+1+2*GetXYLen()+nZWidth-1);
            bSpace |= pszZ[0] == ' ';
            const double dfZ = atoi(pszZ)
                * dfZMult;
            if( bSpace && CPLGetErrorCounter() != nErrorsBefore )
            {
                delete poGeometry;
                return nullptr;
            }

            if( iCoord == 0 )
            {
                dfXLast = dfX;
                dfYLast = dfY;
                poLine->setPoint( nOutCount++, dfX, dfY, dfZ );
            }
            else if( dfXLast != dfX || dfYLast != dfY )
            {
                dfXLast = dfX;
                dfYLast = dfY;
                poLine->setPoint( nOutCount++, dfX, dfY, dfZ );
            }
        }
        poLine->setNumPoints( nOutCount );

        CacheAddByGeomId( atoi(poRecord->GetField(3,8)), poLine );
    }

    if( poGeometry != nullptr )
        poGeometry->assignSpatialReference( poDS->DSGetSpatialRef() );

    return poGeometry;
}

/************************************************************************/
/*                           ProcessAttDesc()                           */
/************************************************************************/

int NTFFileReader::ProcessAttDesc( NTFRecord * poRecord, NTFAttDesc* psAD )

{
    psAD->poCodeList = nullptr;
    if( poRecord->GetType() != NRT_ADR || poRecord->GetLength() < 13 )
        return FALSE;

    snprintf( psAD->val_type, sizeof(psAD->val_type), "%s", poRecord->GetField( 3, 4 ));
    snprintf( psAD->fwidth, sizeof(psAD->fwidth), "%s", poRecord->GetField( 5, 7 ));
    snprintf( psAD->finter, sizeof(psAD->finter), "%s", poRecord->GetField( 8, 12 ));

    const char *pszData = poRecord->GetData();
    int iChar = 12;  // Used after for.
    for( ;
         pszData[iChar] != '\0' && pszData[iChar] != '\\';
         iChar++ ) {}

    snprintf( psAD->att_name, sizeof(psAD->att_name), "%s", poRecord->GetField( 13, iChar ));

    return TRUE;
}

/************************************************************************/
/*                         ProcessAttRecGroup()                         */
/*                                                                      */
/*      Extract attribute values from all attribute records in a        */
/*      record set.                                                     */
/************************************************************************/

int NTFFileReader::ProcessAttRecGroup( NTFRecord **papoRecords,
                                       char ***ppapszTypes,
                                       char ***ppapszValues )

{
    *ppapszTypes = nullptr;
    *ppapszValues = nullptr;

    for( int iRec = 0; papoRecords[iRec] != nullptr; iRec++ )
    {
        if( papoRecords[iRec]->GetType() != NRT_ATTREC )
            continue;

        char **papszTypes1 = nullptr;
        char **papszValues1 = nullptr;
        if( !ProcessAttRec( papoRecords[iRec], nullptr,
                            &papszTypes1, &papszValues1 ) )
        {
            CSLDestroy(*ppapszTypes);
            CSLDestroy(*ppapszValues);
            *ppapszTypes = nullptr;
            *ppapszValues = nullptr;
            return FALSE;
        }

        if( *ppapszTypes == nullptr )
        {
            *ppapszTypes = papszTypes1;
            *ppapszValues = papszValues1;
        }
        else
        {
            for( int i=0; papszTypes1[i] != nullptr; i++ )
            {
                *ppapszTypes = CSLAddString( *ppapszTypes, papszTypes1[i] );
                *ppapszValues = CSLAddString( *ppapszValues, papszValues1[i] );
            }
            CSLDestroy( papszTypes1 );
            CSLDestroy( papszValues1 );
        }
    }

    return TRUE;
}

/************************************************************************/
/*                           ProcessAttRec()                            */
/************************************************************************/

int NTFFileReader::ProcessAttRec( NTFRecord * poRecord,
                                  int *pnAttId,
                                  char *** ppapszTypes,
                                  char *** ppapszValues )

{
    if( pnAttId != nullptr )
        *pnAttId = 0;
    *ppapszTypes = nullptr;
    *ppapszValues = nullptr;

    if( poRecord->GetType() != NRT_ATTREC || poRecord->GetLength() < 8 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Extract the attribute id.                                       */
/* -------------------------------------------------------------------- */
    if( pnAttId != nullptr )
        *pnAttId = atoi(poRecord->GetField(3,8));

/* ==================================================================== */
/*      Loop handling attribute till we get a '0' indicating the end    */
/*      of the record.                                                  */
/* ==================================================================== */

    int iOffset = 8;
    const char *pszData = poRecord->GetData();
    bool bError = false;

    while( iOffset < poRecord->GetLength() && pszData[iOffset] != DIGIT_ZERO )
    {
/* -------------------------------------------------------------------- */
/*      Extract the two letter code name for the attribute, and use     */
/*      it to find the correct ATTDESC info.                            */
/* -------------------------------------------------------------------- */
        NTFAttDesc *psAttDesc = GetAttDesc(pszData + iOffset );
        if( psAttDesc == nullptr )
        {
            CPLDebug( "NTF", "Couldn't translate attrec type `%2.2s'.",
                      pszData + iOffset );
            bError = true;
            break;
        }

        *ppapszTypes =
            CSLAddString( *ppapszTypes,
                          poRecord->GetField(iOffset+1,iOffset+2) );

/* -------------------------------------------------------------------- */
/*      Establish the width of the value.  Zero width fields are        */
/*      terminated by a backslash.                                      */
/* -------------------------------------------------------------------- */
        const int nFWidth = atoi(psAttDesc->fwidth);
        if( nFWidth < 0 )
        {
            bError = true;
            break;
        }
        int nEnd = 0;
        if( nFWidth == 0 )
        {
            const char * pszData2 = poRecord->GetData();
            if( iOffset + 2 >= poRecord->GetLength() )
            {
                bError = true;
                break;
            }
            for( nEnd = iOffset + 2;
                 pszData2[nEnd] != '\\' && pszData2[nEnd] != '\0';
                 nEnd++ ) {}
        }
        else
        {
            nEnd = iOffset + 3 + nFWidth - 1;
        }

/* -------------------------------------------------------------------- */
/*      Extract the value.  If it is formatted as fixed point real      */
/*      we reprocess it to insert the decimal point.                    */
/* -------------------------------------------------------------------- */
        const char * pszRawValue = poRecord->GetField(iOffset+3,nEnd);
        *ppapszValues = CSLAddString( *ppapszValues, pszRawValue );

/* -------------------------------------------------------------------- */
/*      Establish new offset position.                                  */
/* -------------------------------------------------------------------- */
        if( nFWidth == 0 )
        {
            iOffset = nEnd;
            if( iOffset >= poRecord->GetLength() )
            {
                bError = (iOffset > poRecord->GetLength());
                break;
            }
            if( pszData[iOffset] == '\\' )
                iOffset++;
        }
        else
            iOffset += 2 + nFWidth;
    }

    if( bError )
    {
        CSLDestroy(*ppapszTypes);
        CSLDestroy(*ppapszValues);
        *ppapszTypes = nullptr;
        *ppapszValues = nullptr;
    }

    return( *ppapszTypes != nullptr );
}

/************************************************************************/
/*                             GetAttDesc()                             */
/************************************************************************/

NTFAttDesc * NTFFileReader::GetAttDesc( const char * pszType )

{
    for( int i = 0; i < nAttCount; i++ )
    {
        if( EQUALN(pszType, pasAttDesc[i].val_type, 2) )
            return pasAttDesc + i;
    }

    return nullptr;
}

/************************************************************************/
/*                          ProcessAttValue()                           */
/*                                                                      */
/*      Take an attribute type/value pair and transform into a          */
/*      meaningful attribute name, and value.  The source can be an     */
/*      ATTREC or the VAL_TYPE/VALUE pair of a POINTREC or LINEREC.     */
/*      The name is transformed from the two character short form to    */
/*      the long user name.  The value will be transformed from         */
/*      fixed point (with the decimal implicit) to fixed point with     */
/*      an explicit decimal point if it has a "R" format.               */
/*      Note: the returned *ppszAttValue has a very short lifetime      */
/*      and should immediately be used. Further calls to                */
/*      ProcessAttValue or CPLSPrintf() will invalidate it.             */
/************************************************************************/

int NTFFileReader::ProcessAttValue( const char *pszValType,
                                    const char *pszRawValue,
                                    const char **ppszAttName,
                                    const char **ppszAttValue,
                                    const char **ppszCodeDesc )

{
/* -------------------------------------------------------------------- */
/*      Find the ATTDESC for this attribute, and assign return name value.*/
/* -------------------------------------------------------------------- */
    NTFAttDesc *psAttDesc = GetAttDesc(pszValType);

    if( psAttDesc == nullptr )
        return FALSE;

    if( ppszAttName != nullptr )
        *ppszAttName = psAttDesc->att_name;

/* -------------------------------------------------------------------- */
/*      Extract the value.  If it is formatted as fixed point real      */
/*      we reprocess it to insert the decimal point.                    */
/* -------------------------------------------------------------------- */
    if( psAttDesc->finter[0] == 'R' )
    {
        const char *pszDecimalPortion = nullptr; // Used after for.

        for( pszDecimalPortion = psAttDesc->finter;
             *pszDecimalPortion != ',' && *pszDecimalPortion != '\0';
             pszDecimalPortion++ ) {}
        if( *pszDecimalPortion == '\0' )
        {
            *ppszAttValue = "";
        }
        else
        {
            const int nWidth = static_cast<int>(strlen(pszRawValue));
            const int nPrecision = atoi(pszDecimalPortion+1);
            if( nPrecision < 0 || nPrecision >= nWidth )
            {
                *ppszAttValue = "";
            }
            else
            {
                CPLString osResult(pszRawValue);
                osResult.resize(nWidth - nPrecision);
                osResult += ".";
                osResult += pszRawValue+nWidth-nPrecision;

                *ppszAttValue = CPLSPrintf("%s", osResult.c_str());
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      If it is an integer, we just reformat to get rid of leading     */
/*      zeros.                                                          */
/* -------------------------------------------------------------------- */
    else if( psAttDesc->finter[0] == 'I' )
    {
        *ppszAttValue = CPLSPrintf("%d", atoi(pszRawValue) );
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we take the value directly.                           */
/* -------------------------------------------------------------------- */
    else
    {
        *ppszAttValue = pszRawValue;
    }

/* -------------------------------------------------------------------- */
/*      Handle processing code values into code descriptions, if        */
/*      applicable.                                                     */
/* -------------------------------------------------------------------- */
    if( ppszCodeDesc == nullptr )
    {
    }
    else if( psAttDesc->poCodeList != nullptr )
    {
        *ppszCodeDesc = psAttDesc->poCodeList->Lookup( *ppszAttValue );
    }
    else
    {
        *ppszCodeDesc = nullptr;
    }

    return TRUE;
}

/************************************************************************/
/*                        ApplyAttributeValues()                        */
/*                                                                      */
/*      Apply a series of attribute values to a feature from generic    */
/*      attribute records.                                              */
/************************************************************************/

void NTFFileReader::ApplyAttributeValues( OGRFeature * poFeature,
                                          NTFRecord ** papoGroup, ... )

{
/* -------------------------------------------------------------------- */
/*      Extract attribute values from record group.                     */
/* -------------------------------------------------------------------- */
    char **papszTypes = nullptr;
    char **papszValues = nullptr;

    if( !ProcessAttRecGroup( papoGroup, &papszTypes, &papszValues ) )
        return;

/* -------------------------------------------------------------------- */
/*      Handle attribute pairs                                          */
/* -------------------------------------------------------------------- */
    va_list hVaArgs;

    va_start(hVaArgs, papoGroup);

    const char *pszAttName = nullptr;
    while( (pszAttName = va_arg(hVaArgs, const char *)) != nullptr )
    {
        const int iField = va_arg(hVaArgs, int);

        ApplyAttributeValue( poFeature, iField, pszAttName,
                             papszTypes, papszValues );
    }

    va_end(hVaArgs);

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    CSLDestroy( papszTypes );
    CSLDestroy( papszValues );
}

/************************************************************************/
/*                        ApplyAttributeValue()                         */
/*                                                                      */
/*      Apply the indicated attribute value to an OGRFeature field      */
/*      if it exists in the attribute value list given.                 */
/************************************************************************/

int NTFFileReader::ApplyAttributeValue( OGRFeature * poFeature, int iField,
                                        const char * pszAttName,
                                        char ** papszTypes,
                                        char ** papszValues )

{
/* -------------------------------------------------------------------- */
/*      Find the requested attribute in the name/value pair             */
/*      provided.  If not found that's fine, just return with           */
/*      notification.                                                   */
/* -------------------------------------------------------------------- */
    const int iValue = CSLFindString( papszTypes, pszAttName );
    if( iValue < 0 )
        return FALSE;

    CPLAssert(papszValues != nullptr);
/* -------------------------------------------------------------------- */
/*      Process the attribute value ... this really only has a          */
/*      useful effect for real numbers.                                 */
/* -------------------------------------------------------------------- */
    const char *pszAttLongName = nullptr;
    const char *pszAttValue = nullptr;
    const char *pszCodeDesc = nullptr;

    if( !ProcessAttValue( pszAttName, papszValues[iValue],
                          &pszAttLongName, &pszAttValue, &pszCodeDesc ) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Apply the value to the field using the simple set string        */
/*      method.  Leave it to the OGRFeature::SetField() method to       */
/*      take care of translation to other types.                        */
/* -------------------------------------------------------------------- */
    poFeature->SetField( iField, pszAttValue );

/* -------------------------------------------------------------------- */
/*      Apply the code description if we found one.                     */
/* -------------------------------------------------------------------- */
    if( pszCodeDesc != nullptr )
    {
        char    szDescFieldName[256];

        snprintf( szDescFieldName, sizeof(szDescFieldName), "%s_DESC",
                 poFeature->GetDefnRef()->GetFieldDefn(iField)->GetNameRef() );
        poFeature->SetField( szDescFieldName, pszCodeDesc );
    }

    return TRUE;
}

/************************************************************************/
/*                             SaveRecord()                             */
/************************************************************************/

void NTFFileReader::SaveRecord( NTFRecord * poRecord )

{
    CPLAssert( poSavedRecord == nullptr );
    poSavedRecord = poRecord;
}

/************************************************************************/
/*                             ReadRecord()                             */
/************************************************************************/

NTFRecord *NTFFileReader::ReadRecord()

{
    if( poSavedRecord != nullptr )
    {
        NTFRecord *poReturn = poSavedRecord;

        poSavedRecord = nullptr;

        return poReturn;
    }
    else
    {
        CPLErrorReset();
        if( fp != nullptr )
            nPreSavedPos = VSIFTellL( fp );
        NTFRecord *poRecord = new NTFRecord( fp );
        if( fp != nullptr )
            nPostSavedPos = VSIFTellL( fp );

        /* ensure termination if we fail to read a record */
        if( CPLGetLastErrorType() == CE_Failure )
        {
            delete poRecord;
            poRecord = nullptr;
        }

        return poRecord;
    }
}

/************************************************************************/
/*                              GetFPPos()                              */
/*                                                                      */
/*      Return the current file pointer position.                       */
/************************************************************************/

void NTFFileReader::GetFPPos( vsi_l_offset *pnPos, long *pnFID )

{
    if( poSavedRecord != nullptr )
        *pnPos = nPreSavedPos;
    else
        *pnPos = nPostSavedPos;

    if( pnFID != nullptr )
        *pnFID = nSavedFeatureId;
}

/************************************************************************/
/*                              SetFPPos()                              */
/************************************************************************/

int NTFFileReader::SetFPPos( vsi_l_offset nNewPos, long nNewFID )

{
    if( nNewFID == nSavedFeatureId )
        return TRUE;

    if( poSavedRecord != nullptr )
    {
        delete poSavedRecord;
        poSavedRecord = nullptr;
    }

    if( fp != nullptr && VSIFSeekL( fp, nNewPos, SEEK_SET ) == 0 )
    {
        nPreSavedPos = nPostSavedPos = nNewPos;
        nSavedFeatureId = nNewFID;
        return TRUE;
    }
    else
        return FALSE;
}

/************************************************************************/
/*                               Reset()                                */
/*                                                                      */
/*      Reset reading to the first feature record.                      */
/************************************************************************/

void NTFFileReader::Reset()

{
    SetFPPos( nStartPos, nBaseFeatureId );
    ClearCGroup();
}

/************************************************************************/
/*                            ClearCGroup()                             */
/*                                                                      */
/*      Clear the currently loaded record group.                        */
/************************************************************************/

void NTFFileReader::ClearCGroup()

{
    for( int i = 0; apoCGroup[i] != nullptr; i++ )
        delete apoCGroup[i];

    apoCGroup[0] = nullptr;
    apoCGroup[1] = nullptr;
}

/************************************************************************/
/*                      DefaultNTFRecordGrouper()                       */
/*                                                                      */
/*      Default rules for figuring out if a new candidate record        */
/*      belongs to a group of records that together form a feature      */
/*      (a record group).                                               */
/************************************************************************/

int DefaultNTFRecordGrouper( NTFFileReader *, NTFRecord ** papoGroup,
                             NTFRecord * poCandidate )

{
/* -------------------------------------------------------------------- */
/*      Is this group going to be a CPOLY set?  We can recognise        */
/*      this because we get repeating POLY/CHAIN sets without an        */
/*      intermediate attribute record.  This is a rather special case!  */
/* -------------------------------------------------------------------- */
    if( papoGroup[0] != nullptr && papoGroup[1] != nullptr
        && papoGroup[0]->GetType() == NRT_POLYGON
        && papoGroup[1]->GetType() == NRT_CHAIN )
    {
        // We keep going till we get the seed geometry.
        int     iRec, bGotCPOLY=FALSE;

        for( iRec = 0; papoGroup[iRec] != nullptr; iRec++ )
        {
            if( papoGroup[iRec]->GetType() == NRT_CPOLY )
                bGotCPOLY = TRUE;
        }

        if( bGotCPOLY
            && poCandidate->GetType() != NRT_GEOMETRY
            && poCandidate->GetType() != NRT_ATTREC )
            return FALSE;

        /*
         * this logic assumes we always get a point geometry with a CPOLY
         * but that isn't always true, for instance with BL2000 data.  The
         * preceding check will handle this case.
         */
        if( papoGroup[iRec-1]->GetType() != NRT_GEOMETRY )
            return TRUE;
        else
            return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Is this a "feature" defining record?  If so break out if it     */
/*      isn't the first record in the group.                            */
/* -------------------------------------------------------------------- */
    if( papoGroup[0] != nullptr
        && (poCandidate->GetType() == NRT_NAMEREC
            || poCandidate->GetType() == NRT_NODEREC
            || poCandidate->GetType() == NRT_LINEREC
            || poCandidate->GetType() == NRT_POINTREC
            || poCandidate->GetType() == NRT_POLYGON
            || poCandidate->GetType() == NRT_CPOLY
            || poCandidate->GetType() == NRT_COLLECT
            || poCandidate->GetType() == NRT_TEXTREC
            || poCandidate->GetType() == NRT_COMMENT) )
    {
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Do we already have a record of this type?  If so, it likely     */
/*      doesn't belong in the group.  Attribute records do repeat in    */
/*      some products.                                                  */
/* -------------------------------------------------------------------- */
    if (poCandidate->GetType() != NRT_ATTREC )
    {
        int iRec = 0;  // Used after for.
        for( ; papoGroup[iRec] != nullptr; iRec++ )
        {
            if( poCandidate->GetType() == papoGroup[iRec]->GetType() )
                break;
        }

        if( papoGroup[iRec] != nullptr )
            return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                          ReadRecordGroup()                           */
/*                                                                      */
/*      Read a group of records that form a single feature.             */
/************************************************************************/

NTFRecord **NTFFileReader::ReadRecordGroup()

{

   ClearCGroup();

/* -------------------------------------------------------------------- */
/*      Loop, reading records till we think we have a grouping.         */
/* -------------------------------------------------------------------- */
   int nRecordCount = 0;
   NTFRecord *poRecord = nullptr;
   while( (poRecord = ReadRecord()) != nullptr && poRecord->GetType() != NRT_VTR )
   {
       if( nRecordCount >= MAX_REC_GROUP )
       {
           CPLError( CE_Failure, CPLE_AppDefined,
                     "Maximum record group size (%d) exceeded.\n",
                     MAX_REC_GROUP );
           break;
       }

       if( !pfnRecordGrouper( this, apoCGroup, poRecord ) )
           break;

       apoCGroup[nRecordCount++] = poRecord;
       apoCGroup[nRecordCount] = nullptr;
   }

/* -------------------------------------------------------------------- */
/*      Push the last record back on the input queue.                   */
/* -------------------------------------------------------------------- */
   if( poRecord != nullptr )
       SaveRecord( poRecord );

/* -------------------------------------------------------------------- */
/*      Return the list, or NULL if we didn't get any records.          */
/* -------------------------------------------------------------------- */
   if( nRecordCount == 0 )
       return nullptr;
   else
       return apoCGroup;
}

/************************************************************************/
/*                          GetFeatureClass()                           */
/************************************************************************/

int NTFFileReader::GetFeatureClass( int iFCIndex,
                                    char ** ppszFCId,
                                    char ** ppszFCName )

{
    if( iFCIndex < 0 || iFCIndex >= nFCCount )
    {
        *ppszFCId = nullptr;
        *ppszFCName = nullptr;
        return FALSE;
    }
    else
    {
        *ppszFCId   = papszFCNum[iFCIndex];
        *ppszFCName = papszFCName[iFCIndex];
        return TRUE;
    }
}

/************************************************************************/
/*                           ReadOGRFeature()                           */
/************************************************************************/

OGRFeature * NTFFileReader::ReadOGRFeature( OGRNTFLayer * poTargetLayer )

{
/* -------------------------------------------------------------------- */
/*      If this is a raster file, use a custom method to read the       */
/*      feature.                                                        */
/* -------------------------------------------------------------------- */
    if( IsRasterProduct() )
        return poRasterLayer->GetNextFeature();

/* -------------------------------------------------------------------- */
/*      Loop looking for a group we can translate, and that if          */
/*      needed matches our layer request.                               */
/* -------------------------------------------------------------------- */
    OGRNTFLayer *poLayer = nullptr;
    OGRFeature *poFeature = nullptr;

    while( true )
    {
        NTFRecord **papoGroup = nullptr;

        if( GetProductId() == NPC_UNKNOWN && nNTFLevel > 2 )
            papoGroup = GetNextIndexedRecordGroup( apoCGroup + 1 );
        else
            papoGroup = ReadRecordGroup();

        if( papoGroup == nullptr || papoGroup[0] == nullptr )
            break;

        int nType = papoGroup[0]->GetType();
        if( nType < 0 ||
            nType >= (int)(sizeof(apoTypeTranslation) / sizeof(apoTypeTranslation[0])) )
            continue;
        poLayer = apoTypeTranslation[nType];
        if( poLayer == nullptr )
            continue;

        if( poTargetLayer != nullptr && poTargetLayer != poLayer )
        {
            CacheLineGeometryInGroup( papoGroup );
            nSavedFeatureId++;
            continue;
        }

        poFeature = poLayer->FeatureTranslate( this, papoGroup );
        if( poFeature == nullptr )
        {
            // should this be a real error?
            CPLDebug( "NTF",
                      "FeatureTranslate() failed for a type %d record group\n"
                      "in a %s type file.\n",
                      papoGroup[0]->GetType(),
                      GetProduct() );
        }
        else
        {
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      If we got a feature, set the TILE_REF on it.                    */
/* -------------------------------------------------------------------- */
    if( poFeature != nullptr )
    {
        int iTileRefField = poLayer->GetLayerDefn()->GetFieldCount()-1;

        CPLAssert( EQUAL(poLayer->GetLayerDefn()->GetFieldDefn(iTileRefField)->
                         GetNameRef(), "TILE_REF") );

        poFeature->SetField( iTileRefField, GetTileName() );
        poFeature->SetFID( nSavedFeatureId );

        nSavedFeatureId++;
    }

/* -------------------------------------------------------------------- */
/*      If we got to the end we can establish our feature count for     */
/*      the file.                                                       */
/* -------------------------------------------------------------------- */
    else
    {
        // This assertion was triggered by https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=1982
        // it doesn't look critical enough to be enabled.
        //CPLAssert( nFeatureCount == -1
        //           || nFeatureCount == nSavedFeatureId - nBaseFeatureId );
        nFeatureCount = nSavedFeatureId - nBaseFeatureId;
    }

    return poFeature;
}

/************************************************************************/
/*                            TestForLayer()                            */
/*                                                                      */
/*      Return indicator of whether this file contains any features     */
/*      of the indicated layer type.                                    */
/************************************************************************/

int NTFFileReader::TestForLayer( OGRNTFLayer * poLayer )

{
    for( int i = 0; i < 100; i++ )
    {
        if( apoTypeTranslation[i] == poLayer )
            return TRUE;
    }

    return FALSE;
}

/************************************************************************/
/*                            FreshenIndex()                            */
/*                                                                      */
/*      Rebuild the index if it is needed, and currently missing.       */
/************************************************************************/

void NTFFileReader::FreshenIndex()

{
    if( !bIndexBuilt && bIndexNeeded )
        IndexFile();
}

/************************************************************************/
/*                             IndexFile()                              */
/*                                                                      */
/*      Read all records beyond the section header and build an         */
/*      internal index of them.                                         */
/************************************************************************/

void NTFFileReader::IndexFile()

{
    Reset();

    DestroyIndex();

    bIndexNeeded = TRUE;
    bIndexBuilt = TRUE;
    bCacheLines = FALSE;

/* -------------------------------------------------------------------- */
/*      Process all records after the section header, and before 99     */
/*      to put them in the index.                                       */
/* -------------------------------------------------------------------- */
    NTFRecord *poRecord = nullptr;
    while( (poRecord = ReadRecord()) != nullptr && poRecord->GetType() != 99 )
    {
        const int iType = poRecord->GetType();
        const int iId = atoi(poRecord->GetField( 3, 8 ));

        if( iType < 0 || iType >= 100 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Illegal type %d record, skipping.",
                      iType );
            delete poRecord;
            continue;
        }
        if( iId < 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Illegal id %d record, skipping.",
                      iId );
            delete poRecord;
            continue;
        }

/* -------------------------------------------------------------------- */
/*      Grow type specific subindex if needed.                          */
/* -------------------------------------------------------------------- */
        if( anIndexSize[iType] <= iId )
        {
            const int nNewSize = std::max(iId+1, anIndexSize[iType] * 2 + 10);

            apapoRecordIndex[iType] = static_cast<NTFRecord **>(
                CPLRealloc(apapoRecordIndex[iType],
                           sizeof(void *) * nNewSize));

            for( int i = anIndexSize[iType]; i < nNewSize; i++ )
                (apapoRecordIndex[iType])[i] = nullptr;

            anIndexSize[iType] = nNewSize;
        }

/* -------------------------------------------------------------------- */
/*      Put record into type specific subindex based on its id as       */
/*      the key.                                                        */
/* -------------------------------------------------------------------- */
        if( apapoRecordIndex[iType][iId] != nullptr )
        {
            CPLDebug( "OGR_NTF",
                      "Duplicate record with index %d and type %d\n"
                      "in NTFFileReader::IndexFile().",
                      iId, iType );
            delete apapoRecordIndex[iType][iId];
        }
        (apapoRecordIndex[iType])[iId] = poRecord;
    }

    if( poRecord != nullptr )
        delete poRecord;
}

/************************************************************************/
/*                            DestroyIndex()                            */
/************************************************************************/

void NTFFileReader::DestroyIndex()

{
    for( int i = 0; i < 100; i++ )
    {
        for( int iId = 0; iId < anIndexSize[i]; iId++ )
        {
            if( (apapoRecordIndex[i])[iId] != nullptr )
                delete (apapoRecordIndex[i])[iId];
        }

        CPLFree( apapoRecordIndex[i] );
        apapoRecordIndex[i] = nullptr;
        anIndexSize[i] = 0;
    }

    bIndexBuilt = FALSE;
}

/************************************************************************/
/*                          GetIndexedRecord()                          */
/************************************************************************/

NTFRecord * NTFFileReader::GetIndexedRecord( int iType, int iId )

{
    if( (iType < 0 || iType > 99)
        || (iId < 0 || iId >= anIndexSize[iType])
        || (apapoRecordIndex[iType])[iId] == nullptr )
    {
        /* If NRT_GEOMETRY3D is an acceptable alternative to 2D */
        if( iType == NRT_GEOMETRY )
            return GetIndexedRecord( NRT_GEOMETRY3D, iId );
        else
            return nullptr;
    }

    return (apapoRecordIndex[iType])[iId];
}

/************************************************************************/
/*                          AddToIndexGroup()                           */
/************************************************************************/

void NTFFileReader::AddToIndexGroup( NTFRecord * poRecord )

{
    int i = 1;  // Used after for.
    for( ; apoCGroup[i] != nullptr; i++ )
    {
        if( apoCGroup[i] == poRecord )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Record already inserted in group");
            return;
        }
    }
    if( i == MAX_REC_GROUP )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Maximum number of records in group reached");
        delete poRecord;
        return;
    }

    apoCGroup[i] = poRecord;
    apoCGroup[i+1] = nullptr;
}

/************************************************************************/
/*                     GetNextIndexedRecordGroup()                      */
/************************************************************************/

NTFRecord **NTFFileReader::GetNextIndexedRecordGroup( NTFRecord **
                                                      papoPrevGroup )

{
    int         nPrevType, nPrevId;

/* -------------------------------------------------------------------- */
/*      What was the identify of our previous anchor record?            */
/* -------------------------------------------------------------------- */
    if( papoPrevGroup == nullptr || papoPrevGroup[0] == nullptr )
    {
        nPrevType = NRT_POINTREC;
        nPrevId = 0;
        FreshenIndex();
    }
    else
    {
        nPrevType = papoPrevGroup[0]->GetType();
        nPrevId = atoi(papoPrevGroup[0]->GetField(3,8));
        if( nPrevId < 0 )
            return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Find the next anchor record.                                    */
/* -------------------------------------------------------------------- */
    NTFRecord   *poAnchor = nullptr;

    while( nPrevType != 99 && poAnchor == nullptr )
    {
        nPrevId++;
        if( nPrevId >= anIndexSize[nPrevType] )
        {
            do
            {
                nPrevType++;
            }
            while( nPrevType != NRT_VTR
                   && nPrevType != NRT_NODEREC
                   && nPrevType != NRT_TEXTREC
                   && nPrevType != NRT_NAMEREC
                   && nPrevType != NRT_COLLECT
                   && nPrevType != NRT_POLYGON
                   && nPrevType != NRT_CPOLY
                   && nPrevType != NRT_POINTREC
                   && nPrevType != NRT_LINEREC );

            nPrevId = 0;
        }
        else
        {
            poAnchor = (apapoRecordIndex[nPrevType])[nPrevId];
        }
    }

    if( poAnchor == nullptr )
    {
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Build record group depending on type of anchor and what it      */
/*      refers to.                                                      */
/* -------------------------------------------------------------------- */
    apoCGroup[0] = nullptr;
    apoCGroup[1] = poAnchor;
    apoCGroup[2] = nullptr;

/* -------------------------------------------------------------------- */
/*      Handle POINTREC/LINEREC                                         */
/* -------------------------------------------------------------------- */
    if( poAnchor->GetType() == NRT_POINTREC
         || poAnchor->GetType() == NRT_LINEREC )
    {
        int             l_nAttCount = 0;

        AddToIndexGroup( GetIndexedRecord( NRT_GEOMETRY,
                                           atoi(poAnchor->GetField(9,14)) ) );

        if( poAnchor->GetLength() >= 16 )
            l_nAttCount = atoi(poAnchor->GetField(15,16));

        for( int iAtt = 0; iAtt < l_nAttCount; iAtt++ )
        {
            AddToIndexGroup(
                GetIndexedRecord( NRT_ATTREC,
                                  atoi(poAnchor->GetField(17+6*iAtt,
                                                          22+6*iAtt)) ) );
        }
    }

/* -------------------------------------------------------------------- */
/*      Handle TEXTREC                                                  */
/* -------------------------------------------------------------------- */
    else if( poAnchor->GetType() == NRT_TEXTREC )
    {
        int             l_nAttCount = 0;
        int             nSelCount = 0;

        // Add all the text position records.
        nSelCount = atoi(poAnchor->GetField(9,10));
        if( nSelCount < 0 )
            return nullptr;

        for( int iSel = 0; iSel < nSelCount; iSel++ )
        {
            int iStart = 11 + 12*iSel + 6;

            AddToIndexGroup(
                GetIndexedRecord( NRT_TEXTPOS,
                                  atoi(poAnchor->GetField(iStart,iStart+5)) ));
        }

        // Add all geometry and TEXR records pointed to by text position
        // records.
        for( int iRec = 1; apoCGroup[iRec] != nullptr; iRec++ )
        {
            NTFRecord  *poRecord = apoCGroup[iRec];

            if( poRecord->GetType() != NRT_TEXTPOS )
                continue;

            const int nNumTEXR = atoi(poRecord->GetField(9,10));
            for( int iTEXR = 0; iTEXR < nNumTEXR; iTEXR++ )
            {
                AddToIndexGroup(
                    GetIndexedRecord( NRT_TEXTREP,
                                      atoi(poRecord->GetField(11+iTEXR*12,
                                                              16+iTEXR*12))));
                AddToIndexGroup(
                    GetIndexedRecord( NRT_GEOMETRY,
                                      atoi(poRecord->GetField(17+iTEXR*12,
                                                              22+iTEXR*12))));
            }
        }

        // Add all the attribute records.
        if( poAnchor->GetLength() >= 10 + nSelCount*12 + 2 )
            l_nAttCount = atoi(poAnchor->GetField(11+nSelCount*12,
                                                12+nSelCount*12));

        for( int iAtt = 0; iAtt < l_nAttCount; iAtt++ )
        {
            int iStart = 13 + nSelCount*12 + 6 * iAtt;

            AddToIndexGroup(
                GetIndexedRecord( NRT_ATTREC,
                                  atoi(poAnchor->GetField(iStart,iStart+5)) ));
        }
    }

/* -------------------------------------------------------------------- */
/*      Handle NODEREC.                                                 */
/* -------------------------------------------------------------------- */
    else if( poAnchor->GetType() == NRT_NODEREC )
    {
        AddToIndexGroup( GetIndexedRecord( NRT_GEOMETRY,
                                           atoi(poAnchor->GetField(9,14)) ) );
    }

/* -------------------------------------------------------------------- */
/*      Handle COLLECT.                                                 */
/* -------------------------------------------------------------------- */
    else if( poAnchor->GetType() == NRT_COLLECT )
    {
        const int nParts = atoi(poAnchor->GetField(9,12));
        if( nParts < 0 )
            return nullptr;
        const int nAttOffset = 13 + nParts * 8;
        int l_nAttCount = 0;

        if( poAnchor->GetLength() > nAttOffset + 2 )
            l_nAttCount = atoi(poAnchor->GetField(nAttOffset,nAttOffset+1));

        for( int iAtt = 0; iAtt < l_nAttCount; iAtt++ )
        {
            const int iStart = nAttOffset + 2 + iAtt * 6;

            AddToIndexGroup(
                GetIndexedRecord( NRT_ATTREC,
                                  atoi(poAnchor->GetField(iStart,iStart+5)) ));
        }
    }

/* -------------------------------------------------------------------- */
/*      Handle POLYGON                                                  */
/* -------------------------------------------------------------------- */
    else if( poAnchor->GetType() == NRT_POLYGON )
    {
        AddToIndexGroup( GetIndexedRecord( NRT_CHAIN,
                                           atoi(poAnchor->GetField(9,14)) ) );

        if( poAnchor->GetLength() >= 20 )
            AddToIndexGroup(
                        GetIndexedRecord( NRT_GEOMETRY,
                                          atoi(poAnchor->GetField(15,20)) ) );

        // Attributes
        int l_nAttCount = 0;

        if( poAnchor->GetLength() >= 22 )
            l_nAttCount = atoi(poAnchor->GetField(21,22));

        for( int iAtt = 0; iAtt < l_nAttCount; iAtt++ )
        {
            AddToIndexGroup(
                GetIndexedRecord( NRT_ATTREC,
                                  atoi(poAnchor->GetField(23+6*iAtt,
                                                          28+6*iAtt)) ) );
        }
    }
/* -------------------------------------------------------------------- */
/*      Handle CPOLY                                                    */
/* -------------------------------------------------------------------- */
    else if( poAnchor->GetType() == NRT_CPOLY )
    {
        int nPolyCount = atoi(poAnchor->GetField(9,12));
        if( nPolyCount < 0 )
            return nullptr;
        int nPostPoly = nPolyCount*7 + 12;

        if( poAnchor->GetLength() >= nPostPoly + 6 )
        {
            int  nGeomId = atoi(poAnchor->GetField(nPostPoly+1,nPostPoly+6));

            AddToIndexGroup( GetIndexedRecord( NRT_GEOMETRY, nGeomId) );
        }

        if( poAnchor->GetLength() >= nPostPoly + 8 )
        {
            int l_nAttCount = atoi(poAnchor->GetField(nPostPoly+7,nPostPoly+8));

            for( int iAtt = 0; iAtt < l_nAttCount; iAtt++ )
            {
                int nAttId = atoi(poAnchor->GetField(nPostPoly+9+iAtt*6,
                                                     nPostPoly+14+iAtt*6));
                AddToIndexGroup( GetIndexedRecord( NRT_ATTREC, nAttId) );
            }
        }
    }

    return apoCGroup + 1;
}

/************************************************************************/
/*                          OverrideTileName()                          */
/************************************************************************/

void NTFFileReader::OverrideTileName( const char *pszNewName )

{
    CPLFree( pszTileName );
    pszTileName = CPLStrdup( pszNewName );
}

/************************************************************************/
/*                          CacheAddByGeomId()                          */
/*                                                                      */
/*      Add a geometry to the geometry cache given its GEOMID as        */
/*      the index.                                                      */
/************************************************************************/

void NTFFileReader::CacheAddByGeomId( int nGeomId, OGRGeometry *poGeometry )

{
    if( !bCacheLines )
        return;

    CPLAssert( nGeomId >= 0 );

/* -------------------------------------------------------------------- */
/*      Grow the cache if it isn't large enough to hold the newly       */
/*      requested geometry id.                                          */
/* -------------------------------------------------------------------- */
    if( nGeomId >= nLineCacheSize )
    {
        const int nNewSize = nGeomId + 100;

        papoLineCache = static_cast<OGRGeometry **>(
            CPLRealloc( papoLineCache, sizeof(void*) * nNewSize ));
        memset( papoLineCache + nLineCacheSize, 0,
                sizeof(void*) * (nNewSize - nLineCacheSize) );
        nLineCacheSize = nNewSize;
    }

/* -------------------------------------------------------------------- */
/*      Make a cloned copy of the geometry for the cache.               */
/* -------------------------------------------------------------------- */
    if( papoLineCache[nGeomId] != nullptr )
        return;

    papoLineCache[nGeomId] = poGeometry->clone();
}

/************************************************************************/
/*                          CacheGetByGeomId()                          */
/************************************************************************/

OGRGeometry *NTFFileReader::CacheGetByGeomId( int nGeomId )

{
    if( nGeomId < 0 || nGeomId >= nLineCacheSize )
        return nullptr;
    else
        return papoLineCache[nGeomId];
}

/************************************************************************/
/*                             CacheClean()                             */
/************************************************************************/

void NTFFileReader::CacheClean()

{
    for( int i = 0; i < nLineCacheSize; i++ )
    {
        if( papoLineCache[i] != nullptr )
            delete papoLineCache[i];
    }
    if( papoLineCache != nullptr )
        CPLFree( papoLineCache );

    nLineCacheSize = 0;
    papoLineCache = nullptr;
}

/************************************************************************/
/*                      CacheLineGeometryInGroup()                      */
/*                                                                      */
/*      Run any line geometries in this group through the               */
/*      ProcessGeometry() call just to ensure the line geometry will    */
/*      be cached.                                                      */
/************************************************************************/

void NTFFileReader::CacheLineGeometryInGroup( NTFRecord **papoGroup )

{
    if( !bCacheLines )
        return;

    for( int iRec = 0; papoGroup[iRec] != nullptr; iRec++ )
    {
        if( papoGroup[iRec]->GetType() == NRT_GEOMETRY
            || papoGroup[iRec]->GetType() == NRT_GEOMETRY3D )
        {
            OGRGeometry *poGeom = ProcessGeometry( papoGroup[iRec], nullptr );
            if( poGeom != nullptr )
                delete poGeom;
        }
    }
}

/************************************************************************/
/*                        FormPolygonFromCache()                        */
/*                                                                      */
/*      This method will attempt to find the line geometries            */
/*      referenced by the GEOM_ID_OF_LINK ids of a feature in the       */
/*      line cache (if available), and if so, assemble them into a      */
/*      polygon.                                                        */
/************************************************************************/

int NTFFileReader::FormPolygonFromCache( OGRFeature * poFeature )

{
    if( !bCacheLines )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Collect all the linked lines.                                   */
/* -------------------------------------------------------------------- */
    int nLinkCount = 0;
    const int *panLinks =
        poFeature->GetFieldAsIntegerList( "GEOM_ID_OF_LINK",
                                          &nLinkCount );

    if( panLinks == nullptr )
        return FALSE;

    OGRGeometryCollection oLines;

    for( int i = 0; i < nLinkCount; i++ )
    {
        OGRGeometry *poLine = CacheGetByGeomId( panLinks[i] );
        if( poLine == nullptr )
        {
            oLines.removeGeometry( -1, FALSE );
            return FALSE;
        }

        oLines.addGeometryDirectly( poLine );
    }

/* -------------------------------------------------------------------- */
/*      Assemble into a polygon geometry.                               */
/* -------------------------------------------------------------------- */
    OGRGeometry* poGeom = reinterpret_cast<OGRGeometry*>(
        OGRBuildPolygonFromEdges( (OGRGeometryH) &oLines, FALSE, FALSE, 0.1,
                                  nullptr ));

    poFeature->SetGeometryDirectly( poGeom );

    oLines.removeGeometry( -1, FALSE );

    return poGeom != nullptr;
}
