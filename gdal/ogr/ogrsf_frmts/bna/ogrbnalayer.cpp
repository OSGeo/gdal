/******************************************************************************
 * $Id$
 *
 * Project:  BNA Translator
 * Purpose:  Implements OGRBNALayer class.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2007-2010, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogr_p.h"

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
                          OGRwkbGeometryType eLayerGeomType,
                          int bWriter,
                          OGRBNADataSource* poDS,
                          int nIDs)

{
    eof = FALSE;
    failed = FALSE;
    curLine = 0;
    nNextFID = 0;
    
    this->bWriter = bWriter;
    this->poDS = poDS;
    this->nIDs = nIDs;

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
    SetDescription( poFeatureDefn->GetName() );
    this->bnaFeatureType = bnaFeatureType;

    if (! bWriter )
    {
        int i;
        for(i=0;i<nIDs;i++)
        {
            if (i < (int) (sizeof(iKnowHowToCount)/sizeof(iKnowHowToCount[0])) )
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

        fpBNA = VSIFOpenL( pszFilename, "rb" );
        if( fpBNA == NULL )
            return;
    }
    else
    {
        fpBNA = NULL;
    }
}

/************************************************************************/
/*                            ~OGRBNALayer()                            */
/************************************************************************/

OGRBNALayer::~OGRBNALayer()

{
    poFeatureDefn->Release();

    CPLFree(offsetAndLineFeaturesTable);

    if (fpBNA)
        VSIFCloseL( fpBNA );
}

/************************************************************************/
/*                         SetFeatureIndexTable()                       */
/************************************************************************/
void  OGRBNALayer::SetFeatureIndexTable(int nFeatures, OffsetAndLine* offsetAndLineFeaturesTable, int partialIndexTable)
{
    this->nFeatures = nFeatures;
    this->offsetAndLineFeaturesTable = offsetAndLineFeaturesTable;
    this->partialIndexTable = partialIndexTable;
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
    VSIFSeekL( fpBNA, 0, SEEK_SET );
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
        offset = (int) VSIFTellL(fpBNA);
        line = curLine;
        if (nNextFID < nFeatures)
        {
            VSIFSeekL( fpBNA, offsetAndLineFeaturesTable[nNextFID].offset, SEEK_SET );
            curLine = offsetAndLineFeaturesTable[nNextFID].line;
        }
        record =  BNA_GetNextRecord(fpBNA, &ok, &curLine, TRUE, bnaFeatureType);
        if (ok == FALSE)
        {
            BNA_FreeRecord(record);
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

            if(   (m_poFilterGeom == NULL
                || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature )) )
            {
                 return poFeature;
            }

            delete poFeature;
        }
        else
        {
            BNA_FreeRecord(record);
        }
    }
}


/************************************************************************/
/*                      WriteFeatureAttributes()                        */
/************************************************************************/

void OGRBNALayer::WriteFeatureAttributes(VSILFILE* fp, OGRFeature *poFeature )
{
    int i;
    OGRFieldDefn *poFieldDefn;
    int nbOutID = poDS->GetNbOutId();
    if (nbOutID < 0)
        nbOutID = poFeatureDefn->GetFieldCount();
    for(i=0;i<nbOutID;i++)
    { 
        if (i < poFeatureDefn->GetFieldCount())
        {
            poFieldDefn = poFeatureDefn->GetFieldDefn( i );
            if( poFeature->IsFieldSet( i ) )
            {
                if (poFieldDefn->GetType() == OFTReal)
                {
                    char szBuffer[64];
                    OGRFormatDouble(szBuffer, sizeof(szBuffer),
                                    poFeature->GetFieldAsDouble(i), '.');
                    VSIFPrintfL( fp, "\"%s\",", szBuffer);
                }
                else
                {
                    const char *pszRaw = poFeature->GetFieldAsString( i );
                    VSIFPrintfL( fp, "\"%s\",", pszRaw);
                }
            }
            else
            {
                VSIFPrintfL( fp, "\"\",");
            }
        }
        else
        {
            VSIFPrintfL( fp, "\"\",");
        }
    }
}

/************************************************************************/
/*                             WriteCoord()                             */
/************************************************************************/

void OGRBNALayer::WriteCoord(VSILFILE* fp, double dfX, double dfY)
{
    char szBuffer[64];
    OGRFormatDouble(szBuffer, sizeof(szBuffer), dfX, '.', poDS->GetCoordinatePrecision());
    VSIFPrintfL( fp, "%s", szBuffer);
    VSIFPrintfL( fp, "%s", poDS->GetCoordinateSeparator());
    OGRFormatDouble(szBuffer, sizeof(szBuffer), dfY, '.', poDS->GetCoordinatePrecision());
    VSIFPrintfL( fp, "%s", szBuffer);
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRBNALayer::CreateFeature( OGRFeature *poFeature )

{
    int i,j,k,n;
    OGRGeometry     *poGeom = poFeature->GetGeometryRef();
    char eol[3];
    const char* partialEol = (poDS->GetMultiLine()) ? eol : poDS->GetCoordinateSeparator();

    if (poGeom == NULL || poGeom->IsEmpty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "OGR BNA driver cannot write features with empty geometries.");
        return OGRERR_FAILURE;
    }

    if (poDS->GetUseCRLF())
    {
        eol[0] = 13;
        eol[1] = 10;
        eol[2] = 0;
    }
    else
    {
        eol[0] = 10;
        eol[1] = 0;
    }
    
    if ( ! bWriter )
    {
        return OGRERR_FAILURE;
    }
    
    if( poFeature->GetFID() == OGRNullFID )
        poFeature->SetFID( nFeatures++ );
    
    VSILFILE* fp = poDS->GetOutputFP();
    int nbPairPerLine = poDS->GetNbPairPerLine();

    switch( poGeom->getGeometryType() )
    {
        case wkbPoint:
        case wkbPoint25D:
        {
            OGRPoint* point = (OGRPoint*)poGeom;
            WriteFeatureAttributes(fp, poFeature);
            VSIFPrintfL( fp, "1");
            VSIFPrintfL( fp, "%s", partialEol);
            WriteCoord(fp, point->getX(), point->getY());
            VSIFPrintfL( fp, "%s", eol);
            break;
        }
            
        case wkbPolygon:
        case wkbPolygon25D:
        {
            OGRPolygon* polygon = (OGRPolygon*)poGeom;
            OGRLinearRing* ring = polygon->getExteriorRing();
            if (ring == NULL)
            {
                return OGRERR_FAILURE;
            }
            
            double firstX = ring->getX(0);
            double firstY = ring->getY(0);
            int nBNAPoints = ring->getNumPoints();
            int is_ellipse = FALSE;
            
            /* This code tries to detect an ellipse in a polygon geometry */
            /* This will only work presumably on ellipses already read from a BNA file */
            /* Mostly a BNA to BNA feature... */
            if (poDS->GetEllipsesAsEllipses() &&
                polygon->getNumInteriorRings() == 0 &&
                nBNAPoints == 361)
            {
                double oppositeX = ring->getX(180);
                double oppositeY = ring->getY(180);
                double quarterX = ring->getX(90);
                double quarterY = ring->getY(90);
                double antiquarterX = ring->getX(270);
                double antiquarterY = ring->getY(270);
                double center1X = 0.5*(firstX + oppositeX);
                double center1Y = 0.5*(firstY + oppositeY);
                double center2X = 0.5*(quarterX + antiquarterX);
                double center2Y = 0.5*(quarterY + antiquarterY);
                if (fabs(center1X - center2X) < 1e-5 && fabs(center1Y - center2Y) < 1e-5 &&
                    fabs(oppositeY - firstY) < 1e-5 &&
                    fabs(quarterX - antiquarterX) < 1e-5)
                {
                    double major_radius = fabs(firstX - center1X);
                    double minor_radius = fabs(quarterY - center1Y);
                    is_ellipse = TRUE;
                    for(i=0;i<360;i++)
                    {
                        if (!(fabs(center1X + major_radius * cos(i * (M_PI / 180)) - ring->getX(i)) < 1e-5 &&
                              fabs(center1Y + minor_radius * sin(i * (M_PI / 180)) - ring->getY(i)) < 1e-5))
                        {
                            is_ellipse = FALSE;
                            break;
                        }
                    }
                    if ( is_ellipse == TRUE )
                    {
                        WriteFeatureAttributes(fp, poFeature);
                        VSIFPrintfL( fp, "2");
                        VSIFPrintfL( fp, "%s", partialEol);
                        WriteCoord(fp, center1X, center1Y);
                        VSIFPrintfL( fp, "%s", partialEol);
                        WriteCoord(fp, major_radius, minor_radius);
                        VSIFPrintfL( fp, "%s", eol);
                    }
                }
            }

            if ( is_ellipse == FALSE)
            {
                int nInteriorRings = polygon->getNumInteriorRings();
                for(i=0;i<nInteriorRings;i++)
                {
                    nBNAPoints += polygon->getInteriorRing(i)->getNumPoints() + 1;
                }
                if (nBNAPoints <= 3)
                {
                    CPLError( CE_Failure, CPLE_AppDefined, "Invalid geometry" );
                    return OGRERR_FAILURE;
                }
                WriteFeatureAttributes(fp, poFeature);
                VSIFPrintfL( fp, "%d", nBNAPoints);
                n = ring->getNumPoints();
                int nbPair = 0;
                for(i=0;i<n;i++)
                {
                    VSIFPrintfL( fp, "%s", ((nbPair % nbPairPerLine) == 0) ? partialEol : " ");
                    WriteCoord(fp, ring->getX(i), ring->getY(i));
                    nbPair++;
                }
                for(i=0;i<nInteriorRings;i++)
                {
                    ring = polygon->getInteriorRing(i);
                    n = ring->getNumPoints();
                    for(j=0;j<n;j++)
                    {
                        VSIFPrintfL( fp, "%s", ((nbPair % nbPairPerLine) == 0) ? partialEol : " ");
                        WriteCoord(fp, ring->getX(j), ring->getY(j));
                        nbPair++;
                    }
                    VSIFPrintfL( fp, "%s", ((nbPair % nbPairPerLine) == 0) ? partialEol : " ");
                    WriteCoord(fp, firstX, firstY);
                    nbPair++;
                }
                VSIFPrintfL( fp, "%s", eol);
            }
            break;
        }

        case wkbMultiPolygon:
        case wkbMultiPolygon25D:
        {
            OGRMultiPolygon* multipolygon = (OGRMultiPolygon*)poGeom;
            int N = multipolygon->getNumGeometries();
            int nBNAPoints = 0;
            double firstX = 0, firstY = 0; 
            for(i=0;i<N;i++)
            {
                OGRPolygon* polygon = (OGRPolygon*)multipolygon->getGeometryRef(i);
                OGRLinearRing* ring = polygon->getExteriorRing();
                if (ring == NULL)
                    continue;

                if (nBNAPoints)
                    nBNAPoints ++;
                else
                {
                    firstX = ring->getX(0);
                    firstY = ring->getY(0);
                }
                nBNAPoints += ring->getNumPoints();
                int nInteriorRings = polygon->getNumInteriorRings();
                for(j=0;j<nInteriorRings;j++)
                {
                    nBNAPoints += polygon->getInteriorRing(j)->getNumPoints() + 1;
                }
            }
            if (nBNAPoints <= 3)
            {
                CPLError( CE_Failure, CPLE_AppDefined, "Invalid geometry" );
                return OGRERR_FAILURE;
            }
            WriteFeatureAttributes(fp, poFeature);
            VSIFPrintfL( fp, "%d", nBNAPoints);
            int nbPair = 0;
            for(i=0;i<N;i++)
            {
                OGRPolygon* polygon = (OGRPolygon*)multipolygon->getGeometryRef(i);
                OGRLinearRing* ring = polygon->getExteriorRing();
                if (ring == NULL)
                    continue;

                n = ring->getNumPoints();
                int nInteriorRings = polygon->getNumInteriorRings();
                for(j=0;j<n;j++)
                {
                    VSIFPrintfL( fp, "%s", ((nbPair % nbPairPerLine) == 0) ? partialEol : " ");
                    WriteCoord(fp, ring->getX(j), ring->getY(j));
                    nbPair++;
                }
                if (i != 0)
                {
                    VSIFPrintfL( fp, "%s", ((nbPair % nbPairPerLine) == 0) ? partialEol : " ");
                    WriteCoord(fp, firstX, firstY);
                    nbPair++;
                }
                for(j=0;j<nInteriorRings;j++)
                {
                    ring = polygon->getInteriorRing(j);
                    n = ring->getNumPoints();
                    for(k=0;k<n;k++)
                    {
                        VSIFPrintfL( fp, "%s", ((nbPair % nbPairPerLine) == 0) ? partialEol : " ");
                        WriteCoord(fp, ring->getX(k), ring->getY(k));
                        nbPair++;
                    }
                    VSIFPrintfL( fp, "%s", ((nbPair % nbPairPerLine) == 0) ? partialEol : " ");
                    WriteCoord(fp, firstX, firstY);
                    nbPair++;
                }
            }
            VSIFPrintfL( fp, "%s", eol);
            break;
        }

        case wkbLineString:
        case wkbLineString25D:
        {
            OGRLineString* line = (OGRLineString*)poGeom;
            int n = line->getNumPoints();
            int i;
            if (n < 2)
            {
                CPLError( CE_Failure, CPLE_AppDefined, "Invalid geometry" );
                return OGRERR_FAILURE;
            }
            WriteFeatureAttributes(fp, poFeature);
            VSIFPrintfL( fp, "-%d", n);
            int nbPair = 0;
            for(i=0;i<n;i++)
            {
                VSIFPrintfL( fp, "%s", partialEol);
                WriteCoord(fp, line->getX(i), line->getY(i));
                nbPair++;
            }
            VSIFPrintfL( fp, "%s", eol);
            break;
        }
            
        default:
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unsupported geometry type : %s.",
                      poGeom->getGeometryName() );

            return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
        }
    }
    
    return OGRERR_NONE;
}



/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRBNALayer::CreateField( OGRFieldDefn *poField, int bApproxOK )

{
    if( !bWriter || nFeatures != 0)
        return OGRERR_FAILURE;

    poFeatureDefn->AddFieldDefn( poField );

    return OGRERR_NONE;
}


/************************************************************************/
/*                           BuildFeatureFromBNARecord()                */
/************************************************************************/
OGRFeature *    OGRBNALayer::BuildFeatureFromBNARecord (BNARecord* record, long fid)
{
    OGRFeature  *poFeature;
    int i;

    poFeature = new OGRFeature( poFeatureDefn );
    for(i=0;i<nIDs;i++)
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
        double firstX = record->tabCoords[0][0];
        double firstY = record->tabCoords[0][1];
        int isFirstPolygon = 1;
        double secondaryFirstX = 0, secondaryFirstY = 0;
  
        OGRLinearRing* ring = new OGRLinearRing ();
        ring->setCoordinateDimension(2);
        ring->addPoint(record->tabCoords[0][0], record->tabCoords[0][1] );
  
        /* record->nCoords is really a safe upper bound */
        int nbPolygons = 0;
        OGRPolygon** tabPolygons =
            (OGRPolygon**)CPLMalloc(record->nCoords * sizeof(OGRPolygon*));

        for(i=1;i<record->nCoords;i++)
        {
            ring->addPoint(record->tabCoords[i][0], record->tabCoords[i][1] );
            if (isFirstPolygon == 1 &&
                record->tabCoords[i][0] == firstX &&
                record->tabCoords[i][1] == firstY)
            {
                OGRPolygon* polygon = new OGRPolygon ();
                polygon->addRingDirectly(ring);
                tabPolygons[nbPolygons] = polygon;
                nbPolygons++;
    
                if (i == record->nCoords - 1)
                {
                    break;
                }
    
                isFirstPolygon = 0;
    
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

                OGRPolygon* polygon = new OGRPolygon ();
                polygon->addRingDirectly(ring);
                tabPolygons[nbPolygons] = polygon;
                nbPolygons++;

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
#if 0
                        CPLError(CE_Warning, CPLE_AppDefined, 
                                 "Geometry of polygon of fid %d starting at line %d is not strictly conformant. "
                                 "Trying to go on...\n",
                                 fid,
                                 offsetAndLineFeaturesTable[fid].line + 1);
#endif
                    }

                    i ++;
                    secondaryFirstX = record->tabCoords[i][0];
                    secondaryFirstY = record->tabCoords[i][1];
                    ring = new OGRLinearRing ();
                    ring->setCoordinateDimension(2);
                    ring->addPoint(record->tabCoords[i][0], record->tabCoords[i][1] );
                }
                else
                {
#if 0
                    CPLError(CE_Warning, CPLE_AppDefined, 
                        "Geometry of polygon of fid %d starting at line %d is not strictly conformant. Trying to go on...\n",
                        fid,
                        offsetAndLineFeaturesTable[fid].line + 1);
#endif
                }
            }
        }
        if (i == record->nCoords)
        {
            /* Let's be a bit tolerant abount non closing polygons */
            if (isFirstPolygon)
            {
                ring->addPoint(record->tabCoords[0][0], record->tabCoords[0][1] );

                OGRPolygon* polygon = new OGRPolygon ();
                polygon->addRingDirectly(ring);
                tabPolygons[nbPolygons] = polygon;
                nbPolygons++;
            }
        }
        
        if (nbPolygons == 1)
        {
            /* Special optimization here : we directly put the polygon into the multipolygon. */
            /* This should save quite a few useless copies */
            OGRMultiPolygon* multipolygon = new OGRMultiPolygon();
            multipolygon->addGeometryDirectly(tabPolygons[0]);
            poFeature->SetGeometryDirectly(multipolygon);
        }
        else
        {
            int isValidGeometry;
            poFeature->SetGeometryDirectly(
                OGRGeometryFactory::organizePolygons((OGRGeometry**)tabPolygons, nbPolygons, &isValidGeometry, NULL));
            
            if (!isValidGeometry)
            {
                CPLError(CE_Warning, CPLE_AppDefined, 
                        "Geometry of polygon of fid %ld starting at line %d cannot be translated to Simple Geometry. "
                        "All polygons will be contained in a multipolygon.\n",
                        fid,
                        offsetAndLineFeaturesTable[fid].line + 1);
            }
        }

        CPLFree(tabPolygons);
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

        poFeature->SetField( nIDs, major_radius);
        poFeature->SetField( nIDs+1, minor_radius);
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
            VSIFSeekL( fpBNA, offsetAndLineFeaturesTable[nFeatures-1].offset, SEEK_SET );
            curLine = offsetAndLineFeaturesTable[nFeatures-1].line;

            /* Just skip the last read one */
            int ok = FALSE;
            record =  BNA_GetNextRecord(fpBNA, &ok, &curLine, TRUE, BNA_READ_NONE);
            BNA_FreeRecord(record);
        }

        while(1)
        {
            int ok = FALSE;
            int offset = (int) VSIFTellL(fpBNA);
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

                BNA_FreeRecord(record);

                if (nFeatures - 1 == interestFID)
                  return;
            }
            else
            {
                BNA_FreeRecord(record);
            }
        }
    }
}

/************************************************************************/
/*                           GetFeature()                               */
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

    VSIFSeekL( fpBNA, offsetAndLineFeaturesTable[nFID].offset, SEEK_SET );
    curLine = offsetAndLineFeaturesTable[nFID].line;
    record =  BNA_GetNextRecord(fpBNA, &ok, &curLine, TRUE, bnaFeatureType);

    poFeature = BuildFeatureFromBNARecord(record, nFID);

    BNA_FreeRecord(record);

    return poFeature;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRBNALayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCSequentialWrite) )
        return bWriter;
    else if( EQUAL(pszCap,OLCCreateField) )
        return bWriter && nFeatures == 0;
    else
        return FALSE;
}

