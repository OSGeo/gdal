/******************************************************************************
 * $Id: ogrbnalayer.cpp 10646 2007-01-18 02:38:10Z warmerdam $
 *
 * Project:  BNA Translator
 * Purpose:  Implements OGRBNALayer class.
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

#ifndef M_PI
# define M_PI  3.1415926535897932384626433832795
#endif

/************************************************************************/
/*                            OGRBNALayer()                             */
/*                                                                      */
/*      Note that the OGRBNALayer assumes ownership of the passed       */
/*      file pointer.                                                   */
/************************************************************************/

OGRBNALayer::OGRBNALayer( const char *pszFilename,
                          const char* layerName,
                          BNAFeatureType bnaFeatureType,
                          OGRwkbGeometryType eLayerGeomType )

{
    eof = FALSE;
    failed = FALSE;
    curLine = 0;
    nNextFID = 0;

    nFeatures = 0;
    partialIndexTable = TRUE;
    offsetAndLineFeaturesTable = NULL;

    const char* iKnowHowToCount[] = { "Primary", "Secondary", "Third", "Fourth", "Fifth" };
    char tmp[32];

    poFeatureDefn = new OGRFeatureDefn( CPLSPrintf("%s_%s", 
                                                   CPLGetBasename( pszFilename ) , 
                                                   layerName ));
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( eLayerGeomType );
    this->bnaFeatureType = bnaFeatureType;

    int i;
    for(i=0;i<NB_MAX_BNA_IDS;i++)
    {
        if (i < sizeof(iKnowHowToCount)/sizeof(iKnowHowToCount[0]))
        {
            sprintf(tmp, "%s ID", iKnowHowToCount[i]);
            OGRFieldDefn oFieldID(tmp, OFTString );
            poFeatureDefn->AddFieldDefn( &oFieldID );
        }
        else
        {
            sprintf(tmp, "%dth ID", i+1);
            OGRFieldDefn oFieldID(tmp, OFTString );
            poFeatureDefn->AddFieldDefn( &oFieldID );
        }
    }
    
    if (bnaFeatureType == BNA_ELLIPSE)
    {
        OGRFieldDefn oFieldMajorRadius( "Major radius", OFTReal );
        poFeatureDefn->AddFieldDefn( &oFieldMajorRadius );
      
        OGRFieldDefn oFieldMinorRadius( "Minor radius", OFTReal );
        poFeatureDefn->AddFieldDefn( &oFieldMinorRadius );
    }

    fpBNA = VSIFOpen( pszFilename, "rb" );
    if( fpBNA == NULL )
        return;
}

/************************************************************************/
/*                            ~OGRBNALayer()                            */
/************************************************************************/

OGRBNALayer::~OGRBNALayer()

{
    poFeatureDefn->Release();

    CPLFree(offsetAndLineFeaturesTable);

    if (fpBNA)
        VSIFClose( fpBNA );
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRBNALayer::ResetReading()

{
    eof = FALSE;
    failed = FALSE;
    curLine = 0;
    nNextFID = 0;
    VSIFSeek( fpBNA, 0, SEEK_SET );
}


/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRBNALayer::GetNextFeature()
{
    OGRFeature  *poFeature;
    BNARecord* record;
    int offset, line;

    if (failed || eof) return NULL;

    while(1)
    {
        int ok = FALSE;
        offset = VSIFTell(fpBNA);
        line = curLine;
        if (nNextFID < nFeatures)
        {
            VSIFSeek( fpBNA, offsetAndLineFeaturesTable[nNextFID].offset, SEEK_SET );
            curLine = offsetAndLineFeaturesTable[nNextFID].line;
        }
        record =  BNA_GetNextRecord(fpBNA, &ok, &curLine, TRUE, bnaFeatureType);
        if (ok == FALSE)
        {
            failed = TRUE;
            return NULL;
        }
        if (record == NULL)
        {
            /* end of file */
            eof = TRUE;
        
            /* and we have finally build the whole index table */
            partialIndexTable = FALSE;
            return NULL;
        }

        if (record->featureType == bnaFeatureType)
        {
            break;
        }
        else
        {
            BNA_FreeRecord(record);
        }
    }
    
    if (nNextFID >= nFeatures)
    {
        nFeatures++;
        offsetAndLineFeaturesTable =
            (OffsetAndLine*)CPLRealloc(offsetAndLineFeaturesTable, nFeatures * sizeof(OffsetAndLine));
        offsetAndLineFeaturesTable[nFeatures-1].offset = offset;
        offsetAndLineFeaturesTable[nFeatures-1].line = line;
    }

    poFeature = BuildFeatureFromBNARecord(record, nNextFID++);

    BNA_FreeRecord(record);

    return poFeature;
}

/************************************************************************/
/*                           BuildFeatureFromBNARecord()                          */
/************************************************************************/
OGRFeature *    OGRBNALayer::BuildFeatureFromBNARecord (BNARecord* record, long fid)
{
    OGRFeature  *poFeature;
    int i;

    poFeature = new OGRFeature( poFeatureDefn );
    for(i=0;i<NB_MAX_BNA_IDS;i++)
    {
        poFeature->SetField( i, record->ids[i] ? record->ids[i] : "");
    }
    poFeature->SetFID( fid );
    if (bnaFeatureType == BNA_POINT)
    {
        poFeature->SetGeometryDirectly( new OGRPoint( record->tabCoords[0][0], record->tabCoords[0][1] ) );
    }
    else if (bnaFeatureType == BNA_POLYLINE)
    {
        OGRLineString* lineString = new OGRLineString ();
        lineString->setCoordinateDimension(2);
        lineString->setNumPoints(record->nCoords);
        for(i=0;i<record->nCoords;i++)
        {
            lineString->setPoint(i, record->tabCoords[i][0], record->tabCoords[i][1] );
        }
        poFeature->SetGeometryDirectly(lineString);
    }
    else if (bnaFeatureType == BNA_POLYGON)
    {
        OGRMultiPolygon* multipolygon = new OGRMultiPolygon();

        double firstX = record->tabCoords[0][0];
        double firstY = record->tabCoords[0][1];
        double doubleArea = 0;
        int isFirstPolygon = 1;
        double secondaryFirstX = 0, secondaryFirstY = 0;

        OGRLinearRing* ring = new OGRLinearRing ();
        ring->setCoordinateDimension(2);
        ring->addPoint(record->tabCoords[0][0], record->tabCoords[0][1] );

        /* record->nCoords is really a safe upper bound */
        int nbPolygons = 0;
        OGRPolygon** tabPolygons =
            (OGRPolygon**)CPLMalloc(record->nCoords * sizeof(OGRPolygon*));
        double* tabPolygonsDoubleSignedArea =
            (double*)CPLMalloc(record->nCoords * sizeof(double));

        for(i=1;i<record->nCoords;i++)
        {
            ring->addPoint(record->tabCoords[i][0], record->tabCoords[i][1] );
            doubleArea += record->tabCoords[i-1][0] * record->tabCoords[i][1] -
                record->tabCoords[i][0] * record->tabCoords[i-1][1];
            if (isFirstPolygon == 1 &&
                record->tabCoords[i][0] == firstX &&
                record->tabCoords[i][1] == firstY)
            {
                /* First polygon : just add it to the multipolygon */
                OGRPolygon* polygon = new OGRPolygon ();
                polygon->addRingDirectly(ring);
                multipolygon-> addGeometryDirectly (polygon);
                tabPolygons[nbPolygons] = polygon;
                tabPolygonsDoubleSignedArea[nbPolygons] = doubleArea;
                nbPolygons++;

                if (i == record->nCoords - 1)
                {
                    break;
                }

                isFirstPolygon = 0;

                doubleArea = 0;
                i ++;
                secondaryFirstX = record->tabCoords[i][0];
                secondaryFirstY = record->tabCoords[i][1];
                ring = new OGRLinearRing ();
                ring->setCoordinateDimension(2);
                ring->addPoint(record->tabCoords[i][0], record->tabCoords[i][1] );
            }
            else if (isFirstPolygon == 0 &&
                     record->tabCoords[i][0] == secondaryFirstX &&
                     record->tabCoords[i][1] == secondaryFirstY)
            {
                /* More than one polygons in that feature. We must find if this polygon is a
                   new one or an internal ring of the smallest enclosing existing polygon of
                   same winding.
                   Assumption : an lake into a polygon appears after the enclosing polygon.
                   The BNA 'specification' does not say it, but it seems to be a reasonable
                   assumption.
                */
                int j;
                int bestCandidate = -1;
                double bestCandidateArea = 0;
                for(j=0;j<nbPolygons;j++)
                {
                    if (tabPolygonsDoubleSignedArea[j] * doubleArea > 0 &&
                        tabPolygons[j]->getExteriorRing()->Contains(ring))
                    {
                        if (bestCandidate < 0 ||
                            tabPolygonsDoubleSignedArea[j] < bestCandidateArea)
                        {
                            bestCandidate = j;
                            bestCandidateArea = tabPolygonsDoubleSignedArea[j];
                        }
                    }
                }
                if (bestCandidate >= 0)
                {
                    tabPolygons[bestCandidate]->addRingDirectly( ring );
                }
                else
                {
                    OGRPolygon* polygon = new OGRPolygon ();
                    polygon->addRingDirectly(ring);
                    multipolygon-> addGeometryDirectly (polygon);
                    tabPolygons[nbPolygons] = polygon;
                    tabPolygonsDoubleSignedArea[nbPolygons] = doubleArea;
                    nbPolygons++;
                }

                if (i < record->nCoords - 1)
                {
                    /* After the closing of a subpolygon, the first coordinates of the first polygon */
                    /* should be recalled... in theory */
                    if (record->tabCoords[i+1][0] == firstX &&  record->tabCoords[i+1][1] == firstY)
                    {
                        if (i + 1 == record->nCoords - 1)
                            break;
                        i ++;
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined, 
                                 "Geometry of polygon of fid %d starting at line %d is not strictly conformant. Trying to go on...\n",
                                 fid,
                                 offsetAndLineFeaturesTable[fid].line);
                    }

                    doubleArea = 0;
                    i ++;
                    secondaryFirstX = record->tabCoords[i][0];
                    secondaryFirstY = record->tabCoords[i][1];
                    ring = new OGRLinearRing ();
                    ring->setCoordinateDimension(2);
                    ring->addPoint(record->tabCoords[i][0], record->tabCoords[i][1] );
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined, 
                             "Geometry of polygon of fid %d starting at line %d is not strictly conformant. Trying to go on...\n",
                             fid,
                             offsetAndLineFeaturesTable[fid].line);
                }
            }
        }
        if (i == record->nCoords)
        {
            /* Let's be a bit tolerant abount non closing polygons */
            if (isFirstPolygon)
            {
                OGRPolygon* polygon = new OGRPolygon();
                ring->addPoint(record->tabCoords[0][0], record->tabCoords[0][1] );
                polygon->addRingDirectly  (  ring );
                multipolygon-> addGeometryDirectly (polygon);
            }
        }
      
        CPLFree(tabPolygons);
        CPLFree(tabPolygonsDoubleSignedArea);

        poFeature->SetGeometryDirectly(multipolygon);
    }
    else
    {
        /* Circle or ellipses are not part of the OGR Simple Geometry, so we discretize them
           into polygons by 1 degree step */
        OGRPolygon* polygon = new OGRPolygon ();
        OGRLinearRing* ring = new OGRLinearRing ();
        ring->setCoordinateDimension(2);
        double center_x = record->tabCoords[0][0];
        double center_y = record->tabCoords[0][1];
        double major_radius = record->tabCoords[1][0];
        double minor_radius = record->tabCoords[1][1];
        if (minor_radius == 0)
            minor_radius = major_radius;
        for(i=0;i<360;i++)
        {
            ring->addPoint(center_x + major_radius * cos(i * (M_PI / 180)),
                           center_y + minor_radius * sin(i * (M_PI / 180)) );
        }
        ring->addPoint(center_x + major_radius, center_y);
        polygon->addRingDirectly  (  ring );
        poFeature->SetGeometryDirectly(polygon);

        poFeature->SetField( NB_MAX_BNA_IDS, major_radius);
        poFeature->SetField( NB_MAX_BNA_IDS+1, minor_radius);
    }
    
    return poFeature;
}


/************************************************************************/
/*                           FastParseUntil()                           */
/************************************************************************/
void OGRBNALayer::FastParseUntil ( int interestFID)
{
    if (partialIndexTable)
    {
        ResetReading();

        BNARecord* record;

        if (nFeatures > 0)
        {
            VSIFSeek( fpBNA, offsetAndLineFeaturesTable[nFeatures-1].offset, SEEK_SET );
            curLine = offsetAndLineFeaturesTable[nFeatures-1].line;

            /* Just skip the last read one */
            int ok = FALSE;
            record =  BNA_GetNextRecord(fpBNA, &ok, &curLine, TRUE, BNA_READ_NONE);
            BNA_FreeRecord(record);
        }

        while(1)
        {
            int ok = FALSE;
            int offset = VSIFTell(fpBNA);
            int line = curLine;
            record =  BNA_GetNextRecord(fpBNA, &ok, &curLine, TRUE, BNA_READ_NONE);
            if (ok == FALSE)
            {
                failed = TRUE;
                return;
            }
            if (record == NULL)
            {
                /* end of file */
                eof = TRUE;
        
                /* and we have finally build the whole index table */
                partialIndexTable = FALSE;
                return;
            }

            if (record->featureType == bnaFeatureType)
            {
                nFeatures++;
                offsetAndLineFeaturesTable =
                    (OffsetAndLine*)CPLRealloc(offsetAndLineFeaturesTable, nFeatures * sizeof(OffsetAndLine));
                offsetAndLineFeaturesTable[nFeatures-1].offset = offset;
                offsetAndLineFeaturesTable[nFeatures-1].line = line;
            }
            BNA_FreeRecord(record);

            if (nFeatures - 1 == interestFID)
                return;
        }
    }
}

/************************************************************************/
/*                           GetFeatureCount()                          */
/************************************************************************/

OGRFeature *  OGRBNALayer::GetFeature( long nFID )
{
    OGRFeature  *poFeature;
    BNARecord* record;
    int ok;
    
    if (nFID < 0)
        return NULL;

    FastParseUntil(nFID);

    if (nFID >= nFeatures)
        return NULL;

    VSIFSeek( fpBNA, offsetAndLineFeaturesTable[nFID].offset, SEEK_SET );
    curLine = offsetAndLineFeaturesTable[nFID].line;
    record =  BNA_GetNextRecord(fpBNA, &ok, &curLine, TRUE, bnaFeatureType);

    poFeature = BuildFeatureFromBNARecord(record, nFID);

    BNA_FreeRecord(record);

    return poFeature;
}

/************************************************************************/
/*                           GetFeatureCount()                          */
/************************************************************************/

int  OGRBNALayer::GetFeatureCount( int )
{
    FastParseUntil(-1);
    return nFeatures;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRBNALayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCSequentialWrite) )
        return FALSE;
    else if( EQUAL(pszCap,OLCCreateField) )
        return FALSE;
    else
        return FALSE;
}

