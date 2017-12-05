/******************************************************************************
 *
 * Project:  NTF Translator
 * Purpose:  NTFFileReader methods related to establishing the schemas
 *           of features that could occur in this product and the functions
 *           for actually performing the NTFRecord to OGRFeature conversion.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 ****************************************************************************/

#include <stdarg.h>
#include "ntf.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")

static const int MAX_LINK = 5000;

/************************************************************************/
/*                         TranslateCodePoint()                         */
/*                                                                      */
/*      Used for code point, and code point plus.                       */
/************************************************************************/

static OGRFeature *TranslateCodePoint( NTFFileReader *poReader,
                                       OGRNTFLayer *poLayer,
                                       NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) < 2
        || papoGroup[0]->GetType() != NRT_POINTREC
        || papoGroup[1]->GetType() != NRT_GEOMETRY )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // POINT_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // Geometry
    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1]));

    // Attributes
    if( EQUAL(poLayer->GetLayerDefn()->GetName(),"CODE_POINT") )
        poReader->ApplyAttributeValues( poFeature, papoGroup,
                                        "PC", 1, "PQ", 2, "PR", 3, "TP", 4,
                                        "DQ", 5, "RP", 6, "BP", 7, "PD", 8,
                                        "MP", 9, "UM", 10, "RV", 11,
                                        NULL );
    else
        poReader->ApplyAttributeValues( poFeature, papoGroup,
                                        "PC", 1, "PQ", 2, "PR", 3, "TP", 4,
                                        "DQ", 5, "RP", 6, "BP", 7, "PD", 8,
                                        "MP", 9, "UM", 10, "RV", 11,
                                        "RH", 12, "LH", 13, "CC", 14,
                                        "DC", 15, "WC", 16,
                                        NULL );

    return poFeature;
}

/************************************************************************/
/*                       TranslateAddressPoint()                        */
/************************************************************************/

static OGRFeature *TranslateAddressPoint( NTFFileReader *poReader,
                                          OGRNTFLayer *poLayer,
                                          NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) < 2
        || papoGroup[0]->GetType() != NRT_POINTREC
        || papoGroup[1]->GetType() != NRT_GEOMETRY )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // POINT_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // CHG_TYPE
    poFeature->SetField( 17, papoGroup[0]->GetField( 22, 22 ) );

    // CHG_DATE
    poFeature->SetField( 18, papoGroup[0]->GetField( 23, 28 ) );

    // Geometry
    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1]));

    // Attributes
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "OA", 1, "ON", 2, "DP", 3, "PB", 4,
                                    "SB", 5, "BD", 6, "BN", 7, "DR", 8,
                                    "TN", 9, "DD", 10, "DL", 11, "PT", 12,
                                    "CN", 13, "PC", 14, "SF", 15, "RV", 16,
                                    NULL );

    return poFeature;
}

/************************************************************************/
/*                        TranslateOscarPoint()                         */
/*                                                                      */
/*      Used for OSCAR Traffic and Asset datasets.                      */
/************************************************************************/

static OGRFeature *TranslateOscarPoint( NTFFileReader *poReader,
                                        OGRNTFLayer *poLayer,
                                        NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) < 2
        || papoGroup[0]->GetType() != NRT_POINTREC
        || papoGroup[1]->GetType() != NRT_GEOMETRY )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // POINT_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // Geometry
    int nGeomId = 0;

    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1],
                                                             &nGeomId));

    poFeature->SetField( 1, nGeomId );

    // Attributes
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "FC", 2, "OD", 3, "JN", 4, "SN", 5,
                                    NULL );

    return poFeature;
}

/************************************************************************/
/*                         TranslateOscarLine()                         */
/************************************************************************/

static OGRFeature *TranslateOscarLine( NTFFileReader *poReader,
                                       OGRNTFLayer *poLayer,
                                       NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) < 2
        || papoGroup[0]->GetType() != NRT_LINEREC
        || papoGroup[1]->GetType() != NRT_GEOMETRY )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // LINE_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // Geometry
    int nGeomId = 0;

    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1],
                                                             &nGeomId));

    poFeature->SetField( 1, nGeomId );

    // Attributes
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "FC", 2, "OD", 3, "PN", 4, "LL", 5,
                                    "SC", 6, "FW", 7, "RN", 8, "TR", 9,
                                    NULL );

    return poFeature;
}

/************************************************************************/
/*                      TranslateOscarRoutePoint()                      */
/************************************************************************/

static OGRFeature *TranslateOscarRoutePoint( NTFFileReader *poReader,
                                             OGRNTFLayer *poLayer,
                                             NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) < 2
        || papoGroup[0]->GetType() != NRT_POINTREC
        || papoGroup[1]->GetType() != NRT_GEOMETRY )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // POINT_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // Geometry
    int nGeomId = 0;

    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1],
                                                             &nGeomId));

    poFeature->SetField( 1, nGeomId );

    // Attributes
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "FC", 2, "OD", 3, "JN", 4, "SN", 5,
                                    "NP", 6, "RT", 8,
                                    NULL );

    // PARENT_OSODR
    char        **papszTypes, **papszValues;

    if( poReader->ProcessAttRecGroup( papoGroup, &papszTypes, &papszValues ) )
    {
        char    **papszOSODRList = NULL;

        for( int i = 0; papszTypes != NULL && papszTypes[i] != NULL; i++ )
        {
            if( EQUAL(papszTypes[i],"PO") )
                papszOSODRList = CSLAddString(papszOSODRList,papszValues[i]);
        }

        poFeature->SetField( 7, papszOSODRList );
        CPLAssert( CSLCount(papszOSODRList) ==
                   poFeature->GetFieldAsInteger( 6 ) );

        CSLDestroy( papszOSODRList );
        CSLDestroy( papszTypes );
        CSLDestroy( papszValues );
    }

    return poFeature;
}

/************************************************************************/
/*                      TranslateOscarRouteLine()                       */
/************************************************************************/

static OGRFeature *TranslateOscarRouteLine( NTFFileReader *poReader,
                                            OGRNTFLayer *poLayer,
                                            NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) < 2
        || papoGroup[0]->GetType() != NRT_LINEREC
        || papoGroup[1]->GetType() != NRT_GEOMETRY )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // LINE_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // Geometry
    int nGeomId = 0;

    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1],
                                                             &nGeomId));

    poFeature->SetField( 1, nGeomId );

    // Attributes
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "FC", 2, "OD", 3, "PN", 4, "LL", 5,
                                    "RN", 6, "TR", 7, "NP", 8,
                                    NULL );

    // PARENT_OSODR
    char        **papszTypes, **papszValues;

    if( poReader->ProcessAttRecGroup( papoGroup, &papszTypes, &papszValues ) )
    {
        char    **papszOSODRList = NULL;

        for( int i = 0; papszTypes != NULL && papszTypes[i] != NULL; i++ )
        {
            if( EQUAL(papszTypes[i],"PO") )
                papszOSODRList = CSLAddString(papszOSODRList,papszValues[i]);
        }

        poFeature->SetField( 9, papszOSODRList );
        CPLAssert( CSLCount(papszOSODRList) ==
                   poFeature->GetFieldAsInteger( 8 ) );

        CSLDestroy( papszOSODRList );
        CSLDestroy( papszTypes );
        CSLDestroy( papszValues );
    }

    return poFeature;
}

/************************************************************************/
/*                       TranslateOscarComment()                        */
/************************************************************************/

static OGRFeature *TranslateOscarComment( CPL_UNUSED NTFFileReader *poReader,
                                          OGRNTFLayer *poLayer,
                                          NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) != 1
        || papoGroup[0]->GetType() != NRT_COMMENT )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // RECORD_TYPE
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 4 )) );

    // RECORD_ID
    poFeature->SetField( 1, papoGroup[0]->GetField( 5, 17 ) );

    // CHANGE_TYPE
    poFeature->SetField( 2, papoGroup[0]->GetField( 18, 18 ) );

    return poFeature;
}

/************************************************************************/
/*                     TranslateOscarNetworkPoint()                     */
/************************************************************************/

static OGRFeature *TranslateOscarNetworkPoint( NTFFileReader *poReader,
                                               OGRNTFLayer *poLayer,
                                               NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) < 2
        || papoGroup[0]->GetType() != NRT_POINTREC
        || papoGroup[1]->GetType() != NRT_GEOMETRY )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // POINT_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // Geometry
    int nGeomId = 0;

    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1],
                                                             &nGeomId));

    poFeature->SetField( 1, nGeomId );

    // Attributes
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "FC", 2, "OD", 3, "JN", 4, "SN", 5,
                                    "RT", 6,
                                    NULL );

    return poFeature;
}

/************************************************************************/
/*                      TranslateOscarNetworkLine()                     */
/************************************************************************/

static OGRFeature *TranslateOscarNetworkLine( NTFFileReader *poReader,
                                              OGRNTFLayer *poLayer,
                                              NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) < 2
        || papoGroup[0]->GetType() != NRT_LINEREC
        || papoGroup[1]->GetType() != NRT_GEOMETRY )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // LINE_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // Geometry
    int nGeomId = 0;

    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1],
                                                             &nGeomId));

    poFeature->SetField( 1, nGeomId );

    // Attributes
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "FC", 2, "OD", 3, "PN", 4, "LL", 5,
                                    "RN", 6,
                                    NULL );

    return poFeature;
}

/************************************************************************/
/*                       TranslateBasedataPoint()                       */
/************************************************************************/

static OGRFeature *TranslateBasedataPoint( NTFFileReader *poReader,
                                           OGRNTFLayer *poLayer,
                                           NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) < 2
        || papoGroup[0]->GetType() != NRT_POINTREC
        || papoGroup[1]->GetType() != NRT_GEOMETRY )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // POINT_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // Geometry
    int nGeomId = 0;

    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1],
                                                             &nGeomId));

    // GEOM_ID
    poFeature->SetField( 1, nGeomId );

    // Attributes
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "FC", 2, "PN", 3, "NU", 4, "CM", 5,
                                    "UN", 6, "OR", 7,
                                    NULL );

    return poFeature;
}

/************************************************************************/
/*                       TranslateBasedataLine()                        */
/************************************************************************/

static OGRFeature *TranslateBasedataLine( NTFFileReader *poReader,
                                          OGRNTFLayer *poLayer,
                                          NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) < 2
        || papoGroup[0]->GetType() != NRT_LINEREC
        || papoGroup[1]->GetType() != NRT_GEOMETRY )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // LINE_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // Geometry
    int nGeomId = 0;

    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1],
                                                             &nGeomId));

    // GEOM_ID
    poFeature->SetField( 2, nGeomId );

    // Attributes
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "FC", 1, "PN", 3, "NU", 4, "RB", 5,
                                    NULL );

    return poFeature;
}

/************************************************************************/
/*                  TranslateBoundarylineCollection()                   */
/************************************************************************/

static OGRFeature *TranslateBoundarylineCollection( NTFFileReader *poReader,
                                                    OGRNTFLayer *poLayer,
                                                    NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) != 2
        || papoGroup[0]->GetType() != NRT_COLLECT
        || papoGroup[1]->GetType() != NRT_ATTREC )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // COLL_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // NUM_PARTS
    int nNumLinks = atoi(papoGroup[0]->GetField( 9, 12 ));

    if( nNumLinks > MAX_LINK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "MAX_LINK exceeded in ntf_estlayers.cpp." );
        return poFeature;
    }

    poFeature->SetField( 1, nNumLinks );

    // POLY_ID
    int         i, anList[MAX_LINK];

    for( i = 0; i < nNumLinks; i++ )
        anList[i] = atoi(papoGroup[0]->GetField( 15+i*8, 20+i*8 ));

    poFeature->SetField( 2, nNumLinks, anList );

    // Attributes
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "AI", 3, "OP", 4, "NM", 5,
                                    NULL );

    return poFeature;
}

/************************************************************************/
/*                     TranslateBoundarylinePoly()                      */
/************************************************************************/

static OGRFeature *TranslateBoundarylinePoly( NTFFileReader *poReader,
                                              OGRNTFLayer *poLayer,
                                              NTFRecord **papoGroup )

{
/* ==================================================================== */
/*      Traditional POLYGON record groups.                              */
/* ==================================================================== */
    if( CSLCount((char **) papoGroup) == 4
        && papoGroup[0]->GetType() == NRT_POLYGON
        && papoGroup[1]->GetType() == NRT_ATTREC
        && papoGroup[2]->GetType() == NRT_CHAIN
        && papoGroup[3]->GetType() == NRT_GEOMETRY )
    {

        OGRFeature      *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

        // POLY_ID
        poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

        // NUM_PARTS
        int             nNumLinks = atoi(papoGroup[2]->GetField( 9, 12 ));

        if( nNumLinks > MAX_LINK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "MAX_LINK exceeded in ntf_estlayers.cpp." );
            return poFeature;
        }

        poFeature->SetField( 4, nNumLinks );

        // DIR
        int             i, anList[MAX_LINK];

        for( i = 0; i < nNumLinks; i++ )
            anList[i] = atoi(papoGroup[2]->GetField( 19+i*7, 19+i*7 ));

        poFeature->SetField( 5, nNumLinks, anList );

        // GEOM_ID_OF_LINK
        for( i = 0; i < nNumLinks; i++ )
            anList[i] = atoi(papoGroup[2]->GetField( 13+i*7, 18+i*7 ));

        poFeature->SetField( 6, nNumLinks, anList );

        // RingStart
        int     nRingList = 0;
        poFeature->SetField( 7, 1, &nRingList );

        // Attributes
        poReader->ApplyAttributeValues( poFeature, papoGroup,
                                        "FC", 1, "PI", 2, "HA", 3,
                                        NULL );

        // Read point geometry
        poFeature->SetGeometryDirectly(
            poReader->ProcessGeometry(papoGroup[3]));

        // Try to assemble polygon geometry.
        poReader->FormPolygonFromCache( poFeature );

        return poFeature;
    }

/* ==================================================================== */
/*      CPOLYGON Group                                                  */
/* ==================================================================== */

/* -------------------------------------------------------------------- */
/*      First we do validation of the grouping.                         */
/* -------------------------------------------------------------------- */
    int iRec = 0;  // Used after for.

    for( ;
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
/*      boundaries are.  The boundary information will be emitted      */
/*      in the RingStart field.                                         */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = new OGRFeature( poLayer->GetLayerDefn() );
    int nNumLink = 0;
    int anDirList[MAX_LINK*2] = {};
    int anGeomList[MAX_LINK*2] = {};
    int anRingStart[MAX_LINK] = {};
    int nRings = 0;

    for( iRec = 0;
         papoGroup[iRec] != NULL && papoGroup[iRec+1] != NULL
             && papoGroup[iRec]->GetType() == NRT_POLYGON
             && papoGroup[iRec+1]->GetType() == NRT_CHAIN;
         iRec += 2 )
    {
        const int nLineCount = atoi(papoGroup[iRec+1]->GetField(9,12));

        anRingStart[nRings++] = nNumLink;

        for( int i = 0; i < nLineCount && nNumLink < MAX_LINK*2; i++ )
        {
            anDirList[nNumLink] =
                atoi(papoGroup[iRec+1]->GetField( 19+i*7, 19+i*7 ));
            anGeomList[nNumLink] =
                atoi(papoGroup[iRec+1]->GetField( 13+i*7, 18+i*7 ));
            nNumLink++;
        }

        if( nNumLink == MAX_LINK*2 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "MAX_LINK exceeded in ntf_estlayers.cpp." );

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
/*      collect information for whole complex polygon.                  */
/* -------------------------------------------------------------------- */
    // POLY_ID
    if( papoGroup[iRec] != NULL )
        poFeature->SetField( 0, atoi(papoGroup[iRec]->GetField( 3, 8 )) );

    // Attributes
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "FC", 1, "PI", 2, "HA", 3,
                                    NULL );

    // point geometry for seed.
    poFeature->SetGeometryDirectly(
        poReader->ProcessGeometry(papoGroup[iRec+2]));

    // Try to assemble polygon geometry.
    poReader->FormPolygonFromCache( poFeature );

    return poFeature;
}

/************************************************************************/
/*                     TranslateBoundarylineLink()                      */
/************************************************************************/

static OGRFeature *TranslateBoundarylineLink( NTFFileReader *poReader,
                                              OGRNTFLayer *poLayer,
                                              NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) != 2
        || papoGroup[0]->GetType() != NRT_GEOMETRY
        || papoGroup[1]->GetType() != NRT_ATTREC )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // Geometry
    int nGeomId = 0;

    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[0],
                                                             &nGeomId));

    // GEOM_ID
    poFeature->SetField( 0, nGeomId );

    // Attributes
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "FC", 1, "LK", 2, "HW", 3,
                                    NULL );

    return poFeature;
}

/************************************************************************/
/*                        TranslateBL2000Poly()                         */
/************************************************************************/

static OGRFeature *TranslateBL2000Poly( NTFFileReader *poReader,
                                        OGRNTFLayer *poLayer,
                                        NTFRecord **papoGroup )

{
/* ==================================================================== */
/*      Traditional POLYGON record groups.                              */
/* ==================================================================== */
    if( CSLCount((char **) papoGroup) == 3
        && papoGroup[0]->GetType() == NRT_POLYGON
        && papoGroup[1]->GetType() == NRT_ATTREC
        && papoGroup[2]->GetType() == NRT_CHAIN  )
    {

        OGRFeature      *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

        // POLY_ID
        poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

        // NUM_PARTS
        int             nNumLinks = atoi(papoGroup[2]->GetField( 9, 12 ));

        if( nNumLinks > MAX_LINK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "MAX_LINK exceeded in ntf_estlayers.cpp." );

            return poFeature;
        }

        poFeature->SetField( 3, nNumLinks );

        // DIR
        int             i, anList[MAX_LINK];

        for( i = 0; i < nNumLinks; i++ )
            anList[i] = atoi(papoGroup[2]->GetField( 19+i*7, 19+i*7 ));

        poFeature->SetField( 4, nNumLinks, anList );

        // GEOM_ID_OF_LINK
        for( i = 0; i < nNumLinks; i++ )
            anList[i] = atoi(papoGroup[2]->GetField( 13+i*7, 18+i*7 ));

        poFeature->SetField( 5, nNumLinks, anList );

        // RingStart
        int     nRingList = 0;
        poFeature->SetField( 6, 1, &nRingList );

        // Attributes
        poReader->ApplyAttributeValues( poFeature, papoGroup,
                                        "PI", 1, "HA", 2,
                                        NULL );

        // Try to assemble polygon geometry.
        poReader->FormPolygonFromCache( poFeature );

        return poFeature;
    }

/* ==================================================================== */
/*      CPOLYGON Group                                                  */
/* ==================================================================== */

/* -------------------------------------------------------------------- */
/*      First we do validation of the grouping.                         */
/* -------------------------------------------------------------------- */
    int iRec = 0;  // Used after for.

    for( ;
         papoGroup[iRec] != NULL && papoGroup[iRec+1] != NULL
             && papoGroup[iRec]->GetType() == NRT_POLYGON
             && papoGroup[iRec+1]->GetType() == NRT_CHAIN;
         iRec += 2 ) {}

    if( CSLCount((char **) papoGroup) != iRec + 2 )
        return NULL;

    if( papoGroup[iRec]->GetType() != NRT_CPOLY
        || papoGroup[iRec+1]->GetType() != NRT_ATTREC )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Collect the chains for each of the rings, and just aggregate    */
/*      these into the master list without any concept of where the     */
/*      boundaries are.  The boundary information will be emitted      */
/*      in the RingStart field.                                         */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = new OGRFeature( poLayer->GetLayerDefn() );
    int nNumLink = 0;
    int anDirList[MAX_LINK*2] = {};
    int anGeomList[MAX_LINK*2] = {};
    int anRingStart[MAX_LINK] = {};
    int nRings = 0;

    for( iRec = 0;
         papoGroup[iRec] != NULL && papoGroup[iRec+1] != NULL
             && papoGroup[iRec]->GetType() == NRT_POLYGON
             && papoGroup[iRec+1]->GetType() == NRT_CHAIN;
         iRec += 2 )
    {
        const int nLineCount = atoi(papoGroup[iRec+1]->GetField(9,12));

        anRingStart[nRings++] = nNumLink;

        for( int i = 0; i < nLineCount && nNumLink < MAX_LINK*2; i++ )
        {
            anDirList[nNumLink] =
                atoi(papoGroup[iRec+1]->GetField( 19+i*7, 19+i*7 ));
            anGeomList[nNumLink] =
                atoi(papoGroup[iRec+1]->GetField( 13+i*7, 18+i*7 ));
            nNumLink++;
        }

        if( nNumLink == MAX_LINK*2 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "MAX_LINK exceeded in ntf_estlayers.cpp." );

            delete poFeature;
            return NULL;
        }
    }

    // NUM_PART
    poFeature->SetField( 3, nNumLink );

    // DIR
    poFeature->SetField( 4, nNumLink, anDirList );

    // GEOM_ID_OF_LINK
    poFeature->SetField( 5, nNumLink, anGeomList );

    // RingStart
    poFeature->SetField( 6, nRings, anRingStart );

/* -------------------------------------------------------------------- */
/*      collect information for whole complex polygon.                  */
/* -------------------------------------------------------------------- */
    // POLY_ID
    if( papoGroup[iRec] != NULL )
        poFeature->SetField( 0, atoi(papoGroup[iRec]->GetField( 3, 8 )) );

    // Attributes
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "PI", 1, "HA", 2,
                                    NULL );

    // Try to assemble polygon geometry.
    poReader->FormPolygonFromCache( poFeature );

    return poFeature;
}

/************************************************************************/
/*                        TranslateBL2000Link()                         */
/************************************************************************/

static OGRFeature *TranslateBL2000Link( NTFFileReader *poReader,
                                        OGRNTFLayer *poLayer,
                                        NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) != 3
        || papoGroup[0]->GetType() != NRT_LINEREC
        || papoGroup[1]->GetType() != NRT_GEOMETRY
        || papoGroup[2]->GetType() != NRT_ATTREC )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // LINE_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // Geometry
    int nGeomId = 0;

    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1],
                                                             &nGeomId));

    // GEOM_ID
    poFeature->SetField( 1, nGeomId );

    // Attributes
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "FC", 2, "LK", 3,
                                    NULL );

    return poFeature;
}

/************************************************************************/
/*                     TranslateBL2000Collection()                      */
/************************************************************************/

static OGRFeature *TranslateBL2000Collection( NTFFileReader *poReader,
                                              OGRNTFLayer *poLayer,
                                              NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) < 2
        || papoGroup[0]->GetType() != NRT_COLLECT
        || papoGroup[1]->GetType() != NRT_ATTREC )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // COLL_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // NUM_PARTS
    int         nNumLinks = atoi(papoGroup[0]->GetField( 9, 12 ));

    if( nNumLinks > MAX_LINK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "MAX_LINK exceeded in ntf_estlayers.cpp." );

        return poFeature;
    }

    poFeature->SetField( 1, nNumLinks );

    // POLY_ID / COLL_ID_REFS
    int         anList[MAX_LINK], anCollList[MAX_LINK];
    int         nPolys=0, nCollections=0;

    for( int i = 0; i < nNumLinks; i++ )
    {
        if( atoi(papoGroup[0]->GetField( 13+i*8, 14+i*8 )) == 34 )
            anCollList[nCollections++] =
                atoi(papoGroup[0]->GetField( 15+i*8, 20+i*8 ));
        else
            anList[nPolys++] =
                atoi(papoGroup[0]->GetField( 15+i*8, 20+i*8 ));
    }

    // coverity[uninit_use_in_call]
    poFeature->SetField( 2, nPolys, anList );
    // coverity[uninit_use_in_call]
    poFeature->SetField( 10, nCollections, anCollList );

    // Attributes
    // Node that _CODE_DESC values are automatically applied if
    // the target fields exist.
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "AI", 3, "OP", 4, "NM", 5, "TY", 6,
                                    "AC", 7, "NB", 8, "NA", 9,
                                    NULL );

    return poFeature;
}

/************************************************************************/
/*                      TranslateMeridianPoint()                        */
/************************************************************************/

static OGRFeature *TranslateMeridianPoint( NTFFileReader *poReader,
                                           OGRNTFLayer *poLayer,
                                           NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) < 2
        || papoGroup[0]->GetType() != NRT_POINTREC
        || papoGroup[1]->GetType() != NRT_GEOMETRY )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // POINT_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // Geometry
    int nGeomId = 0;

    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1],
                                                             &nGeomId));

    // GEOM_ID
    poFeature->SetField( 1, nGeomId );

    // Attributes
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "FC", 2, "PN", 3, "OS", 4, "JN", 5,
                                    "RT", 6, "SI", 7, "PI", 8, "NM", 9,
                                    "DA", 10,
                                    NULL );

    return poFeature;
}

/************************************************************************/
/*                       TranslateMeridianLine()                        */
/************************************************************************/

static OGRFeature *TranslateMeridianLine( NTFFileReader *poReader,
                                          OGRNTFLayer *poLayer,
                                          NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) < 2
        || papoGroup[0]->GetType() != NRT_LINEREC
        || papoGroup[1]->GetType() != NRT_GEOMETRY )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // LINE_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // Geometry
    int nGeomId = 0;

    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1],
                                                             &nGeomId));

    // GEOM_ID
    poFeature->SetField( 2, nGeomId );

    // Attributes
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "FC", 1, "OM", 3, "RN", 4, "TR", 5,
                                    "RI", 6, "LC", 7, "RC", 8, "LD", 9,
                                    "RD", 10,
                                    NULL );

    return poFeature;
}

/************************************************************************/
/*                      TranslateMeridian2Point()                       */
/************************************************************************/

static OGRFeature *TranslateMeridian2Point( NTFFileReader *poReader,
                                            OGRNTFLayer *poLayer,
                                            NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) < 2
        || papoGroup[0]->GetType() != NRT_POINTREC
        || papoGroup[1]->GetType() != NRT_GEOMETRY )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // POINT_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // Geometry
    int nGeomId = 0;

    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1],
                                                             &nGeomId));

    // GEOM_ID
    poFeature->SetField( 1, nGeomId );

    // Attributes
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "FC", 2, "PN", 3, "OD", 4, "PO", 5,
                                    "JN", 6, "RT", 7, "SN", 8, "SI", 9,
                                    "PI", 10, "NM", 11, "DA", 12,
                                    "WA", 13, "HT", 14, "FA", 15,
                                    NULL );

    return poFeature;
}

/************************************************************************/
/*                       TranslateMeridian2Line()                       */
/************************************************************************/

static OGRFeature *TranslateMeridian2Line( NTFFileReader *poReader,
                                           OGRNTFLayer *poLayer,
                                           NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) < 2
        || papoGroup[0]->GetType() != NRT_LINEREC
        || papoGroup[1]->GetType() != NRT_GEOMETRY )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // LINE_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // Geometry
    int nGeomId = 0;

    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1],
                                                             &nGeomId));

    // GEOM_ID
    poFeature->SetField( 2, nGeomId );

    // Attributes
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "FC", 1, "OD", 3, "PO", 4, "RN", 5,
                                    "TR", 6, "PN", 7, "RI", 8, "LC", 9,
                                    "RC", 10, "LD", 11, "RD", 12, "WI", 14,
                                    NULL );

    return poFeature;
}

/************************************************************************/
/*                       TranslateStrategiNode()                        */
/*                                                                      */
/*      Also used for Meridian, Oscar and BaseData.GB nodes.            */
/************************************************************************/

static OGRFeature *TranslateStrategiNode( CPL_UNUSED NTFFileReader *poReader,
                                          OGRNTFLayer *poLayer,
                                          NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) != 1
        || papoGroup[0]->GetType() != NRT_NODEREC )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // NODE_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // GEOM_ID_OF_POINT
    poFeature->SetField( 1, atoi(papoGroup[0]->GetField( 9, 14 )) );

    // NUM_LINKS
    int         nNumLinks = atoi(papoGroup[0]->GetField( 15, 18 ));

    if( nNumLinks > MAX_LINK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "MAX_LINK exceeded in ntf_estlayers.cpp." );

        return poFeature;
    }

    poFeature->SetField( 2, nNumLinks );

    // DIR
    int         i, anList[MAX_LINK];

    for( i = 0; i < nNumLinks; i++ )
        anList[i] = atoi(papoGroup[0]->GetField( 19+i*12, 19+i*12 ));

    poFeature->SetField( 3, nNumLinks, anList );

    // GEOM_ID_OF_POINT
    for( i = 0; i < nNumLinks; i++ )
        anList[i] = atoi(papoGroup[0]->GetField( 19+i*12+1, 19+i*12+6 ));

    poFeature->SetField( 4, nNumLinks, anList );

    // LEVEL
    for( i = 0; i < nNumLinks; i++ )
        anList[i] = atoi(papoGroup[0]->GetField( 19+i*12+11, 19+i*12+11 ));

    poFeature->SetField( 5, nNumLinks, anList );

    // ORIENT (optional)
    if( EQUAL(poFeature->GetDefnRef()->GetFieldDefn(6)->GetNameRef(),
              "ORIENT") )
    {
        double  adfList[MAX_LINK];

        for( i = 0; i < nNumLinks; i++ )
            adfList[i] =
                atoi(papoGroup[0]->GetField( 19+i*12+7, 19+i*12+10 )) * 0.1;

        poFeature->SetField( 6, nNumLinks, adfList );
    }

    return poFeature;
}

/************************************************************************/
/*                       TranslateStrategiText()                        */
/*                                                                      */
/*      Also used for Meridian, BaseData and Generic text.              */
/************************************************************************/

static OGRFeature *TranslateStrategiText( NTFFileReader *poReader,
                                          OGRNTFLayer *poLayer,
                                          NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) < 4
        || papoGroup[0]->GetType() != NRT_TEXTREC
        || papoGroup[1]->GetType() != NRT_TEXTPOS
        || papoGroup[2]->GetType() != NRT_TEXTREP
        || papoGroup[3]->GetType() != NRT_GEOMETRY )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // POINT_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // FONT
    poFeature->SetField( 2, atoi(papoGroup[2]->GetField( 9, 12 )) );

    // TEXT_HT
    poFeature->SetField( 3, atoi(papoGroup[2]->GetField( 13, 15 )) * 0.1 );

    // DIG_POSTN
    poFeature->SetField( 4, atoi(papoGroup[2]->GetField( 16, 16 )) );

    // ORIENT
    poFeature->SetField( 5, atoi(papoGroup[2]->GetField( 17, 20 )) * 0.1 );

    // TEXT_HT_GROUND
    poFeature->SetField( 7, poFeature->GetFieldAsDouble(3)
                         * poReader->GetPaperToGround() );

    // Geometry
    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[3]));

    // Attributes
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "FC", 1, "TX", 6, "DE", 8,
                                    NULL );

    return poFeature;
}

/************************************************************************/
/*                      TranslateStrategiPoint()                        */
/************************************************************************/

static OGRFeature *TranslateStrategiPoint( NTFFileReader *poReader,
                                           OGRNTFLayer *poLayer,
                                           NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) < 2
        || papoGroup[0]->GetType() != NRT_POINTREC
        || papoGroup[1]->GetType() != NRT_GEOMETRY )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // POINT_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // Geometry
    int nGeomId = 0;

    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1],
                                                             &nGeomId));

    // GEOM_ID
    poFeature->SetField( 10, nGeomId );

    // Attributes
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "FC",  1, "PN",  2, "NU",  3, "RB",  4,
                                    "RU",  5, "AN",  6, "AO",  7, "CM",  8,
                                    "UN",  9, "DE", 11, "DN", 12, "FM", 13,
                                    "GS", 14, "HI", 15, "HM", 16, "LO", 17,
                                    "OR", 18, "OW", 19, "RJ", 20, "RL", 21,
                                    "RM", 22, "RQ", 23, "RW", 24, "RZ", 25,
                                    "UE", 26,
                                    NULL );

    return poFeature;
}

/************************************************************************/
/*                       TranslateStrategiLine()                        */
/************************************************************************/

static OGRFeature *TranslateStrategiLine( NTFFileReader *poReader,
                                          OGRNTFLayer *poLayer,
                                          NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) < 2
        || papoGroup[0]->GetType() != NRT_LINEREC
        || papoGroup[1]->GetType() != NRT_GEOMETRY )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // LINE_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // Geometry
    int nGeomId = 0;

    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1],
                                                             &nGeomId));

    // GEOM_ID
    poFeature->SetField( 3, nGeomId );

    // Attributes
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "FC",   1, "PN",  2, "DE",  4, "FE",  5,
                                    "FF",   6, "FI",  7, "FM",  8, "FP",  9,
                                    "FR",  10, "FT", 11, "GS", 12, "NU", 13,
                                    "TX", 14,
                                    NULL );

    return poFeature;
}

/************************************************************************/
/*                      TranslateLandrangerPoint()                      */
/************************************************************************/

static OGRFeature *TranslateLandrangerPoint( NTFFileReader *poReader,
                                             OGRNTFLayer *poLayer,
                                             NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) != 2
        || papoGroup[0]->GetType() != NRT_POINTREC
        || papoGroup[1]->GetType() != NRT_GEOMETRY )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // POINT_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // FEAT_CODE
    poFeature->SetField( 1, papoGroup[0]->GetField( 17, 20 ) );

    // HEIGHT
    poFeature->SetField( 2, atoi(papoGroup[0]->GetField( 11, 16 )) );

    // Geometry
    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1]));

    return poFeature;
}

/************************************************************************/
/*                      TranslateLandrangerLine()                       */
/************************************************************************/

static OGRFeature *TranslateLandrangerLine( NTFFileReader *poReader,
                                            OGRNTFLayer *poLayer,
                                            NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) != 2
        || papoGroup[0]->GetType() != NRT_LINEREC
        || papoGroup[1]->GetType() != NRT_GEOMETRY )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // LINE_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // FEAT_CODE
    poFeature->SetField( 1, papoGroup[0]->GetField( 17, 20 ) );

    // HEIGHT
    poFeature->SetField( 2, atoi(papoGroup[0]->GetField( 11, 16 )) );

    // Geometry
    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1]));

    return poFeature;
}

/************************************************************************/
/*                       TranslateProfilePoint()                        */
/************************************************************************/

static OGRFeature *TranslateProfilePoint( NTFFileReader *poReader,
                                          OGRNTFLayer *poLayer,
                                          NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) < 2
        || papoGroup[0]->GetType() != NRT_POINTREC
        || (papoGroup[1]->GetType() != NRT_GEOMETRY
            && papoGroup[1]->GetType() != NRT_GEOMETRY3D) )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // POINT_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // FEAT_CODE
    poFeature->SetField( 1, papoGroup[0]->GetField( 17, 20 ) );

    // Geometry
    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1]));

    // Attributes
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "HT", 2,
                                    NULL );

    // Set HEIGHT/elevation
    OGRPoint    *poPoint = dynamic_cast<OGRPoint *>(poFeature->GetGeometryRef());

    if( poPoint != NULL && poPoint->getCoordinateDimension() == 3 )
    {
        poFeature->SetField( 2, poPoint->getZ() );
    }
    else if( poPoint != NULL )
    {
        poFeature->SetField( 2, poFeature->GetFieldAsDouble(2) * 0.01 );
        poPoint->setZ( poFeature->GetFieldAsDouble(2) );
    }

    return poFeature;
}

/************************************************************************/
/*                      TranslateProfileLine()                          */
/************************************************************************/

static OGRFeature *TranslateProfileLine( NTFFileReader *poReader,
                                         OGRNTFLayer *poLayer,
                                         NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) < 2
        || papoGroup[0]->GetType() != NRT_LINEREC
        || (papoGroup[1]->GetType() != NRT_GEOMETRY
            && papoGroup[1]->GetType() != NRT_GEOMETRY3D) )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // LINE_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // FEAT_CODE
    poFeature->SetField( 1, papoGroup[0]->GetField( 17, 20 ) );

    // Geometry
    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1]));

    // Attributes
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "HT", 2,
                                    NULL );

    // Set HEIGHT/elevation
    OGRLineString *poLine = dynamic_cast<OGRLineString*>(poFeature->GetGeometryRef());

    poFeature->SetField( 2, poFeature->GetFieldAsDouble(2) * 0.01 );
    if( poLine != NULL && poLine->getCoordinateDimension() == 2 )
    {
        for( int i = 0; i < poLine->getNumPoints(); i++ )
        {
            poLine->setPoint( i, poLine->getX(i), poLine->getY(i),
                              poFeature->GetFieldAsDouble(2) );
        }
    }
    else if( poLine != NULL )
    {
        double  dfAccum = 0.0;

        for( int i = 0; i < poLine->getNumPoints(); i++ )
        {
            dfAccum += poLine->getZ(i);
        }
        poFeature->SetField( 2, dfAccum / poLine->getNumPoints() );
    }

    return poFeature;
}

/************************************************************************/
/*                      TranslateLandlinePoint()                        */
/************************************************************************/

static OGRFeature *TranslateLandlinePoint( NTFFileReader *poReader,
                                           OGRNTFLayer *poLayer,
                                           NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) < 2
        || papoGroup[0]->GetType() != NRT_POINTREC
        || papoGroup[1]->GetType() != NRT_GEOMETRY )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // POINT_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // FEAT_CODE
    poFeature->SetField( 1, papoGroup[0]->GetField( 17, 20 ) );

    // ORIENT
    poFeature->SetField( 2, atoi(papoGroup[0]->GetField( 11, 16 )) * 0.1 );

    // DISTANCE
    poReader->ApplyAttributeValues( poFeature, papoGroup,
                                    "DT", 3,
                                    NULL );

    // Geometry
    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1]));

    // CHG_DATE (optional)
    if( poFeature->GetFieldIndex("CHG_DATE") == 4 )
    {
        poFeature->SetField( 4, papoGroup[0]->GetField( 23, 28 ) );
    }

    // CHG_TYPE (optional)
    if( poFeature->GetFieldIndex("CHG_TYPE") == 5 )
    {
        poFeature->SetField( 5, papoGroup[0]->GetField( 22, 22 ) );
    }

    return poFeature;
}

/************************************************************************/
/*                       TranslateLandlineLine()                        */
/************************************************************************/

static OGRFeature *TranslateLandlineLine( NTFFileReader *poReader,
                                          OGRNTFLayer *poLayer,
                                          NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) != 2
        || papoGroup[0]->GetType() != NRT_LINEREC
        || papoGroup[1]->GetType() != NRT_GEOMETRY )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // LINE_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // FEAT_CODE
    poFeature->SetField( 1, papoGroup[0]->GetField( 17, 20 ) );

    // Geometry
    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[1]));

    // CHG_DATE (optional)
    if( poFeature->GetFieldIndex("CHG_DATE") == 2 )
    {
        poFeature->SetField( 2, papoGroup[0]->GetField( 23, 28 ) );
    }

    // CHG_TYPE (optional)
    if( poFeature->GetFieldIndex("CHG_TYPE") == 3 )
    {
        poFeature->SetField( 3, papoGroup[0]->GetField( 22, 22 ) );
    }
    return poFeature;
}

/************************************************************************/
/*                       TranslateLandlineName()                        */
/************************************************************************/

static OGRFeature *TranslateLandlineName( NTFFileReader *poReader,
                                          OGRNTFLayer *poLayer,
                                          NTFRecord **papoGroup )

{
    if( CSLCount((char **) papoGroup) != 3
        || papoGroup[0]->GetType() != NRT_NAMEREC
        || papoGroup[1]->GetType() != NRT_NAMEPOSTN
        || papoGroup[2]->GetType() != NRT_GEOMETRY )
        return NULL;

    int         nNumChar = atoi(papoGroup[0]->GetField(13,14));
    if( nNumChar <= 0 )
        return NULL;

    OGRFeature  *poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    // NAME_ID
    poFeature->SetField( 0, atoi(papoGroup[0]->GetField( 3, 8 )) );

    // TEXT_CODE
    poFeature->SetField( 1, papoGroup[0]->GetField( 9, 12 ) );

    // TEXT
    poFeature->SetField( 2, papoGroup[0]->GetField( 15, 15+nNumChar-1) );

    // FONT
    poFeature->SetField( 3, atoi(papoGroup[1]->GetField( 3, 6 )) );

    // TEXT_HT
    poFeature->SetField( 4, atoi(papoGroup[1]->GetField(7,9)) * 0.1 );

    // DIG_POSTN
    poFeature->SetField( 5, atoi(papoGroup[1]->GetField(10,10)) );

    // ORIENT
    poFeature->SetField( 6, CPLAtof(papoGroup[1]->GetField( 11, 14 )) * 0.1 );

    // TEXT_HT_GROUND
    poFeature->SetField( 7, poFeature->GetFieldAsDouble(4)
                         * poReader->GetPaperToGround() );

    // CHG_DATE (optional)
    if( poFeature->GetFieldIndex("CHG_DATE") == 7 )
    {
        poFeature->SetField( 8, papoGroup[0]->GetField( 15+nNumChar+2,
                                                        15+nNumChar+2+5) );
    }

    // CHG_TYPE (optional)
    if( poFeature->GetFieldIndex("CHG_TYPE") == 9 )
    {
        poFeature->SetField( 9, papoGroup[0]->GetField( 15+nNumChar+1,
                                                        15+nNumChar+1 ) );
    }

    // Geometry
    poFeature->SetGeometryDirectly(poReader->ProcessGeometry(papoGroup[2]));

    return poFeature;
}

/************************************************************************/
/*                           EstablishLayer()                           */
/*                                                                      */
/*      Establish one layer based on a simplified description of the    */
/*      fields to be present.                                           */
/************************************************************************/

void NTFFileReader::EstablishLayer( const char * pszLayerName,
                                    OGRwkbGeometryType eGeomType,
                                    NTFFeatureTranslator pfnTranslator,
                                    int nLeadRecordType,
                                    NTFGenericClass *poClass,
                                    ... )

{
/* -------------------------------------------------------------------- */
/*      Does this layer already exist?  If so, we do nothing            */
/*      ... note that we don't check the definition.                    */
/* -------------------------------------------------------------------- */
    OGRNTFLayer *poLayer = poDS->GetNamedLayer(pszLayerName);

/* ==================================================================== */
/*      Create a new layer matching the request if we don't already      */
/*      have one.                                                       */
/* ==================================================================== */
    if( poLayer == NULL )
    {
/* -------------------------------------------------------------------- */
/*      Create a new feature definition.                                */
/* -------------------------------------------------------------------- */
        OGRFeatureDefn *poDefn = new OGRFeatureDefn( pszLayerName );
        poDefn->GetGeomFieldDefn(0)->SetSpatialRef(poDS->GetSpatialRef());
        poDefn->SetGeomType( eGeomType );
        poDefn->Reference();

/* -------------------------------------------------------------------- */
/*      Fetch definitions of each field in turn.                        */
/* -------------------------------------------------------------------- */
        va_list hVaArgs;
        va_start(hVaArgs, poClass);
        while( true )
        {
            const char *pszFieldName = va_arg(hVaArgs, const char *);

            if( pszFieldName == NULL )
                break;

            const OGRFieldType eType = (OGRFieldType) va_arg(hVaArgs, int);
            const int nWidth = va_arg(hVaArgs, int);
            const int nPrecision = va_arg(hVaArgs, int);

            OGRFieldDefn oFieldDefn( pszFieldName, eType );
            oFieldDefn.SetWidth( nWidth );
            oFieldDefn.SetPrecision( nPrecision );

            poDefn->AddFieldDefn( &oFieldDefn );
        }

        va_end(hVaArgs);

/* -------------------------------------------------------------------- */
/*      Add attributes collected in the generic class survey.           */
/* -------------------------------------------------------------------- */
        if( poClass != NULL )
        {
            for( int iGAtt = 0; iGAtt < poClass->nAttrCount; iGAtt++ )
            {
                const char      *pszFormat = poClass->papszAttrFormats[iGAtt];
                OGRFieldDefn    oFieldDefn( poClass->papszAttrNames[iGAtt],
                                            OFTInteger );

                if( STARTS_WITH_CI(pszFormat, "I") )
                {
                    oFieldDefn.SetType( OFTInteger );
                    oFieldDefn.SetWidth( poClass->panAttrMaxWidth[iGAtt] );
                }
                else if( STARTS_WITH_CI(pszFormat, "D")
                         || STARTS_WITH_CI(pszFormat, "A") )
                {
                    oFieldDefn.SetType( OFTString );
                    oFieldDefn.SetWidth( poClass->panAttrMaxWidth[iGAtt] );
                }
                else if( STARTS_WITH_CI(pszFormat, "R") )
                {
                    oFieldDefn.SetType( OFTReal );
                    oFieldDefn.SetWidth( poClass->panAttrMaxWidth[iGAtt]+1 );
                    const size_t nFormatLen = strlen(pszFormat);
                    if( nFormatLen >= 4 && pszFormat[2] == ',' )
                        oFieldDefn.SetPrecision(atoi(pszFormat+3));
                    else if( nFormatLen >= 5 && pszFormat[3] == ',' )
                        oFieldDefn.SetPrecision(atoi(pszFormat+4));
                }

                poDefn->AddFieldDefn( &oFieldDefn );

                /*
                ** If this field can appear multiple times, create an
                ** additional attribute to hold lists of values.  This
                ** is always created as a variable length string field.
                */
                if( poClass->pabAttrMultiple[iGAtt] )
                {
                    char szName[128];

                    snprintf( szName, sizeof(szName), "%s_LIST",
                             poClass->papszAttrNames[iGAtt] );

                    OGRFieldDefn oFieldDefnL( szName, OFTString );

                    poDefn->AddFieldDefn( &oFieldDefnL );
                }
            }
        }

/* -------------------------------------------------------------------- */
/*      Add the TILE_REF attribute.                                     */
/* -------------------------------------------------------------------- */
        OGRFieldDefn     oTileID( "TILE_REF", OFTString );

        oTileID.SetWidth( 10 );

        poDefn->AddFieldDefn( &oTileID );

/* -------------------------------------------------------------------- */
/*      Create the layer, and give over to the data source object to    */
/*      maintain.                                                       */
/* -------------------------------------------------------------------- */
        poLayer = new OGRNTFLayer( poDS, poDefn, pfnTranslator );

        poDS->AddLayer( poLayer );
    }

/* -------------------------------------------------------------------- */
/*      Register this translator with this file reader for handling     */
/*      the indicate record type.                                       */
/* -------------------------------------------------------------------- */
    apoTypeTranslation[nLeadRecordType] = poLayer;
}

/************************************************************************/
/*                          EstablishLayers()                           */
/*                                                                      */
/*      This method is responsible for creating any missing             */
/*      OGRNTFLayers needed for the current product based on the        */
/*      product name.                                                   */
/*                                                                      */
/*      NOTE: Any changes to the order of attribute fields in the       */
/*      following EstablishLayer() calls must also result in updates    */
/*      to the translate functions.  Changes of names, widths and to    */
/*      some extent types can be done without side effects.             */
/************************************************************************/

void NTFFileReader::EstablishLayers()

{
    if( poDS == NULL || fp == NULL )
        return;

    if( GetProductId() == NPC_LANDLINE )
    {
        EstablishLayer( "LANDLINE_POINT", wkbPoint,
                        TranslateLandlinePoint, NRT_POINTREC, NULL,
                        "POINT_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "ORIENT", OFTReal, 5, 1,
                        "DISTANCE", OFTReal, 6, 3,
                        NULL );

        EstablishLayer( "LANDLINE_LINE", wkbLineString,
                        TranslateLandlineLine, NRT_LINEREC, NULL,
                        "LINE_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        NULL );

        EstablishLayer( "LANDLINE_NAME", wkbPoint,
                        TranslateLandlineName, NRT_NAMEREC, NULL,
                        "NAME_ID", OFTInteger, 6, 0,
                        "TEXT_CODE", OFTString, 4, 0,
                        "TEXT", OFTString, 0, 0,
                        "FONT", OFTInteger, 4, 0,
                        "TEXT_HT", OFTReal, 4, 1,
                        "DIG_POSTN", OFTInteger, 1, 0,
                        "ORIENT", OFTReal, 5, 1,
                        "TEXT_HT_GROUND", OFTReal, 10, 3,
                        NULL );
    }
    else if( GetProductId() == NPC_LANDLINE99 )
    {
        EstablishLayer( "LANDLINE99_POINT", wkbPoint,
                        TranslateLandlinePoint, NRT_POINTREC, NULL,
                        "POINT_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "ORIENT", OFTReal, 5, 1,
                        "DISTANCE", OFTReal, 6, 3,
                        "CHG_DATE", OFTString, 6, 0,
                        "CHG_TYPE", OFTString, 1, 0,
                        NULL );

        EstablishLayer( "LANDLINE99_LINE", wkbLineString,
                        TranslateLandlineLine, NRT_LINEREC, NULL,
                        "LINE_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "CHG_DATE", OFTString, 6, 0,
                        "CHG_TYPE", OFTString, 1, 0,
                        NULL );

        EstablishLayer( "LANDLINE99_NAME", wkbPoint,
                        TranslateLandlineName, NRT_NAMEREC, NULL,
                        "NAME_ID", OFTInteger, 6, 0,
                        "TEXT_CODE", OFTString, 4, 0,
                        "TEXT", OFTString, 0, 0,
                        "FONT", OFTInteger, 4, 0,
                        "TEXT_HT", OFTReal, 4, 1,
                        "DIG_POSTN", OFTInteger, 1, 0,
                        "ORIENT", OFTReal, 5, 1,
                        "TEXT_HT_GROUND", OFTReal, 10, 3,
                        "CHG_DATE", OFTString, 6, 0,
                        "CHG_TYPE", OFTString, 1, 0,
                        NULL );
    }
    else if( GetProductId() == NPC_LANDRANGER_CONT )
    {
        EstablishLayer( "PANORAMA_POINT", wkbPoint,
                        TranslateLandrangerPoint, NRT_POINTREC, NULL,
                        "POINT_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "HEIGHT", OFTReal, 7, 2,
                        NULL );

        EstablishLayer( "PANORAMA_CONTOUR", wkbLineString,
                        TranslateLandrangerLine, NRT_LINEREC, NULL,
                        "LINE_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "HEIGHT", OFTReal, 7, 2,
                        NULL );
    }
    else if( GetProductId() == NPC_LANDFORM_PROFILE_CONT )
    {
        EstablishLayer( "PROFILE_POINT", wkbPoint25D,
                        TranslateProfilePoint, NRT_POINTREC, NULL,
                        "POINT_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "HEIGHT", OFTReal, 7, 2,
                        NULL );

        EstablishLayer( "PROFILE_LINE", wkbLineString25D,
                        TranslateProfileLine, NRT_LINEREC, NULL,
                        "LINE_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "HEIGHT", OFTReal, 7, 2,
                        NULL );
    }
    else if( GetProductId() == NPC_STRATEGI )
    {
        EstablishLayer( "STRATEGI_POINT", wkbPoint,
                        TranslateStrategiPoint, NRT_POINTREC, NULL,
                        "POINT_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "PROPER_NAME", OFTString, 0, 0,
                        "FEATURE_NUMBER", OFTString, 0, 0,
                        "RB", OFTString, 1, 0,
                        "RU", OFTString, 1, 0,
                        "AN", OFTString, 0, 0,
                        "AO", OFTString, 0, 0,
                        "COUNTY_NAME", OFTString, 0, 0,
                        "UNITARY_NAME", OFTString, 0, 0,
                        "GEOM_ID", OFTInteger, 6, 0,
                        "DATE", OFTInteger, 8, 0,
                        "DISTRICT_NAME", OFTString, 0, 0,
                        "FEATURE_NAME", OFTString, 0, 0,
                        "GIS", OFTString, 0, 0,
                        "HEIGHT_IMPERIAL", OFTInteger, 4, 0,
                        "HEIGHT_METRIC", OFTInteger, 4, 0,
                        "LOCATION", OFTInteger, 1, 0,
                        "ORIENTATION", OFTReal, 4, 1,
                        "OWNER", OFTString, 0, 0,
                        "RESTRICTION_NORTH", OFTString, 0, 0,
                        "RESTRICTION_SOUTH", OFTString, 0, 0,
                        "RESTRICTION_EAST", OFTString, 0, 0,
                        "RESTRICTION_WEST", OFTString, 0, 0,
                        "RESTRICTION_CLOCKWISE", OFTString, 0, 0,
                        "RESTRICTION_ANTICLOCKWISE", OFTString, 0, 0,
                        "USAGE", OFTInteger, 1, 0,
                        NULL );

        EstablishLayer( "STRATEGI_LINE", wkbLineString,
                        TranslateStrategiLine, NRT_LINEREC, NULL,
                        "LINE_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "PROPER_NAME", OFTString, 0, 0,
                        "GEOM_ID", OFTInteger, 6, 0,
                        "DATE", OFTInteger, 8, 0,
                        "FERRY_ACCESS", OFTString, 0, 0,
                        "FERRY_FROM", OFTString, 0, 0,
                        "FERRY_TIME", OFTString, 0, 0,
                        "FEATURE_NAME", OFTString, 0, 0,
                        "FERRY_TYPE", OFTString, 0, 0,
                        "FERRY_RESTRICTIONS", OFTString, 0, 0,
                        "FERRY_TO", OFTString, 0, 0,
                        "GIS", OFTString, 0, 0,
                        "FEATURE_NUMBER", OFTString, 0, 0,
                        NULL );

        EstablishLayer( "STRATEGI_TEXT", wkbPoint,
                        TranslateStrategiText, NRT_TEXTREC, NULL,
                        "TEXT_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "FONT", OFTInteger, 4, 0,
                        "TEXT_HT", OFTReal, 5, 1,
                        "DIG_POSTN", OFTInteger, 1, 0,
                        "ORIENT", OFTReal, 5, 1,
                        "TEXT", OFTString, 0, 0,
                        "TEXT_HT_GROUND", OFTReal, 10, 3,
                        "DATE", OFTInteger, 8, 0,
                        NULL );

        EstablishLayer( "STRATEGI_NODE", wkbNone,
                        TranslateStrategiNode, NRT_NODEREC, NULL,
                        "NODE_ID", OFTInteger, 6, 0,
                        "GEOM_ID_OF_POINT", OFTInteger, 6, 0,
                        "NUM_LINKS", OFTInteger, 4, 0,
                        "DIR", OFTIntegerList, 1, 0,
                        "GEOM_ID_OF_LINK", OFTIntegerList, 6, 0,
                        "LEVEL", OFTIntegerList, 1, 0,
                        "ORIENT", OFTRealList, 5, 1,
                        NULL );
    }
    else if( GetProductId() == NPC_MERIDIAN )
    {
        EstablishLayer( "MERIDIAN_POINT", wkbPoint,
                        TranslateMeridianPoint, NRT_POINTREC, NULL,
                        "POINT_ID", OFTInteger, 6, 0,
                        "GEOM_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "PROPER_NAME", OFTString, 0, 0,
                        "OSMDR", OFTString, 13, 0,
                        "JUNCTION_NAME", OFTString, 0, 0,
                        "ROUNDABOUT", OFTString, 1, 0,
                        "STATION_ID", OFTString, 13, 0,
                        "GLOBAL_ID", OFTInteger, 6, 0,
                        "ADMIN_NAME", OFTString, 0, 0,
                        "DA_DLUA_ID", OFTString, 13, 0,
                        NULL );

        EstablishLayer( "MERIDIAN_LINE", wkbLineString,
                        TranslateMeridianLine, NRT_LINEREC, NULL,
                        "LINE_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "GEOM_ID", OFTInteger, 6, 0,
                        "OSMDR", OFTString, 13, 0,
                        "ROAD_NUM", OFTString, 0, 0,
                        "TRUNK_ROAD", OFTString, 1, 0,
                        "RAIL_ID", OFTString, 13, 0,
                        "LEFT_COUNTY", OFTInteger, 6, 0,
                        "RIGHT_COUNTY", OFTInteger, 6, 0,
                        "LEFT_DISTRICT", OFTInteger, 6, 0,
                        "RIGHT_DISTRICT", OFTInteger, 6, 0,
                        NULL );

        EstablishLayer( "MERIDIAN_TEXT", wkbPoint,
                        TranslateStrategiText, NRT_TEXTREC, NULL,
                        "TEXT_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "FONT", OFTInteger, 4, 0,
                        "TEXT_HT", OFTReal, 5, 1,
                        "DIG_POSTN", OFTInteger, 1, 0,
                        "ORIENT", OFTReal, 5, 1,
                        "TEXT", OFTString, 0, 0,
                        "TEXT_HT_GROUND", OFTReal, 10, 3,
                        NULL );

        EstablishLayer( "MERIDIAN_NODE", wkbNone,
                        TranslateStrategiNode, NRT_NODEREC, NULL,
                        "NODE_ID", OFTInteger, 6, 0,
                        "GEOM_ID_OF_POINT", OFTInteger, 6, 0,
                        "NUM_LINKS", OFTInteger, 4, 0,
                        "DIR", OFTIntegerList, 1, 0,
                        "GEOM_ID_OF_LINK", OFTIntegerList, 6, 0,
                        "LEVEL", OFTIntegerList, 1, 0,
                        "ORIENT", OFTRealList, 5, 1,
                        NULL );
    }
    else if( GetProductId() == NPC_MERIDIAN2 )
    {
        EstablishLayer( "MERIDIAN2_POINT", wkbPoint,
                        TranslateMeridian2Point, NRT_POINTREC, NULL,
                        "POINT_ID", OFTInteger, 6, 0,
                        "GEOM_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "PROPER_NAME", OFTString, 0, 0,
                        "OSODR", OFTString, 13, 0,
                        "PARENT_OSODR", OFTString, 13, 0,
                        "JUNCTION_NAME", OFTString, 0, 0,
                        "ROUNDABOUT", OFTString, 1, 0,
                        "SETTLEMENT_NAME", OFTString, 0, 0,
                        "STATION_ID", OFTString, 13, 0,
                        "GLOBAL_ID", OFTInteger, 6, 0,
                        "ADMIN_NAME", OFTString, 0, 0,
                        "DA_DLUA_ID", OFTString, 13, 0,
                        "WATER_AREA", OFTString, 13, 0,
                        "HEIGHT", OFTInteger, 8, 0,
                        "FOREST_ID", OFTString, 13, 0,
                        NULL );

        EstablishLayer( "MERIDIAN2_LINE", wkbLineString,
                        TranslateMeridian2Line, NRT_LINEREC, NULL,
                        "LINE_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "GEOM_ID", OFTInteger, 6, 0,
                        "OSODR", OFTString, 13, 0,
                        "PARENT_OSODR", OFTString, 13, 0,
                        "ROAD_NUM", OFTString, 0, 0,
                        "TRUNK_ROAD", OFTString, 1, 0,
                        "PROPER_NAME", OFTString, 0, 0,
                        "RAIL_ID", OFTString, 13, 0,
                        "LEFT_COUNTY", OFTInteger, 6, 0,
                        "RIGHT_COUNTY", OFTInteger, 6, 0,
                        "LEFT_DISTRICT", OFTInteger, 6, 0,
                        "RIGHT_DISTRICT", OFTInteger, 6, 0,
                        "WATER_LINK_ID", OFTString, 13, 0,
                        NULL );

        EstablishLayer( "MERIDIAN2_TEXT", wkbPoint,
                        TranslateStrategiText, NRT_TEXTREC, NULL,
                        "TEXT_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "FONT", OFTInteger, 4, 0,
                        "TEXT_HT", OFTReal, 5, 1,
                        "DIG_POSTN", OFTInteger, 1, 0,
                        "ORIENT", OFTReal, 5, 1,
                        "TEXT", OFTString, 0, 0,
                        "TEXT_HT_GROUND", OFTReal, 10, 3,
                        NULL );

        EstablishLayer( "MERIDIAN2_NODE", wkbNone,
                        TranslateStrategiNode, NRT_NODEREC, NULL,
                        "NODE_ID", OFTInteger, 6, 0,
                        "GEOM_ID_OF_POINT", OFTInteger, 6, 0,
                        "NUM_LINKS", OFTInteger, 4, 0,
                        "DIR", OFTIntegerList, 1, 0,
                        "GEOM_ID_OF_LINK", OFTIntegerList, 6, 0,
                        "LEVEL", OFTIntegerList, 1, 0,
                        "ORIENT", OFTRealList, 5, 1,
                        NULL );
    }
    else if( GetProductId() == NPC_BOUNDARYLINE )
    {
        EstablishLayer( "BOUNDARYLINE_LINK", wkbLineString,
                        TranslateBoundarylineLink, NRT_GEOMETRY, NULL,
                        "GEOM_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "GLOBAL_LINK_ID", OFTInteger, 10, 0,
                        "HWM_FLAG", OFTInteger, 1, 0,
                        NULL );

        EstablishLayer( "BOUNDARYLINE_POLY",
                        bCacheLines ? wkbPolygon : wkbPoint,
                        TranslateBoundarylinePoly, NRT_POLYGON, NULL,
                        "POLY_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "GLOBAL_SEED_ID", OFTInteger, 6, 0,
                        "HECTARES", OFTReal, 9, 3,
                        "NUM_PARTS", OFTInteger, 4, 0,
                        "DIR", OFTIntegerList, 1, 0,
                        "GEOM_ID_OF_LINK", OFTIntegerList, 6, 0,
                        "RingStart", OFTIntegerList, 6, 0,
                        NULL );

        EstablishLayer( "BOUNDARYLINE_COLLECTIONS", wkbNone,
                        TranslateBoundarylineCollection, NRT_COLLECT, NULL,
                        "COLL_ID", OFTInteger, 6, 0,
                        "NUM_PARTS", OFTInteger, 4, 0,
                        "POLY_ID", OFTIntegerList, 6, 0,
                        "ADMIN_AREA_ID", OFTInteger, 6, 0,
                        "OPCS_CODE", OFTString, 6, 0,
                        "ADMIN_NAME", OFTString, 0, 0,
                        NULL );
    }
    else if( GetProductId() == NPC_BL2000 )
    {
        EstablishLayer( "BL2000_LINK", wkbLineString,
                        TranslateBL2000Link, NRT_LINEREC, NULL,
                        "LINE_ID", OFTInteger, 6, 0,
                        "GEOM_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "GLOBAL_LINK_ID", OFTInteger, 10, 0,
                        NULL );
        EstablishLayer( "BL2000_POLY",
                        bCacheLines ? wkbPolygon : wkbNone,
                        TranslateBL2000Poly, NRT_POLYGON, NULL,
                        "POLY_ID", OFTInteger, 6, 0,
                        "GLOBAL_SEED_ID", OFTInteger, 6, 0,
                        "HECTARES", OFTReal, 12, 3,
                        "NUM_PARTS", OFTInteger, 4, 0,
                        "DIR", OFTIntegerList, 1, 0,
                        "GEOM_ID_OF_LINK", OFTIntegerList, 6, 0,
                        "RingStart", OFTIntegerList, 6, 0,
                        NULL );
        if( poDS->GetOption("CODELIST") != NULL
            && EQUAL(poDS->GetOption("CODELIST"),"ON") )
            EstablishLayer( "BL2000_COLLECTIONS", wkbNone,
                            TranslateBL2000Collection, NRT_COLLECT, NULL,
                            "COLL_ID", OFTInteger, 6, 0,
                            "NUM_PARTS", OFTInteger, 4, 0,
                            "POLY_ID", OFTIntegerList, 6, 0,
                            "ADMIN_AREA_ID", OFTInteger, 6, 0,
                            "CENSUS_CODE", OFTString, 7, 0,
                            "ADMIN_NAME", OFTString, 0, 0,
                            "AREA_TYPE", OFTString, 2, 0,
                            "AREA_CODE", OFTString, 3, 0,
                            "NON_TYPE_CODE", OFTString, 3, 0,
                            "NON_INLAND_AREA", OFTReal, 12, 3,
                            "COLL_ID_REFS", OFTIntegerList, 6, 0,
                            "AREA_TYPE_DESC", OFTString, 0, 0,
                            "AREA_CODE_DESC", OFTString, 0, 0,
                            "NON_TYPE_CODE_DESC", OFTString, 0, 0,
                            NULL );
        else
            EstablishLayer( "BL2000_COLLECTIONS", wkbNone,
                            TranslateBL2000Collection, NRT_COLLECT, NULL,
                            "COLL_ID", OFTInteger, 6, 0,
                            "NUM_PARTS", OFTInteger, 4, 0,
                            "POLY_ID", OFTIntegerList, 6, 0,
                            "ADMIN_AREA_ID", OFTInteger, 6, 0,
                            "CENSUS_CODE", OFTString, 7, 0,
                            "ADMIN_NAME", OFTString, 0, 0,
                            "AREA_TYPE", OFTString, 2, 0,
                            "AREA_CODE", OFTString, 3, 0,
                            "NON_TYPE_CODE", OFTString, 3, 0,
                            "NON_INLAND_AREA", OFTReal, 12, 3,
                            "COLL_ID_REFS", OFTIntegerList, 6, 0,
                            NULL );
    }
    else if( GetProductId() == NPC_BASEDATA )
    {
        EstablishLayer( "BASEDATA_POINT", wkbPoint,
                        TranslateBasedataPoint, NRT_POINTREC, NULL,
                        "POINT_ID", OFTInteger, 6, 0,
                        "GEOM_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "PROPER_NAME", OFTString, 0, 0,
                        "FEATURE_NUMBER", OFTString, 0, 0,
                        "COUNTY_NAME", OFTString, 0, 0,
                        "UNITARY_NAME", OFTString, 0, 0,
                        "ORIENT", OFTRealList, 5, 1,
                        NULL );

        EstablishLayer( "BASEDATA_LINE", wkbLineString,
                        TranslateBasedataLine, NRT_LINEREC, NULL,
                        "LINE_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "GEOM_ID", OFTInteger, 6, 0,
                        "PROPER_NAME", OFTString, 0, 0,
                        "FEATURE_NUMBER", OFTString, 0, 0,
                        "RB", OFTString, 1, 0,
                        NULL );

        EstablishLayer( "BASEDATA_TEXT", wkbPoint,
                        TranslateStrategiText, NRT_TEXTREC, NULL,
                        "TEXT_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "FONT", OFTInteger, 4, 0,
                        "TEXT_HT", OFTReal, 5, 1,
                        "DIG_POSTN", OFTInteger, 1, 0,
                        "ORIENT", OFTReal, 5, 1,
                        "TEXT", OFTString, 0, 0,
                        "TEXT_HT_GROUND", OFTReal, 10, 3,
                        NULL );

        EstablishLayer( "BASEDATA_NODE", wkbNone,
                        TranslateStrategiNode, NRT_NODEREC, NULL,
                        "NODE_ID", OFTInteger, 6, 0,
                        "GEOM_ID_OF_POINT", OFTInteger, 6, 0,
                        "NUM_LINKS", OFTInteger, 4, 0,
                        "DIR", OFTIntegerList, 1, 0,
                        "GEOM_ID_OF_LINK", OFTIntegerList, 6, 0,
                        "LEVEL", OFTIntegerList, 1, 0,
                        "ORIENT", OFTRealList, 5, 1,
                        NULL );
    }
    else if( GetProductId() == NPC_OSCAR_ASSET
             || GetProductId() == NPC_OSCAR_TRAFFIC )
    {
        EstablishLayer( "OSCAR_POINT", wkbPoint,
                        TranslateOscarPoint, NRT_POINTREC, NULL,
                        "POINT_ID", OFTInteger, 6, 0,
                        "GEOM_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "OSODR", OFTString, 13, 0,
                        "JUNCTION_NAME", OFTString, 0, 0,
                        "SETTLE_NAME", OFTString, 0, 0,
                        NULL );

        EstablishLayer( "OSCAR_LINE", wkbLineString,
                        TranslateOscarLine, NRT_LINEREC, NULL,
                        "LINE_ID", OFTInteger, 6, 0,
                        "GEOM_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "OSODR", OFTString, 13, 0,
                        "PROPER_NAME", OFTString, 0, 0,
                        "LINE_LENGTH", OFTInteger, 5, 0,
                        "SOURCE", OFTString, 1, 0,
                        "FORM_OF_WAY", OFTString, 1, 0,
                        "ROAD_NUM", OFTString, 0, 0,
                        "TRUNK_ROAD", OFTString, 1, 0,
                        NULL );

        EstablishLayer( "OSCAR_NODE", wkbNone,
                        TranslateStrategiNode, NRT_NODEREC, NULL,
                        "NODE_ID", OFTInteger, 6, 0,
                        "GEOM_ID_OF_POINT", OFTInteger, 6, 0,
                        "NUM_LINKS", OFTInteger, 4, 0,
                        "DIR", OFTIntegerList, 1, 0,
                        "GEOM_ID_OF_LINK", OFTIntegerList, 6, 0,
                        "LEVEL", OFTIntegerList, 1, 0,
                        NULL );

        EstablishLayer( "OSCAR_COMMENT", wkbNone,
                        TranslateOscarComment, NRT_COMMENT, NULL,
                        "RECORD_TYPE", OFTInteger, 2, 0,
                        "RECORD_ID", OFTString, 13, 0,
                        "CHANGE_TYPE", OFTString, 1, 0,
                        NULL );
    }
    else if( GetProductId() == NPC_OSCAR_ROUTE )
    {
        EstablishLayer( "OSCAR_ROUTE_POINT", wkbPoint,
                        TranslateOscarRoutePoint, NRT_POINTREC, NULL,
                        "POINT_ID", OFTInteger, 6, 0,
                        "GEOM_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "OSODR", OFTString, 13, 0,
                        "JUNCTION_NAME", OFTString, 0, 0,
                        "SETTLE_NAME", OFTString, 0, 0,
                        "NUM_PARENTS", OFTInteger, 2, 0,
                        "PARENT_OSODR", OFTStringList, 13, 0,
                        "ROUNDABOUT", OFTString, 1, 0,
                        NULL );

        EstablishLayer( "OSCAR_ROUTE_LINE", wkbLineString,
                        TranslateOscarRouteLine, NRT_LINEREC, NULL,
                        "LINE_ID", OFTInteger, 6, 0,
                        "GEOM_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "OSODR", OFTString, 13, 0,
                        "PROPER_NAME", OFTString, 0, 0,
                        "LINE_LENGTH", OFTInteger, 5, 0,
                        "ROAD_NUM", OFTString, 0, 0,
                        "TRUNK_ROAD", OFTString, 1, 0,
                        "NUM_PARENTS", OFTInteger, 2, 0,
                        "PARENT_OSODR", OFTStringList, 13, 0,
                        NULL );

        EstablishLayer( "OSCAR_ROUTE_NODE", wkbNone,
                        TranslateStrategiNode, NRT_NODEREC, NULL,
                        "NODE_ID", OFTInteger, 6, 0,
                        "GEOM_ID_OF_POINT", OFTInteger, 6, 0,
                        "NUM_LINKS", OFTInteger, 4, 0,
                        "DIR", OFTIntegerList, 1, 0,
                        "GEOM_ID_OF_LINK", OFTIntegerList, 6, 0,
                        "LEVEL", OFTIntegerList, 1, 0,
                        NULL );

        EstablishLayer( "OSCAR_COMMENT", wkbNone,
                        TranslateOscarComment, NRT_COMMENT, NULL,
                        "RECORD_TYPE", OFTInteger, 2, 0,
                        "RECORD_ID", OFTString, 13, 0,
                        "CHANGE_TYPE", OFTString, 1, 0,
                        NULL );
    }
    else if( GetProductId() == NPC_OSCAR_NETWORK )
    {
        EstablishLayer( "OSCAR_NETWORK_POINT", wkbPoint,
                        TranslateOscarNetworkPoint, NRT_POINTREC, NULL,
                        "POINT_ID", OFTInteger, 6, 0,
                        "GEOM_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "OSODR", OFTString, 13, 0,
                        "JUNCTION_NAME", OFTString, 0, 0,
                        "SETTLE_NAME", OFTString, 0, 0,
                        "ROUNDABOUT", OFTString, 1, 0,
                        NULL );

        EstablishLayer( "OSCAR_NETWORK_LINE", wkbLineString,
                        TranslateOscarNetworkLine, NRT_LINEREC, NULL,
                        "LINE_ID", OFTInteger, 6, 0,
                        "GEOM_ID", OFTInteger, 6, 0,
                        "FEAT_CODE", OFTString, 4, 0,
                        "OSODR", OFTString, 13, 0,
                        "PROPER_NAME", OFTString, 0, 0,
                        "LINE_LENGTH", OFTInteger, 5, 0,
                        "ROAD_NUM", OFTString, 0, 0,
                        NULL );

        EstablishLayer( "OSCAR_NETWORK_NODE", wkbNone,
                        TranslateStrategiNode, NRT_NODEREC, NULL,
                        "NODE_ID", OFTInteger, 6, 0,
                        "GEOM_ID_OF_POINT", OFTInteger, 6, 0,
                        "NUM_LINKS", OFTInteger, 4, 0,
                        "DIR", OFTIntegerList, 1, 0,
                        "GEOM_ID_OF_LINK", OFTIntegerList, 6, 0,
                        "LEVEL", OFTIntegerList, 1, 0,
                        NULL );

        EstablishLayer( "OSCAR_COMMENT", wkbNone,
                        TranslateOscarComment, NRT_COMMENT, NULL,
                        "RECORD_TYPE", OFTInteger, 2, 0,
                        "RECORD_ID", OFTString, 13, 0,
                        "CHANGE_TYPE", OFTString, 1, 0,
                        NULL );
    }
    else if( GetProductId() == NPC_ADDRESS_POINT )
    {
        EstablishLayer( "ADDRESS_POINT", wkbPoint,
                        TranslateAddressPoint, NRT_POINTREC, NULL,
                        "POINT_ID", OFTInteger, 6, 0,
                        "OSAPR", OFTString, 18, 0,
                        "ORGANISATION_NAME", OFTString, 0, 0,
                        "DEPARTMENT_NAME", OFTString, 0, 0,
                        "PO_BOX", OFTString, 6, 0,
                        "SUBBUILDING_NAME", OFTString, 0, 0,
                        "BUILDING_NAME", OFTString, 0, 0,
                        "BUILDING_NUMBER", OFTInteger, 4, 0,
                        "DEPENDENT_THOROUGHFARE_NAME", OFTString, 0, 0,
                        "THOROUGHFARE_NAME", OFTString, 0, 0,
                        "DOUBLE_DEPENDENT_LOCALITY_NAME", OFTString, 0, 0,
                        "DEPENDENT_LOCALITY_NAME", OFTString, 0, 0,
                        "POST_TOWN_NAME", OFTString, 0, 0,
                        "COUNTY_NAME", OFTString, 0, 0,
                        "POSTCODE", OFTString, 7, 0,
                        "STATUS_FLAG", OFTString, 4, 0,
                        "RM_VERSION_DATE", OFTString, 8, 0,
                        "CHG_TYPE", OFTString, 1, 0,
                        "CHG_DATE", OFTString, 6, 0,
                        NULL );
    }
    else if( GetProductId() == NPC_CODE_POINT )
    {
        EstablishLayer( "CODE_POINT", wkbPoint,
                        TranslateCodePoint, NRT_POINTREC, NULL,
                        "POINT_ID", OFTInteger, 6, 0,
                        "UNIT_POSTCODE", OFTString, 7, 0,
                        "POSITIONAL_QUALITY", OFTInteger, 1, 0,
                        "PO_BOX_INDICATOR", OFTString, 1, 0,
                        "TOTAL_DELIVERY_POINTS", OFTInteger, 3, 0,
                        "DELIVERY_POINTS", OFTInteger, 3, 0,
                        "DOMESTIC_DELIVERY_POINTS", OFTInteger, 3, 0,
                        "NONDOMESTIC_DELIVERY_POINTS", OFTInteger, 3, 0,
                        "POBOX_DELIVERY_POINTS", OFTInteger, 3, 0,
                        "MATCHED_ADDRESS_PREMISES", OFTInteger, 3, 0,
                        "UNMATCHED_DELIVERY_POINTS", OFTInteger, 3, 0,
                        "RM_VERSION_DATA", OFTString, 8, 0,
                        NULL );
    }
    else if( GetProductId() == NPC_CODE_POINT_PLUS )
    {
        EstablishLayer( "CODE_POINT_PLUS", wkbPoint,
                        TranslateCodePoint, NRT_POINTREC, NULL,
                        "POINT_ID", OFTInteger, 6, 0,
                        "UNIT_POSTCODE", OFTString, 7, 0,
                        "POSITIONAL_QUALITY", OFTInteger, 1, 0,
                        "PO_BOX_INDICATOR", OFTString, 1, 0,
                        "TOTAL_DELIVERY_POINTS", OFTInteger, 3, 0,
                        "DELIVERY_POINTS", OFTInteger, 3, 0,
                        "DOMESTIC_DELIVERY_POINTS", OFTInteger, 3, 0,
                        "NONDOMESTIC_DELIVERY_POINTS", OFTInteger, 3, 0,
                        "POBOX_DELIVERY_POINTS", OFTInteger, 3, 0,
                        "MATCHED_ADDRESS_PREMISES", OFTInteger, 3, 0,
                        "UNMATCHED_DELIVERY_POINTS", OFTInteger, 3, 0,
                        "RM_VERSION_DATA", OFTString, 8, 0,
                        "NHS_REGIONAL_HEALTH_AUTHORITY", OFTString, 3, 0,
                        "NHS_HEALTH_AUTHORITY", OFTString, 3, 0,
                        "ADMIN_COUNTY", OFTString, 2, 0,
                        "ADMIN_DISTRICT", OFTString, 2, 0,
                        "ADMIN_WARD", OFTString, 2, 0,
                        NULL );
    }
    else // generic case
    {
        CPLAssert( GetProductId() == NPC_UNKNOWN );

        poDS->WorkupGeneric( this );
    }
}
