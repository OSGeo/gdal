/******************************************************************************
 * $Id$
 *
 * Project:  NTF Translator
 * Purpose:  Handle NTF products that aren't recognised generically.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 * Revision 1.1  1999/10/01 14:45:42  warmerda
 * New
 *
 */

#include <stdarg.h>
#include "ntf.h"
#include "cpl_string.h"

#define MAX_LINK	200

/************************************************************************/
/* ==================================================================== */
/*			    NTFGenericClass				*/
/*									*/
/*	The NTFGenericClass class exists to hold aggregated 	        */
/*	information for each type of record encountered in a set of	*/
/*	NTF files, primarily the list of attributes actually		*/
/*	encountered.							*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           NTFGenericClass                            */
/************************************************************************/

NTFGenericClass::NTFGenericClass()
{
    nFeatureCount = 0;

    nAttrCount = 0;
    papszAttrNames = NULL;
    papszAttrFormats = NULL;
    panAttrMaxWidth = NULL;
}

/************************************************************************/
/*                           ~NTFGenericClass                           */
/************************************************************************/

NTFGenericClass::~NTFGenericClass()

{
    CSLDestroy( papszAttrNames );
    CSLDestroy( papszAttrFormats );
    CPLFree( panAttrMaxWidth );
}

/************************************************************************/
/*                            CheckAddAttr()                            */
/*                                                                      */
/*      Check if an attribute already exists.  If not add it with       */
/*      it's format.  Note we don't check for format conflicts at       */
/*      this time.                                                      */
/************************************************************************/

void NTFGenericClass::CheckAddAttr( const char * pszName,
                                    const char * pszFormat,
                                    int nWidth )

{
    int		iAttrOffset = CSLFindString( papszAttrNames, pszName );
    
    if( iAttrOffset == -1 )
    {
        papszAttrNames = CSLAddString( papszAttrNames, pszName );
        papszAttrFormats = CSLAddString( papszAttrFormats, pszFormat );

        panAttrMaxWidth = (int *)
            CPLRealloc( panAttrMaxWidth, sizeof(int) * (++nAttrCount) );

        panAttrMaxWidth[nAttrCount-1] = nWidth;
    }
    else
    {
        if( panAttrMaxWidth[iAttrOffset] < nWidth )
            panAttrMaxWidth[iAttrOffset] = nWidth;
    }
}

/************************************************************************/
/*                           WorkupGeneric()                            */
/*                                                                      */
/*      Scan a whole file, in order to build up a list of attributes    */
/*      for the generic types.                                          */
/************************************************************************/

void OGRNTFDataSource::WorkupGeneric( NTFFileReader * poReader )

{
    NTFRecord	**papoGroup = NULL;

    if( poReader->GetNTFLevel() > 2 )
        poReader->IndexFile();
    else
        poReader->Reset();

/* ==================================================================== */
/*      Read all record groups in the file.                             */
/* ==================================================================== */
    while( TRUE )
    {
/* -------------------------------------------------------------------- */
/*      Read a record group                                             */
/* -------------------------------------------------------------------- */
        if( poReader->GetNTFLevel() > 2 )
            papoGroup = poReader->GetNextIndexedRecordGroup(papoGroup);
        else
            papoGroup = poReader->ReadRecordGroup();

        if( papoGroup == NULL || papoGroup[0]->GetType() == 99 )
            break;
        
/* -------------------------------------------------------------------- */
/*      Get the class corresponding to the anchor record.               */
/* -------------------------------------------------------------------- */
        NTFGenericClass	*poClass = GetGClass( papoGroup[0]->GetType() );

        poClass->nFeatureCount++;

        if( poClass->nFeatureCount == 1 )
        {
            printf( "Group: " );
            for( int i = 0; papoGroup[i] != NULL; i++ )
                printf( "%d ", papoGroup[i]->GetType() );
            printf( "\n" );
        }

/* -------------------------------------------------------------------- */
/*      Loop over constituent records collecting attributes.            */
/* -------------------------------------------------------------------- */
        for( int iRec = 0; papoGroup[iRec] != NULL; iRec++ )
        {
            NTFRecord	*poRecord = papoGroup[iRec];

            switch( poRecord->GetType() )
            {
              case NRT_ATTREC:
              {
                  char	**papszTypes, **papszValues;

                  poReader->ProcessAttRec( poRecord, NULL,
                                           &papszTypes, &papszValues );

                  for( int iAtt = 0; papszTypes[iAtt] != NULL; iAtt++ )
                  {
                      NTFAttDesc	*poAttDesc;

                      poAttDesc = poReader->GetAttDesc( papszTypes[iAtt] );
                      if( poAttDesc != NULL )
                      {
                          poClass->CheckAddAttr( poAttDesc->val_type,
                                                 poAttDesc->finter,
                                                 strlen(papszValues[iAtt]) );
                      }
                  }

                  CSLDestroy( papszTypes );
                  CSLDestroy( papszValues );
              }
              break;

              case NRT_TEXTREP:
                poClass->CheckAddAttr( "FONT", "I4", 4 );
                poClass->CheckAddAttr( "TEXT_HT", "R3,1", 3 );
                poClass->CheckAddAttr( "DIG_POSTN", "I1", 1 );
                poClass->CheckAddAttr( "ORIENT", "R4,1", 4 );
                break;

              case NRT_GEOMETRY:
              case NRT_GEOMETRY3D:
                  if( atoi(poRecord->GetField(3,8)) != 0 )
                      poClass->CheckAddAttr( "GEOM_ID", "I6", 6 );
                  break;

              case NRT_POINTREC:
              case NRT_LINEREC:
                if( poReader->GetNTFLevel() < 3 )
                {
                    NTFAttDesc	*poAttDesc;
                      
                    poAttDesc = poReader->GetAttDesc(poRecord->GetField(9,10));
                    if( poAttDesc != NULL )
                        poClass->CheckAddAttr( poAttDesc->val_type,
                                               poAttDesc->finter, 6 );

                    if( !EQUAL(poRecord->GetField(17,20),"    ") )
                        poClass->CheckAddAttr( "FEAT_CODE", "A4", 4 );
                }
                break;
                
              default:
                break;
            }
        }
    }

    poReader->Reset();
}

/************************************************************************/
/*                        AddGenericAttributes()                        */
/************************************************************************/

static void AddGenericAttributes( NTFFileReader * poReader,
                                  NTFRecord **papoGroup,
                                  OGRFeature * poFeature )

{
    char	**papszTypes, **papszValues;

    if( !poReader->ProcessAttRecGroup( papoGroup, &papszTypes, &papszValues ) )
        return;

    for( int iAtt = 0; papszTypes != NULL && papszTypes[iAtt] != NULL; iAtt++ )
    {
        int		iField = poFeature->GetFieldIndex(papszTypes[iAtt]);

        if( iField == -1 )
            continue;

        poReader->ApplyAttributeValue( poFeature, iField, papszTypes[iAtt],
                                       papszTypes, papszValues );
    }

    CSLDestroy( papszTypes );
    CSLDestroy( papszValues );
}

/************************************************************************/
/*                        TranslateGenericNode()                        */
/************************************************************************/

static OGRFeature *TranslateGenericNode( NTFFileReader *poReader,
                                         OGRNTFLayer *poLayer,
                                         NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) < 2
        || papoGroup[0]->GetType() != NRT_NODEREC
        || papoGroup[1]->GetType() != NRT_GEOMETRY )
        return NULL;
        
    OGRFeature	*poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // NODE_ID
    poFeature->SetField( "NODE_ID", atoi(papoGroup[0]->GetField( 3, 8 )) );

    // Geometry
    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1]));
    poFeature->SetField( "GEOM_ID", papoGroup[1]->GetField(3,8) );

    // NUM_LINKS
    int 	nLinkCount=0;
    int		*panLinks = NULL;

    if( papoGroup[0]->GetLength() > 18 )
    {
        nLinkCount = atoi(papoGroup[0]->GetField(15,18));
        panLinks = (int *) CPLCalloc(sizeof(int),nLinkCount);
    }

    poFeature->SetField( "NUM_LINKS", nLinkCount );

    // GEOM_ID_OF_LINK
    for( int iLink = 0; iLink < nLinkCount; iLink++ )
        panLinks[iLink] = atoi(papoGroup[0]->GetField(20+iLink*12,
                                                      25+iLink*12));

    poFeature->SetField( "GEOM_ID_OF_LINK", nLinkCount, panLinks );

    // DIR
    for( int iLink = 0; iLink < nLinkCount; iLink++ )
        panLinks[iLink] = atoi(papoGroup[0]->GetField(19+iLink*12,
                                                      19+iLink*12));

    poFeature->SetField( "DIR", nLinkCount, panLinks );

    // should we add LEVEL and/or ORIENT?

    CPLFree( panLinks );

    return poFeature;
}

/************************************************************************/
/*                        TranslateGenericText()                        */
/************************************************************************/

static OGRFeature *TranslateGenericText( NTFFileReader *poReader,
                                         OGRNTFLayer *poLayer,
                                         NTFRecord **papoGroup )

{
    int		iRec;
    
    if( CSLCount((char **) papoGroup) < 2
        || papoGroup[0]->GetType() != NRT_TEXTREC )
        return NULL;
        
    OGRFeature	*poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // TEXT_ID
    poFeature->SetField( "TEXT_ID", atoi(papoGroup[0]->GetField( 3, 8 )) );

    // Geometry
    for( iRec = 0; papoGroup[iRec] != NULL; iRec++ )
    {
        if( papoGroup[iRec]->GetType() == NRT_GEOMETRY
            || papoGroup[iRec]->GetType() == NRT_GEOMETRY3D )
        {
            poFeature->SetGeometryDirectly(
                poReader->ProcessGeometry(papoGroup[iRec]));
            poFeature->SetField( "GEOM_ID", papoGroup[iRec]->GetField(3,8) );
            break;
        }
    }

    // ATTREC Attributes
    AddGenericAttributes( poReader, papoGroup, poFeature );

    // TEXTREP information
    for( iRec = 0; papoGroup[iRec] != NULL; iRec++ )
    {
        NTFRecord	*poRecord = papoGroup[iRec];
        
        if( poRecord->GetType() == NRT_TEXTREP )
        {
            poFeature->SetField( "FONT", atoi(poRecord->GetField(9,12)) );
            poFeature->SetField( "TEXT_HT",
                                 atoi(poRecord->GetField(13,15)) * 0.1 );
            poFeature->SetField( "DIG_POSTN",
                                 atoi(poRecord->GetField(16,16)) );
            poFeature->SetField( "ORIENT",
                                 atoi(poRecord->GetField(17,20)) * 0.1 );
            break;
        }
    }

    return poFeature;
}

/************************************************************************/
/*                       TranslateGenericPoint()                        */
/************************************************************************/

static OGRFeature *TranslateGenericPoint( NTFFileReader *poReader,
                                          OGRNTFLayer *poLayer,
                                          NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) < 2
        || papoGroup[0]->GetType() != NRT_POINTREC
        || papoGroup[1]->GetType() != NRT_GEOMETRY )
        return NULL;
        
    OGRFeature	*poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // POINT_ID
    poFeature->SetField( "POINT_ID", atoi(papoGroup[0]->GetField( 3, 8 )) );

    // Geometry
    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1]));
    poFeature->SetField( "GEOM_ID", papoGroup[1]->GetField(3,8) );

    // ATTREC Attributes
    AddGenericAttributes( poReader, papoGroup, poFeature );

    // Handle singular attribute in pre-level 3 POINTREC.
    if( poReader->GetNTFLevel() < 3 )
    {
        char	szValType[3];

        strcpy( szValType, papoGroup[0]->GetField(9,10) );
        if( !EQUAL(szValType,"  ") )
        {
            char	*pszProcessedValue;

            if( poReader->ProcessAttValue(szValType,
                                          papoGroup[0]->GetField(11,16),
                                          NULL, &pszProcessedValue ) )
                poFeature->SetField(szValType, pszProcessedValue);
        }

        if( !EQUAL(papoGroup[0]->GetField(17,20),"    ") )
        {
            poFeature->SetField("FEAT_CODE",papoGroup[0]->GetField(17,20));
        }
    }

    return poFeature;
}

/************************************************************************/
/*                        TranslateGenericLine()                        */
/************************************************************************/

static OGRFeature *TranslateGenericLine( NTFFileReader *poReader,
                                         OGRNTFLayer *poLayer,
                                         NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) < 2
        || papoGroup[0]->GetType() != NRT_LINEREC
        || papoGroup[1]->GetType() != NRT_GEOMETRY )
        return NULL;
        
    OGRFeature	*poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // LINE_ID
    poFeature->SetField( "LINE_ID", atoi(papoGroup[0]->GetField( 3, 8 )) );

    // Geometry
    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1]));
    poFeature->SetField( "GEOM_ID", papoGroup[1]->GetField(3,8) );

    // ATTREC Attributes
    AddGenericAttributes( poReader, papoGroup, poFeature );

    // Handle singular attribute in pre-level 3 LINEREC.
    if( poReader->GetNTFLevel() < 3 )
    {
        char	szValType[3];

        strcpy( szValType, papoGroup[0]->GetField(9,10) );
        if( !EQUAL(szValType,"  ") )
        {
            char	*pszProcessedValue;

            if( poReader->ProcessAttValue(szValType,
                                          papoGroup[0]->GetField(11,16),
                                          NULL, &pszProcessedValue ) )
                poFeature->SetField(szValType, pszProcessedValue);
        }

        if( !EQUAL(papoGroup[0]->GetField(17,20),"    ") )
        {
            poFeature->SetField("FEAT_CODE",papoGroup[0]->GetField(17,20));
        }
    }

    return poFeature;
}

/************************************************************************/
/*                        TranslateGenericPoly()                        */
/************************************************************************/

static OGRFeature *TranslateGenericPoly( NTFFileReader *poReader,
                                         OGRNTFLayer *poLayer,
                                         NTFRecord **papoGroup )

{
/* ==================================================================== */
/*      Traditional POLYGON record groups.                              */
/* ==================================================================== */
    if( CSLCount((char **) papoGroup) >= 2 
        && papoGroup[0]->GetType() == NRT_POLYGON
        && papoGroup[1]->GetType() == NRT_CHAIN )
    {
        OGRFeature	*poFeature = new OGRFeature( poLayer->GetLayerDefn() );

        // POLY_ID
        poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

        // NUM_PARTS
        int		nNumLinks = atoi(papoGroup[1]->GetField( 9, 12 ));
    
        if( nNumLinks > MAX_LINK )
            return poFeature;
    
        poFeature->SetField( "NUM_PARTS", nNumLinks );

        // DIR
        int		i, anList[MAX_LINK];

        for( i = 0; i < nNumLinks; i++ )
            anList[i] = atoi(papoGroup[1]->GetField( 19+i*7, 19+i*7 ));

        poFeature->SetField( "DIR", nNumLinks, anList );

        // GEOM_ID_OF_LINK
        for( i = 0; i < nNumLinks; i++ )
            anList[i] = atoi(papoGroup[1]->GetField( 13+i*7, 18+i*7 ));

        poFeature->SetField( "GEOM_ID_OF_LINK", nNumLinks, anList );

        // RingStart
        int	nRingList = 0;
        poFeature->SetField( "RingStart", 1, &nRingList );

        // ATTREC Attributes
        AddGenericAttributes( poReader, papoGroup, poFeature );

        // Read point geometry
        if( papoGroup[2]->GetType() == NRT_GEOMETRY
            || papoGroup[2]->GetType() == NRT_GEOMETRY3D )
        {
            poFeature->SetGeometryDirectly(
                poReader->ProcessGeometry(papoGroup[2]));
            poFeature->SetField( "GEOM_ID", papoGroup[2]->GetField(3,8) );
        }

        return poFeature;
    }
#ifdef notdef
/* ==================================================================== */
/*      CPOLYGON Group                                                  */
/* ==================================================================== */

/* -------------------------------------------------------------------- */
/*      First we do validation of the grouping.                         */
/* -------------------------------------------------------------------- */
    int		iRec;
    
    for( iRec = 0;
         papoGroup[iRec] != NULL && papoGroup[iRec+1] != NULL
             && papoGroup[iRec]->GetType() == NRT_POLYGON
             && papoGroup[iRec+1]->GetType() == NRT_CHAIN;
         iRec += 2 ) {}

    if( CSLCount((char **) papoGroup) != iRec + 3 )
        return NULL;

    if( papoGroup[iRec]->GetType() != NRT_CPOLY
        || papoGroup[iRec+1]->GetType() != NRT_ATTREC
        || papoGroup[iRec+2]->GetType() != NRT_GEOMETRY )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Collect the chains for each of the rings, and just aggregate    */
/*      these into the master list without any concept of where the     */
/*      boundaries are.  The boundary information will be emmitted      */
/*	in the RingStart field.						*/
/* -------------------------------------------------------------------- */
    OGRFeature	*poFeature = new OGRFeature( poLayer->GetLayerDefn() );
    int		nNumLink = 0;
    int		anDirList[MAX_LINK*2], anGeomList[MAX_LINK*2];
    int		anRingStart[MAX_LINK], nRings = 0;

    for( iRec = 0;
         papoGroup[iRec] != NULL && papoGroup[iRec+1] != NULL
             && papoGroup[iRec]->GetType() == NRT_POLYGON
             && papoGroup[iRec+1]->GetType() == NRT_CHAIN;
         iRec += 2 )
    {
        int		i, nLineCount;

        nLineCount = atoi(papoGroup[iRec+1]->GetField(9,12));

        anRingStart[nRings++] = nNumLink;
        
        for( i = 0; i < nLineCount && nNumLink < MAX_LINK*2; i++ )
        {
            anDirList[nNumLink] =
                atoi(papoGroup[iRec+1]->GetField( 19+i*7, 19+i*7 ));
            anGeomList[nNumLink] =
                atoi(papoGroup[iRec+1]->GetField( 13+i*7, 18+i*7 ));
            nNumLink++;
        }

        if( nNumLink == MAX_LINK*2 )
        {
            delete poFeature;
            return NULL;
        }
    }

    // NUM_PART
    poFeature->SetField( 4, nNumLink );

    // DIR
    poFeature->SetField( 5, nNumLink, anDirList );

    // GEOM_ID_OF_LINK
    poFeature->SetField( 6, nNumLink, anGeomList );

    // RingStart
    poFeature->SetField( 7, nRings, anRingStart );

    
/* -------------------------------------------------------------------- */
/*	collect information for whole complex polygon.			*/
/* -------------------------------------------------------------------- */
    // POLY_ID
    poFeature->SetField( 0, atoi(papoGroup[iRec]->GetField( 3, 8 )) );

    // Attributes
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "FC", 1, "PI", 2, "HA", 3,
                                    NULL );

    // point geometry for seed.
    poFeature->SetGeometryDirectly(
        poReader->ProcessGeometry(papoGroup[iRec+2]));

    return poFeature;
#endif
    return NULL;
}

/************************************************************************/
/*                       EstablishGenericLayers()                       */
/************************************************************************/

void OGRNTFDataSource::EstablishGenericLayers()

{
    int		iType;
    
    for( iType = 0; iType < 100; iType++ )
    {
        NTFGenericClass	*poClass = aoGenericClass + iType;
        
        if( poClass->nFeatureCount == 0 )
            continue;

        printf( "\n" );
        printf( "Feature Type = %d\n", iType );
        printf( "Feature Count = %d\n", poClass->nFeatureCount );

        for( int iAttr = 0; iAttr < poClass->nAttrCount; iAttr++ )
        {
            printf( "  Name=%s Format=%s MaxWidth=%d\n",
                    poClass->papszAttrNames[iAttr],
                    poClass->papszAttrFormats[iAttr],
                    poClass->panAttrMaxWidth[iAttr] );
        }
    }

/* -------------------------------------------------------------------- */
/*      Pick an initial NTFFileReader to build the layers against.      */
/* -------------------------------------------------------------------- */
    NTFFileReader	*poPReader = NULL;
    for( int iFile = 0; poPReader == NULL && iFile < nNTFFileCount; iFile++ )
    {
        if( papoNTFFileReader[iFile]->GetProductId() == NPC_UNKNOWN )
            poPReader = papoNTFFileReader[iFile];
    }

    if( poPReader == NULL )
        return;
    
/* -------------------------------------------------------------------- */
/*      Create layers for all recognised layer types with features.     */
/* -------------------------------------------------------------------- */
    for( iType = 0; iType < 99; iType++ )
    {
        NTFGenericClass	*poClass = aoGenericClass + iType;
        
        if( poClass->nFeatureCount == 0 )
            continue;

        if( iType == NRT_POINTREC )
        {
            poPReader->
            EstablishLayer( "GENERIC_POINT", wkbPoint,
                            TranslateGenericPoint, NRT_POINTREC, poClass,
                            "POINT_ID", OFTInteger, 6, 0,
                            NULL );
        }
        else if( iType == NRT_LINEREC )
        {
            poPReader->
            EstablishLayer( "GENERIC_LINE", wkbLineString,
                            TranslateGenericLine, NRT_LINEREC, poClass,
                            "LINE_ID", OFTInteger, 6, 0,
                            NULL );
        }
        else if( iType == NRT_TEXTREC )
        {
            poPReader->
            EstablishLayer( "GENERIC_TEXT", wkbPoint,
                            TranslateGenericText, NRT_TEXTREC, poClass,
                            "TEXT_ID", OFTInteger, 6, 0,
                            NULL );
        }
        else if( iType == NRT_NODEREC )
        {
            poPReader->
            EstablishLayer( "GENERIC_NODE", wkbPoint,
                            TranslateGenericNode, NRT_NODEREC, poClass,
                            "NODE_ID", OFTInteger, 6, 0,
                            "NUM_LINKS", OFTInteger, 4, 0,
                            "GEOM_ID_OF_LINK", OFTIntegerList, 6, 0,
                            "DIR", OFTIntegerList, 1, 0,
                            NULL );
        }
        else if( iType == NRT_POLYGON )
        {
            poPReader->
            EstablishLayer( "GENERIC_POLY", wkbPoint,
                            TranslateGenericPoly, NRT_POLYGON, poClass,
                            "POLY_ID", OFTInteger, 6, 0,
                            "NUM_PARTS", OFTInteger, 4, 0, 
                            "DIR", OFTIntegerList, 1, 0,
                            "GEOM_ID_OF_LINK", OFTIntegerList, 6, 0,
                            "RingStart", OFTIntegerList, 6, 0,
                            NULL );
        }
    }
}

