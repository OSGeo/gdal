#include "ogr_tiger.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

TigerPoint::TigerPoint( int bRequireGeom )
{
  this->bRequireGeom = bRequireGeom;
}

int TigerPoint::SetModule( const char * pszModule, char *pszFileCode )
{
  if( !OpenFile( pszModule, pszFileCode ) )
    return FALSE;
  EstablishFeatureCount();
  return TRUE;
}

OGRFeature *TigerPoint::GetFeature( int nRecordId,
				    TigerRecordInfo *psRTInfo,
				    int nX0, int nX1,
				    int nY0, int nY1 )
{
  char        achRecord[OGR_TIGER_RECBUF_LEN];

  if( nRecordId < 0 || nRecordId >= nFeatures ) {
    CPLError( CE_Failure, CPLE_FileIO,
	      "Request for out-of-range feature %d of %sP",
	      nRecordId, pszModule );
    return NULL;
  }

  /* -------------------------------------------------------------------- */
  /*      Read the raw record data from the file.                         */
  /* -------------------------------------------------------------------- */

  if( fpPrimary == NULL )
    return NULL;

  if( VSIFSeek( fpPrimary, nRecordId * nRecordLength, SEEK_SET ) != 0 ) {
    CPLError( CE_Failure, CPLE_FileIO,
	      "Failed to seek to %d of %sP",
	      nRecordId * nRecordLength, pszModule );
    return NULL;
  }

  if( VSIFRead( achRecord, psRTInfo->reclen, 1, fpPrimary ) != 1 ) {
    CPLError( CE_Failure, CPLE_FileIO,
	      "Failed to read record %d of %sP",
	      nRecordId, pszModule );
    return NULL;
  }

  /* -------------------------------------------------------------------- */
  /*      Set fields.                                                     */
  /* -------------------------------------------------------------------- */

  OGRFeature  *poFeature = new OGRFeature( poFeatureDefn );

  SetFields( psRTInfo, poFeature, achRecord);

  /* -------------------------------------------------------------------- */
  /*      Set geometry                                                    */
  /* -------------------------------------------------------------------- */

  double      dfX, dfY;

  dfX = atoi(GetField(achRecord, nX0, nX1)) / 1000000.0;
  dfY = atoi(GetField(achRecord, nY0, nY1)) / 1000000.0;

  if( dfX != 0.0 || dfY != 0.0 ) {
    poFeature->SetGeometryDirectly( new OGRPoint( dfX, dfY ) );
  }
        
  return poFeature;
}

OGRErr TigerPoint::CreateFeature( OGRFeature *poFeature, 
				  TigerRecordInfo *psRTInfo,
				  int pointIndex,
				  char *pszFileCode)

{
  char        szRecord[OGR_TIGER_RECBUF_LEN];
  OGRPoint    *poPoint = (OGRPoint *) poFeature->GetGeometryRef();

  if( !SetWriteModule( pszFileCode, psRTInfo->reclen+2, poFeature ) )
    return OGRERR_FAILURE;

  memset( szRecord, ' ', psRTInfo->reclen );

  WriteFields( psRTInfo, poFeature, szRecord );

  if( poPoint != NULL 
      && (poPoint->getGeometryType() == wkbPoint
	  || poPoint->getGeometryType() == wkbPoint25D) ) {
    WritePoint( szRecord, pointIndex, poPoint->getX(), poPoint->getY() );
  } else {
    if (bRequireGeom) {
      return OGRERR_FAILURE;
    }
  }

  WriteRecord( szRecord, psRTInfo->reclen, pszFileCode );

  return OGRERR_NONE;
}
