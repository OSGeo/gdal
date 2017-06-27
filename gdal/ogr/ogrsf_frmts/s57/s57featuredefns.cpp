/******************************************************************************
 *
 * Project:  S-57 Translator
 * Purpose:  Implements methods to create OGRFeatureDefns for various
 *           object classes, and primitive features.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, 2001, 2003 Frank Warmerdam
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_api.h"
#include "s57.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                     S57GenerateGeomFeatureDefn()                     */
/************************************************************************/

OGRFeatureDefn *S57GenerateDSIDFeatureDefn()

{
    OGRFeatureDefn      *poFDefn = new OGRFeatureDefn( "DSID" );

    poFDefn->SetGeomType( wkbNone );
    poFDefn->Reference();

/* -------------------------------------------------------------------- */
/*      DSID fields.                                                    */
/* -------------------------------------------------------------------- */
    OGRFieldDefn oField( "", OFTInteger );

    oField.Set( "DSID_EXPP", OFTInteger, 3, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSID_INTU", OFTInteger, 3, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSID_DSNM", OFTString, 0, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSID_EDTN", OFTString, 0, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSID_UPDN", OFTString, 0, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSID_UADT", OFTString, 8, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSID_ISDT", OFTString, 8, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSID_STED", OFTReal, 11, 6 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSID_PRSP", OFTInteger, 3, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSID_PSDN", OFTString, 0, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSID_PRED", OFTString, 0, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSID_PROF", OFTInteger, 3, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSID_AGEN", OFTInteger, 5, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSID_COMT", OFTString, 0, 0 );
    poFDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      DSSI fields.                                                    */
/* -------------------------------------------------------------------- */

    oField.Set( "DSSI_DSTR", OFTInteger, 3, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSSI_AALL", OFTInteger, 3, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSSI_NALL", OFTInteger, 3, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSSI_NOMR", OFTInteger, 10, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSSI_NOCR", OFTInteger, 10, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSSI_NOGR", OFTInteger, 10, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSSI_NOLR", OFTInteger, 10, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSSI_NOIN", OFTInteger, 10, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSSI_NOCN", OFTInteger, 10, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSSI_NOED", OFTInteger, 10, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSSI_NOFA", OFTInteger, 10, 0 );
    poFDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      DSPM fields.                                                    */
/* -------------------------------------------------------------------- */

    oField.Set( "DSPM_HDAT", OFTInteger, 3, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSPM_VDAT", OFTInteger, 3, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSPM_SDAT", OFTInteger, 3, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSPM_CSCL", OFTInteger, 10, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSPM_DUNI", OFTInteger, 3, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSPM_HUNI", OFTInteger, 3, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSPM_PUNI", OFTInteger, 3, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSPM_COUN", OFTInteger, 3, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSPM_COMF", OFTInteger, 10, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSPM_SOMF", OFTInteger, 10, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "DSPM_COMT", OFTString, 0, 0 );
    poFDefn->AddFieldDefn( &oField );

    return poFDefn;
}

/************************************************************************/
/*                     S57GenerateGeomFeatureDefn()                     */
/************************************************************************/

OGRFeatureDefn *S57GenerateGeomFeatureDefn( OGRwkbGeometryType eGType,
                                            int nOptionFlags )

{
    OGRFeatureDefn      *poFDefn = NULL;

    if( eGType == wkbPoint )
    {
        poFDefn = new OGRFeatureDefn( "Point" );
        poFDefn->SetGeomType( eGType );
    }
    else if( eGType == wkbLineString )
    {
        poFDefn = new OGRFeatureDefn( "Line" );
        poFDefn->SetGeomType( eGType );
    }
    else if( eGType == wkbPolygon )
    {
        poFDefn = new OGRFeatureDefn( "Area" );
        poFDefn->SetGeomType( eGType );
    }
    else if( eGType == wkbNone )
    {
        poFDefn = new OGRFeatureDefn( "Meta" );
        poFDefn->SetGeomType( eGType );
    }
    else if( eGType == wkbUnknown )
    {
        poFDefn = new OGRFeatureDefn( "Generic" );
        poFDefn->SetGeomType( eGType );
    }
    else
        return NULL;

    poFDefn->Reference();
    S57GenerateStandardAttributes( poFDefn, nOptionFlags );

    return poFDefn;
}

/************************************************************************/
/*               S57GenerateVectorPrimitiveFeatureDefn()                */
/************************************************************************/

OGRFeatureDefn *
S57GenerateVectorPrimitiveFeatureDefn( int nRCNM,
                                       int /* nOptionFlags */ )
{
    OGRFeatureDefn      *poFDefn = NULL;

    if( nRCNM == RCNM_VI )
    {
        poFDefn = new OGRFeatureDefn( OGRN_VI );
        poFDefn->SetGeomType( wkbPoint );
    }
    else if( nRCNM == RCNM_VC )
    {
        poFDefn = new OGRFeatureDefn( OGRN_VC );
        poFDefn->SetGeomType( wkbPoint );
    }
    else if( nRCNM == RCNM_VE )
    {
        poFDefn = new OGRFeatureDefn( OGRN_VE );
        poFDefn->SetGeomType( wkbUnknown );
    }
    else if( nRCNM == RCNM_VF )
    {
        poFDefn = new OGRFeatureDefn( OGRN_VF );
        poFDefn->SetGeomType( wkbPolygon );
    }
    else
        return NULL;

    poFDefn->Reference();

/* -------------------------------------------------------------------- */
/*      Core vector primitive attributes                                */
/* -------------------------------------------------------------------- */
    OGRFieldDefn oField("",OFTInteger);

    oField.Set( "RCNM", OFTInteger, 3, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "RCID", OFTInteger, 8, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "RVER", OFTInteger, 2, 0 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "RUIN", OFTInteger, 2, 0 );
    poFDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      Geometric primitive attributes                                  */
/* -------------------------------------------------------------------- */
    oField.Set( "POSACC", OFTReal, 10, 2 );
    poFDefn->AddFieldDefn( &oField );

    oField.Set( "QUAPOS", OFTInteger, 2, 0 );
    poFDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      For lines we want to capture the point links for the first      */
/*      and last nodes.                                                 */
/* -------------------------------------------------------------------- */
    if( nRCNM == RCNM_VE )
    {
        oField.Set( "NAME_RCNM_0", OFTInteger, 3, 0 );
        poFDefn->AddFieldDefn( &oField );

        oField.Set( "NAME_RCID_0", OFTInteger, 8, 0 );
        poFDefn->AddFieldDefn( &oField );

        oField.Set( "ORNT_0", OFTInteger, 3, 0 );
        poFDefn->AddFieldDefn( &oField );

        oField.Set( "USAG_0", OFTInteger, 3, 0 );
        poFDefn->AddFieldDefn( &oField );

        oField.Set( "TOPI_0", OFTInteger, 1, 0 );
        poFDefn->AddFieldDefn( &oField );

        oField.Set( "MASK_0", OFTInteger, 3, 0 );
        poFDefn->AddFieldDefn( &oField );

        oField.Set( "NAME_RCNM_1", OFTInteger, 3, 0 );
        poFDefn->AddFieldDefn( &oField );

        oField.Set( "NAME_RCID_1", OFTInteger, 8, 0 );
        poFDefn->AddFieldDefn( &oField );

        oField.Set( "ORNT_1", OFTInteger, 3, 0 );
        poFDefn->AddFieldDefn( &oField );

        oField.Set( "USAG_1", OFTInteger, 3, 0 );
        poFDefn->AddFieldDefn( &oField );

        oField.Set( "TOPI_1", OFTInteger, 1, 0 );
        poFDefn->AddFieldDefn( &oField );

        oField.Set( "MASK_1", OFTInteger, 3, 0 );
        poFDefn->AddFieldDefn( &oField );
    }

    return poFDefn;
}

/************************************************************************/
/*                     S57GenerateObjectClassDefn()                     */
/************************************************************************/

OGRFeatureDefn *S57GenerateObjectClassDefn(
    S57ClassRegistrar *poCR,
    S57ClassContentExplorer* poClassContentExplorer,
    int nOBJL, int nOptionFlags )

{
    if( !poClassContentExplorer->SelectClass( nOBJL ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create the feature definition based on the object class         */
/*      acronym.                                                        */
/* -------------------------------------------------------------------- */
    OGRFeatureDefn *poFDefn
        = new OGRFeatureDefn( poClassContentExplorer->GetAcronym() );
    poFDefn->Reference();

/* -------------------------------------------------------------------- */
/*      Try and establish the geometry type.  If more than one          */
/*      geometry type is allowed we just fall back to wkbUnknown.       */
/* -------------------------------------------------------------------- */
    char **papszGeomPrim = poClassContentExplorer->GetPrimitives();
    if( CSLCount(papszGeomPrim) == 0 )
    {
        poFDefn->SetGeomType( wkbNone );
    }
    else if( CSLCount(papszGeomPrim) > 1 )
    {
        // leave as unknown geometry type.
    }
    else if( papszGeomPrim[0][0] == 'P' )
    {
        if( EQUAL(poClassContentExplorer->GetAcronym(),"SOUNDG") )
        {
            if( nOptionFlags & S57M_SPLIT_MULTIPOINT )
                poFDefn->SetGeomType( wkbPoint25D );
            else
                poFDefn->SetGeomType( wkbMultiPoint25D );
        }
        else
            poFDefn->SetGeomType( wkbPoint );
    }
    else if( papszGeomPrim[0][0] == 'A' )
    {
        poFDefn->SetGeomType( wkbPolygon );
    }
    else if( papszGeomPrim[0][0] == 'L' )
    {
        // unfortunately this could be a multilinestring
        poFDefn->SetGeomType( wkbUnknown );
    }

/* -------------------------------------------------------------------- */
/*      Add the standard attributes.                                    */
/* -------------------------------------------------------------------- */
    S57GenerateStandardAttributes( poFDefn, nOptionFlags );

/* -------------------------------------------------------------------- */
/*      Add the attributes specific to this object class.               */
/* -------------------------------------------------------------------- */
    char **papszAttrList = poClassContentExplorer->GetAttributeList();

    for( int iAttr = 0;
         papszAttrList != NULL && papszAttrList[iAttr] != NULL;
         iAttr++ )
    {
        const int iAttrIndex = poCR->FindAttrByAcronym( papszAttrList[iAttr] );

        if( iAttrIndex == -1 )
        {
            CPLDebug( "S57", "Can't find attribute %s from class %s:%s.",
                      papszAttrList[iAttr],
                      poClassContentExplorer->GetAcronym(),
                      poClassContentExplorer->GetDescription() );
            continue;
        }

        OGRFieldDefn oField( papszAttrList[iAttr], OFTInteger );

        switch( poCR->GetAttrType( iAttrIndex ) )
        {
          case SAT_ENUM:
          case SAT_INT:
            oField.SetType( OFTInteger );
            break;

          case SAT_FLOAT:
            oField.SetType( OFTReal );
            break;

          case SAT_CODE_STRING:
          case SAT_FREE_TEXT:
            oField.SetType( OFTString );
            break;

          case SAT_LIST:
            oField.SetType( OFTString );
            break;
        }

        poFDefn->AddFieldDefn( &oField );
    }

/* -------------------------------------------------------------------- */
/*      Do we need to add DEPTH attributes to soundings?                */
/* -------------------------------------------------------------------- */
    const char* pszClassAcronym = poClassContentExplorer->GetAcronym();
    if( pszClassAcronym != NULL && EQUAL(pszClassAcronym, "SOUNDG")
        && (nOptionFlags & S57M_ADD_SOUNDG_DEPTH) )
    {
        OGRFieldDefn oField( "DEPTH", OFTReal );
        poFDefn->AddFieldDefn( &oField );
    }

    return poFDefn;
}

/************************************************************************/
/*                   S57GenerateStandardAttributes()                    */
/*                                                                      */
/*      Attach standard feature attributes to a feature definition.     */
/************************************************************************/

void S57GenerateStandardAttributes( OGRFeatureDefn *poFDefn, int nOptionFlags )

{
    OGRFieldDefn        oField( "", OFTInteger );

/* -------------------------------------------------------------------- */
/*      RCID                                                            */
/* -------------------------------------------------------------------- */
    oField.Set( "RCID", OFTInteger, 10, 0 );
    poFDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      PRIM                                                            */
/* -------------------------------------------------------------------- */
    oField.Set( "PRIM", OFTInteger, 3, 0 );
    poFDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      GRUP                                                            */
/* -------------------------------------------------------------------- */
    oField.Set( "GRUP", OFTInteger, 3, 0 );
    poFDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      OBJL                                                            */
/* -------------------------------------------------------------------- */
    oField.Set( "OBJL", OFTInteger, 5, 0 );
    poFDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      RVER                                                            */
/* -------------------------------------------------------------------- */
    oField.Set( "RVER", OFTInteger, 3, 0 );
    poFDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      AGEN                                                            */
/* -------------------------------------------------------------------- */
    oField.Set( "AGEN", OFTInteger, 5, 0 );
    poFDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      FIDN                                                            */
/* -------------------------------------------------------------------- */
    oField.Set( "FIDN", OFTInteger, 10, 0 );
    poFDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      FIDS                                                            */
/* -------------------------------------------------------------------- */
    oField.Set( "FIDS", OFTInteger, 5, 0 );
    poFDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      LNAM - only generated when LNAM strings are being used.         */
/* -------------------------------------------------------------------- */
    if( nOptionFlags & S57M_LNAM_REFS )
    {
        oField.Set( "LNAM", OFTString, 16, 0 );
        poFDefn->AddFieldDefn( &oField );

        oField.Set( "LNAM_REFS", OFTStringList, 16, 0 );
        poFDefn->AddFieldDefn( &oField );

        oField.Set( "FFPT_RIND", OFTIntegerList, 1, 0 );
        poFDefn->AddFieldDefn( &oField );

        // We should likely include FFPT_COMT here.
    }

/* -------------------------------------------------------------------- */
/*      Values from FSPT field.                                         */
/* -------------------------------------------------------------------- */
    if( nOptionFlags & S57M_RETURN_LINKAGES )
    {
        oField.Set( "NAME_RCNM", OFTIntegerList, 3, 0 );
        poFDefn->AddFieldDefn( &oField );

        oField.Set( "NAME_RCID", OFTIntegerList, 10, 0 );
        poFDefn->AddFieldDefn( &oField );

        oField.Set( "ORNT", OFTIntegerList, 1, 0 );
        poFDefn->AddFieldDefn( &oField );

        oField.Set( "USAG", OFTIntegerList, 1, 0 );
        poFDefn->AddFieldDefn( &oField );

        oField.Set( "MASK", OFTIntegerList, 3, 0 );
        poFDefn->AddFieldDefn( &oField );
    }
}
