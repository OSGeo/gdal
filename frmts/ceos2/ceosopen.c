/* Copyright (c) 1997
 * Atlantis Scientific Inc, 20 Colonnade, Suite 110
 * Nepean, Ontario, K2E 7M6, Canada
 *
 * All rights reserved.  Not to be used, reproduced
 * or disclosed without permission.
 */

/* +---------------------------------------------------------------------+
 * |@@@@@@@@@@    @@@| EASI/PACE V6.0, Copyright (c) 1997.               |
 * |@@@@@@ ***      @|                                                   |
 * |@@@  *******    @| PCI Inc., 50 West Wilmot Street,                  |
 * |@@  *********  @@| Richmond Hill, Ontario, L4B 1M5, Canada.          |
 * |@    *******  @@@|                                                   |
 * |@      *** @@@@@@| All rights reserved. Not to be used, reproduced   |
 * |@@@    @@@@@@@@@@| or disclosed without permission.                  |
 * +---------------------------------------------------------------------+
 */

#include "gdbfrmts.h"
#include "ceos.h"

char *CeosExtension[7][5] = { { "vol", "led", "img", "trl", "nul" },
			      { "vol", "lea", "img", "trl", "nul" },
			      { "vol", "led", "img", "tra", "nul" },
			      { "vol", "lea", "img", "tra", "nul" },
			      { NULL, NULL, NULL, NULL, NULL },
			      { "vdf_dat", "lea_%02d", "dat_%02d", "tra_%02d", "nul_vdf" },
			      { NULL, NULL, NULL, NULL, NULL } };
#define RSAT 5

RCSID("$Id$")

typedef struct spl {
    struct spl      *next;
    FILE            *fpCEOS;
    ProjInfo_t      ProjInfo;
    EphemerisSeg_t  OrbInfo;
    RadarSeg_t      RadarSeg;
    CeosSARVolume_t *volume;
} CEOSInfo_t;

static int ProcessData( FILE *fp, int fileid, CeosSARVolume_t *sar, int max_records, int max_bytes );

void ReadCeosRecords( FILE *fp, void *token, int line, int pixels, uchar *buffer );

#define CEOSPromote(x) (Promote(x, &CEOSList))


CEOSInfo_t	*CEOSList = NULL;		/** Primary declaration **/



/************************************************************************/
/*                              CEOSOpen()                               */
/*                                                                      */
/*      GDB Open method for CEOS image formats.                        */
/************************************************************************/
static FILE *CEOSOpen( char * filename, char * access )
{
    struct CeosSARImageDesc  *ImageDesc;
    CeosSARVolume_t          *CeosSARVolume = NULL;
    CeosSARImageDescRecipe_t *recipe;
    FILE                     *fp, *process_fp;
    unsigned char            buffer[__CEOS_HEADER_LENGTH];
    int                      type, sequence, i, e, interleave,
	                     StartData, LineSize, LineSize2, channels;
    Glob_t                   *psGlob;
    char                     *temp_filename;
    CEOSInfo_t               *ceosinfo = NULL, *xinfo = NULL;
    

    if( IMPProtect() )
    {
	DeleteCeosSARVolume( CeosSARVolume );
	return( NULL );
    }

    HMHandler(HM_OOM_IMPERROR);

    fp = DKOpen( filename, FL_CEOS, access );

    DKRead( fp, buffer, 0L, __CEOS_HEADER_LENGTH );

/* ------------------------------------------------------------- */
/* Check to make sure it's a CEOS file                           */
/* ------------------------------------------------------------- */

    CeosToNative( &sequence, buffer + __SEQUENCE_OFF, sizeof( int ), sizeof( int ) );
    CeosToNative( &type, buffer + __TYPE_OFF, sizeof( int ), sizeof( int ) );

    if(   sequence != 1
       || (type != 0x3FC01212 && type != 0x32C01212 ) )
    {
	xinfo = CEOSList;
	CEOSList = CEOSList->next;
	HFree( xinfo );
	DKClose( fp );
	IMPErrChar( 177, ERRTYP_PFATAL, "filename", filename );
    }

/* ------------------------------------------------------------- */
/* Create a new CEOSInfo_t link                                  */
/* ------------------------------------------------------------- */

    ceosinfo = (CEOSInfo_t *) HMalloc( sizeof( CEOSInfo_t ) );
    ceosinfo->next = CEOSList;
    CEOSList = ceosinfo;
    CEOSList->fpCEOS = fp;

    memset( &( CEOSList->OrbInfo ), 0, sizeof( EphemerisSeg_t ) );
    memset( &( CEOSList->RadarSeg ), 0, sizeof( RadarSeg_t ) );

    CEOSList->OrbInfo.OrbitLine = 
      (OrbitData_t *) HMalloc( sizeof( OrbitData_t ) );

    CEOSList->OrbInfo.OrbitLine->RadarSeg = &( CEOSList->RadarSeg );


/* -------------------------------------------------------------- */
/* Now we build a CeosSARVolume_t from the *fp to start the       */
/* ball rolling                                                   */
/* -------------------------------------------------------------- */

    CeosSARVolume = HMalloc( sizeof( CeosSARVolume_t ) );

    InitCeosSARVolume ( CeosSARVolume, 0 );

    CEOSList->volume = CeosSARVolume;

    CeosSARVolume->ImagryOptionsFile = TRUE;

    if(ProcessData( fp,__CEOS_IMAGRY_OPT_FILE, CeosSARVolume, 4, -1 ))
    {
	/* Failed getting data */
	xinfo = CEOSList;
	CEOSList = CEOSList->next;
	HFree( xinfo );
	IMPError( ERR_APPDEFINED, ERRTYP_UFATAL,
                 "Corrupted or unknown CEOS file format.\n"
                 "FILE = %s\n",
                 filename );
    }

    psGlob = IMPFile2Glob( fp2filename(fp), GLOB_LOCAL );

    if( atoi( psGlob->extension ) > 0 )
    {
	/* Process as RadarSAT naming scheme */
	int num = atoi( psGlob->extension );

	for(i = 0; i < 5; i++ )
	{
	    if( i != 2)
	    {
		char temp_str[24];
		char *oldbasename;
		
		oldbasename = psGlob->basename;
		sprintf( temp_str, CeosExtension[RSAT][i], num );
		psGlob->basename = temp_str;
		temp_filename = IMPGlob2File( psGlob, GLOB_LOCAL );
		if( DKCheck( temp_filename ) )
		{
		    process_fp = DKOpen( temp_filename, FL_OTHER, "r" );
		    if(! ProcessData( process_fp, i, CeosSARVolume, -1, DKSize( temp_filename ) ) )
		    {
			switch( i )
			{
			case 0: CeosSARVolume->VolumeDirectoryFile = TRUE;
			    break;
			case 1: CeosSARVolume->SARLeaderFile = TRUE;
			    break;
			case 3: CeosSARVolume->SARTrailerFile = TRUE;
			    break;
			case 4: CeosSARVolume->NullVolumeDirectoryFile = TRUE;
			    break;
			}
		    }
		    DKClose( process_fp );
		}
		psGlob->basename = oldbasename;
	    }
	}
    } else {

	int MaxExtLen;

	MaxExtLen = strlen( psGlob->extension );

	for( i = 0;i < 5;i++ )
	{
	    if( i != 2 )
	    {
		e = 0;
		while( CeosExtension[e][i] != NULL )
		{
		    strncpy( psGlob->extension, CeosExtension[e][i], MaxExtLen );
		    temp_filename = IMPGlob2File( psGlob, GLOB_LOCAL );
		    if( DKCheck( temp_filename ) )
		    {
			process_fp = DKOpen( filename, FL_OTHER, "r" );
			ProcessData( process_fp, i, CeosSARVolume, -1, DKSize( temp_filename ) );
			DKClose( process_fp );
			switch( i )
			{
			case 0: CeosSARVolume->VolumeDirectoryFile = TRUE;
			    break;
			case 1: CeosSARVolume->SARLeaderFile = TRUE;
			    break;
			case 3: CeosSARVolume->SARTrailerFile = TRUE;
			    break;
			case 4: CeosSARVolume->NullVolumeDirectoryFile = TRUE;
			    break;
			}
			break;  /* Exit the while loop, we have this data type */
		    }
		    HFree( temp_filename );
		    e++;
		}
	    }
	}
    }


    IMPDestroyGlob( psGlob );

    GetCeosSARImageDesc( CeosSARVolume );

    ImageDesc = &( CeosSARVolume->ImageDesc );

    if( ImageDesc->ImageDescValid == FALSE )
    {
	xinfo = CEOSList;
	CEOSList = CEOSList->next;
	HFree( xinfo );
	DeleteCeosSARVolume( CeosSARVolume );
	IMPError( ERR_APPDEFINED, ERRTYP_UFATAL,
                 "Unable to open CEOS file.  Unsupported type.\n"
                 "FILE = %s\n",
                 filename );
    } else {

	/* We get Projection information */

	GetCeosProjectionData( fp, CeosSARVolume, &( CEOSList->ProjInfo ) );

	/* Then OrbitalData */

	GetCeosOrbitalData( CeosSARVolume, &( CEOSList->OrbInfo ), &( CEOSList->ProjInfo ) );
	
	switch( ImageDesc->DataType )
	{
	case __CEOS_TYP_CHAR:
	case __CEOS_TYP_UCHAR:
	case __CEOS_TYP_COMPLEX_CHAR:
	case __CEOS_TYP_COMPLEX_UCHAR:
	    type = CHN_8U;
	    break;
	case __CEOS_TYP_SHORT:
	case __CEOS_TYP_COMPLEX_SHORT:
	    type = CHN_16S;
	    break;
	case __CEOS_TYP_USHORT:
	case __CEOS_TYP_COMPLEX_USHORT:
	    type = CHN_16U;
	    break;
	case __CEOS_TYP_LONG:
	case __CEOS_TYP_ULONG:
	case __CEOS_TYP_COMPLEX_LONG:
	case __CEOS_TYP_COMPLEX_ULONG:
	case __CEOS_TYP_FLOAT:
	case __CEOS_TYP_DOUBLE:
	    type = CHN_32R;
	    break;
	}

	switch( ImageDesc->ChannelInterleaving )
	{
	case __CEOS_IL_PIXEL:
	    interleave = IL_PIXEL;
	    break;
	case __CEOS_IL_LINE:
	    interleave = IL_LINE;
	    break;
	case __CEOS_IL_BAND:
	    interleave = IL_BAND;
	    break;
	}

	CalcCeosSARImageFilePosition( CeosSARVolume, 1, 1, NULL, &StartData );

	StartData += ImageDesc->ImageDataStart;

	if( ImageDesc->DataType >= __CEOS_TYP_COMPLEX_CHAR )
	    channels = 2*ImageDesc->NumChannels;
	else
	    channels = ImageDesc->NumChannels;

	RawDefine( fp, ImageDesc->PixelsPerLine, ImageDesc->Lines,
		   channels, type, interleave,
		   -1 );

	if( ImageDesc->RecordsPerLine > 1
	    || ImageDesc->DataType == __CEOS_TYP_CHAR
	    || ImageDesc->DataType == __CEOS_TYP_LONG
	    || ImageDesc->DataType == __CEOS_TYP_ULONG
	    || ImageDesc->DataType == __CEOS_TYP_DOUBLE
	    || ImageDesc->DataType >= __CEOS_TYP_COMPLEX_CHAR )
	{
	    /* Roll your own Reading routines */

	    for( i = 0; i < channels; i++ )
	    {
		FCNSetChanInfo( fp, i+1, type, SWAPPED_FLAG, &ReadCeosRecords, NULL, (void *) (long) (i) );
	    }
	}
	else
	{

	    CalcCeosSARImageFilePosition( CeosSARVolume, 1, 1, NULL, &LineSize );
	    CalcCeosSARImageFilePosition( CeosSARVolume, 1, 2, NULL, &LineSize2 );
	    LineSize = LineSize2 - LineSize;

	    for( i = 0; i < ImageDesc->NumChannels; i++ )
	    {
		switch( interleave )
		{
		case IL_PIXEL:
		    CalcCeosSARImageFilePosition( CeosSARVolume, 1, 1, NULL, &StartData);
		    StartData += ImageDesc->ImageDataStart ;
		    StartData += (ImageDesc->BytesPerPixel * i);

		    RawSetChanInfo( fp, i+1, StartData, ImageDesc->BytesPerPixel*ImageDesc->NumChannels, LineSize, type, SWAPPED_FLAG );
		    break;
		case IL_LINE:
		    CalcCeosSARImageFilePosition( CeosSARVolume, i+1, 1, NULL, &StartData );
		    StartData += ImageDesc->ImageDataStart;

		    RawSetChanInfo( fp, i+1, StartData,
				    ImageDesc->BytesPerPixel,
				    LineSize * ImageDesc->NumChannels,
				    type, SWAPPED_FLAG );
		    break;
		case IL_BAND:
		    CalcCeosSARImageFilePosition( CeosSARVolume, i+1, 1, NULL, &StartData );

		    StartData += ImageDesc->ImageDataStart;

		    RawSetChanInfo( fp, i+1, StartData,
				    ImageDesc->BytesPerPixel,
				    LineSize,
				    type, SWAPPED_FLAG );
		    break;
		}
	    }
	}
    }

    IMPUnprotect();

    HMHandler(HM_OOM_DIE);

    return fp;
}

static int CEOSProjectionIO( FILE *fp, int func, ProjInfo_t *proj_info )

{

    if( !fpValidate(fp) || !CEOSPromote(fp) )
	IMPErrChar (177,0,"fp", "");
    
    if( func == GDB_READ )
	ByteCopy( &(CEOSList->ProjInfo), proj_info, sizeof(ProjInfo_t) );
    else
	IMPErrChar( 177,0,"fp", "" );

    return( DecodeGeosys(proj_info->Units, proj_info->Units) );
}


static int ProcessData( FILE *fp, int fileid, CeosSARVolume_t *sar, int max_records, int max_bytes )
{
    unsigned char      temp_buffer[__CEOS_HEADER_LENGTH];
    unsigned char      *temp_body = NULL;
    int                start = 0;
    int                CurrentBodyLength = 0;
    int                CurrentType = 0;
    int                CurrentSequence = 0;
    Link_t             *TheLink;
    CeosRecord_t       *record;

    if(IMPProtect())
    {
	if(CurrentBodyLength > 0)
	{
	    HFree( temp_body );
	}
	return 1;
    }

    while(max_records != 0 && max_bytes != 0)
    {

	record = HMalloc( sizeof( CeosRecord_t ) );
	DKRead( fp, temp_buffer, start, __CEOS_HEADER_LENGTH );
	record->Length = DetermineCeosRecordBodyLength( temp_buffer );

	if( record->Length > CurrentBodyLength )
	{
	    if(CurrentBodyLength == 0 )
		temp_body = HMalloc( record->Length );
	    else
	    {
		temp_body = HRealloc( temp_body, record->Length );
                CurrentBodyLength = record->Length;
            }
	}

	DKRead( fp, temp_body, start + __CEOS_HEADER_LENGTH, record->Length - __CEOS_HEADER_LENGTH );

	InitCeosRecordWithHeader( record, temp_buffer, temp_body );

	if( CurrentType == record->TypeCode.Int32Code )
	    record->Subsequence = ++CurrentSequence;
	else {
	    CurrentType = record->TypeCode.Int32Code;
	    record->Subsequence = CurrentSequence = 0;
	}

	record->FileId = fileid;

	TheLink = CreateLink( record );

	if( sar->RecordList == NULL )
	    sar->RecordList = TheLink;
	else
	    sar->RecordList = InsertLink( sar->RecordList, TheLink );

	start += record->Length;

	if(max_records > 0)
	    max_records--;
	if(max_bytes > 0)
        {
	    max_bytes -= record->Length;
            if(max_bytes < 0)
                max_bytes = 0;
        }
    }

    HFree(temp_body);

    IMPUnprotect();

    return 0;

}

void ReadCeosRecords( FILE *fp, void *token, int line, int pixels, uchar *buffer )
{
    int                     channel,
	                    complex,
                            offset,
                            Length,
                            TotalRead,
                            i;
    struct CeosSARImageDesc *ImageDesc;

    /* How to look at the data in the buffer pointer that is passed */
    signed short            *ShortBuffer    = NULL;
    unsigned short          *UShortBuffer   = NULL;
    float                   *FloatBuffer    = NULL;

    /* Temporary buffers we all set to same location so we can read data differently depending on what it is */
    signed   char  *TempChar   = NULL;
    unsigned char  *TempUChar  = NULL;
    signed   short *TempShort  = NULL;
    unsigned short *TempUShort = NULL;
    signed   int   *TempInt   = NULL;
    unsigned int   *TempUInt  = NULL;
    float          *TempFloat  = NULL;
    double         *TempDouble = NULL;

    channel = (int) (long) (token);

    complex = channel % 2;

    channel /= 2;

    CEOSPromote(fp);

    /* Let's calculate the size of a line */

    ImageDesc = &( CEOSList->volume->ImageDesc );

    TempChar   = ( signed     char * ) HMalloc( ImageDesc->BytesPerPixel * pixels );
    TempUChar  = ( unsigned   char * ) TempChar;
    TempShort  = (           short * ) TempChar;
    TempUShort = ( unsigned  short * ) TempChar;
    TempInt   = (            int   * ) TempChar;
    TempUInt  = ( unsigned   int   * ) TempChar;
    TempFloat  = (           float * ) TempChar;
    TempDouble = (          double * ) TempChar;
    

    CalcCeosSARImageFilePosition( CEOSList->volume, channel + 1, line + 1,NULL,&offset );

    Length = ImageDesc->PixelsPerRecord ;

    TotalRead = 0;

    offset += ImageDesc->ImageDataStart;

    for( i = 0; i < ImageDesc->RecordsPerLine; i++ )
    {
        if( TotalRead + Length > pixels )
	{
	    Length = pixels - TotalRead;
	}

	if( Length > 0 )
	    DKRead( fp, TempUChar + ( TotalRead * ImageDesc->BytesPerPixel ), offset, Length * ImageDesc->BytesPerPixel );

	offset += ImageDesc->BytesPerRecord;

	TotalRead += Length ;
    }

    switch( ImageDesc->DataType )
    {
    case __CEOS_TYP_UCHAR:
    case __CEOS_TYP_SHORT:
    case __CEOS_TYP_USHORT:
    case __CEOS_TYP_FLOAT: memcpy( buffer, TempUChar, pixels * ImageDesc->BytesPerPixel );
	break;
    case __CEOS_TYP_CHAR:
	/* Case for signed char data */
	for( i = 0; i < pixels; i++ )
	{
	    buffer[i] = TempChar[i] + 127;
	}
	break;
    case __CEOS_TYP_LONG:
	FloatBuffer = (float *) buffer;
	for( i = 0;i < pixels; i++ )
	{
	    FloatBuffer[i] = (float) TempInt[i];
	}
	break;
    case __CEOS_TYP_ULONG:
	FloatBuffer = (float *) buffer;
	for( i = 0; i < pixels; i++ )
	{
	    FloatBuffer[i] = (float) (TempUInt[i] - 2147483647);
	}
	break;
    case __CEOS_TYP_COMPLEX_UCHAR:
	for( i = 0;i < pixels; i++ )
	{
	    buffer[i] = TempUChar[2*i+complex];
	}
	break;
    case __CEOS_TYP_COMPLEX_SHORT:
	ShortBuffer = (short *) buffer;
	for( i = 0; i < pixels; i++ )
	{
	    ShortBuffer[i] = TempShort[2*i+complex];
	}
	break;
    case __CEOS_TYP_COMPLEX_USHORT:
	UShortBuffer = (unsigned short *) buffer;
	for( i = 0; i < pixels; i++ )
	{
	    UShortBuffer[i] = TempUShort[2*i+complex];
	}
	break;
    case __CEOS_TYP_COMPLEX_LONG:
	FloatBuffer = (float *) buffer;
	for( i = 0;i < pixels; i++ )
	{
	    FloatBuffer[i] = (float) TempInt[2*i+complex];
	}
	break;
    case __CEOS_TYP_COMPLEX_ULONG:
	FloatBuffer = (float *) buffer;
	for( i = 0; i < pixels; i++ )
	{
	    FloatBuffer[i] = (float) (TempUInt[2*i+complex] - 2147483647);
	}
	break;
    case __CEOS_TYP_COMPLEX_FLOAT:
	FloatBuffer = (float *) buffer;
	for( i = 0;i < pixels; i++ )
	{
	    FloatBuffer[i] = (float) TempFloat[2*i+complex];
	}
	break;
    case __CEOS_TYP_DOUBLE:
	FloatBuffer = (float *) buffer;
	for( i = 0;i < pixels; i++ )
	{
	    FloatBuffer[i] = (float) TempDouble[i];
	}
	break;
    }

    HFree( TempUChar );

}

/************************************************************************/
/*                            CEOSTestOpen()                             */
/*                                                                      */
/*      Test to see if a file is a CEOS Image File.			*/
/************************************************************************/

static int	CEOSTestOpen( GDBTestInfo_t *test_info )
{
    CeosRecord_t	    *record;


    if( test_info->Type != FTT_FILE || test_info->CacheSize < __CEOS_HEADER_LENGTH )
    {
	return( FALSE );
    }
/* -------------------------------------------------------------------- */
/*	If it's a plain file, test to see if it's a CEOS file.		*/
/* -------------------------------------------------------------------- */
    record = HMalloc( sizeof( CeosRecord_t ) );


    CeosToNative( &( record->Length ), test_info->DataCache+__LENGTH_OFF, sizeof( record->Length ), sizeof( record->Length ) );

     memcpy( &( record->TypeCode.Int32Code ), test_info->DataCache+__TYPE_OFF, sizeof( record->TypeCode.Int32Code ) );

     CeosToNative( &( record->Sequence ), test_info->DataCache+__SEQUENCE_OFF, sizeof( record->Sequence ), sizeof( record->Sequence ) );

     if( (record->TypeCode.UCharCode.Subtype1 == 0x3f
	  || record->TypeCode.UCharCode.Subtype1 == 0x32)
       && record->TypeCode.UCharCode.Type == 0xc0
       && record->TypeCode.UCharCode.Subtype2 == 0x12
       && record->TypeCode.UCharCode.Subtype3 == 0x12
       && record->Sequence == 1)
    {
	HFree( record );
	return( TRUE );
    } else {
/* -------------------------------------------------------------------- */
/*	It can't possible be a CEOS file.			*/
/* -------------------------------------------------------------------- */
	HFree( record );
	return( FALSE );
    }
}

/************************************************************************/
/*                              CEOSClose()                              */
/*                                                                      */
/*      Close the file.                                                 */
/************************************************************************/

void	CEOSClose( FILE * fp )

{
	CEOSInfo_t	*xinfo;

	if( IMPProtect() )
	    return;

/* -------------------------------------------------------------------- */
/*	Check for errors						*/
/* -------------------------------------------------------------------- */
	if( !fpValidate(fp) || !CEOSPromote(fp) )
	    IMPErrChar (177,0,"fp", "");

/* -------------------------------------------------------------------- */
/*	Close the image file.						*/
/* -------------------------------------------------------------------- */
	RawClose( fp );

	DKClose( fp ); 

/* -------------------------------------------------------------------- */
/*	Deallocate the CEOSInfo link.					*/
/* -------------------------------------------------------------------- */
	xinfo = CEOSList;
	CEOSList = CEOSList->next;
	
	DeleteCeosSARVolume(xinfo->volume);

        HFree( xinfo->OrbInfo.OrbitLine);

	HFree( xinfo );
	    
	IMPUnprotect();
}

/************************************************************************/
/*                            CEOSRegister()                             */
/*                                                                      */
/*      Register CEOS format information.                                */
/************************************************************************/

void	CEOSRegister()

{
	GDBInfo_t	*gdb_info;

	if( IMPProtect() )
	    return;

/* -------------------------------------------------------------------- */
/*	Register the CEOS services with the GDB layer.			*/
/* -------------------------------------------------------------------- */
	gdb_info = GDBRegister( FL_CEOS );
	if( gdb_info != NULL )
	{
	    gdb_info->SizeInfo = RawSizeInfo;
	    gdb_info->ChanType = RawChanType;
	    gdb_info->ByteChanIO = RawByteChanIO;
	    gdb_info->IntChanIO = RawIntChanIO;
	    gdb_info->RealChanIO = RawRealChanIO;
	    gdb_info->Hint = RawHint;
	    gdb_info->Close = CEOSClose;
	    gdb_info->Open = CEOSOpen;
	    gdb_info->GetChanInfo = RawGetChanInfo;
	    gdb_info->ProjectionIO = CEOSProjectionIO;
	    gdb_info->TestOpen = CEOSTestOpen;
	    strcpy( gdb_info->ShortName, "CEO" );
	    gdb_info->LongName = "CEOS Image";
	}

	IMPUnprotect();
}
