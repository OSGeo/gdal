/******************************************************************************
 * $Id$
 *
 * Project:  NTF Translator
 * Purpose:  NTFFileReader class implementation.
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
 * Revision 1.2  1999/08/28 18:24:42  warmerda
 * added TestForLayer() optimization
 *
 * Revision 1.1  1999/08/28 03:13:35  warmerda
 * New
 *
 */

#include <stdarg.h>
#include "ntf.h"
#include "cpl_conv.h"
#include "cpl_string.h"

static int DefaultNTFRecordGrouper( NTFFileReader *, NTFRecord **,
                                    NTFRecord * );

/************************************************************************/
/*                            NTFFileReader                             */
/************************************************************************/

NTFFileReader::NTFFileReader( OGRNTFDataSource * poDataSource )

{
    fp = NULL;

    nFCCount = 0;
    panFCNum = NULL;
    papszFCName = NULL;

    nPreSavedPos = nPostSavedPos = 0;
    nSavedFeatureId = nBaseFeatureId = 1;
    nFeatureCount = -1;
    poSavedRecord = NULL;

    nAttCount = 0;
    pasAttDesc = NULL;

    pszTileName = NULL;
    pszProduct = NULL;
    pszFilename = NULL;

    apoCGroup[0] = NULL;

    poDS = poDataSource;

    memset( apoTypeTranslation, 0, sizeof(apoTypeTranslation) );

    nProduct = NPC_UNKNOWN;

    pfnRecordGrouper = DefaultNTFRecordGrouper;

    dfXYMult = 1.0;
    dfXOrigin = 0;
    dfYOrigin = 0;
    nNTFLevel = 0;
    dfTileXSize = 0;
    dfTileYSize = 0;
}

/************************************************************************/
/*                           ~NTFFileReader()                           */
/************************************************************************/

NTFFileReader::~NTFFileReader()

{
    ClearDefs();
    CPLFree( pszFilename );
}

/************************************************************************/
/*                             SetBaseFID()                             */
/************************************************************************/

void NTFFileReader::SetBaseFID( long nNewBase )

{
    CPLAssert( nSavedFeatureId == 1 );

    nBaseFeatureId = nNewBase;
    nSavedFeatureId = nBaseFeatureId;
}

/************************************************************************/
/*                             ClearDefs()                              */
/*                                                                      */
/*      Clear attribute definitions and feature classes.  All the       */
/*      stuff that would have to be cleaned up by Open(), and the       */
/*      destructor.                                                     */
/************************************************************************/

void NTFFileReader::ClearDefs()

{
    Close();
    
    ClearCGroup();
    
    if( poSavedRecord != NULL )
        delete poSavedRecord;
    poSavedRecord = NULL;

    CPLFree( panFCNum );
    panFCNum = NULL;
    CSLDestroy( papszFCName );
    papszFCName = NULL;
    nFCCount = 0;

    CPLFree( pasAttDesc );
    nAttCount = 0;
    pasAttDesc = NULL;
    
    CPLFree( pszProduct );
    pszProduct = NULL;
    
    CPLFree( pszTileName );
    pszTileName = NULL;
}

/************************************************************************/
/*                               Close()                                */
/*                                                                      */
/*      Close the file, but don't wipe out our knowledge about this     */
/*      file.                                                           */
/************************************************************************/

void NTFFileReader::Close()

{
    nPreSavedPos = nPostSavedPos = 0;
    nSavedFeatureId = nBaseFeatureId;
    if( fp != NULL )
    {
        VSIFClose( fp );
        fp = NULL;
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int NTFFileReader::Open( const char * pszFilenameIn )

{
    ClearDefs();
    
    if( pszFilenameIn != NULL )
    {
        CPLFree( pszFilename );
        pszFilename = CPLStrdup( pszFilenameIn );
    }
    
/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    fp = VSIFOpen( pszFilename, "r" );

    // notdef: we should likely issue a proper CPL error message based
    // based on errno here. 
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to open file `%s' for read access.\n",
                  pszFilename );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Read the first record, and verify it is a proper volume header. */
/* -------------------------------------------------------------------- */
    NTFRecord      oVHR( fp );

    if( oVHR.GetType() != NRT_VHR )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "File `%s' appears to not be a UK NTF file.\n",
                  pszFilename );
        return FALSE;
    }

    nNTFLevel = atoi(oVHR.GetField( 57, 57 ));
    CPLAssert( nNTFLevel >= 1 && nNTFLevel <= 3 );

/* -------------------------------------------------------------------- */
/*      Read records till we get the section header.                    */
/* -------------------------------------------------------------------- */
    NTFRecord      *poRecord;

    for( poRecord = new NTFRecord( fp ); 
         poRecord->GetType() != NRT_VTR && poRecord->GetType() != NRT_SHR;
         poRecord = new NTFRecord( fp ) )
    {
/* -------------------------------------------------------------------- */
/*      Handle feature class name records.                              */
/* -------------------------------------------------------------------- */
        if( poRecord->GetType() == NRT_FCR )
        {
            const char      *pszData;
            int             iChar;

            nFCCount++;
            panFCNum = (int *) CPLRealloc(panFCNum, sizeof(int)*nFCCount);
            panFCNum[nFCCount-1] = atoi(poRecord->GetField(3,6));
            
            pszData = poRecord->GetData();
            for( iChar = 36; 
                 pszData[iChar] != '\0' && pszData[iChar] != '\\';
                 iChar++ ) {}

            papszFCName = CSLAddString(papszFCName, 
                                       poRecord->GetField(37,iChar));
        }

/* -------------------------------------------------------------------- */
/*      Handle attribute description records.                           */
/* -------------------------------------------------------------------- */
        else if( poRecord->GetType() == NRT_ADR )
        {
            nAttCount++;

            pasAttDesc = (NTFAttDesc *) 
                CPLRealloc( pasAttDesc, sizeof(NTFAttDesc) * nAttCount );

            ProcessAttDesc( poRecord, pasAttDesc + nAttCount - 1 );
        }

/* -------------------------------------------------------------------- */
/*	Handle database header record.					*/
/* -------------------------------------------------------------------- */
        else if( poRecord->GetType() == NRT_DHR )
        {
            pszProduct = CPLStrdup(poRecord->GetField(3,22));
            for( int iChar = strlen(pszProduct)-1;
                 iChar > 0 && pszProduct[iChar] == ' ';
                 pszProduct[iChar--] = '\0' ) {}
        }

        delete poRecord;
    }

/* -------------------------------------------------------------------- */
/*      Did we fall off the end without finding what we were looking    */
/*      for?                                                            */
/* -------------------------------------------------------------------- */
    if( poRecord->GetType() == NRT_VTR )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Cound not find section header record in %s.\n", 
                  pszFilename );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Classify the product type.                                      */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszProduct,"LAND-LINE",9) )
        nProduct = NPC_LANDLINE;
    else if( EQUAL(pszProduct,"OS_LANDRANGER_CONT") ) // Panorama
        nProduct = NPC_LANDRANGER_CONT;
    else if( EQUALN(pszProduct,"Strategi",8) )
        nProduct = NPC_STRATEGI;
    else if( EQUALN(pszProduct,"Meridian",8) )
        nProduct = NPC_MERIDIAN;
    else if( EQUAL(pszProduct,NTF_BOUNDARYLINE) )
        nProduct = NPC_BOUNDARYLINE;
    else if( EQUALN(pszProduct,"BaseData.GB",11) )
        nProduct = NPC_BASEDATA;
    
/* -------------------------------------------------------------------- */
/*      Handle the section header record.                               */
/* -------------------------------------------------------------------- */
    pszTileName = CPLStrdup(poRecord->GetField(3,12));        // SECT_REF
    while( pszTileName[strlen(pszTileName)-1] == ' ' )
        pszTileName[strlen(pszTileName)-1] = '\0';

    nCoordWidth = atoi(poRecord->GetField(15,19));            // XYLEN
    if( nCoordWidth == 0 )
        nCoordWidth = 10;
    
    dfXYMult = atoi(poRecord->GetField(21,30)) / 1000.0;      // XY_MULT
    dfXOrigin = atoi(poRecord->GetField(47,56));
    dfYOrigin = atoi(poRecord->GetField(57,66));
    dfTileXSize = atoi(poRecord->GetField(23+74,32+74));
    dfTileYSize = atoi(poRecord->GetField(33+74,42+74));

    nSavedFeatureId = nBaseFeatureId;
    nStartPos = VSIFTell(fp);
    
/* -------------------------------------------------------------------- */
/*      Ensure we have appropriate layers defined.                      */
/* -------------------------------------------------------------------- */
    EstablishLayers();
    
    return TRUE;
}

/************************************************************************/
/*                            DumpReadable()                            */
/************************************************************************/

void NTFFileReader::DumpReadable( FILE *fpLog )

{
    fprintf( fpLog, "Tile Name = %s\n", pszTileName );
    fprintf( fpLog, "Product = %s\n", pszProduct );
    fprintf( fpLog, "NTFLevel = %d\n", nNTFLevel );
    fprintf( fpLog, "XYLEN = %d\n", nCoordWidth );
    fprintf( fpLog, "XY_MULT = %g\n", dfXYMult );
    fprintf( fpLog, "X_ORIG = %g\n", dfXOrigin );
    fprintf( fpLog, "Y_ORIG = %g\n", dfYOrigin ); 
    fprintf( fpLog, "XMAX = %g\n", dfTileXSize );
    fprintf( fpLog, "YMAX = %g\n", dfTileYSize );
}

/************************************************************************/
/*                          ProcessGeometry()                           */
/************************************************************************/

OGRGeometry *NTFFileReader::ProcessGeometry( NTFRecord * poRecord,
                                             int * pnGeomId )

{
    int            nGType, nNumCoord;
    OGRGeometry    *poGeometry = NULL;

    if( poRecord->GetType() != NRT_GEOMETRY )
        return NULL;

    nGType = atoi(poRecord->GetField(9,9));            // GTYPE
    nNumCoord = atoi(poRecord->GetField(10,13));       // NUM_COORD
    if( pnGeomId != NULL )
        *pnGeomId = atoi(poRecord->GetField(3,8));     // GEOM_ID

    if( nGType == 1 )
    {
        double      dfX, dfY;
        
        dfX = atoi(poRecord->GetField(14,14+GetXYLen()-1)) * GetXYMult() 
            + GetXOrigin();
        dfY = atoi(poRecord->GetField(14+GetXYLen(),14+GetXYLen()*2-1))
            * GetXYMult() + GetYOrigin();
      
        poGeometry = new OGRPoint( dfX, dfY );
    }
    
    else if( nGType == 2 )
    {
        OGRLineString      *poLine = new OGRLineString;
        double             dfX, dfY;
        int                iCoord;

        poGeometry = poLine;
        poLine->setNumPoints( nNumCoord );
        for( iCoord = 0; iCoord < nNumCoord; iCoord++ )
        {
            int            iStart = 14 + iCoord * (GetXYLen()*2+1);

            dfX = atoi(poRecord->GetField(iStart+0,
                                          iStart+GetXYLen()-1)) 
                * GetXYMult() + GetXOrigin();
            dfY = atoi(poRecord->GetField(iStart+GetXYLen(),
                                          iStart+GetXYLen()*2-1)) 
                * GetXYMult() + GetYOrigin();

            poLine->setPoint( iCoord, dfX, dfY );
        }
    }

    return poGeometry;
}

/************************************************************************/
/*                           ProcessAttDesc()                           */
/************************************************************************/

int NTFFileReader::ProcessAttDesc( NTFRecord * poRecord, NTFAttDesc* psAD )

{
    int      iChar;
    const char *pszData;

    if( poRecord->GetType() != NRT_ADR )
        return FALSE;

    strcpy( psAD->val_type, poRecord->GetField( 3, 4 ));
    strcpy( psAD->fwidth, poRecord->GetField( 5, 7 ));
    strcpy( psAD->finter, poRecord->GetField( 8, 12 ));
    
    pszData = poRecord->GetData();
    for( iChar = 12; 
         pszData[iChar] != '\0' && pszData[iChar] != '\\';
         iChar++ ) {}

    strcpy( psAD->att_name, poRecord->GetField( 13, iChar ));

    return TRUE;
}

/************************************************************************/
/*                         ProcessAttRecGroup()                         */
/*                                                                      */
/*      Extract attribute values from all attribute records in a        */
/*      record set.                                                     */
/************************************************************************/

int NTFFileReader::ProcessAttRecGroup( NTFRecord **papoRecords,
                                       char ***ppapszTypes,
                                       char ***ppapszValues )

{
    *ppapszTypes = NULL;
    *ppapszValues = NULL;
    
    for( int iRec = 0; papoRecords[iRec] != NULL; iRec++ )
    {
        char	**papszTypes1 = NULL, **papszValues1 = NULL;
        
        if( papoRecords[iRec]->GetType() != NRT_ATTREC )
            continue;

        if( !ProcessAttRec( papoRecords[iRec], NULL,
                            &papszTypes1, &papszValues1 ) )
            return FALSE;

        if( *ppapszTypes == NULL )
        {
            *ppapszTypes = papszTypes1;
            *ppapszValues = papszValues1;
        }
        else
        {
            for( int i=0; papszTypes1[i] != NULL; i++ )
            {
                *ppapszTypes = CSLAddString( *ppapszTypes, papszTypes1[i] );
                *ppapszValues = CSLAddString( *ppapszValues, papszValues1[i] );
            }
            CSLDestroy( papszTypes1 );
            CSLDestroy( papszValues1 );
        }
    }

    return TRUE;
}

/************************************************************************/
/*                           ProcessAttRec()                            */
/************************************************************************/

int NTFFileReader::ProcessAttRec( NTFRecord * poRecord, 
                                  int *pnAttId,
                                  char *** ppapszTypes, 
                                  char *** ppapszValues )

{
    int            iOffset;
    const char     *pszData;

    if( poRecord->GetType() != NRT_ATTREC )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Extract the attribute id.                                       */
/* -------------------------------------------------------------------- */
    if( pnAttId != NULL )
        *pnAttId = atoi(poRecord->GetField(3,8));

/* ==================================================================== */
/*      Loop handling attribute till we get a '0' indicating the end    */
/*      of the record.                                                  */
/* ==================================================================== */
    *ppapszTypes = NULL;
    *ppapszValues = NULL;

    iOffset = 8;
    pszData = poRecord->GetData();

    while( pszData[iOffset] != '0' && pszData[iOffset] != '\0' )
    {
        NTFAttDesc *psAttDesc;
        int         nEnd;
        int         nFWidth;

/* -------------------------------------------------------------------- */
/*      Extract the two letter code name for the attribute, and use     */
/*      it to find the correct ATTDESC info.                            */
/* -------------------------------------------------------------------- */
        psAttDesc = GetAttDesc(pszData + iOffset );
        if( psAttDesc == NULL )
        {
            CPLDebug( "NTF", "Couldn't translate attrec type `%2.2s'.", 
                      pszData + iOffset );
            return FALSE;
        }

        *ppapszTypes =
            CSLAddString( *ppapszTypes, 
                          poRecord->GetField(iOffset+1,iOffset+2) );

/* -------------------------------------------------------------------- */
/*      Establish the width of the value.  Zero width fields are        */
/*      terminated by a backslash.                                      */
/* -------------------------------------------------------------------- */
        nFWidth = atoi(psAttDesc->fwidth);
        if( nFWidth == 0 )
        {
            const char * pszData = poRecord->GetData();

            for( nEnd = iOffset + 2; 
                 pszData[nEnd] != '\\' && pszData[nEnd] != '\0';
                 nEnd++ ) {}
        }
        else
        {
            nEnd = iOffset + 3 + nFWidth - 1;
        }

/* -------------------------------------------------------------------- */
/*      Extract the value.  If it is formatted as fixed point real      */
/*      we reprocess it to insert the decimal point.                    */
/* -------------------------------------------------------------------- */
        const char * pszRawValue = poRecord->GetField(iOffset+3,nEnd);
        *ppapszValues = CSLAddString( *ppapszValues, pszRawValue );

/* -------------------------------------------------------------------- */
/*      Establish new offset position.                                  */
/* -------------------------------------------------------------------- */
        if( nFWidth == 0 )
            iOffset = nEnd + 1;
        else
            iOffset += 2 + atoi(psAttDesc->fwidth);
    }

    return TRUE;
}

/************************************************************************/
/*                             GetAttDesc()                             */
/************************************************************************/

NTFAttDesc * NTFFileReader::GetAttDesc( const char * pszType )

{
    for( int i = 0; i < nAttCount; i++ )
    {
        if( EQUALN(pszType, pasAttDesc[i].val_type, 2) )
            return pasAttDesc + i;
    }

    return NULL;
}

/************************************************************************/
/*                          ProcessAttValue()                           */
/*                                                                      */
/*      Take an attribute type/value pair and transform into a          */
/*      meaningful attribute name, and value.  The source can be an     */
/*      ATTREC or the VAL_TYPE/VALUE pair of a POINTREC or LINEREC.     */
/*      The name is transformed from the two character short form to    */
/*      the long user name.  The value will be transformed from         */
/*      fixed point (with the decimal implicit) to fixed point with     */
/*      an explicit decimal point if it has a "R" format.               */
/************************************************************************/

int NTFFileReader::ProcessAttValue( const char *pszValType, 
                                    const char *pszRawValue,
                                    char **ppszAttName, 
                                    char **ppszAttValue )

{
/* -------------------------------------------------------------------- */
/*      Find the ATTDESC for this attribute, and assign return name value.*/
/* -------------------------------------------------------------------- */
    NTFAttDesc      *psAttDesc = GetAttDesc(pszValType);

    if( psAttDesc == NULL )
        return FALSE;
    
    *ppszAttName = psAttDesc->att_name;

/* -------------------------------------------------------------------- */
/*      Extract the value.  If it is formatted as fixed point real      */
/*      we reprocess it to insert the decimal point.                    */
/* -------------------------------------------------------------------- */
    if( psAttDesc->finter[0] == 'R' )
    {
        static char      szRealString[30];
        const char *pszDecimalPortion;
        int       nWidth, nPrecision;

        for( pszDecimalPortion = psAttDesc->finter; 
             *pszDecimalPortion != ',' && *pszDecimalPortion != '\0';
             pszDecimalPortion++ ) {}

        nWidth = strlen(pszRawValue);
        nPrecision = atoi(pszDecimalPortion+1);

        strncpy( szRealString, pszRawValue, nWidth - nPrecision );
        szRealString[nWidth-nPrecision] = '.';
        strcpy( szRealString+nWidth-nPrecision+1, 
                pszRawValue+nWidth-nPrecision );
        
        *ppszAttValue = szRealString;
    }

/* -------------------------------------------------------------------- */
/*      If it is an integer, we just reformat to get rid of leading     */
/*      zeros.                                                          */
/* -------------------------------------------------------------------- */
    else if( psAttDesc->finter[0] == 'I' )
    {
        static char    szIntString[30];

        sprintf( szIntString, "%d", atoi(pszRawValue) );

        *ppszAttValue = szIntString;
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we take the value directly.                           */
/* -------------------------------------------------------------------- */
    else
    {
        *ppszAttValue = (char *) pszRawValue;
    }

    return TRUE;
}

/************************************************************************/
/*                        ApplyAttributeValues()                        */
/*                                                                      */
/*      Apply a series of attribute values to a feature from generic    */
/*      attribute records.                                              */
/************************************************************************/

void NTFFileReader::ApplyAttributeValues( OGRFeature * poFeature,
                                          NTFRecord ** papoGroup, ... )

{
    char	**papszTypes = NULL, **papszValues = NULL;

/* -------------------------------------------------------------------- */
/*      Extract attribute values from record group.                     */
/* -------------------------------------------------------------------- */
    if( !ProcessAttRecGroup( papoGroup, &papszTypes, &papszValues ) )
        return;
    
/* -------------------------------------------------------------------- */
/*      Handle attribute pairs                                          */
/* -------------------------------------------------------------------- */
    va_list	hVaArgs;
    const char	*pszAttName;
    
    va_start(hVaArgs, papoGroup);

    while( (pszAttName = va_arg(hVaArgs, const char *)) != NULL )
    {
        int	iField = va_arg(hVaArgs, int);

        ApplyAttributeValue( poFeature, iField, pszAttName,
                             papszTypes, papszValues );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    CSLDestroy( papszTypes );
    CSLDestroy( papszValues );
}
                                          

/************************************************************************/
/*                        ApplyAttributeValue()                         */
/*                                                                      */
/*      Apply the indicated attribute value to an OGRFeature field      */
/*      if it exists in the attribute value list given.                 */
/************************************************************************/

int NTFFileReader::ApplyAttributeValue( OGRFeature * poFeature, int iField,
                                        const char * pszAttName,
                                        char ** papszTypes,
                                        char ** papszValues )

{
/* -------------------------------------------------------------------- */
/*      Find the requested attribute in the name/value pair             */
/*      provided.  If not found that's fine, just return with           */
/*      notification.                                                   */
/* -------------------------------------------------------------------- */
    int		iValue;

    iValue = CSLFindString( papszTypes, pszAttName );
    if( iValue < 0 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Apply the value to the field using the simple set string        */
/*      method.  Leave it to the OGRFeature::SetField() method to       */
/*      take care of translation to other types.                        */
/* -------------------------------------------------------------------- */
    poFeature->SetField( iField, papszValues[iValue] );

    return TRUE;
}

/************************************************************************/
/*                             SaveRecord()                             */
/************************************************************************/

void NTFFileReader::SaveRecord( NTFRecord * poRecord )

{
    CPLAssert( poSavedRecord == NULL );
    poSavedRecord = poRecord;
}

/************************************************************************/
/*                             ReadRecord()                             */
/************************************************************************/

NTFRecord *NTFFileReader::ReadRecord()

{
    if( poSavedRecord != NULL )
    {
        NTFRecord       *poReturn;

        poReturn = poSavedRecord;

        poSavedRecord = NULL;

        return poReturn;
    }
    else
    {
        NTFRecord	*poRecord;

        if( fp != NULL )
            nPreSavedPos = VSIFTell( fp );
        poRecord = new NTFRecord( fp );
        if( fp != NULL )
            nPostSavedPos = VSIFTell( fp );

        return poRecord;
    }
}

/************************************************************************/
/*                              GetFPPos()                              */
/*                                                                      */
/*      Return the current file pointer position.                       */
/************************************************************************/

void NTFFileReader::GetFPPos( long *pnPos, long *pnFID )

{
    if( poSavedRecord != NULL )
        *pnPos = nPreSavedPos;
    else
        *pnPos = nPostSavedPos;

    *pnFID = nSavedFeatureId;
}

/************************************************************************/
/*                              SetFPPos()                              */
/************************************************************************/

int NTFFileReader::SetFPPos( long nNewPos, long nNewFID )

{
    if( nNewFID == nSavedFeatureId )
        return TRUE;

    if( poSavedRecord != NULL )
    {
        delete poSavedRecord;
        poSavedRecord = NULL;
    }

    if( fp != NULL && VSIFSeek( fp, nNewPos, SEEK_SET ) == 0 )
    {
        nPreSavedPos = nPostSavedPos = nNewPos;
        nSavedFeatureId = nNewFID;
        return TRUE;
    }
    else
        return FALSE;
}

/************************************************************************/
/*                               Reset()                                */
/*                                                                      */
/*      Reset reading to the first feature record.                      */
/************************************************************************/

void NTFFileReader::Reset()

{
    SetFPPos( nStartPos, nBaseFeatureId );
}

/************************************************************************/
/*                            ClearCGroup()                             */
/*                                                                      */
/*      Clear the currently loaded record group.                        */
/************************************************************************/

void NTFFileReader::ClearCGroup()

{
    for( int i = 0; apoCGroup[i] != NULL; i++ )
        delete apoCGroup[i];

    apoCGroup[0] = NULL;
}

/************************************************************************/
/*                      DefaultNTFRecordGrouper()                       */
/*                                                                      */
/*      Default rules for figuring out if a new candidate record        */
/*      belongs to a group of records that together form a feature      */
/*      (a record group).                                               */
/************************************************************************/

int DefaultNTFRecordGrouper( NTFFileReader *, NTFRecord ** papoGroup,
                             NTFRecord * poCandidate )

{
/* -------------------------------------------------------------------- */
/*      Is this group going to be a CPOLY set?  We can recognise        */
/*      this because we get repeating POLY/CHAIN sets without an        */
/*      intermediate attribute record.  This is a rather special case!  */
/* -------------------------------------------------------------------- */
    if( papoGroup[0] != NULL && papoGroup[1] != NULL
        && papoGroup[0]->GetType() == NRT_POLYGON
        && papoGroup[1]->GetType() == NRT_CHAIN )
    {
        // We keep going till we get the seed geometry.
        int 	iRec;

        for( iRec = 0; papoGroup[iRec] != NULL; iRec++ ) {}

        if( papoGroup[iRec-1]->GetType() != NRT_GEOMETRY )
            return TRUE;
        else
            return FALSE;
    }
    
/* -------------------------------------------------------------------- */
/*      Is this a "feature" defining record?  If so break out if it     */
/*      isn't the first record in the group.                            */
/* -------------------------------------------------------------------- */
    if( papoGroup[0] != NULL 
        && (poCandidate->GetType() == NRT_NAMEREC
            || poCandidate->GetType() == NRT_NODEREC
            || poCandidate->GetType() == NRT_LINEREC
            || poCandidate->GetType() == NRT_POINTREC
            || poCandidate->GetType() == NRT_POLYGON
            || poCandidate->GetType() == NRT_CPOLY
            || poCandidate->GetType() == NRT_COLLECT
            || poCandidate->GetType() == NRT_TEXTREC) )
    {
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Do we already have a record of this type?  If so, it likely     */
/*      doesn't belong in the group.  Attribute records do repeat in    */
/*      some products.                                                  */
/* -------------------------------------------------------------------- */
    if (poCandidate->GetType() != NRT_ATTREC )
    {
        int	iRec;
        for( iRec = 0; papoGroup[iRec] != NULL; iRec++ )
        {
            if( poCandidate->GetType() == papoGroup[iRec]->GetType() )
                break;
        }
           
        if( papoGroup[iRec] != NULL )
            return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                          ReadRecordGroup()                           */
/*                                                                      */
/*      Read a group of records that form a single feature.             */
/************************************************************************/

NTFRecord **NTFFileReader::ReadRecordGroup()

{
   NTFRecord	 *poRecord;
   int		  nRecordCount = 0;

   ClearCGroup();
   
/* -------------------------------------------------------------------- */
/*      Loop, reading records till we think we have a grouping.         */
/* -------------------------------------------------------------------- */
   while( (poRecord = ReadRecord()) != NULL && poRecord->GetType() != NRT_VTR )
   {
       if( nRecordCount >= MAX_REC_GROUP )
           break;

       if( !pfnRecordGrouper( this, apoCGroup, poRecord ) )
           break;
       
       apoCGroup[nRecordCount++] = poRecord;
       apoCGroup[nRecordCount] = NULL;
   }
   
/* -------------------------------------------------------------------- */
/*      Push the last record back on the input queue.                   */
/* -------------------------------------------------------------------- */
   SaveRecord( poRecord );

/* -------------------------------------------------------------------- */
/*      Return the list, or NULL if we didn't get any records.          */
/* -------------------------------------------------------------------- */
   if( nRecordCount == 0 )
       return NULL;
   else
       return apoCGroup;
}

/************************************************************************/
/*                          GetFeatureClass()                           */
/************************************************************************/

int NTFFileReader::GetFeatureClass( int iFCIndex, int *pnFCId,
                                    char ** ppszFCName )

{
    if( iFCIndex < 0 || iFCIndex >= nFCCount )
    {
        *pnFCId = -1;
        *ppszFCName = NULL;
        return FALSE;
    }
    else
    {
        *pnFCId = panFCNum[iFCIndex];
        *ppszFCName = papszFCName[iFCIndex];
        return TRUE;
    }
}

/************************************************************************/
/*                           ReadOGRFeature()                           */
/************************************************************************/

OGRFeature * NTFFileReader::ReadOGRFeature( OGRNTFLayer * poTargetLayer )

{
    OGRNTFLayer	*poLayer = NULL;
    NTFRecord	**papoGroup;
    OGRFeature  *poFeature = NULL;

/* -------------------------------------------------------------------- */
/*      Loop looking for a group we can translate, and that if          */
/*      needed matches our layer request.                               */
/* -------------------------------------------------------------------- */
    while( poFeature == NULL && (papoGroup = ReadRecordGroup()) != NULL )
    {
        poLayer = apoTypeTranslation[papoGroup[0]->GetType()];
        if( poLayer == NULL )
            continue;

        if( poTargetLayer != NULL && poTargetLayer != poLayer )
        {
            nSavedFeatureId++;
            continue;
        }

        poFeature = poLayer->FeatureTranslate( this, papoGroup );
        if( poFeature == NULL )
        {
            // should this be a real error?
            CPLDebug( "NTF",
                      "FeatureTranslate() failed for a %d record group in\n"
                      "a %s type file.\n",
                      papoGroup[0]->GetType(),
                      GetProduct() );
            CPLAssert( FALSE );
        }
    }

/* -------------------------------------------------------------------- */
/*      If we got a feature, set the TILE_REF on it.                    */
/* -------------------------------------------------------------------- */
    if( poFeature != NULL )
    {
        int		iTileRefField;

        iTileRefField = poLayer->GetLayerDefn()->GetFieldCount()-1;
    
        CPLAssert( EQUAL(poLayer->GetLayerDefn()->GetFieldDefn(iTileRefField)->
                         GetNameRef(), "TILE_REF") );

        poFeature->SetField( iTileRefField, GetTileName() );
        poFeature->SetFID( nSavedFeatureId );

        nSavedFeatureId++;
    }

/* -------------------------------------------------------------------- */
/*      If we got to the end we can establish our feature count for     */
/*      the file.                                                       */
/* -------------------------------------------------------------------- */
    else
    {
        CPLAssert( nFeatureCount == -1
                   || nFeatureCount == nSavedFeatureId - nBaseFeatureId );
        nFeatureCount = nSavedFeatureId - nBaseFeatureId;
    }

    return( poFeature );
}

/************************************************************************/
/*                            TestForLayer()                            */
/*                                                                      */
/*      Return indicator of whether this file contains any features     */
/*      of the indicated layer type.                                    */
/************************************************************************/

int NTFFileReader::TestForLayer( OGRNTFLayer * poLayer )

{
    for( int i = 0; i < 100; i++ )
    {
        if( apoTypeTranslation[i] == poLayer )
            return TRUE;
    }

    return FALSE;
}

