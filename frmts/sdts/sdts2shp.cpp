/******************************************************************************
 * $Id$
 *
 * Project:  SDTS Translator
 * Purpose:  Mainline for converting to ArcView Shapefiles.
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
 * Revision 1.2  1999/09/02 03:40:03  warmerda
 * added indexed readers
 *
 * Revision 1.1  1999/05/07 13:44:57  warmerda
 * New
 *
 */

#include "sdts_al.h"
#include "shapefil.h"

static int  bVerbose = FALSE;

static void WriteLineShapefile( const char *, SDTS_IREF *, SDTS_CATD *,
                                const char * );
static void WritePointShapefile( const char *, SDTS_IREF *, SDTS_CATD *,
                                 const char * );
static void WriteAttributeDBF( const char *, SDTS_IREF *, SDTS_CATD *,
                               const char * );
static void WritePolygonShapefile( const char *, SDTSTransfer *, 
                                   const char * );

static void
AddPrimaryAttrToDBFSchema( DBFHandle hDBF, SDTSTransfer * poTransfer,
                           char ** papszModuleList );
static void
WritePrimaryAttrToDBF( DBFHandle hDBF, int nRecord, 
                       SDTSTransfer *,
                       SDTSFeature * poFeature );

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( "Usage: sdts2shp CATD_filename [-o shapefile_name]\n"
            "                [-m module_name] [-v]\n"
            "\n"
            "Modules include `LE01', `PC01', `NP01' and `ARDF'\n" );
    
    exit( 1 );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    int		i;
    const char	*pszCATDFilename = NULL;
    const char  *pszMODN = "LE01";
    char  	*pszShapefile = "sdts_out.shp";
    SDTSTransfer oTransfer;
   

/* -------------------------------------------------------------------- */
/*      Interpret commandline switches.                                 */
/* -------------------------------------------------------------------- */
    if( nArgc < 2 )
        Usage();

    pszCATDFilename = papszArgv[1];

    for( i = 2; i < nArgc; i++ )
    {
        if( EQUAL(papszArgv[i],"-m") && i+1 < nArgc )
            pszMODN = papszArgv[++i];
        else if( EQUAL(papszArgv[i],"-o") && i+1 < nArgc )
            pszShapefile = papszArgv[++i];
        else if( EQUAL(papszArgv[i],"-v") )
            bVerbose = TRUE;
        else
        {
            printf( "Incomplete, or unsupported option `%s'\n\n",
                    papszArgv[i] );
            Usage();
        }
    }

/* -------------------------------------------------------------------- */
/*      Massage shapefile name to have no extension.                    */
/* -------------------------------------------------------------------- */
    pszShapefile = strdup( pszShapefile );
    for( i = strlen(pszShapefile)-1; i >= 0; i-- )
    {
        if( pszShapefile[i] == '.' )
        {
            pszShapefile[i] = '\0';
            break;
        }
        else if( pszShapefile[i] == '/' || pszShapefile[i] == '\\' )
            break;
    }

/* -------------------------------------------------------------------- */
/*      Open the transfer.                                              */
/* -------------------------------------------------------------------- */
    if( !oTransfer.Open( pszCATDFilename ) )
    {
        fprintf( stderr,
                 "Failed to read CATD file `%s'\n",
                 pszCATDFilename );
        exit( 100 );
    }

/* -------------------------------------------------------------------- */
/*      Dump available layer in verbose mode.                           */
/* -------------------------------------------------------------------- */
    if( bVerbose )
    {
        printf( "Layers:\n" );
        for( i = 0; i < oTransfer.GetLayerCount(); i++ )
        {
            int		iCATDEntry = oTransfer.GetLayerCATDEntry(i);
            
            printf( "  %s: `%s'\n",
                    oTransfer.GetCATD()->GetEntryModule(iCATDEntry),
                    oTransfer.GetCATD()->GetEntryTypeDesc(iCATDEntry) );
        }
        printf( "\n" );
    }
        
/* -------------------------------------------------------------------- */
/*      If the module is an LE module, write it to an Arc file.         */
/* -------------------------------------------------------------------- */
    if( pszMODN[0] == 'L' )
    {
        WriteLineShapefile( pszShapefile, oTransfer.GetIREF(),
                            oTransfer.GetCATD(), pszMODN );
    }

/* -------------------------------------------------------------------- */
/*      If the module is an attribute primary one, dump to DBF.		*/
/* -------------------------------------------------------------------- */
    else if( pszMODN[0] == 'A' )
    {
        WriteAttributeDBF( pszShapefile, oTransfer.GetIREF(),
                           oTransfer.GetCATD(), pszMODN );
    }

/* -------------------------------------------------------------------- */
/*      If the module is a point one, dump to Shapefile.                */
/* -------------------------------------------------------------------- */
    else if( pszMODN[0] == 'N' )
    {
        WritePointShapefile( pszShapefile, oTransfer.GetIREF(),
                             oTransfer.GetCATD(), pszMODN );
    }

/* -------------------------------------------------------------------- */
/*      If the module is a polygon one, dump to Shapefile.              */
/* -------------------------------------------------------------------- */
    else if( pszMODN[0] == 'P' )
    {
        WritePolygonShapefile( pszShapefile, &oTransfer, pszMODN );
    }
}

/************************************************************************/
/*                         WriteLineShapefile()                         */
/************************************************************************/

static void WriteLineShapefile( const char * pszShapefile,
                                SDTS_IREF * poIREF, SDTS_CATD * poCATD,
                                const char * pszMODN )

{
    SDTSLineReader	oLineReader( poIREF );
    int			i;
    
/* -------------------------------------------------------------------- */
/*      Open the line module file.                                      */
/* -------------------------------------------------------------------- */
    if( !oLineReader.Open( poCATD->GetModuleFilePath( pszMODN ) ) )
    {
        fprintf( stderr, "Failed to open %s.\n",
                 poCATD->GetModuleFilePath( pszMODN ) );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Create the Shapefile.                                           */
/* -------------------------------------------------------------------- */
    SHPHandle	hSHP;

    hSHP = SHPCreate( pszShapefile, SHPT_ARC );
    if( hSHP == NULL )
    {
        fprintf( stderr, "Unable to create shapefile `%s'\n",
                 pszShapefile );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Create the database file, and our basic set of attributes.      */
/* -------------------------------------------------------------------- */
    DBFHandle	hDBF;
    int		nLeftPolyField, nRightPolyField;
    int		anAttribField[MAX_ATID];
    int		nStartNodeField, nEndNodeField, nSDTSRecordField;
    char	szDBFFilename[1024];

    sprintf( szDBFFilename, "%s.dbf", pszShapefile );

    hDBF = DBFCreate( szDBFFilename );
    if( hDBF == NULL )
    {
        fprintf( stderr, "Unable to create shapefile .dbf for `%s'\n",
                 pszShapefile );
        return;
    }

    nSDTSRecordField = DBFAddField( hDBF, "SDTSRecId", FTInteger, 8, 0 );
    nLeftPolyField = DBFAddField( hDBF, "LeftPoly", FTString, 12, 0 );
    nRightPolyField = DBFAddField( hDBF, "RightPoly", FTString, 12, 0 );
    nStartNodeField = DBFAddField( hDBF, "StartNode", FTString, 12, 0 );
    nEndNodeField = DBFAddField( hDBF, "EndNode", FTString, 12, 0 );
    
    for( i = 0; i < MAX_ATID; i++ )
    {
        char	szID[20];
        
        sprintf( szID, "ARef_%d", i );
        anAttribField[i] = DBFAddField( hDBF, szID, FTString, 12, 0 );
    }

/* ==================================================================== */
/*      Process all the line features in the module.                    */
/* ==================================================================== */
    SDTSRawLine	*poRawLine;
        
    while( (poRawLine = oLineReader.GetNextLine()) != NULL )
    {
        int		iShape;
        
/* -------------------------------------------------------------------- */
/*      Write out a shape with the vertices.                            */
/* -------------------------------------------------------------------- */
        SHPObject	*psShape;

        psShape = SHPCreateSimpleObject( SHPT_ARC, poRawLine->nVertices,
                                         poRawLine->padfX, poRawLine->padfY,
                                         poRawLine->padfZ );

        iShape = SHPWriteObject( hSHP, -1, psShape );

        SHPDestroyObject( psShape );

/* -------------------------------------------------------------------- */
/*      Write out the attributes.                                       */
/* -------------------------------------------------------------------- */
        char	szID[13];

        DBFWriteIntegerAttribute( hDBF, iShape, nSDTSRecordField,
                                  poRawLine->oModId.nRecord );
        
        sprintf( szID, "%s:%ld",
                 poRawLine->oLeftPoly.szModule,
                 poRawLine->oLeftPoly.nRecord );
        DBFWriteStringAttribute( hDBF, iShape, nLeftPolyField, szID );

        sprintf( szID, "%s:%ld",
                 poRawLine->oRightPoly.szModule,
                 poRawLine->oRightPoly.nRecord );
        DBFWriteStringAttribute( hDBF, iShape, nRightPolyField, szID );

        sprintf( szID, "%s:%ld",
                 poRawLine->oStartNode.szModule,
                 poRawLine->oStartNode.nRecord );
        DBFWriteStringAttribute( hDBF, iShape, nStartNodeField, szID );

        sprintf( szID, "%s:%ld",
                 poRawLine->oEndNode.szModule,
                 poRawLine->oEndNode.nRecord );
        DBFWriteStringAttribute( hDBF, iShape, nEndNodeField, szID );

        for( i = 0; i < MAX_ATID; i++ )
        {
            char	szID[12];

            sprintf( szID, "%s:%ld",
                     poRawLine->aoATID[i].szModule,
                     poRawLine->aoATID[i].nRecord );
            DBFWriteStringAttribute( hDBF, iShape, anAttribField[i], szID );
        }

        delete poRawLine;
    }

/* -------------------------------------------------------------------- */
/*      Close, and cleanup.                                             */
/* -------------------------------------------------------------------- */
    DBFClose( hDBF );
    SHPClose( hSHP );
    
    oLineReader.Close();
}    

/************************************************************************/
/*                        WritePointShapefile()                         */
/************************************************************************/

static void WritePointShapefile( const char * pszShapefile,
                                 SDTS_IREF * poIREF, SDTS_CATD * poCATD,
                                 const char * pszMODN )

{
    SDTSPointReader	oPointReader( poIREF );
    int			i;
    
/* -------------------------------------------------------------------- */
/*      Open the line module file.                                      */
/* -------------------------------------------------------------------- */
    if( !oPointReader.Open( poCATD->GetModuleFilePath( pszMODN ) ) )
    {
        fprintf( stderr, "Failed to open %s.\n",
                 poCATD->GetModuleFilePath( pszMODN ) );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Create the Shapefile.                                           */
/* -------------------------------------------------------------------- */
    SHPHandle	hSHP;

    hSHP = SHPCreate( pszShapefile, SHPT_POINT );
    if( hSHP == NULL )
    {
        fprintf( stderr, "Unable to create shapefile `%s'\n",
                 pszShapefile );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Create the database file, and our basic set of attributes.      */
/* -------------------------------------------------------------------- */
    DBFHandle	hDBF;
    int		anAttribField[MAX_ATID];
    int		nAreaField, nSDTSRecordField;
    char	szDBFFilename[1024];

    sprintf( szDBFFilename, "%s.dbf", pszShapefile );

    hDBF = DBFCreate( szDBFFilename );
    if( hDBF == NULL )
    {
        fprintf( stderr, "Unable to create shapefile .dbf for `%s'\n",
                 pszShapefile );
        return;
    }

    nSDTSRecordField = DBFAddField( hDBF, "SDTSRecId", FTInteger, 8, 0 );
    nAreaField = DBFAddField( hDBF, "Area", FTString, 12, 0 );
    
    for( i = 0; i < MAX_ATID; i++ )
    {
        char	szID[20];
        
        sprintf( szID, "ARef_%d", i );
        anAttribField[i] = DBFAddField( hDBF, szID, FTString, 12, 0 );
    }

/* ==================================================================== */
/*      Process all the line features in the module.                    */
/* ==================================================================== */
    SDTSRawPoint	*poRawPoint;
        
    while( (poRawPoint = oPointReader.GetNextPoint()) != NULL )
    {
        int		iShape;
        
/* -------------------------------------------------------------------- */
/*      Write out a shape with the vertices.                            */
/* -------------------------------------------------------------------- */
        SHPObject	*psShape;

        psShape = SHPCreateSimpleObject( SHPT_POINT, 1,
                                         &(poRawPoint->dfX),
                                         &(poRawPoint->dfY),
                                         &(poRawPoint->dfZ) );

        iShape = SHPWriteObject( hSHP, -1, psShape );

        SHPDestroyObject( psShape );

/* -------------------------------------------------------------------- */
/*      Write out the attributes.                                       */
/* -------------------------------------------------------------------- */
        char	szID[13];

        DBFWriteIntegerAttribute( hDBF, iShape, nSDTSRecordField,
                                  poRawPoint->oModId.nRecord );
        
        sprintf( szID, "%s:%ld",
                 poRawPoint->oAreaId.szModule,
                 poRawPoint->oAreaId.nRecord );
        DBFWriteStringAttribute( hDBF, iShape, nAreaField, szID );

        for( i = 0; i < MAX_ATID; i++ )
        {
            char	szID[12];

            sprintf( szID, "%s:%ld",
                     poRawPoint->aoATID[i].szModule,
                     poRawPoint->aoATID[i].nRecord );
            DBFWriteStringAttribute( hDBF, iShape, anAttribField[i], szID );
        }

        delete poRawPoint;
    }

/* -------------------------------------------------------------------- */
/*      Close, and cleanup.                                             */
/* -------------------------------------------------------------------- */
    DBFClose( hDBF );
    SHPClose( hSHP );
    
    oPointReader.Close();
}    

/************************************************************************/
/*                         WriteAttributeDBF()                          */
/************************************************************************/

static void WriteAttributeDBF( const char * pszShapefile,
                               SDTS_IREF * poIREF, SDTS_CATD * poCATD,
                               const char * pszMODN )

{
    SDTSAttrReader	oAttrReader( poIREF );
    int			iSF;
    
/* -------------------------------------------------------------------- */
/*      Open the line module file.                                      */
/* -------------------------------------------------------------------- */
    if( !oAttrReader.Open( poCATD->GetModuleFilePath( pszMODN ) ) )
    {
        fprintf( stderr, "Failed to open %s.\n",
                 poCATD->GetModuleFilePath( pszMODN ) );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Create the database file, and our basic set of attributes.      */
/* -------------------------------------------------------------------- */
    DBFHandle	hDBF;
    char	szDBFFilename[1024];

    sprintf( szDBFFilename, "%s.dbf", pszShapefile );

    hDBF = DBFCreate( szDBFFilename );
    if( hDBF == NULL )
    {
        fprintf( stderr, "Unable to create shapefile .dbf for `%s'\n",
                 pszShapefile );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Read the first record so we can clone schema information off    */
/*      of it.                                                          */
/* -------------------------------------------------------------------- */
    DDFField *poSR;
    SDTSModId oModId;

    poSR = oAttrReader.GetNextRecord(&oModId);
    if( poSR == NULL )
    {
        fprintf( stderr,
                 "Didn't find any meaningful attribute records in %s.\n",
                 pszMODN );
        
        return;
    }

/* -------------------------------------------------------------------- */
/*      Create one field to hold the record identity.                   */
/* -------------------------------------------------------------------- */
    DBFAddField( hDBF, "SDTSRecId", FTInteger, 8, 0 );
    
/* -------------------------------------------------------------------- */
/*      Clone schema off the first record.  Eventually we need to       */
/*      get the information out of the DDR record, but it isn't         */
/*      clear to me how to accomplish that with the SDTS++ API.         */
/*                                                                      */
/*      The following approach may fail (dramatically) if some          */
/*      records do not include all subfields.  Furthermore, no          */
/*      effort is made to make DBF field names unique.  The SDTS        */
/*      attributes often have names much beyond the 14 character dbf    */
/*      limit which may result in non-unique attributes.                */
/* -------------------------------------------------------------------- */
    DDFFieldDefn 	*poFDefn = poSR->GetFieldDefn();
    
    for( iSF=0; iSF < poFDefn->GetSubfieldCount(); iSF++ )
    {
        DDFSubfieldDefn	*poSFDefn = poFDefn->GetSubfield( iSF );
        int		nWidth = poSFDefn->GetWidth();
            
        switch( poSFDefn->GetType() )
        {
          case DDFString:
            if( nWidth == 0 )
            {
                int		nMaxBytes;
                
                const char * pachData = poSR->GetSubfieldData(poSFDefn,
                                                              &nMaxBytes);

                nWidth = strlen(poSFDefn->ExtractStringData(pachData,
                                                            nMaxBytes, NULL ));
            }
            
            DBFAddField( hDBF, poSFDefn->GetName(), FTString, nWidth, 0 );
            break;

          case DDFInt:
            if( nWidth == 0 )
                nWidth = 9;

            DBFAddField( hDBF, poSFDefn->GetName(), FTInteger, nWidth, 0 );
            break;

          case DDFFloat:
            DBFAddField( hDBF, poSFDefn->GetName(), FTDouble, 18, 6 );
            break;

          default:
            fprintf( stderr,
                     "Dropping attribute `%s' of module `%s'.  "
                     "Type unsupported\n",
                     poSFDefn->GetName(), pszMODN );
            break;
        }
    }
    
/* ==================================================================== */
/*      Process all the records in the module.                          */
/* ==================================================================== */
    int		iRecord = 0;

    while( poSR != NULL )
    {
        int		iField = 1;
        
/* -------------------------------------------------------------------- */
/*      Write the record number ... the module is assumed.              */
/* -------------------------------------------------------------------- */
        DBFWriteIntegerAttribute( hDBF, iRecord, 0, oModId.nRecord );
        
/* -------------------------------------------------------------------- */
/*      Process each field in the record.                               */
/* -------------------------------------------------------------------- */
        for( iSF=0; iSF < poFDefn->GetSubfieldCount(); iSF++ )
        {
            DDFSubfieldDefn	*poSFDefn = poFDefn->GetSubfield( iSF );
            int			nMaxBytes;
            const char * 	pachData = poSR->GetSubfieldData(poSFDefn,
                                                                 &nMaxBytes);
            
            switch( poSFDefn->GetType() )
            {
              case DDFString:
                DBFWriteStringAttribute(hDBF,iRecord, iField,
                                        poSFDefn->ExtractStringData(pachData,
                                                                    nMaxBytes,
                                                                    NULL) );
                iField++;
                break;

              case DDFFloat:
                DBFWriteDoubleAttribute( hDBF, iRecord, iField, 
                                        poSFDefn->ExtractFloatData(pachData,
                                                                   nMaxBytes,
                                                                   NULL) );
                iField++;
                break;

              case DDFInt:
                DBFWriteIntegerAttribute( hDBF, iRecord, iField, 
                                        poSFDefn->ExtractIntData(pachData,
                                                                 nMaxBytes,
                                                                 NULL) );
                iField++;
                break;

              default:
                break;
            }
        }

/* -------------------------------------------------------------------- */
/*      Read another record.  Don't delete the previously returned      */
/*      record ... we don't really own it.                              */
/* -------------------------------------------------------------------- */
        poSR = oAttrReader.GetNextRecord(&oModId);

        iRecord++;
    }

/* -------------------------------------------------------------------- */
/*      Close, and cleanup.                                             */
/* -------------------------------------------------------------------- */
    DBFClose( hDBF );
    
    oAttrReader.Close();
}    

/************************************************************************/
/*                       WritePolygonShapefile()                        */
/************************************************************************/

static void WritePolygonShapefile( const char * pszShapefile,
                                   SDTSTransfer * poTransfer, 
                                   const char * pszMODN )

{
    SDTSPolygonReader *poPolyReader;

/* -------------------------------------------------------------------- */
/*      Fetch a reference to the indexed polygon reader.                */
/* -------------------------------------------------------------------- */
    poPolyReader = (SDTSPolygonReader *) 
        poTransfer->GetLayerIndexedReader( poTransfer->FindLayer( pszMODN ) );
    
    if( poPolyReader == NULL )
    {
        fprintf( stderr, "Failed to open %s.\n",
                 poTransfer->GetCATD()->GetModuleFilePath( pszMODN ) );
        return;
    }

/* -------------------------------------------------------------------- */
/*      To write polygons we need to build them from their related      */
/*      arcs.  We don't know off hand which arc (line) layers           */
/*      contribute so we process all line layers, attaching them to     */
/*      polygons as appropriate.                                        */
/* -------------------------------------------------------------------- */
    for( int iLineLayer = 0;
         iLineLayer < poTransfer->GetLayerCount();
         iLineLayer++ )
    {
        SDTSLineReader	*poLineReader;
        
        if( poTransfer->GetLayerType(iLineLayer) != SLTLine )
            continue;

        poLineReader = (SDTSLineReader *)
            poTransfer->GetLayerIndexedReader( iLineLayer );
        if( poLineReader == NULL )
            continue;

        poLineReader->AttachToPolygons( poTransfer );
    }

/* -------------------------------------------------------------------- */
/*      Create the Shapefile.                                           */
/* -------------------------------------------------------------------- */
    SHPHandle	hSHP;

    hSHP = SHPCreate( pszShapefile, SHPT_POLYGON );
    if( hSHP == NULL )
    {
        fprintf( stderr, "Unable to create shapefile `%s'\n",
                 pszShapefile );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Create the database file, and our basic set of attributes.      */
/* -------------------------------------------------------------------- */
    DBFHandle	hDBF;
    int		nSDTSRecordField;
    char	szDBFFilename[1024];

    sprintf( szDBFFilename, "%s.dbf", pszShapefile );

    hDBF = DBFCreate( szDBFFilename );
    if( hDBF == NULL )
    {
        fprintf( stderr, "Unable to create shapefile .dbf for `%s'\n",
                 pszShapefile );
        return;
    }

    nSDTSRecordField = DBFAddField( hDBF, "SDTSRecId", FTInteger, 8, 0 );
    AddPrimaryAttrToDBFSchema( hDBF, poTransfer,
                               poPolyReader->ScanModuleReferences() );

/* ==================================================================== */
/*      Process all the polygon features in the module.                 */
/* ==================================================================== */
    SDTSRawPolygon	*poRawPoly;
        
    while( (poRawPoly = (SDTSRawPolygon *) poPolyReader->GetNextFeature())
           != NULL )
    {
        int		iShape;

        if( poRawPoly->nEdges == 0 )
        {
            printf( "Skipping polygon %s with no edges.\n",
                    poRawPoly->oModId.GetName() );
            continue;
        }

        if( !poRawPoly->AssembleRings() )
        {
            printf( "Problem assembling rings for %s.  Write anyways.\n",
                    poRawPoly->oModId.GetName() );
        }

/* -------------------------------------------------------------------- */
/*      Write out a shape with the vertices.                            */
/* -------------------------------------------------------------------- */
        SHPObject	*psShape;

        psShape = SHPCreateObject( SHPT_POLYGON, -1, poRawPoly->nRings,
                                   poRawPoly->panRingStart, NULL,
                                   poRawPoly->nVertices,
                                   poRawPoly->padfX, 
                                   poRawPoly->padfY, 
                                   poRawPoly->padfZ,
                                   NULL );

        iShape = SHPWriteObject( hSHP, -1, psShape );

        SHPDestroyObject( psShape );

/* -------------------------------------------------------------------- */
/*      Write out the attributes.                                       */
/* -------------------------------------------------------------------- */
        DBFWriteIntegerAttribute( hDBF, iShape, nSDTSRecordField,
                                  poRawPoly->oModId.nRecord );
        WritePrimaryAttrToDBF( hDBF, iShape, poTransfer, poRawPoly );
    }

/* -------------------------------------------------------------------- */
/*      Close, and cleanup.                                             */
/* -------------------------------------------------------------------- */
    DBFClose( hDBF );
    SHPClose( hSHP );
}    

/************************************************************************/
/*                        AddPrimaryAttrToDBF()                         */
/*                                                                      */
/*      Add the fields from all the given primary attribute modules     */
/*      to the schema of the passed DBF file.                           */
/************************************************************************/

static void
AddPrimaryAttrToDBFSchema( DBFHandle hDBF, SDTSTransfer *poTransfer,
                           char ** papszModuleList )

{
    for( int iModule = 0;
         papszModuleList != NULL && papszModuleList[iModule] != NULL;
         iModule++ )
    {
        SDTSAttrReader	*poAttrReader;

/* -------------------------------------------------------------------- */
/*      Get a reader on the desired module.                             */
/* -------------------------------------------------------------------- */
        poAttrReader = (SDTSAttrReader *)
            poTransfer->GetLayerIndexedReader(
                poTransfer->FindLayer( papszModuleList[iModule] ) );

        if( poAttrReader == NULL )
        {
            printf( "Unable to open attribute module %s, skipping.\n" ,
                    papszModuleList[iModule] );
            continue;
        }

        poAttrReader->Rewind();
        
/* -------------------------------------------------------------------- */
/*      Read the first record so we can clone schema information off    */
/*      of it.                                                          */
/* -------------------------------------------------------------------- */
        SDTSAttrRecord 	*poAttrFeature;

        poAttrFeature = (SDTSAttrRecord *) poAttrReader->GetNextFeature();
        if( poAttrFeature == NULL )
        {
            fprintf( stderr,
                     "Didn't find any meaningful attribute records in %s.\n",
                     papszModuleList[iModule] );
        
            continue;
        }

/* -------------------------------------------------------------------- */
/*      Clone schema off the first record.  Eventually we need to       */
/*      get the information out of the DDR record, but it isn't         */
/*      clear to me how to accomplish that with the SDTS++ API.         */
/*                                                                      */
/*      The following approach may fail (dramatically) if some          */
/*      records do not include all subfields.  Furthermore, no          */
/*      effort is made to make DBF field names unique.  The SDTS        */
/*      attributes often have names much beyond the 14 character dbf    */
/*      limit which may result in non-unique attributes.                */
/* -------------------------------------------------------------------- */
        DDFFieldDefn 	*poFDefn = poAttrFeature->poATTR->GetFieldDefn();
        int		iSF;
        DDFField	*poSR = poAttrFeature->poATTR;
    
        for( iSF=0; iSF < poFDefn->GetSubfieldCount(); iSF++ )
        {
            DDFSubfieldDefn	*poSFDefn = poFDefn->GetSubfield( iSF );
            int		nWidth = poSFDefn->GetWidth();
            
            switch( poSFDefn->GetType() )
            {
              case DDFString:
                if( nWidth == 0 )
                {
                    int		nMaxBytes;
                
                    const char * pachData = poSR->GetSubfieldData(poSFDefn,
                                                                  &nMaxBytes);

                    nWidth = strlen(poSFDefn->ExtractStringData(pachData,
                                                                nMaxBytes, NULL ));
                }
            
                DBFAddField( hDBF, poSFDefn->GetName(), FTString, nWidth, 0 );
                break;

              case DDFInt:
                if( nWidth == 0 )
                    nWidth = 9;

                DBFAddField( hDBF, poSFDefn->GetName(), FTInteger, nWidth, 0 );
                break;

              case DDFFloat:
                DBFAddField( hDBF, poSFDefn->GetName(), FTDouble, 18, 6 );
                break;

              default:
                fprintf( stderr,
                         "Dropping attribute `%s' of module `%s'.  "
                         "Type unsupported\n",
                         poSFDefn->GetName(),
                         papszModuleList[iModule] );
                break;
            }
        }
    }    
}

/************************************************************************/
/*                       WritePrimaryAttrToDBF()                        */
/************************************************************************/

static void
WritePrimaryAttrToDBF( DBFHandle hDBF, int iRecord, 
                       SDTSTransfer * poTransfer, SDTSFeature * poFeature )

{
/* ==================================================================== */
/*      Loop over all the attribute records linked to this feature.     */
/* ==================================================================== */
    int		iAttrRecord;
    
    for( iAttrRecord = 0; iAttrRecord < poFeature->nAttributes; iAttrRecord++)
    {
        DDFField	*poSR;

        poSR = poTransfer->GetAttr( poFeature->aoATID+iAttrRecord );
          
/* -------------------------------------------------------------------- */
/*      Process each subfield in the record.                            */
/* -------------------------------------------------------------------- */
        DDFFieldDefn 	*poFDefn = poSR->GetFieldDefn();
        
        for( int iSF=0; iSF < poFDefn->GetSubfieldCount(); iSF++ )
        {
            DDFSubfieldDefn	*poSFDefn = poFDefn->GetSubfield( iSF );
            int			iField;
            int			nMaxBytes;
            const char * 	pachData = poSR->GetSubfieldData(poSFDefn,
                                                                 &nMaxBytes);

/* -------------------------------------------------------------------- */
/*      Identify the related DBF field, if any.                         */
/* -------------------------------------------------------------------- */
            for( iField = 0; iField < hDBF->nFields; iField++ )
            {
                if( EQUALN(poSFDefn->GetName(),
                           hDBF->pszHeader+iField*32,10) )
                    break;
            }

            if( iField == hDBF->nFields )
                iField = -1;
            
/* -------------------------------------------------------------------- */
/*      Handle each of the types.                                       */
/* -------------------------------------------------------------------- */
            switch( poSFDefn->GetType() )
            {
              case DDFString:
                const char	*pszValue;

                pszValue = poSFDefn->ExtractStringData(pachData, nMaxBytes,
                                                       NULL);

                if( iField != -1 )
                    DBFWriteStringAttribute(hDBF, iRecord, iField, pszValue );
                break;

              case DDFFloat:
                double	dfValue;

                dfValue = poSFDefn->ExtractFloatData(pachData, nMaxBytes,
                                                     NULL);

                if( iField != -1 )
                    DBFWriteDoubleAttribute( hDBF, iRecord, iField, dfValue );
                break;

              case DDFInt:
                int		nValue;

                nValue = poSFDefn->ExtractIntData(pachData, nMaxBytes, NULL);

                if( iField != -1 )
                    DBFWriteIntegerAttribute( hDBF, iRecord, iField, nValue );
                break;

              default:
                break;
            }
        } /* next subfield */
    } /* next related record */
}
