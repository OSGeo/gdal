/******************************************************************************
 * $Id$
 *
 * Project:  S-57 Translator
 * Purpose:  Implements S57Reader class.
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
 * Revision 1.11  1999/11/26 15:17:01  warmerda
 * fixed lname to lnam
 *
 * Revision 1.10  1999/11/26 15:08:38  warmerda
 * added setoptions, and LNAM support
 *
 * Revision 1.9  1999/11/26 13:50:34  warmerda
 * added NATF support
 *
 * Revision 1.8  1999/11/26 13:26:03  warmerda
 * fixed up sounding support
 *
 * Revision 1.7  1999/11/25 20:53:49  warmerda
 * added sounding and S57_SPLIT_MULTIPOINT support
 *
 * Revision 1.6  1999/11/18 19:01:25  warmerda
 * expanded tabs
 *
 * Revision 1.5  1999/11/18 18:58:16  warmerda
 * make permissive of missing geometry so that update files work
 *
 * Revision 1.4  1999/11/16 21:47:32  warmerda
 * updated class occurance collection
 *
 * Revision 1.3  1999/11/08 22:23:00  warmerda
 * added object class support
 *
 * Revision 1.2  1999/11/04 21:19:13  warmerda
 * added polygon support
 *
 * Revision 1.1  1999/11/03 22:12:43  warmerda
 * New
 *
 */

#include "s57.h"
#include "cpl_conv.h"
#include "cpl_string.h"

/************************************************************************/
/*                             S57Reader()                              */
/************************************************************************/

S57Reader::S57Reader( const char * pszFilename )

{
    pszModuleName = CPLStrdup( pszFilename );

    poModule = NULL;

    nFDefnCount = 0;
    papoFDefnList = NULL;

    nCOMF = 1000000;
    nSOMF = 10;

    poRegistrar = NULL;
    bFileIngested = FALSE;

    nNextFEIndex = 0;

    iPointOffset = 0;
    poMultiPoint = NULL;

    papszOptions = NULL;
    bSplitMultiPoint = FALSE;
    bGenerateLNAM = FALSE;
}

/************************************************************************/
/*                             ~S57Reader()                             */
/************************************************************************/

S57Reader::~S57Reader()

{
    Close();
    
    CPLFree( pszModuleName );
    CSLDestroy( papszOptions );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int S57Reader::Open( int bTestOpen )

{
    if( poModule != NULL )
    {
        Rewind();
        return TRUE;
    }

    poModule = new DDFModule();
    if( !poModule->Open( pszModuleName ) )
    {
        // notdef: test bTestOpen.
        delete poModule;
        poModule = NULL;
        return FALSE;
    }

    // note that the following won't work for catalogs.
    if( poModule->FindFieldDefn("DSID") == NULL )
    {
        if( !bTestOpen )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s is an ISO8211 file, but not an S-57 data file.\n",
                      pszModuleName );
        }
        delete poModule;
        poModule = NULL;
        return FALSE;
    }

    nNextFEIndex = 0;
    
    return TRUE;
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

void S57Reader::Close()

{
    if( poModule != NULL )
    {
        oVI_Index.Clear();
        oVC_Index.Clear();
        oVE_Index.Clear();
        oVF_Index.Clear();
        oFE_Index.Clear();

        ClearPendingMultiPoint();

        delete poModule;
        poModule = NULL;

        bFileIngested = FALSE;
    }
}

/************************************************************************/
/*                       ClearPendingMultiPoint()                       */
/************************************************************************/

void S57Reader::ClearPendingMultiPoint()

{
    if( poMultiPoint != NULL )
    {
        delete poMultiPoint;
        poMultiPoint = NULL;
    }
        
}

/************************************************************************/
/*                       NextPendingMultiPoint()                        */
/************************************************************************/

OGRFeature *S57Reader::NextPendingMultiPoint()

{
    CPLAssert( poMultiPoint != NULL );
    CPLAssert( poMultiPoint->GetGeometryRef()->getGeometryType()
               						== wkbMultiPoint );

    OGRFeatureDefn *poDefn = poMultiPoint->GetDefnRef();
    OGRFeature	*poPoint = new OGRFeature( poDefn );
    OGRMultiPoint *poMPGeom = (OGRMultiPoint *) poMultiPoint->GetGeometryRef();

    poPoint->SetFID( poMultiPoint->GetFID() );
    
    for( int i = 0; i < poDefn->GetFieldCount(); i++ )
    {
        poPoint->SetField( i, poMultiPoint->GetRawFieldRef(i) );
    }

    poPoint->SetGeometry( poMPGeom->getGeometryRef( iPointOffset++ ) );

    if( iPointOffset >= poMPGeom->getNumGeometries() )
        ClearPendingMultiPoint();

    return poPoint;
}

/************************************************************************/
/*                             SetOptions()                             */
/************************************************************************/

void S57Reader::SetOptions( char ** papszOptionsIn )

{
    const char * pszOptionValue;
    
    CSLDestroy( papszOptions );
    papszOptions = CSLDuplicate( papszOptionsIn );

    pszOptionValue = CSLFetchNameValue( papszOptions, "SPLIT_MULTIPOINT" );
    if( pszOptionValue != NULL && !EQUAL(pszOptionValue,"OFF") )
        bSplitMultiPoint = TRUE;
    else
        bSplitMultiPoint = FALSE;

    pszOptionValue = CSLFetchNameValue( papszOptions, "LNAM_REFS" );
    if( pszOptionValue != NULL && !EQUAL(pszOptionValue,"OFF") )
        bGenerateLNAM = TRUE;
    else
        bGenerateLNAM = FALSE;
}

/************************************************************************/
/*                           SetClassBased()                            */
/************************************************************************/

void S57Reader::SetClassBased( S57ClassRegistrar * poReg )

{
    poRegistrar = poReg;
}

/************************************************************************/
/*                               Rewind()                               */
/************************************************************************/

void S57Reader::Rewind()

{
    ClearPendingMultiPoint();
    nNextFEIndex = 0;
}

/************************************************************************/
/*                               Ingest()                               */
/*                                                                      */
/*      Read all the records into memory, adding to the appropriate     */
/*      indexes.                                                        */
/************************************************************************/

void S57Reader::Ingest()

{
    DDFRecord   *poRecord;
    
    if( poModule == NULL || bFileIngested )
        return;

    while( (poRecord = poModule->ReadRecord()) != NULL )
    {
        DDFField        *poKeyField = poRecord->GetField(1);
        
        if( EQUAL(poKeyField->GetFieldDefn()->GetName(),"VRID") )
        {
            int         nRCNM = poRecord->GetIntSubfield( "VRID",0, "RCNM",0);
            int         nRCID = poRecord->GetIntSubfield( "VRID",0, "RCID",0);

            switch( nRCNM )
            {
              case RCNM_VI:
                oVI_Index.AddRecord( nRCID, poRecord->Clone() );
                break;

              case RCNM_VC:
                oVC_Index.AddRecord( nRCID, poRecord->Clone() );
                break;

              case RCNM_VE:
                oVE_Index.AddRecord( nRCID, poRecord->Clone() );
                break;

              case RCNM_VF:
                oVF_Index.AddRecord( nRCID, poRecord->Clone() );
                break;

              default:
                CPLAssert( FALSE );
                break;
            }
        }

        else if( EQUAL(poKeyField->GetFieldDefn()->GetName(),"DSPM") )
        {
            nCOMF = MAX(1,poRecord->GetIntSubfield( "DSPM",0, "COMF",0));
            nSOMF = MAX(1,poRecord->GetIntSubfield( "DSPM",0, "SOMF",0));
        }

        else if( EQUAL(poKeyField->GetFieldDefn()->GetName(),"FRID") )
        {
            int         nRCID = poRecord->GetIntSubfield( "FRID",0, "RCID",0);
            
            oFE_Index.AddRecord( nRCID, poRecord->Clone() );
        }

        else if( EQUAL(poKeyField->GetFieldDefn()->GetName(),"DSID") )
        {
            // currently not used, but we don't want to generate a message.
        }

        else
        {
            CPLDebug( "S57",
                      "Skipping %s record in S57Reader::Ingest().\n",
                      poKeyField->GetFieldDefn()->GetName() );
        }
    }

    bFileIngested = TRUE;
}

/************************************************************************/
/*                           SetNextFEIndex()                           */
/************************************************************************/

void S57Reader::SetNextFEIndex( int nNewIndex )

{
    if( nNextFEIndex != nNewIndex )
        ClearPendingMultiPoint();
    
    nNextFEIndex = nNewIndex;
}

/************************************************************************/
/*                          ReadNextFeature()                           */
/************************************************************************/

OGRFeature * S57Reader::ReadNextFeature( OGRFeatureDefn * poTarget )

{
    if( !bFileIngested )
        Ingest();

    if( poMultiPoint != NULL )
    {
        if( poTarget == NULL || poTarget == poMultiPoint->GetDefnRef() )
        {
            return NextPendingMultiPoint();
        }
        else
        {
            ClearPendingMultiPoint();
        }
    }
        
    while( nNextFEIndex < oFE_Index.GetCount() )
    {
        OGRFeature      *poFeature;

        poFeature = AssembleFeature( oFE_Index.GetByIndex(nNextFEIndex++),
                                     poTarget );

        if( poFeature != NULL )
        {
            poFeature->SetFID( nNextFEIndex );
            
            if( bSplitMultiPoint && poFeature->GetGeometryRef() != NULL
                && poFeature->GetGeometryRef()->getGeometryType()
                					== wkbMultiPoint)
            {
                poMultiPoint = poFeature;
                iPointOffset = 0;
                return NextPendingMultiPoint();
            }

            return poFeature;
        }
    }

    return NULL;
}

/************************************************************************/
/*                          AssembleFeature()                           */
/*                                                                      */
/*      Assemble an OGR feature based on a feature record.              */
/************************************************************************/

OGRFeature *S57Reader::AssembleFeature( DDFRecord * poRecord,
                                        OGRFeatureDefn * poTarget )

{
    int         nPRIM, nOBJL;
    OGRFeatureDefn *poFDefn;

/* -------------------------------------------------------------------- */
/*      Find the feature definition to use.  Currently this is based    */
/*      on the primitive, but eventually this should be based on the    */
/*      object class (FRID.OBJL) in some cases, and the primitive in    */
/*      others.                                                         */
/* -------------------------------------------------------------------- */
    poFDefn = FindFDefn( poRecord );
    if( poFDefn == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Does this match our target feature definition?  If not skip     */
/*      this feature.                                                   */
/* -------------------------------------------------------------------- */
    if( poTarget != NULL && poFDefn != poTarget )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create the new feature object.                                  */
/* -------------------------------------------------------------------- */
    OGRFeature          *poFeature;

    poFeature = new OGRFeature( poFDefn );

/* -------------------------------------------------------------------- */
/*      Assign a few standard feature attribues.                        */
/* -------------------------------------------------------------------- */
    nOBJL = poRecord->GetIntSubfield( "FRID", 0, "OBJL", 0 );
    poFeature->SetField( "OBJL", nOBJL );

    poFeature->SetField( "GRUP",
                         poRecord->GetIntSubfield( "FRID", 0, "GRUP", 0 ));
    poFeature->SetField( "RVER",
                         poRecord->GetIntSubfield( "FRID", 0, "RVER", 0 ));
    poFeature->SetField( "AGEN",
                         poRecord->GetIntSubfield( "FOID", 0, "AGEN", 0 ));
    poFeature->SetField( "FIDN",
                         poRecord->GetIntSubfield( "FOID", 0, "FIDN", 0 ));
    poFeature->SetField( "FIDS",
                         poRecord->GetIntSubfield( "FOID", 0, "FIDS", 0 ));

/* -------------------------------------------------------------------- */
/*      Generate long name, if requested.                               */
/* -------------------------------------------------------------------- */
    if( bGenerateLNAM )
    {
        GenerateLNAMAndRefs( poRecord, poFeature );
    }

/* -------------------------------------------------------------------- */
/*      Apply object class specific attributes, if supported.           */
/* -------------------------------------------------------------------- */
    if( poRegistrar != NULL )
        ApplyObjectClassAttributes( poRecord, poFeature );

/* -------------------------------------------------------------------- */
/*      Find and assign spatial component.                              */
/* -------------------------------------------------------------------- */
    nPRIM = poRecord->GetIntSubfield( "FRID", 0, "PRIM", 0 );

    if( nPRIM == PRIM_P )
    {
        if( nOBJL == 129 ) /* SOUNDG */
            AssembleSoundingGeometry( poRecord, poFeature );
        else
            AssemblePointGeometry( poRecord, poFeature );
    }
    else if( nPRIM == PRIM_L )
    {
        AssembleLineGeometry( poRecord, poFeature );
    }
    else if( nPRIM == PRIM_A )
    {
        AssembleAreaGeometry( poRecord, poFeature );
    }

    return poFeature;
}

/************************************************************************/
/*                     ApplyObjectClassAttributes()                     */
/************************************************************************/

void S57Reader::ApplyObjectClassAttributes( DDFRecord * poRecord,
                                            OGRFeature * poFeature )

{
/* -------------------------------------------------------------------- */
/*      ATTF Attributes                                                 */
/* -------------------------------------------------------------------- */
    DDFField    *poATTF = poRecord->FindField( "ATTF" );
    int         nAttrCount, iAttr;

    if( poATTF == NULL )
        return;

    nAttrCount = poATTF->GetRepeatCount();
    for( iAttr = 0; iAttr < nAttrCount; iAttr++ )
    {
        int     nAttrId = poRecord->GetIntSubfield("ATTF",0,"ATTL",iAttr);
        
        poFeature->SetField( poRegistrar->GetAttrAcronym(nAttrId),
                          poRecord->GetStringSubfield("ATTF",0,"ATVL",iAttr) );
    }
    
/* -------------------------------------------------------------------- */
/*      NATF (national) attributes                                      */
/* -------------------------------------------------------------------- */
    DDFField    *poNATF = poRecord->FindField( "NATF" );

    if( poNATF == NULL )
        return;

    nAttrCount = poNATF->GetRepeatCount();
    for( iAttr = 0; iAttr < nAttrCount; iAttr++ )
    {
        int     nAttrId = poRecord->GetIntSubfield("NATF",0,"ATTL",iAttr);
        
        poFeature->SetField( poRegistrar->GetAttrAcronym(nAttrId),
                          poRecord->GetStringSubfield("NATF",0,"ATVL",iAttr) );
    }
}

/************************************************************************/
/*                        GenerateLNAMAndRefs()                         */
/************************************************************************/

void S57Reader::GenerateLNAMAndRefs( DDFRecord * poRecord,
                                     OGRFeature * poFeature )

{
    char	szLNAM[32];
        
/* -------------------------------------------------------------------- */
/*      Apply the LNAM to the object.                                   */
/* -------------------------------------------------------------------- */
    sprintf( szLNAM, "%04X%08X%04X",
             poFeature->GetFieldAsInteger( "AGEN" ),
             poFeature->GetFieldAsInteger( "FIDN" ),
             poFeature->GetFieldAsInteger( "FIDS" ) );
    poFeature->SetField( "LNAM", szLNAM );

/* -------------------------------------------------------------------- */
/*      Do we have references to other features.                        */
/* -------------------------------------------------------------------- */
    DDFField	*poFFPT;

    poFFPT = poRecord->FindField( "FFPT" );

    if( poFFPT == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Apply references.                                               */
/* -------------------------------------------------------------------- */
    int		nRefCount = poFFPT->GetRepeatCount();
    DDFSubfieldDefn *poLNAM;
    char	**papszRefs = NULL;

    poLNAM = poFFPT->GetFieldDefn()->FindSubfieldDefn( "LNAM" );
    if( poLNAM == NULL )
        return;

    for( int iRef = 0; iRef < nRefCount; iRef++ )
    {
        unsigned char *pabyData;

        pabyData = (unsigned char *)
            poFFPT->GetSubfieldData( poLNAM, NULL, iRef );
        
        sprintf( szLNAM, "%02X%02X%02X%02X%02X%02X%02X%02X",
                 pabyData[1], pabyData[0], /* AGEN */
                 pabyData[5], pabyData[4], pabyData[3], pabyData[2], /* FIDN */
                 pabyData[7], pabyData[6] );

        papszRefs = CSLAddString( papszRefs, szLNAM );
    }

    poFeature->SetField( "LNAM_REFS", papszRefs );
    CSLDestroy( papszRefs );
}

/************************************************************************/
/*                             FetchPoint()                             */
/*                                                                      */
/*      Fetch the location of a spatial point object.                   */
/************************************************************************/

int S57Reader::FetchPoint( int nRCNM, int nRCID,
                           double * pdfX, double * pdfY, double * pdfZ )

{
    DDFRecord   *poSRecord;
    
    if( nRCNM == RCNM_VI )
        poSRecord = oVI_Index.FindRecord( nRCID );
    else
        poSRecord = oVC_Index.FindRecord( nRCID );

    if( poSRecord == NULL )
        return FALSE;

    double      dfX = 0.0, dfY = 0.0, dfZ = 0.0;

    if( poSRecord->FindField( "SG2D" ) != NULL )
    {
        dfX = poSRecord->GetIntSubfield("SG2D",0,"XCOO",0) / (double)nCOMF;
        dfY = poSRecord->GetIntSubfield("SG2D",0,"YCOO",0) / (double)nCOMF;
    }
    else if( poSRecord->FindField( "SG3D" ) != NULL )
    {
        dfX = poSRecord->GetIntSubfield("SG3D",0,"XCOO",0) / (double)nCOMF;
        dfY = poSRecord->GetIntSubfield("SG3D",0,"YCOO",0) / (double)nCOMF;
        dfZ = poSRecord->GetIntSubfield("SG3D",0,"VE3D",0) / (double)nSOMF;
    }
    else
        return FALSE;

    if( pdfX != NULL )
        *pdfX = dfX;
    if( pdfY != NULL )
        *pdfY = dfY;
    if( pdfZ != NULL )
        *pdfZ = dfZ;

    return TRUE;
}

/************************************************************************/
/*                       AssemblePointGeometry()                        */
/************************************************************************/

void S57Reader::AssemblePointGeometry( DDFRecord * poFRecord,
                                       OGRFeature * poFeature )

{
    DDFField    *poFSPT;
    int         nRCNM, nRCID;

/* -------------------------------------------------------------------- */
/*      Feature the spatial record containing the point.                */
/* -------------------------------------------------------------------- */
    poFSPT = poFRecord->FindField( "FSPT" );
    if( poFSPT == NULL )
        return;

    CPLAssert( poFSPT->GetRepeatCount() == 1 );
        
    nRCID = ParseName( poFSPT, 0, &nRCNM );

    double      dfX = 0.0, dfY = 0.0, dfZ = 0.0;

    if( !FetchPoint( nRCNM, nRCID, &dfX, &dfY, &dfZ ) )
    {
        CPLAssert( FALSE );
        return;
    }

    poFeature->SetGeometryDirectly( new OGRPoint( dfX, dfY, dfZ ) );
}

/************************************************************************/
/*                      AssembleSoundingGeometry()                      */
/************************************************************************/

void S57Reader::AssembleSoundingGeometry( DDFRecord * poFRecord,
                                          OGRFeature * poFeature )

{
    DDFField    *poFSPT;
    int         nRCNM, nRCID;
    DDFRecord   *poSRecord;
    

/* -------------------------------------------------------------------- */
/*      Feature the spatial record containing the point.                */
/* -------------------------------------------------------------------- */
    poFSPT = poFRecord->FindField( "FSPT" );
    if( poFSPT == NULL )
        return;

    CPLAssert( poFSPT->GetRepeatCount() == 1 );
        
    nRCID = ParseName( poFSPT, 0, &nRCNM );

    if( nRCNM == RCNM_VI )
        poSRecord = oVI_Index.FindRecord( nRCID );
    else
        poSRecord = oVC_Index.FindRecord( nRCID );

    if( poSRecord == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Extract vertices.                                               */
/* -------------------------------------------------------------------- */
    OGRMultiPoint	*poMP = new OGRMultiPoint();
    DDFField		*poField;
    int			nPointCount, i, nBytesLeft;
    DDFSubfieldDefn    *poXCOO, *poYCOO, *poVE3D;
    const char	       *pachData;

    poField = poSRecord->FindField( "SG2D" );
    if( poField == NULL )
        poField = poSRecord->FindField( "SG3D" );
    if( poField == NULL )
        return;

    poXCOO = poField->GetFieldDefn()->FindSubfieldDefn( "XCOO" );
    poYCOO = poField->GetFieldDefn()->FindSubfieldDefn( "YCOO" );
    poVE3D = poField->GetFieldDefn()->FindSubfieldDefn( "VE3D" );

    nPointCount = poField->GetRepeatCount();

    pachData = poField->GetData();
    nBytesLeft = poField->GetDataSize();

    for( i = 0; i < nPointCount; i++ )
    {
        double		dfX, dfY, dfZ = 0.0;
        int		nBytesConsumed;

        dfX = poXCOO->ExtractIntData( pachData, nBytesLeft,
                                      &nBytesConsumed ) / (double) nCOMF;
        nBytesLeft -= nBytesConsumed;
        pachData += nBytesConsumed;
        
        dfY = poYCOO->ExtractIntData( pachData, nBytesLeft,
                                      &nBytesConsumed ) / (double) nCOMF;
        nBytesLeft -= nBytesConsumed;
        pachData += nBytesConsumed;

        if( poVE3D != NULL )
        {
            dfZ = poYCOO->ExtractIntData( pachData, nBytesLeft,
                                          &nBytesConsumed ) / (double) nSOMF;
            nBytesLeft -= nBytesConsumed;
            pachData += nBytesConsumed;
        }

        poMP->addGeometryDirectly( new OGRPoint( dfX, dfY, dfZ ) );
    }

    poFeature->SetGeometryDirectly( poMP );
}

/************************************************************************/
/*                        AssembleLineGeometry()                        */
/************************************************************************/

void S57Reader::AssembleLineGeometry( DDFRecord * poFRecord,
                                      OGRFeature * poFeature )

{
    DDFField    *poFSPT;
    int         nEdgeCount;
    OGRLineString *poLine = new OGRLineString();

/* -------------------------------------------------------------------- */
/*      Find the FSPT field.                                            */
/* -------------------------------------------------------------------- */
    poFSPT = poFRecord->FindField( "FSPT" );
    if( poFSPT == NULL )
        return;

    nEdgeCount = poFSPT->GetRepeatCount();

/* ==================================================================== */
/*      Loop collecting edges.                                          */
/* ==================================================================== */
    for( int iEdge = 0; iEdge < nEdgeCount; iEdge++ )
    {
        DDFRecord       *poSRecord;
        int             nRCID;

/* -------------------------------------------------------------------- */
/*      Find the spatial record for this edge.                          */
/* -------------------------------------------------------------------- */
        nRCID = ParseName( poFSPT, iEdge );

        poSRecord = oVE_Index.FindRecord( nRCID );
        if( poSRecord == NULL )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Couldn't find spatial record %d.\n", nRCID );
            continue;
        }
    
/* -------------------------------------------------------------------- */
/*      Establish the number of vertices, and whether we need to        */
/*      reverse or not.                                                 */
/* -------------------------------------------------------------------- */
        int             nVCount;
        int             nStart, nEnd, nInc;
        DDFField        *poSG2D = poSRecord->FindField( "SG2D" );
        DDFSubfieldDefn *poXCOO, *poYCOO;

        if( poSG2D != NULL )
        {
            poXCOO = poSG2D->GetFieldDefn()->FindSubfieldDefn("XCOO");
            poYCOO = poSG2D->GetFieldDefn()->FindSubfieldDefn("YCOO");

            nVCount = poSG2D->GetRepeatCount();
        }
        else
            nVCount = 0;

        if( poFRecord->GetIntSubfield( "FSPT", 0, "ORNT", iEdge ) == 2 )
        {
            nStart = nVCount-1;
            nEnd = 0;
            nInc = -1;
        }
        else
        {
            nStart = 0;
            nEnd = nVCount-1;
            nInc = 1;
        }

/* -------------------------------------------------------------------- */
/*      Add the start node, if this is the first edge.                  */
/* -------------------------------------------------------------------- */
        if( iEdge == 0 )
        {
            int         nVC_RCID;
            double      dfX, dfY;
            
            if( nInc == 1 )
                nVC_RCID = ParseName( poSRecord->FindField( "VRPT" ), 0 );
            else
                nVC_RCID = ParseName( poSRecord->FindField( "VRPT" ), 1 );

            if( FetchPoint( RCNM_VC, nVC_RCID, &dfX, &dfY ) )
                poLine->addPoint( dfX, dfY );
        }
        
/* -------------------------------------------------------------------- */
/*      Collect the vertices.                                           */
/* -------------------------------------------------------------------- */
        int             nVBase = poLine->getNumPoints();
        
        poLine->setNumPoints( nVCount+nVBase );

        for( int i = nStart; i != nEnd+nInc; i += nInc )
        {
            double      dfX, dfY;
            const char  *pachData;
            int         nBytesRemaining;

            pachData = poSG2D->GetSubfieldData(poXCOO,&nBytesRemaining,i);
                
            dfX = poXCOO->ExtractIntData(pachData,nBytesRemaining,NULL)
                / (double) nCOMF;

            pachData = poSG2D->GetSubfieldData(poYCOO,&nBytesRemaining,i);

            dfY = poXCOO->ExtractIntData(pachData,nBytesRemaining,NULL)
                / (double) nCOMF;
                
            poLine->setPoint( nVBase++, dfX, dfY );
        }

/* -------------------------------------------------------------------- */
/*      Add the end node.                                               */
/* -------------------------------------------------------------------- */
        {
            int         nVC_RCID;
            double      dfX, dfY;
            
            if( nInc == 1 )
                nVC_RCID = ParseName( poSRecord->FindField( "VRPT" ), 1 );
            else
                nVC_RCID = ParseName( poSRecord->FindField( "VRPT" ), 0 );

            if( FetchPoint( RCNM_VC, nVC_RCID, &dfX, &dfY ) )
                poLine->addPoint( dfX, dfY );
        }
    }

    poFeature->SetGeometryDirectly( poLine );
    CPLAssert( poLine->getNumPoints() > 0 );
}

/************************************************************************/
/*                        AssembleAreaGeometry()                        */
/************************************************************************/

void S57Reader::AssembleAreaGeometry( DDFRecord * poFRecord,
                                         OGRFeature * poFeature )

{
    DDFField    *poFSPT;
    int         nEdgeCount;
    OGRGeometryCollection * poLines = new OGRGeometryCollection();

/* -------------------------------------------------------------------- */
/*      Find the FSPT field.                                            */
/* -------------------------------------------------------------------- */
    poFSPT = poFRecord->FindField( "FSPT" );
    if( poFSPT == NULL )
        return;

    nEdgeCount = poFSPT->GetRepeatCount();

/* ==================================================================== */
/*      Loop collecting edges.                                          */
/* ==================================================================== */
    for( int iEdge = 0; iEdge < nEdgeCount; iEdge++ )
    {
        DDFRecord       *poSRecord;
        int             nRCID;

/* -------------------------------------------------------------------- */
/*      Find the spatial record for this edge.                          */
/* -------------------------------------------------------------------- */
        nRCID = ParseName( poFSPT, iEdge );

        poSRecord = oVE_Index.FindRecord( nRCID );
        if( poSRecord == NULL )
        {
            printf( "Couldn't find spatial record %d.\n", nRCID );
            continue;
        }
    
/* -------------------------------------------------------------------- */
/*      Establish the number of vertices, and whether we need to        */
/*      reverse or not.                                                 */
/* -------------------------------------------------------------------- */
        OGRLineString *poLine = new OGRLineString();
        
        int             nVCount;
        DDFField        *poSG2D = poSRecord->FindField( "SG2D" );
        DDFSubfieldDefn *poXCOO, *poYCOO;

        if( poSG2D != NULL )
        {
            poXCOO = poSG2D->GetFieldDefn()->FindSubfieldDefn("XCOO");
            poYCOO = poSG2D->GetFieldDefn()->FindSubfieldDefn("YCOO");

            nVCount = poSG2D->GetRepeatCount();
        }
        else
            nVCount = 0;

/* -------------------------------------------------------------------- */
/*      Add the start node.                                             */
/* -------------------------------------------------------------------- */
        {
            int         nVC_RCID;
            double      dfX, dfY;
            
            nVC_RCID = ParseName( poSRecord->FindField( "VRPT" ), 0 );

            if( FetchPoint( RCNM_VC, nVC_RCID, &dfX, &dfY ) )
                poLine->addPoint( dfX, dfY );
        }
        
/* -------------------------------------------------------------------- */
/*      Collect the vertices.                                           */
/* -------------------------------------------------------------------- */
        int             nVBase = poLine->getNumPoints();
        
        poLine->setNumPoints( nVCount+nVBase );

        for( int i = 0; i < nVCount; i++ )
        {
            double      dfX, dfY;
            const char  *pachData;
            int         nBytesRemaining;

            pachData = poSG2D->GetSubfieldData(poXCOO,&nBytesRemaining,i);
                
            dfX = poXCOO->ExtractIntData(pachData,nBytesRemaining,NULL)
                / (double) nCOMF;

            pachData = poSG2D->GetSubfieldData(poYCOO,&nBytesRemaining,i);

            dfY = poXCOO->ExtractIntData(pachData,nBytesRemaining,NULL)
                / (double) nCOMF;
                
            poLine->setPoint( nVBase++, dfX, dfY );
        }

/* -------------------------------------------------------------------- */
/*      Add the end node.                                               */
/* -------------------------------------------------------------------- */
        {
            int         nVC_RCID;
            double      dfX, dfY;
            
            nVC_RCID = ParseName( poSRecord->FindField( "VRPT" ), 1 );

            if( FetchPoint( RCNM_VC, nVC_RCID, &dfX, &dfY ) )
                poLine->addPoint( dfX, dfY );
        }

        poLines->addGeometryDirectly( poLine );
    }

/* -------------------------------------------------------------------- */
/*      Build lines into a polygon.                                     */
/* -------------------------------------------------------------------- */
    OGRPolygon  *poPolygon;
    OGRErr      eErr;

    poPolygon = OGRBuildPolygonFromEdges( poLines, TRUE, &eErr );
    if( eErr != OGRERR_NONE )
        CPLDebug( "S57", "Polygon assembly failed" );

    delete poLines;

    if( poPolygon != NULL )
        poFeature->SetGeometryDirectly( poPolygon );
}

/************************************************************************/
/*                             FindFDefn()                              */
/*                                                                      */
/*      Find the OGRFeatureDefn corresponding to the passed feature     */
/*      record.  It will search based on geometry class, or object      */
/*      class depending on the bClassBased setting.                     */
/************************************************************************/

OGRFeatureDefn * S57Reader::FindFDefn( DDFRecord * poRecord )

{
    if( poRegistrar != NULL )
    {
        int     nOBJL = poRecord->GetIntSubfield( "FRID", 0, "OBJL", 0 );

        if( !poRegistrar->SelectClass( nOBJL ) )
            return NULL;

        for( int i = 0; i < nFDefnCount; i++ )
        {
            if( EQUAL(papoFDefnList[i]->GetName(),
                      poRegistrar->GetAcronym()) )
                return papoFDefnList[i];
        }
        
        return NULL;
    }
    else
    {
        int     nPRIM = poRecord->GetIntSubfield( "FRID", 0, "PRIM", 0 );
        OGRwkbGeometryType eGType;
        
        if( nPRIM == PRIM_P )
            eGType = wkbPoint;
        else if( nPRIM == PRIM_L )
            eGType = wkbLineString;
        else if( nPRIM == PRIM_A )
            eGType = wkbPolygon;
        else
            eGType = wkbNone;

        for( int i = 0; i < nFDefnCount; i++ )
        {
            if( papoFDefnList[i]->GetGeomType() == eGType )
                return papoFDefnList[i];
        }
    }

    return NULL;
}

/************************************************************************/
/*                             ParseName()                              */
/*                                                                      */
/*      Pull the RCNM and RCID values from a NAME field.  The RCID      */
/*      is returned and the RCNM can be gotten via the pnRCNM argument. */
/************************************************************************/

int S57Reader::ParseName( DDFField * poField, int nIndex, int * pnRCNM )

{
    unsigned char       *pabyData;

    pabyData = (unsigned char *)
        poField->GetSubfieldData(
            poField->GetFieldDefn()->FindSubfieldDefn( "NAME" ),
            NULL, nIndex );

    if( pnRCNM != NULL )
        *pnRCNM = pabyData[0];

    return pabyData[1]
         + pabyData[2] * 256
         + pabyData[3] * 256 * 256
         + pabyData[4] * 256 * 256 * 256;
}

/************************************************************************/
/*                           AddFeatureDefn()                           */
/************************************************************************/

void S57Reader::AddFeatureDefn( OGRFeatureDefn * poFDefn )

{
    nFDefnCount++;
    papoFDefnList = (OGRFeatureDefn **)
        CPLRealloc(papoFDefnList, sizeof(OGRFeatureDefn*)*nFDefnCount );

    papoFDefnList[nFDefnCount-1] = poFDefn;
}

/************************************************************************/
/*                     GenerateStandardAttributes()                     */
/*                                                                      */
/*      Attach standard feature attributes to a feature definition.     */
/************************************************************************/

void S57Reader::GenerateStandardAttributes( OGRFeatureDefn *poFDefn )

{
    OGRFieldDefn        oField( "", OFTInteger );

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
    oField.Set( "AGEN", OFTInteger, 2, 0 );
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
    if( bGenerateLNAM )
    {
        oField.Set( "LNAM", OFTString, 16, 0 );
        poFDefn->AddFieldDefn( &oField );
        
        oField.Set( "LNAM_REFS", OFTStringList, 16, 0 );
        poFDefn->AddFieldDefn( &oField );
    }
}

/************************************************************************/
/*                      GenerateGeomFeatureDefn()                       */
/************************************************************************/

OGRFeatureDefn *S57Reader::GenerateGeomFeatureDefn( OGRwkbGeometryType eGType )

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
    else
        return NULL;

    GenerateStandardAttributes( poFDefn );

    return poFDefn;
}

/************************************************************************/
/*                      GenerateObjectClassDefn()                       */
/************************************************************************/

OGRFeatureDefn *S57Reader::GenerateObjectClassDefn( S57ClassRegistrar *poCR,
                                                    int nOBJL )

{
    OGRFeatureDefn      *poFDefn = NULL;
    char               **papszGeomPrim;

    if( !poCR->SelectClass( nOBJL ) )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Create the feature definition based on the object class         */
/*      acronym.                                                        */
/* -------------------------------------------------------------------- */
    poFDefn = new OGRFeatureDefn( poCR->GetAcronym() );

/* -------------------------------------------------------------------- */
/*      Try and establish the geometry type.  If more than one          */
/*      geometry type is allowed we just fall back to wkbUnknown.       */
/* -------------------------------------------------------------------- */
    papszGeomPrim = poCR->GetPrimitives();
    if( CSLCount(papszGeomPrim) == 0 )
    {
        poFDefn->SetGeomType( wkbNone );
    }
    else if( CSLCount(papszGeomPrim) > 1 )
    {
        // leave as unknown geometry type.
    }
    else if( EQUAL(papszGeomPrim[0],"Point") )
    {
        if( EQUAL(poCR->GetAcronym(),"SOUNDG") )
        {
            if( bSplitMultiPoint )
                poFDefn->SetGeomType( wkbMultiPoint );
            else
                poFDefn->SetGeomType( wkbPoint25D );
        }
        else
            poFDefn->SetGeomType( wkbPoint );
    }
    else if( EQUAL(papszGeomPrim[0],"Area") )
    {
        poFDefn->SetGeomType( wkbPolygon );
    }
    else if( EQUAL(papszGeomPrim[0],"Line") )
    {
        poFDefn->SetGeomType( wkbLineString );
    }
    
/* -------------------------------------------------------------------- */
/*      Add the standard attributes.                                    */
/* -------------------------------------------------------------------- */
    GenerateStandardAttributes( poFDefn );

/* -------------------------------------------------------------------- */
/*      Add the attributes specific to this object class.               */
/* -------------------------------------------------------------------- */
    char        **papszAttrList = poCR->GetAttributeList();

    for( int iAttr = 0;
         papszAttrList != NULL && papszAttrList[iAttr] != NULL;
         iAttr++ )
    {
        int     iAttrIndex = poCR->FindAttrByAcronym( papszAttrList[iAttr] );

        if( iAttrIndex == -1 )
        {
            CPLDebug( "S57", "Can't find attribute %s from class %s:%s.\n",
                      papszAttrList[iAttr],
                      poCR->GetAcronym(),
                      poCR->GetDescription() );
            continue;
        }

        OGRFieldDefn    oField( papszAttrList[iAttr], OFTInteger );
        
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
            // what to do?
            break;
        }

        poFDefn->AddFieldDefn( &oField );
    }

    return poFDefn;
}

/************************************************************************/
/*                          CollectClassList()                          */
/*                                                                      */
/*      Establish the list of classes (unique OBJL values) that         */
/*      occur in this dataset.                                          */
/************************************************************************/

int S57Reader::CollectClassList(int *panClassCount, int nMaxClass )

{
    int         bSuccess = TRUE;

    if( !bFileIngested )
        Ingest();

    for( int iFEIndex = 0; iFEIndex < oFE_Index.GetCount(); iFEIndex++ )
    {
        DDFRecord *poRecord = oFE_Index.GetByIndex( iFEIndex );
        int     nOBJL = poRecord->GetIntSubfield( "FRID", 0, "OBJL", 0 );

        if( nOBJL >= nMaxClass )
            bSuccess = FALSE;
        else
            panClassCount[nOBJL]++;

    }

    return bSuccess;
}
