/******************************************************************************
 * $Id$
 *
 * Project:  S-57 Translator
 * Purpose:  Implements OGRS57Layer class.
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.16  2006/02/15 18:04:45  fwarmerdam
 * implemented DSID feature support
 *
 * Revision 1.15  2005/09/21 00:54:43  fwarmerdam
 * fixup OGRFeatureDefn and OGRSpatialReference refcount handling
 *
 * Revision 1.14  2005/02/22 12:53:12  fwarmerdam
 * use OGRLayer base spatial filter support
 *
 * Revision 1.13  2005/02/02 20:54:27  fwarmerdam
 * track m_nFeaturesRead
 *
 * Revision 1.12  2003/11/15 21:50:52  warmerda
 * Added limited creation support
 *
 * Revision 1.11  2003/09/09 16:42:51  warmerda
 * fixed email
 *
 * Revision 1.10  2003/09/05 19:12:05  warmerda
 * added RETURN_PRIMITIVES support to get low level prims
 *
 * Revision 1.9  2002/03/05 14:25:43  warmerda
 * expanded tabs
 *
 * Revision 1.8  2001/12/19 22:04:48  warmerda
 * ensure setting spatial filter resets reading
 *
 * Revision 1.7  2001/12/17 22:36:35  warmerda
 * implement GetFeature() method
 *
 * Revision 1.6  2001/12/14 19:40:18  warmerda
 * added optimized feature counting, and extents collection
 *
 * Revision 1.5  2001/07/18 04:55:16  warmerda
 * added CPL_CSVID
 *
 * Revision 1.4  2001/06/19 15:50:23  warmerda
 * added feature attribute query support
 *
 * Revision 1.3  1999/11/26 03:07:08  warmerda
 * set spatial reference on features
 *
 * Revision 1.2  1999/11/18 19:01:25  warmerda
 * expanded tabs
 *
 * Revision 1.1  1999/11/03 22:12:43  warmerda
 * New
 *
 */

#include "ogr_s57.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            OGRS57Layer()                             */
/*                                                                      */
/*      Note that the OGRS57Layer assumes ownership of the passed       */
/*      OGRFeatureDefn object.                                          */
/************************************************************************/

OGRS57Layer::OGRS57Layer( OGRS57DataSource *poDSIn,
                          OGRFeatureDefn * poDefnIn,
                          int nFeatureCountIn,
                          int nOBJLIn)

{
    poDS = poDSIn;

    nFeatureCount = nFeatureCountIn;

    poFeatureDefn = poDefnIn;

    nOBJL = nOBJLIn;

    nNextFEIndex = 0;
    nCurrentModule = -1;

    if( EQUAL(poDefnIn->GetName(),OGRN_VI) )
        nRCNM = RCNM_VI;
    else if( EQUAL(poDefnIn->GetName(),OGRN_VC) )
        nRCNM = RCNM_VC;
    else if( EQUAL(poDefnIn->GetName(),OGRN_VE) )
        nRCNM = RCNM_VE;
    else if( EQUAL(poDefnIn->GetName(),OGRN_VF) )
        nRCNM = RCNM_VF;
    else if( EQUAL(poDefnIn->GetName(),"DSID") )
        nRCNM = RCNM_DSID;
    else 
        nRCNM = 100;  /* feature */
}

/************************************************************************/
/*                           ~OGRS57Layer()                           */
/************************************************************************/

OGRS57Layer::~OGRS57Layer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != NULL )
    {
        CPLDebug( "S57", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead, 
                  poFeatureDefn->GetName() );
    }

    poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRS57Layer::ResetReading()

{
    nNextFEIndex = 0;
    nCurrentModule = -1;
}

/************************************************************************/
/*                      GetNextUnfilteredFeature()                      */
/************************************************************************/

OGRFeature *OGRS57Layer::GetNextUnfilteredFeature()

{
    OGRFeature  *poFeature = NULL;
    
/* -------------------------------------------------------------------- */
/*      Are we out of modules to request features from?                 */
/* -------------------------------------------------------------------- */
    if( nCurrentModule >= poDS->GetModuleCount() )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Set the current position on the current module and fetch a      */
/*      feature.                                                        */
/* -------------------------------------------------------------------- */
    S57Reader   *poReader = poDS->GetModule(nCurrentModule);
    
    if( poReader != NULL )
    {
        poReader->SetNextFEIndex( nNextFEIndex, nRCNM );
        poFeature = poReader->ReadNextFeature( poFeatureDefn );
        nNextFEIndex = poReader->GetNextFEIndex( nRCNM );
    }

/* -------------------------------------------------------------------- */
/*      If we didn't get a feature we need to move onto the next file.  */
/* -------------------------------------------------------------------- */
    if( poFeature == NULL )
    {
        nCurrentModule++;
        poReader = poDS->GetModule(nCurrentModule);

        if( poReader != NULL && poReader->GetModule() == NULL )
        {
            if( !poReader->Open( FALSE ) )
                return NULL;
        }

        return GetNextUnfilteredFeature();
    }
    else
    {
        m_nFeaturesRead++;
        if( poFeature->GetGeometryRef() != NULL )
            poFeature->GetGeometryRef()->assignSpatialReference(
                GetSpatialRef() );
    }
    
    return poFeature;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRS57Layer::GetNextFeature()

{
    OGRFeature  *poFeature = NULL;

/* -------------------------------------------------------------------- */
/*      Read features till we find one that satisfies our current       */
/*      spatial criteria.                                               */
/* -------------------------------------------------------------------- */
    while( TRUE )
    {
        poFeature = GetNextUnfilteredFeature();
        if( poFeature == NULL )
            break;

        if( (m_poFilterGeom == NULL
             || FilterGeometry(poFeature->GetGeometryRef()) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature )) )
            break;

        delete poFeature;
    }

    return poFeature;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRS57Layer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return FALSE;

    else if( EQUAL(pszCap,OLCSequentialWrite) )
        return TRUE;

    else if( EQUAL(pszCap,OLCRandomWrite) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastGetExtent) )
    {
        OGREnvelope oEnvelope;

        return GetExtent( &oEnvelope, FALSE ) == OGRERR_NONE;
    }
    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return FALSE;

    else 
        return FALSE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRS57Layer::GetSpatialRef()

{
    return poDS->GetSpatialRef();
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRS57Layer::GetExtent( OGREnvelope *psExtent, int bForce )

{
    return poDS->GetDSExtent( psExtent, bForce );
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/
int OGRS57Layer::GetFeatureCount (int bForce)
{
    
    if( m_poFilterGeom != NULL || m_poAttrQuery != NULL 
        || nFeatureCount == -1 )
        return OGRLayer::GetFeatureCount( bForce );
    else
        return nFeatureCount;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRS57Layer::GetFeature( long nFeatureId )

{
    S57Reader   *poReader = poDS->GetModule(0); // not multi-reader aware

    if( poReader != NULL )
    {
        OGRFeature      *poFeature;

        poFeature = poReader->ReadFeature( nFeatureId, poFeatureDefn );
        if( poFeature != NULL &&  poFeature->GetGeometryRef() != NULL )
            poFeature->GetGeometryRef()->assignSpatialReference(
                GetSpatialRef() );
        return poFeature;
    }
    else
        return NULL;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRS57Layer::CreateFeature( OGRFeature *poFeature )

{
/* -------------------------------------------------------------------- */
/*      Set RCNM if not already set.                                    */
/* -------------------------------------------------------------------- */
    int iRCNMFld = poFeature->GetFieldIndex( "RCNM" );

    if( iRCNMFld != -1 )
    {
        if( !poFeature->IsFieldSet( iRCNMFld ) )
            poFeature->SetField( iRCNMFld, nRCNM );
        else
            CPLAssert( poFeature->GetFieldAsInteger( iRCNMFld ) == nRCNM );
    }

/* -------------------------------------------------------------------- */
/*      Set OBJL if not already set.                                    */
/* -------------------------------------------------------------------- */
    if( nOBJL != -1 )
    {
        int iOBJLFld = poFeature->GetFieldIndex( "OBJL" );

        if( !poFeature->IsFieldSet( iOBJLFld ) )
            poFeature->SetField( iOBJLFld, nOBJL );
        else
            CPLAssert( poFeature->GetFieldAsInteger( iOBJLFld ) == nOBJL );
    }

/* -------------------------------------------------------------------- */
/*      Create the isolated node feature.                               */
/* -------------------------------------------------------------------- */
    if( poDS->GetWriter()->WriteCompleteFeature( poFeature ) )
        return OGRERR_NONE;
    else
        return OGRERR_FAILURE;
}
