#include "ceos.h"

#define PROJ_STR_SIZE 16

#define DATA_SET_RECORD_TYPECODE   { 18, 10, 18, 20 }

static int GetUtmZoneFromLong( double longitude );

int GetCeosOrbitalData( CeosSARVolume_t *volume, EphemerisSeg_t *Orb, ProjInfo_t *Proj )
{
    int x, y;
    ProjInfo_t proj1,proj2;
    char proj_str[ PROJ_STR_SIZE ];
    float temp_float;
    char temp_str32[ 32 ];
    int earthmodel;
    
    CeosRecord_t *data_set_rec;
    unsigned char data_set_code[] = DATA_SET_RECORD_TYPECODE;


    if( !volume || !Orb || !Proj )
        return 0;

/*    memset( Orb, 0, sizeof( EphemerisSeg_t ) ); */

    x = ( volume->ImageDesc.PixelsPerLine / 2 );
    y = ( volume->ImageDesc.Lines / 2 );

    /* Set scene centre coordinates */

    Orb->XCentre = x * Proj->XSize + Proj->XOff ;
    Orb->YCentre = y * Proj->YSize + Proj->YOff ;

    /* Set pixel sizes */

    Orb->PixelRes = Proj->XSize ;
    Orb->LineRes  = Proj->YSize ;
 
    /* Set scene corner coordinates */
               
    Orb->CornerAvail = TRUE ;

    Orb->XUL = Orb->XLL = Proj->XOff ;
    Orb->YUL = Orb->YUR = Proj->YOff ;
    Orb->XUR = Orb->XLR = \
      ( ( volume->ImageDesc.PixelsPerLine - 1 ) * Proj->XSize + Proj->XOff ) ;
    Orb->YLR = Orb->YLL = \
      ( ( volume->ImageDesc.Lines - 1 ) * Proj->YSize + Proj->YOff ) ;

    /* Get lat/long values */

    proj1 = *Proj;
    
    Geosys2ProjInfo( &proj2, "LONG E0", 0.0, 0.0, 1.0, 1.0 );
    PCI2GCTP( &proj1 );
    PCI2GCTP( &proj2 );

    GCTPTransform( &proj1, Orb->XUL, Orb->YUL,
                   &proj2, &( Orb->LatUL ), &( Orb->LongUL ) );

    Orb->LatLL = Orb->LatUL ;
    Orb->LongUR = Orb->LongUL ;

    GCTPTransform( &proj1, Orb->XLR, Orb->YLR,
                   &proj2, &( Orb->LatLR ), &( Orb->LongLR ) );

    Orb->LatUR = Orb->LatLR ;
    Orb->LongLL = Orb->LongLR ;

    GCTPTransform( &proj1, Orb->XCentre, Orb->YCentre,
                   &proj2, &( Orb->LatCentreDeg ), &( Orb->LongCentreDeg ) );

    /* Convert lat/long to UTM -- can be quite inaccurate */

    /* First we do the centre line/pixel                  */

    sprintf( proj_str, "UTM %d", 
	      GetUtmZoneFromLong( Orb->LongCentreDeg ) );

    Geosys2ProjInfo( &proj1, proj_str, 0.0, 0.0, 1.0, 1.0 );
    PCI2GCTP( &proj1 );

    GCTPTransform( &proj2, Orb->LatCentreDeg, Orb->LongCentreDeg,
		   &proj1, &( Orb->UtmXCentre ), &( Orb->UtmYCentre ) );

    /* Next we do the UpperLeft pixel                     */

    sprintf( proj_str, "UTM %d", 
	      GetUtmZoneFromLong( Orb->LongUL ) );

    Geosys2ProjInfo( &proj1, proj_str, 0.0, 0.0, 1.0, 1.0 );
    PCI2GCTP( &proj1 );

    GCTPTransform( &proj2, Orb->LongUL, Orb->LatUL,
		   &proj1, &( Orb->UtmXUL ), &( Orb->UtmYUL ) );

    /* Next we do the UpperRight pixel                     */

    sprintf( proj_str, "UTM %d", 
	      GetUtmZoneFromLong( Orb->LongUR ) );

    Geosys2ProjInfo( &proj1, proj_str, 0.0, 0.0, 1.0, 1.0 );
    PCI2GCTP( &proj1 );

    GCTPTransform( &proj2, Orb->LongUR, Orb->LatUR,
		   &proj1, &( Orb->UtmXUR ), &( Orb->UtmYUR ) );

    /* Transform for LowerRight pixel                      */

    sprintf( proj_str, "UTM %d", 
	      GetUtmZoneFromLong( Orb->LongLL ) );

    Geosys2ProjInfo( &proj1, proj_str, 0.0, 0.0, 1.0, 1.0 );
    PCI2GCTP( &proj1 );

    GCTPTransform( &proj2, Orb->LongLL, Orb->LatLL,
		   &proj1, &( Orb->UtmXLL ), &( Orb->UtmYLL ) );

    /* Transform for LowerLeft pixel                       */

    sprintf( proj_str, "UTM %d", 
	      GetUtmZoneFromLong( Orb->LongLR ) );

    Geosys2ProjInfo( &proj1, proj_str, 0.0, 0.0, 1.0, 1.0 );
    PCI2GCTP( &proj1 );

    GCTPTransform( &proj2, Orb->LongLR, Orb->LatLR,
		   &proj1, &( Orb->UtmXLR ), &( Orb->UtmYLR ) );

    Orb->ImageRecordLength   = volume->ImageDesc.BytesPerRecord ;
    Orb->NumberImageLine     = volume->ImageDesc.Lines ;
    Orb->NumberBytePerPixel  = volume->ImageDesc.BytesPerPixel ;
    Orb->NumberSamplePerLine = volume->ImageDesc.PixelsPerLine ;
    Orb->NumberPrefixBytes   = volume->ImageDesc.ImageDataStart ;
    Orb->NumberSuffixBytes   = volume->ImageDesc.ImageSuffixData ;

    if( sscanf( Proj->Units, "%s E%d", temp_str32, &earthmodel ) < 2 )
    {
        earthmodel = -1 ;
    }

    strncpy( Orb->MapUnit, temp_str32, 16 );

    Orb->MapUnit[16] = '\0';

    /* Look for a map projection data record */
    data_set_rec = FindCeosRecord( volume->RecordList, *(CeosTypeCode_t *) &data_set_code, -1, -1, -1 );
    if ( data_set_rec == NULL )
    {
	/* No data set summary record found.  Use embedded (in-line) lat/long data instead */
	Orb->Type = OrbNone ;
    } else {

        /* Now we get the first field data */

        GetCeosField( data_set_rec, 413, "A32", Orb->SatelliteDesc );
        GetCeosField( data_set_rec, 21, "A16", Orb->SceneID );

        Orb->Type = OrbLatLong ;

        GetCeosField( data_set_rec, 1111, "A32", temp_str32 );
	strncpy( Orb->OrbitLine->RadarSeg->Identifier, temp_str32, 16 );
	Orb->OrbitLine->RadarSeg->Identifier[16] = '\0' ;

	GetCeosField( data_set_rec, 1047, "A16", Orb->OrbitLine->RadarSeg->Facility );

	GetCeosField( data_set_rec, 485, "F8.3", &temp_float );
	Orb->OrbitLine->RadarSeg->IncidenceAngle = ( double ) temp_float ;

	GetCeosField( data_set_rec, 477, "F8.3", &temp_float );
	Orb->OrbitLine->RadarSeg->ClockAngle = ( double ) temp_float ;

	GetCeosField( data_set_rec, 1687, "F16.7", &temp_float );
	Orb->OrbitLine->RadarSeg->LineSpacing = ( double ) temp_float ;

	GetCeosField( data_set_rec, 1703, "F16.7", &temp_float );
	Orb->OrbitLine->RadarSeg->PixelSpacing = ( double ) temp_float ;

        GetCeosField( data_set_rec, 181, "F16.7", &temp_float );
        Orb->OrbitLine->RadarSeg->EquatorialRadius = ( double ) temp_float ;

        GetCeosField( data_set_rec, 197, "F16.7", &temp_float );
        Orb->OrbitLine->RadarSeg->PolarRadius = ( double ) temp_float ;

        GetCeosField( data_set_rec, 165, "A16", Orb->OrbitLine->RadarSeg->Ellipsoid );

#if 0
	/* Get the Ellipsoid parameters */

	if( !GetEllipsoidData( Orb->MapUnit, &earthmodel,
			       &( Orb->OrbitLine->RadarSeg->EquatorialRadius ),
			       &( Orb->OrbitLine->RadarSeg->PolarRadius ) ) )
	{
	    Orb->OrbitLine->RadarSeg->EquatorialRadius = 0.0 ;
	    Orb->OrbitLine->RadarSeg->PolarRadius = 0.0 ;
	}

#endif

    }
    return 0;

}

static int GetUtmZoneFromLong( double longitude )
{
    int Zone ;

    Zone = (int) ( longitude + 180 ) ;
    
    Zone /= 6;

    if( Zone > 60 )
    {
        Zone = 60 ;  /* In the case of 180 */
    }

    return Zone ;
}
