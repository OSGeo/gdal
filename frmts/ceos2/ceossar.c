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


#include "ceos.h"


extern Link_t *RecipeFunctions;

void InitCeosSARVolume(CeosSARVolume_t *volume, int32 file_name_convention)
{
    volume->Flavour = \
	volume->Sensor = \
	volume->ProductType = 0;

    volume->FileNamingConvention = file_name_convention ;

    volume->VolumeDirectoryFile =
	volume->SARLeaderFile =
        volume->SARTrailerFile =
	volume->NullVolumeDirectoryFile =
	volume->ImageDesc.ImageDescValid = FALSE;

    volume->RecordList = NULL;
}


void CalcCeosSARImageFilePosition(CeosSARVolume_t *volume, int channel, int line, int *record, int *file_offset)
{
    struct CeosSARImageDesc *ImageDesc;
    int TotalRecords, TotalBytes;

    if(record)
	*record = 0;
    if(file_offset)
	*file_offset = 0;

    if( volume )
    {
	if( volume->ImageDesc.ImageDescValid )
	{
	    ImageDesc = &( volume->ImageDesc );

	    switch( ImageDesc->ChannelInterleaving )
	    {
	    case __CEOS_IL_PIXEL:
		TotalRecords = (line - 1) * ImageDesc->RecordsPerLine;
		TotalBytes = (TotalRecords) * ( ImageDesc->BytesPerRecord );
		break;
	    case __CEOS_IL_LINE:
		TotalRecords = (ImageDesc->NumChannels * (line - 1) + 
				(channel - 1)) * ImageDesc->RecordsPerLine;
		TotalBytes = (TotalRecords) * ( ImageDesc->BytesPerRecord ) ;
		break;
	    case __CEOS_IL_BAND:
		TotalRecords = (((channel - 1) * ImageDesc->Lines) * 
				ImageDesc->RecordsPerLine) +
				(line - 1) * ImageDesc->RecordsPerLine;

		TotalBytes = (TotalRecords) * ( ImageDesc->BytesPerRecord );
		break;
	    }
	    if(file_offset)
		*file_offset = ImageDesc->FileDescriptorLength + TotalBytes;
	    if(record)
		*record = TotalRecords + 1;
	}
    }
}

int32 GetCeosSARImageData(CeosSARVolume_t *volume, CeosRecord_t *processed_data_record, int channel, int xoff, int xsize, int bufsize, uchar *buffer)
{
    return 0;
}

void DetermineCeosSARPixelOrder( CeosSARVolume_t *volume, CeosRecord_t *record )
{

}

void GetCeosSAREmbeddedInfo(CeosSARVolume_t *volume, CeosRecord_t *processed_data_record, CeosSAREmbeddedInfo_t *info)
{
}

void DeleteCeosSARVolume(CeosSARVolume_t *volume)
{
    Link_t *Links;
    
    if( volume )
    {
	if( volume->RecordList )
	{
	    for(Links = volume->RecordList; Links != NULL; Links = Links->next)
	    {
		if(Links->object)
		{
		    DeleteCeosRecord( Links->object );
		    Links->object = NULL;
		}
	    }
	    DestroyList( volume->RecordList );
	}
	HFree( volume );
    }
}
