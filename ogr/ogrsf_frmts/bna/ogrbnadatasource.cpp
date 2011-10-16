/******************************************************************************
 * $Id$
 *
 * Project:  BNA Translator
 * Purpose:  Implements OGRBNADataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2007, Even Rouault
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

/************************************************************************/
/*                          OGRBNADataSource()                          */
/************************************************************************/

OGRBNADataSource::OGRBNADataSource()

{
    papoLayers = NULL;
    nLayers = 0;
    
    fpOutput = NULL;

    pszName = NULL;
    
    pszCoordinateSeparator = NULL;

    bUpdate = FALSE;
}

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
    else if( EQUAL(pszCap,ODsCDeleteLayer) )
        return FALSE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRBNADataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer * OGRBNADataSource::CreateLayer( const char * pszLayerName,
                                          OGRSpatialReference *poSRS,
                                          OGRwkbGeometryType eType,
                                          char ** papszOptions )

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
    papoLayers = (OGRBNALayer **) CPLRealloc(papoLayers, nLayers * sizeof(OGRBNALayer*));
    papoLayers[nLayers-1] = new OGRBNALayer( pszName, pszLayerName, bnaFeatureType, eType, TRUE, this );
    
    return papoLayers[nLayers-1];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRBNADataSource::Open( const char * pszFilename, int bUpdateIn)

{
    int ok = FALSE;

    pszName = CPLStrdup( pszFilename );
    bUpdate = bUpdateIn;

/* -------------------------------------------------------------------- */
/*      Determine what sort of object this is.                          */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;

    if( VSIStatExL( pszFilename, &sStatBuf, VSI_STAT_NATURE_FLAG ) != 0 )
        return FALSE;
    
// -------------------------------------------------------------------- 
//      Does this appear to be a .bna file?
// --------------------------------------------------------------------
    if( !(EQUAL( CPLGetExtension(pszFilename), "bna" )
           || ((EQUALN( pszFilename, "/vsigzip/", 9) || EQUALN( pszFilename, "/vsizip/", 8)) &&
               (strstr( pszFilename, ".bna") || strstr( pszFilename, ".BNA")))) )
        return FALSE;
    
    VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
    if (fp)
    {
        BNARecord* record;
        int curLine = 0;
        const char* layerRadixName[] = { "points", "polygons", "lines", "ellipses"};
        OGRwkbGeometryType wkbGeomTypes[] = { wkbPoint, wkbMultiPolygon, wkbLineString, wkbPolygon };
        int i;
#if defined(BNA_FAST_DS_OPEN)
        record = BNA_GetNextRecord(fp, &ok, &curLine, FALSE, BNA_READ_NONE);
        BNA_FreeRecord(record);

        if (ok)
        {
            nLayers = 4;

            papoLayers = (OGRBNALayer **) CPLMalloc(nLayers * sizeof(OGRBNALayer*));
            for(i=0;i<4;i++)
                papoLayers[i] = new OGRBNALayer( pszFilename,
                                                 layerRadixName[i],
                                                 (BNAFeatureType)i, wkbGeomTypes[i], FALSE, this );
        }
#else
        int nFeatures[4] = { 0, 0, 0, 0 };
        OffsetAndLine* offsetAndLineFeaturesTable[4] = { NULL, NULL, NULL, NULL };
        int nIDs[4] = {0, 0, 0, 0};
        int partialIndexTable = TRUE;

        while(1)
        {
            int offset = (int) VSIFTellL(fp);
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
                partialIndexTable = FALSE;
                break;
            }

            if (record->nIDs > nIDs[record->featureType])
                nIDs[record->featureType] = record->nIDs;

            nFeatures[record->featureType]++;
            offsetAndLineFeaturesTable[record->featureType] =
                (OffsetAndLine*)CPLRealloc(offsetAndLineFeaturesTable[record->featureType],
                                           nFeatures[record->featureType] * sizeof(OffsetAndLine));
            offsetAndLineFeaturesTable[record->featureType][nFeatures[record->featureType]-1].offset = offset;
            offsetAndLineFeaturesTable[record->featureType][nFeatures[record->featureType]-1].line = line;

            BNA_FreeRecord(record);
        }

        nLayers = (nFeatures[0] != 0) + (nFeatures[1] != 0) + (nFeatures[2] != 0) + (nFeatures[3] != 0);
        papoLayers = (OGRBNALayer **) CPLMalloc(nLayers * sizeof(OGRBNALayer*));
        int iLayer = 0;
        for(i=0;i<4;i++)
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
        CPLAssert( FALSE );
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
        bUseCRLF = TRUE;
#else
        bUseCRLF = FALSE;
#endif
    }
    else if( EQUAL(pszCRLFFormat,"CRLF") )
        bUseCRLF = TRUE;
    else if( EQUAL(pszCRLFFormat,"LF") )
        bUseCRLF = FALSE;
    else
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "LINEFORMAT=%s not understood, use one of CRLF or LF.",
                  pszCRLFFormat );
#ifdef WIN32
        bUseCRLF = TRUE;
#else
        bUseCRLF = FALSE;
#endif
    }

    /* Multi line or single line format ? */
    bMultiLine = CSLFetchBoolean( papszOptions, "MULTILINE", TRUE);
    
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
    bEllipsesAsEllipses = CSLFetchBoolean( papszOptions, "ELLIPSES_AS_ELLIPSES", TRUE);
    
    /* Number of coordinate pairs per line */
    const char* pszNbPairPerLine = CSLFetchNameValue( papszOptions, "NB_PAIRS_PER_LINE");
    if (pszNbPairPerLine)
    {
        nbPairPerLine = atoi(pszNbPairPerLine);
        if (nbPairPerLine <= 0)
            nbPairPerLine = (bMultiLine == FALSE) ? 1000000000 : 1;
        if (bMultiLine == FALSE)
        {
            CPLError( CE_Warning, CPLE_AppDefined, "NB_PAIR_PER_LINE option is ignored when MULTILINE=NO");
        }
    }
    else
    {
        nbPairPerLine = (bMultiLine == FALSE) ? 1000000000 : 1;
    }
    
    /* Coordinate precision */
    const char* pszCoordinatePrecision = CSLFetchNameValue( papszOptions, "COORDINATE_PRECISION");
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
    
    pszCoordinateSeparator = (char*)CSLFetchNameValue( papszOptions, "COORDINATE_SEPARATOR");
    if (pszCoordinateSeparator == NULL)
        pszCoordinateSeparator = CPLStrdup(",");
    else
        pszCoordinateSeparator = CPLStrdup(pszCoordinateSeparator);

    return TRUE;
}
