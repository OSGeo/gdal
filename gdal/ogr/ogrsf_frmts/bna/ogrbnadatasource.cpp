/******************************************************************************
 *
 * Project:  BNA Translator
 * Purpose:  Implements OGRBNADataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2007-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_bna.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_csv.h"
#include "ogrbnaparser.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                          OGRBNADataSource()                          */
/************************************************************************/

OGRBNADataSource::OGRBNADataSource() :
    pszName(NULL),
    papoLayers(NULL),
    nLayers(0),
    bUpdate(false),
    fpOutput(NULL),
    bUseCRLF(false),
    bMultiLine(FALSE),
    nbOutID(0),
    bEllipsesAsEllipses(false),
    nbPairPerLine(FALSE),
    coordinatePrecision(0),
    pszCoordinateSeparator(NULL)
{}

/************************************************************************/
/*                         ~OGRBNADataSource()                          */
/************************************************************************/

OGRBNADataSource::~OGRBNADataSource()

{
    if ( fpOutput != NULL )
    {
        VSIFCloseL( fpOutput);
    }

    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    CPLFree( pszCoordinateSeparator );

    CPLFree( pszName );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRBNADataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRBNADataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;

    return papoLayers[iLayer];
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer * OGRBNADataSource::ICreateLayer( const char * pszLayerName,
                                           OGRSpatialReference * /*poSRS */,
                                           OGRwkbGeometryType eType,
                                           char ** /* papszOptions */ )
{
    BNAFeatureType bnaFeatureType;

    switch(eType)
    {
        case wkbPolygon:
        case wkbPolygon25D:
        case wkbMultiPolygon:
        case wkbMultiPolygon25D:
            bnaFeatureType = BNA_POLYGON;
            break;

        case wkbPoint:
        case wkbPoint25D:
            bnaFeatureType = BNA_POINT;
            break;

        case wkbLineString:
        case wkbLineString25D:
            bnaFeatureType = BNA_POLYLINE;
            break;

        default:
            CPLError( CE_Failure, CPLE_NotSupported,
                    "Geometry type of `%s' not supported in BNAs.\n",
                    OGRGeometryTypeToName(eType) );
            return NULL;
    }

    nLayers++;
    papoLayers = static_cast<OGRBNALayer **>(
        CPLRealloc( papoLayers, nLayers * sizeof(OGRBNALayer*) ) );
    papoLayers[nLayers-1] = new OGRBNALayer(
        pszName, pszLayerName, bnaFeatureType, eType, TRUE, this );

    return papoLayers[nLayers-1];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRBNADataSource::Open( const char * pszFilename, int bUpdateIn)

{
    int ok = FALSE;

    pszName = CPLStrdup( pszFilename );
    bUpdate = CPL_TO_BOOL(bUpdateIn);

    VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
    if (fp)
    {
        int curLine = 0;
        static const char* const layerRadixName[]
            = { "points", "polygons", "lines", "ellipses"};
        static const OGRwkbGeometryType wkbGeomTypes[]
            = { wkbPoint, wkbMultiPolygon, wkbLineString, wkbPolygon };

#if defined(BNA_FAST_DS_OPEN)
        BNARecord* record = BNA_GetNextRecord(fp, &ok, &curLine, FALSE, BNA_READ_NONE);
        BNA_FreeRecord(record);

        if (ok)
        {
            nLayers = 4;

            papoLayers = static_cast<OGRBNALayer **>(
                CPLMalloc(nLayers * sizeof(OGRBNALayer*)));
            for( i = 0; i < 4; i++ )
                papoLayers[i] = new OGRBNALayer(
                    pszFilename,
                    layerRadixName[i],
                    static_cast<BNAFeatureType>( i ),
                    wkbGeomTypes[i], FALSE, this );
        }
#else
        int nFeatures[4] = { 0, 0, 0, 0 };
        OffsetAndLine* offsetAndLineFeaturesTable[4] = { NULL, NULL, NULL, NULL };
        int nIDs[4] = {0, 0, 0, 0};
        bool partialIndexTable = true;

        BNARecord* record = NULL;
        while(1)
        {
            int offset = static_cast<int>( VSIFTellL(fp) );
            int line = curLine;
            record =  BNA_GetNextRecord(fp, &ok, &curLine, FALSE, BNA_READ_NONE);
            if (ok == FALSE)
            {
                BNA_FreeRecord(record);
                if (line != 0)
                    ok = TRUE;
                break;
            }
            if (record == NULL)
            {
                /* end of file */
                ok = TRUE;

                /* and we have finally build the whole index table */
                partialIndexTable = false;
                break;
            }

            if (record->nIDs > nIDs[record->featureType])
                nIDs[record->featureType] = record->nIDs;

            nFeatures[record->featureType]++;
            offsetAndLineFeaturesTable[record->featureType] =
              static_cast<OffsetAndLine *>( CPLRealloc(
                  offsetAndLineFeaturesTable[record->featureType],
                  nFeatures[record->featureType] * sizeof(OffsetAndLine) ) );
            offsetAndLineFeaturesTable[record->featureType][nFeatures[record->featureType]-1].offset = offset;
            offsetAndLineFeaturesTable[record->featureType][nFeatures[record->featureType]-1].line = line;

            BNA_FreeRecord(record);
        }

        nLayers = (nFeatures[0] != 0) + (nFeatures[1] != 0) + (nFeatures[2] != 0) + (nFeatures[3] != 0);
        papoLayers = static_cast<OGRBNALayer **>(
            CPLMalloc(nLayers * sizeof(OGRBNALayer*)) );
        int iLayer = 0;
        for( int i = 0; i < 4; i++ )
        {
            if (nFeatures[i])
            {
                papoLayers[iLayer] = new OGRBNALayer( pszFilename,
                                                      layerRadixName[i],
                                                      (BNAFeatureType)i,
                                                      wkbGeomTypes[i],
                                                      FALSE,
                                                      this,
                                                      nIDs[i]);
                papoLayers[iLayer]->SetFeatureIndexTable(nFeatures[i],
                                                        offsetAndLineFeaturesTable[i],
                                                        partialIndexTable);
                iLayer++;
            }
        }
#endif
        VSIFCloseL(fp);
    }

    return ok;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

int OGRBNADataSource::Create( const char *pszFilename,
                              char **papszOptions )
{
    if( fpOutput != NULL)
    {
        CPLAssert( false );
        return FALSE;
    }

    if( strcmp(pszFilename,"/dev/stdout") == 0 )
        pszFilename = "/vsistdout/";

/* -------------------------------------------------------------------- */
/*     Do not override exiting file.                                    */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;

    if( VSIStatL( pszFilename, &sStatBuf ) == 0 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */
    pszName = CPLStrdup( pszFilename );

    fpOutput = VSIFOpenL( pszFilename, "wb" );
    if( fpOutput == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to create BNA file %s.",
                  pszFilename );
        return FALSE;
    }

    /* EOL token */
    const char *pszCRLFFormat = CSLFetchNameValue( papszOptions, "LINEFORMAT");

    if( pszCRLFFormat == NULL )
    {
#ifdef WIN32
        bUseCRLF = true;
#else
        bUseCRLF = false;
#endif
    }
    else if( EQUAL(pszCRLFFormat,"CRLF") )
        bUseCRLF = true;
    else if( EQUAL(pszCRLFFormat,"LF") )
        bUseCRLF = false;
    else
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "LINEFORMAT=%s not understood, use one of CRLF or LF.",
                  pszCRLFFormat );
#ifdef WIN32
        bUseCRLF = true;
#else
        bUseCRLF = false;
#endif
    }

    /* Multi line or single line format ? */
    bMultiLine = CPLFetchBool( papszOptions, "MULTILINE", true);

    /* Number of identifiers per record */
    const char* pszNbOutID = CSLFetchNameValue ( papszOptions, "NB_IDS");
    if (pszNbOutID == NULL)
    {
        nbOutID = NB_MIN_BNA_IDS;
    }
    else if (EQUAL(pszNbOutID, "NB_SOURCE_FIELDS"))
    {
        nbOutID = -1;
    }
    else
    {
        nbOutID = atoi(pszNbOutID);
        if (nbOutID <= 0)
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                  "NB_ID=%s not understood. Must be >=%d and <=%d or equal to NB_SOURCE_FIELDS",
                  pszNbOutID, NB_MIN_BNA_IDS, NB_MAX_BNA_IDS );
            nbOutID = NB_MIN_BNA_IDS;
        }
        if (nbOutID > NB_MAX_BNA_IDS)
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                  "NB_ID=%s not understood. Must be >=%d and <=%d or equal to NB_SOURCE_FIELDS",
                  pszNbOutID, NB_MIN_BNA_IDS, NB_MAX_BNA_IDS );
            nbOutID = NB_MAX_BNA_IDS;
        }
    }

    /* Ellipses export as ellipses or polygons ? */
    bEllipsesAsEllipses =
        CPLFetchBool( papszOptions, "ELLIPSES_AS_ELLIPSES", true);

    /* Number of coordinate pairs per line */
    const char* pszNbPairPerLine = CSLFetchNameValue( papszOptions, "NB_PAIRS_PER_LINE");
    if (pszNbPairPerLine)
    {
        nbPairPerLine = atoi(pszNbPairPerLine);
        if (nbPairPerLine <= 0)
            nbPairPerLine = (bMultiLine == FALSE) ? 1000000000 : 1;
        if (bMultiLine == FALSE)
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "NB_PAIR_PER_LINE option is ignored when MULTILINE=NO" );
        }
    }
    else
    {
        nbPairPerLine = (bMultiLine == FALSE) ? 1000000000 : 1;
    }

    /* Coordinate precision */
    const char* pszCoordinatePrecision
        = CSLFetchNameValue( papszOptions, "COORDINATE_PRECISION" );
    if (pszCoordinatePrecision)
    {
        coordinatePrecision = atoi(pszCoordinatePrecision);
        if (coordinatePrecision <= 0)
            coordinatePrecision = 0;
        else if (coordinatePrecision >= 20)
            coordinatePrecision = 20;
    }
    else
    {
        coordinatePrecision = 10;
    }

    pszCoordinateSeparator = const_cast<char *>(
        CSLFetchNameValue( papszOptions, "COORDINATE_SEPARATOR" ) );
    if (pszCoordinateSeparator == NULL)
        pszCoordinateSeparator = CPLStrdup(",");
    else
        pszCoordinateSeparator = CPLStrdup(pszCoordinateSeparator);

    return TRUE;
}
