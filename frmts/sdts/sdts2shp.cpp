/* ****************************************************************************
 *
 * Project:  SDTS Translator
 * Purpose:  Mainline for converting to ArcView Shapefiles.
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

#include "sdts_al.h"
#include "shapefil.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")

static int  bVerbose = FALSE;

static void WriteLineShapefile( const char *, SDTSTransfer *,
                                const char * );
static void WritePointShapefile( const char *, SDTSTransfer *,
                                 const char * );
static void WriteAttributeDBF( const char *, SDTSTransfer *,
                               const char * );
static void WritePolygonShapefile( const char *, SDTSTransfer *,
                                   const char * );

static void
AddPrimaryAttrToDBFSchema( DBFHandle hDBF, SDTSTransfer * poTransfer,
                           char ** papszModuleList );
static void
WritePrimaryAttrToDBF( DBFHandle hDBF, int nRecord,
                       SDTSTransfer *, SDTSFeature * poFeature );
static void
WriteAttrRecordToDBF( DBFHandle hDBF, int nRecord,
                      SDTSTransfer *, DDFField * poAttributes );

/* **********************************************************************/
/*                               Usage()                                */
/* **********************************************************************/

static void Usage()

{
    printf( "Usage: sdts2shp CATD_filename [-o shapefile_name]\n" /*ok*/
            "                [-m module_name] [-v]\n"
            "\n"
            "Modules include `LE01', `PC01', `NP01' and `ARDF'\n" );

    exit( 1 );
}

/* **********************************************************************/
/*                                main()                                */
/* **********************************************************************/

int main( int nArgc, char ** papszArgv )

{
{
    int         i;
    const char  *pszCATDFilename = NULL;
    const char  *pszMODN = "LE01";
    char        *pszShapefile = "sdts_out.shp";
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
            printf( "Incomplete, or unsupported option `%s'\n\n",/*ok*/
                    papszArgv[i] );
            Usage();
        }
    }

/* -------------------------------------------------------------------- */
/*      Massage shapefile name to have no extension.                    */
/* -------------------------------------------------------------------- */
    pszShapefile = CPLStrdup(pszShapefile);
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
        printf( "Layers:\n" );/*ok*/
        for( i = 0; i < oTransfer.GetLayerCount(); i++ )
        {
            int         iCATDEntry = oTransfer.GetLayerCATDEntry(i);

            printf( "  %s: `%s'\n",/*ok*/
                    oTransfer.GetCATD()->GetEntryModule(iCATDEntry),
                    oTransfer.GetCATD()->GetEntryTypeDesc(iCATDEntry) );
        }
        printf( "\n" );/*ok*/
    }

/* -------------------------------------------------------------------- */
/*      Check that module exists.                                       */
/* -------------------------------------------------------------------- */
    if( oTransfer.FindLayer( pszMODN ) == -1 )
    {
        fprintf( stderr, "Unable to identify module: %s\n", pszMODN );
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      If the module is an LE module, write it to an Arc file.         */
/* -------------------------------------------------------------------- */
    if( pszMODN[0] == 'L' || pszMODN[0] == 'l' )
    {
        WriteLineShapefile( pszShapefile, &oTransfer, pszMODN );
    }

/* -------------------------------------------------------------------- */
/*      If the module is an attribute primary one, dump to DBF.         */
/* -------------------------------------------------------------------- */
    else if( pszMODN[0] == 'A' || pszMODN[0] == 'a'
             || pszMODN[0] == 'B' || pszMODN[0] == 'b' )
    {
        WriteAttributeDBF( pszShapefile, &oTransfer, pszMODN );
    }

/* -------------------------------------------------------------------- */
/*      If the module is a point one, dump to Shapefile.                */
/* -------------------------------------------------------------------- */
    else if( pszMODN[0] == 'N' || pszMODN[0] == 'n' )
    {
        WritePointShapefile( pszShapefile, &oTransfer, pszMODN );
    }

/* -------------------------------------------------------------------- */
/*      If the module is a polygon one, dump to Shapefile.              */
/* -------------------------------------------------------------------- */
    else if( pszMODN[0] == 'P' || pszMODN[0] == 'p' )
    {
        WritePolygonShapefile( pszShapefile, &oTransfer, pszMODN );
    }

    else
    {
        fprintf( stderr, "Unrecognized module name: %s\n", pszMODN );
    }

    CPLFree( pszShapefile );
}
}

/* **********************************************************************/
/*                         WriteLineShapefile()                         */
/* **********************************************************************/

static void WriteLineShapefile( const char * pszShapefile,
                                SDTSTransfer * poTransfer,
                                const char * pszMODN )

{
/* -------------------------------------------------------------------- */
/*      Fetch a reference to the indexed Pointgon reader.                */
/* -------------------------------------------------------------------- */
    SDTSLineReader *poLineReader = (SDTSLineReader *)
        poTransfer->GetLayerIndexedReader( poTransfer->FindLayer( pszMODN ) );

    if( poLineReader == NULL )
    {
        fprintf( stderr, "Failed to open %s.\n",
                 poTransfer->GetCATD()->GetModuleFilePath( pszMODN ) );
        return;
    }

    poLineReader->Rewind();

/* -------------------------------------------------------------------- */
/*      Create the Shapefile.                                           */
/* -------------------------------------------------------------------- */
    SHPHandle   hSHP;

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
    DBFHandle   hDBF;
    int         nLeftPolyField, nRightPolyField;
    int         nStartNodeField, nEndNodeField, nSDTSRecordField;
    char        szDBFFilename[1024];

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

    char  **papszModRefs = poLineReader->ScanModuleReferences();
    AddPrimaryAttrToDBFSchema( hDBF, poTransfer, papszModRefs );
    CSLDestroy( papszModRefs );

/* ==================================================================== */
/*      Process all the line features in the module.                    */
/* ==================================================================== */
    SDTSRawLine *poRawLine = NULL;

    while( (poRawLine = poLineReader->GetNextLine()) != NULL )
    {
/* -------------------------------------------------------------------- */
/*      Write out a shape with the vertices.                            */
/* -------------------------------------------------------------------- */
        SHPObject *psShape =
            SHPCreateSimpleObject( SHPT_ARC, poRawLine->nVertices,
                                   poRawLine->padfX, poRawLine->padfY,
                                   poRawLine->padfZ );

        int iShape = SHPWriteObject( hSHP, -1, psShape );

        SHPDestroyObject( psShape );

/* -------------------------------------------------------------------- */
/*      Write out the attributes.                                       */
/* -------------------------------------------------------------------- */
        char    szID[13];

        DBFWriteIntegerAttribute( hDBF, iShape, nSDTSRecordField,
                                  poRawLine->oModId.nRecord );

        sprintf( szID, "%s:%d",
                 poRawLine->oLeftPoly.szModule,
                 poRawLine->oLeftPoly.nRecord );
        DBFWriteStringAttribute( hDBF, iShape, nLeftPolyField, szID );

        sprintf( szID, "%s:%d",
                 poRawLine->oRightPoly.szModule,
                 poRawLine->oRightPoly.nRecord );
        DBFWriteStringAttribute( hDBF, iShape, nRightPolyField, szID );

        sprintf( szID, "%s:%d",
                 poRawLine->oStartNode.szModule,
                 poRawLine->oStartNode.nRecord );
        DBFWriteStringAttribute( hDBF, iShape, nStartNodeField, szID );

        sprintf( szID, "%s:%d",
                 poRawLine->oEndNode.szModule,
                 poRawLine->oEndNode.nRecord );
        DBFWriteStringAttribute( hDBF, iShape, nEndNodeField, szID );

        WritePrimaryAttrToDBF( hDBF, iShape, poTransfer, poRawLine );

        if( !poLineReader->IsIndexed() )
            delete poRawLine;
    }

/* -------------------------------------------------------------------- */
/*      Close, and cleanup.                                             */
/* -------------------------------------------------------------------- */
    DBFClose( hDBF );
    SHPClose( hSHP );
}

/* **********************************************************************/
/*                        WritePointShapefile()                         */
/* **********************************************************************/

static void WritePointShapefile( const char * pszShapefile,
                                 SDTSTransfer * poTransfer,
                                 const char * pszMODN )

{
/* -------------------------------------------------------------------- */
/*      Fetch a reference to the indexed Pointgon reader.                */
/* -------------------------------------------------------------------- */
    SDTSPointReader *poPointReader = (SDTSPointReader *)
        poTransfer->GetLayerIndexedReader( poTransfer->FindLayer( pszMODN ) );

    if( poPointReader == NULL )
    {
        fprintf( stderr, "Failed to open %s.\n",
                 poTransfer->GetCATD()->GetModuleFilePath( pszMODN ) );
        return;
    }

    poPointReader->Rewind();

/* -------------------------------------------------------------------- */
/*      Create the Shapefile.                                           */
/* -------------------------------------------------------------------- */
    SHPHandle   hSHP;

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
    DBFHandle   hDBF;
    int         nAreaField, nSDTSRecordField;
    char        szDBFFilename[1024];

    sprintf( szDBFFilename, "%s.dbf", pszShapefile );

    hDBF = DBFCreate( szDBFFilename );
    if( hDBF == NULL )
    {
        fprintf( stderr, "Unable to create shapefile .dbf for `%s'\n",
                 pszShapefile );
        return;
    }

    nSDTSRecordField = DBFAddField( hDBF, "SDTSRecId", FTInteger, 8, 0 );
    nAreaField = DBFAddField( hDBF, "AreaId", FTString, 12, 0 );

    char  **papszModRefs = poPointReader->ScanModuleReferences();
    AddPrimaryAttrToDBFSchema( hDBF, poTransfer, papszModRefs );
    CSLDestroy( papszModRefs );

/* ==================================================================== */
/*      Process all the line features in the module.                    */
/* ==================================================================== */
    SDTSRawPoint *poRawPoint = NULL;

    while( (poRawPoint = poPointReader->GetNextPoint()) != NULL )
    {
/* -------------------------------------------------------------------- */
/*      Write out a shape with the vertices.                            */
/* -------------------------------------------------------------------- */
        SHPObject *psShape =
            SHPCreateSimpleObject( SHPT_POINT, 1,
                                   &(poRawPoint->dfX),
                                   &(poRawPoint->dfY),
                                   &(poRawPoint->dfZ) );

        int iShape = SHPWriteObject( hSHP, -1, psShape );

        SHPDestroyObject( psShape );

/* -------------------------------------------------------------------- */
/*      Write out the attributes.                                       */
/* -------------------------------------------------------------------- */
        char    szID[13];

        DBFWriteIntegerAttribute( hDBF, iShape, nSDTSRecordField,
                                  poRawPoint->oModId.nRecord );

        sprintf( szID, "%s:%d",
                 poRawPoint->oAreaId.szModule,
                 poRawPoint->oAreaId.nRecord );
        DBFWriteStringAttribute( hDBF, iShape, nAreaField, szID );

        WritePrimaryAttrToDBF( hDBF, iShape, poTransfer, poRawPoint );

        if( !poPointReader->IsIndexed() )
            delete poRawPoint;
    }

/* -------------------------------------------------------------------- */
/*      Close, and cleanup.                                             */
/* -------------------------------------------------------------------- */
    DBFClose( hDBF );
    SHPClose( hSHP );
}

/* **********************************************************************/
/*                         WriteAttributeDBF()                          */
/* **********************************************************************/

static void WriteAttributeDBF( const char * pszShapefile,
                               SDTSTransfer * poTransfer,
                               const char * pszMODN )

{
/* -------------------------------------------------------------------- */
/*      Fetch a reference to the indexed Pointgon reader.               */
/* -------------------------------------------------------------------- */
    SDTSAttrReader *poAttrReader = (SDTSAttrReader *)
        poTransfer->GetLayerIndexedReader( poTransfer->FindLayer( pszMODN ) );

    if( poAttrReader == NULL )
    {
        fprintf( stderr, "Failed to open %s.\n",
                 poTransfer->GetCATD()->GetModuleFilePath( pszMODN ) );
        return;
    }

    poAttrReader->Rewind();

/* -------------------------------------------------------------------- */
/*      Create the database file, and our basic set of attributes.      */
/* -------------------------------------------------------------------- */
    DBFHandle   hDBF;
    char        szDBFFilename[1024];

    sprintf( szDBFFilename, "%s.dbf", pszShapefile );

    hDBF = DBFCreate( szDBFFilename );
    if( hDBF == NULL )
    {
        fprintf( stderr, "Unable to create shapefile .dbf for `%s'\n",
                 pszShapefile );
        return;
    }

    DBFAddField( hDBF, "SDTSRecId", FTInteger, 8, 0 );

/* -------------------------------------------------------------------- */
/*      Prepare the schema.                                             */
/* -------------------------------------------------------------------- */
    char        **papszMODNList = CSLAddString( NULL, pszMODN );

    AddPrimaryAttrToDBFSchema( hDBF, poTransfer, papszMODNList );

    CSLDestroy( papszMODNList );

/* ==================================================================== */
/*      Process all the records in the module.                          */
/* ==================================================================== */
    SDTSAttrRecord *poRecord = NULL;
    int iRecord = 0;

    while( (poRecord = (SDTSAttrRecord*)poAttrReader->GetNextFeature())
           != NULL )
    {
        DBFWriteIntegerAttribute( hDBF, iRecord, 0,
                                  poRecord->oModId.nRecord );

        WriteAttrRecordToDBF( hDBF, iRecord, poTransfer, poRecord->poATTR );

        if( !poAttrReader->IsIndexed() )
            delete poRecord;

        iRecord++;
    }

/* -------------------------------------------------------------------- */
/*      Close, and cleanup.                                             */
/* -------------------------------------------------------------------- */
    DBFClose( hDBF );
}

/* **********************************************************************/
/*                       WritePolygonShapefile()                        */
/* **********************************************************************/

static void WritePolygonShapefile( const char * pszShapefile,
                                   SDTSTransfer * poTransfer,
                                   const char * pszMODN )

{
/* -------------------------------------------------------------------- */
/*      Fetch a reference to the indexed polygon reader.                */
/* -------------------------------------------------------------------- */
    SDTSPolygonReader *poPolyReader = (SDTSPolygonReader *)
        poTransfer->GetLayerIndexedReader( poTransfer->FindLayer( pszMODN ) );

    if( poPolyReader == NULL )
    {
        fprintf( stderr, "Failed to open %s.\n",
                 poTransfer->GetCATD()->GetModuleFilePath( pszMODN ) );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Assemble polygon geometries from all the line layers.           */
/* -------------------------------------------------------------------- */
    poPolyReader->AssembleRings( poTransfer, poTransfer->FindLayer(pszMODN) );

/* -------------------------------------------------------------------- */
/*      Create the Shapefile.                                           */
/* -------------------------------------------------------------------- */
    SHPHandle   hSHP;

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
    DBFHandle   hDBF;
    int         nSDTSRecordField;
    char        szDBFFilename[1024];

    sprintf( szDBFFilename, "%s.dbf", pszShapefile );

    hDBF = DBFCreate( szDBFFilename );
    if( hDBF == NULL )
    {
        fprintf( stderr, "Unable to create shapefile .dbf for `%s'\n",
                 pszShapefile );
        return;
    }

    nSDTSRecordField = DBFAddField( hDBF, "SDTSRecId", FTInteger, 8, 0 );

    char  **papszModRefs = poPolyReader->ScanModuleReferences();
    AddPrimaryAttrToDBFSchema( hDBF, poTransfer, papszModRefs );
    CSLDestroy( papszModRefs );

/* ==================================================================== */
/*      Process all the polygon features in the module.                 */
/* ==================================================================== */
    poPolyReader->Rewind();

    SDTSRawPolygon *poRawPoly = NULL;
    while( (poRawPoly = (SDTSRawPolygon *) poPolyReader->GetNextFeature())
           != NULL )
    {
/* -------------------------------------------------------------------- */
/*      Write out a shape with the vertices.                            */
/* -------------------------------------------------------------------- */
        SHPObject *psShape =
            SHPCreateObject( SHPT_POLYGON, -1, poRawPoly->nRings,
                             poRawPoly->panRingStart, NULL,
                             poRawPoly->nVertices,
                             poRawPoly->padfX,
                             poRawPoly->padfY,
                             poRawPoly->padfZ,
                             NULL );

        int iShape = SHPWriteObject( hSHP, -1, psShape );

        SHPDestroyObject( psShape );

/* -------------------------------------------------------------------- */
/*      Write out the attributes.                                       */
/* -------------------------------------------------------------------- */
        DBFWriteIntegerAttribute( hDBF, iShape, nSDTSRecordField,
                                  poRawPoly->oModId.nRecord );
        WritePrimaryAttrToDBF( hDBF, iShape, poTransfer, poRawPoly );

        if( !poPolyReader->IsIndexed() )
            delete poRawPoly;
    }

/* -------------------------------------------------------------------- */
/*      Close, and cleanup.                                             */
/* -------------------------------------------------------------------- */
    DBFClose( hDBF );
    SHPClose( hSHP );
}

/* **********************************************************************/
/*                        AddPrimaryAttrToDBF()                         */
/*                                                                      */
/*      Add the fields from all the given primary attribute modules     */
/*      to the schema of the passed DBF file.                           */
/* **********************************************************************/

static void
AddPrimaryAttrToDBFSchema( DBFHandle hDBF, SDTSTransfer *poTransfer,
                           char ** papszModuleList )

{
    for( int iModule = 0;
         papszModuleList != NULL && papszModuleList[iModule] != NULL;
         iModule++ )
    {
/* -------------------------------------------------------------------- */
/*      Get a reader on the desired module.                             */
/* -------------------------------------------------------------------- */
        SDTSAttrReader *poAttrReader = (SDTSAttrReader *)
            poTransfer->GetLayerIndexedReader(
                poTransfer->FindLayer( papszModuleList[iModule] ) );

        if( poAttrReader == NULL )
        {
            printf( "Unable to open attribute module %s, skipping.\n" ,/*ok*/
                    papszModuleList[iModule] );
            continue;
        }

        poAttrReader->Rewind();

/* -------------------------------------------------------------------- */
/*      Read the first record so we can clone schema information off    */
/*      of it.                                                          */
/* -------------------------------------------------------------------- */
        SDTSAttrRecord *poAttrFeature =
            (SDTSAttrRecord *) poAttrReader->GetNextFeature();
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
        DDFFieldDefn    *poFDefn = poAttrFeature->poATTR->GetFieldDefn();
        int             iSF;
        DDFField        *poSR = poAttrFeature->poATTR;

        for( iSF=0; iSF < poFDefn->GetSubfieldCount(); iSF++ )
        {
            DDFSubfieldDefn     *poSFDefn = poFDefn->GetSubfield( iSF );
            int         nWidth = poSFDefn->GetWidth();

            switch( poSFDefn->GetType() )
            {
              case DDFString:
                if( nWidth == 0 )
                {
                    int         nMaxBytes;

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

        if( !poAttrReader->IsIndexed() )
            delete poAttrFeature;
    } /* next module */
}

/* **********************************************************************/
/*                       WritePrimaryAttrToDBF()                        */
/* **********************************************************************/

static void
WritePrimaryAttrToDBF( DBFHandle hDBF, int iRecord,
                       SDTSTransfer * poTransfer, SDTSFeature * poFeature )

{
/* ==================================================================== */
/*      Loop over all the attribute records linked to this feature.     */
/* ==================================================================== */
    for( int iAttrRecord = 0;
         iAttrRecord < poFeature->nAttributes;
         iAttrRecord++ )
    {
        DDFField *poSR = poTransfer->GetAttr( poFeature->paoATID+iAttrRecord );

        WriteAttrRecordToDBF( hDBF, iRecord, poTransfer, poSR );
    }
}

/* **********************************************************************/
/*                        WriteAttrRecordToDBF()                        */
/* **********************************************************************/

static void
WriteAttrRecordToDBF( DBFHandle hDBF, int iRecord,
                      SDTSTransfer * poTransfer, DDFField * poSR )

{
/* -------------------------------------------------------------------- */
/*      Process each subfield in the record.                            */
/* -------------------------------------------------------------------- */
    DDFFieldDefn        *poFDefn = poSR->GetFieldDefn();

    for( int iSF=0; iSF < poFDefn->GetSubfieldCount(); iSF++ )
    {
        DDFSubfieldDefn *poSFDefn = poFDefn->GetSubfield( iSF );
        int                     iField;
        int                     nMaxBytes;
        const char *    pachData = poSR->GetSubfieldData(poSFDefn,
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
        switch (poSFDefn->GetType())
        {
            case DDFString:
            {
                const char *pszValue = poSFDefn->ExtractStringData(pachData, nMaxBytes, NULL);

                if (iField != -1)
                    DBFWriteStringAttribute(hDBF, iRecord, iField, pszValue);
            }
        break;

        case DDFFloat:
            {
                double dfValue;

                dfValue = poSFDefn->ExtractFloatData(pachData, nMaxBytes,
                                                     NULL);

                if (iField != -1)
                    DBFWriteDoubleAttribute(hDBF, iRecord, iField, dfValue);
            }
            break;

        case DDFInt:
            {
                int nValue;

                nValue = poSFDefn->ExtractIntData(pachData, nMaxBytes, NULL);

                if (iField != -1)
                    DBFWriteIntegerAttribute(hDBF, iRecord, iField, nValue);
            }
            break;
        default:
            break;
        }
    } /* next subfield */
}
