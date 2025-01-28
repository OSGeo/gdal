/******************************************************************************
 *
 * Project:  ASI CEOS Translator
 * Purpose:  Functions related to CeosSARVolume_t.
 * Author:   Paul Lahaie, pjlahaie@atlsci.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Atlantis Scientific Inc
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ceos.h"

extern Link_t *RecipeFunctions;

void InitCeosSARVolume(CeosSARVolume_t *volume, int32 file_name_convention)
{
    volume->Flavor = volume->Sensor = volume->ProductType = 0;

    volume->FileNamingConvention = file_name_convention;

    volume->VolumeDirectoryFile = volume->SARLeaderFile =
        volume->SARTrailerFile = volume->NullVolumeDirectoryFile =
            volume->ImageDesc.ImageDescValid = FALSE;

    volume->RecordList = NULL;
}

void CalcCeosSARImageFilePosition(CeosSARVolume_t *volume, int channel,
                                  int line, int *record, int *file_offset)
{
    struct CeosSARImageDesc *ImageDesc;
    int TotalRecords = 0, TotalBytes = 0;

    if (record)
        *record = 0;
    if (file_offset)
        *file_offset = 0;

    if (volume)
    {
        if (volume->ImageDesc.ImageDescValid)
        {
            ImageDesc = &(volume->ImageDesc);

            switch (ImageDesc->ChannelInterleaving)
            {
                case CEOS_IL_PIXEL:
                    TotalRecords = (line - 1) * ImageDesc->RecordsPerLine;
                    TotalBytes = (TotalRecords) * (ImageDesc->BytesPerRecord);
                    break;
                case CEOS_IL_LINE:
                    TotalRecords =
                        (ImageDesc->NumChannels * (line - 1) + (channel - 1)) *
                        ImageDesc->RecordsPerLine;
                    TotalBytes = (TotalRecords) * (ImageDesc->BytesPerRecord);
                    break;
                case CEOS_IL_BAND:
                    TotalRecords = (((channel - 1) * ImageDesc->Lines) *
                                    ImageDesc->RecordsPerLine) +
                                   (line - 1) * ImageDesc->RecordsPerLine;
                    TotalBytes = (TotalRecords) * (ImageDesc->BytesPerRecord);
                    break;
            }
            if (file_offset)
                *file_offset = ImageDesc->FileDescriptorLength + TotalBytes;
            if (record)
                *record = TotalRecords + 1;
        }
    }
}

int32 GetCeosSARImageData(CPL_UNUSED CeosSARVolume_t *volume,
                          CPL_UNUSED CeosRecord_t *processed_data_record,
                          CPL_UNUSED int channel, CPL_UNUSED int xoff,
                          CPL_UNUSED int xsize, CPL_UNUSED int bufsize,
                          CPL_UNUSED uchar *buffer)
{
    return 0;
}

void DetermineCeosSARPixelOrder(CPL_UNUSED CeosSARVolume_t *volume,
                                CPL_UNUSED CeosRecord_t *record)
{
}

void GetCeosSAREmbeddedInfo(CPL_UNUSED CeosSARVolume_t *volume,
                            CPL_UNUSED CeosRecord_t *processed_data_record,
                            CPL_UNUSED CeosSAREmbeddedInfo_t *info)
{
}

void DeleteCeosSARVolume(CeosSARVolume_t *volume)
{
    Link_t *Links;

    if (volume)
    {
        if (volume->RecordList)
        {
            for (Links = volume->RecordList; Links != NULL; Links = Links->next)
            {
                if (Links->object)
                {
                    DeleteCeosRecord(Links->object);
                    Links->object = NULL;
                }
            }
            DestroyList(volume->RecordList);
        }
        HFree(volume);
    }
}
