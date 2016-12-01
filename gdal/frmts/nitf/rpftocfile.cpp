/******************************************************************************
 *
 * Project:  RPF A.TOC read Library
 * Purpose:  Module responsible for opening a RPF TOC file, populating RPFToc
 *           structure
 * Author:   Even Rouault, even.rouault at mines-paris.org
 *
 **********************************************************************
 * Copyright (c) 2007-2010, Even Rouault <even dot rouault at mines-paris dot org>
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

/* Portions of code are placed under the following copyright : */
/*
 ******************************************************************************
 * Copyright (C) 1995 Logiciels et Applications Scientifiques (L.A.S.) Inc
 * Permission to use, copy, modify and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies, that
 * both the copyright notice and this permission notice appear in
 * supporting documentation, and that the name of L.A.S. Inc not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. L.A.S. Inc. makes no
 * representations about the suitability of this software for any purpose.
 * It is provided "as is" without express or implied warranty.
 ******************************************************************************
 */

#include "rpftoclib.h"
#include "cpl_vsi.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        RPFTOCTrim()                                    */
/************************************************************************/

static void RPFTOCTrim(char* str)
{
    char* c = str;
    if (str == NULL || *str == 0)
        return;

    while(*c == ' ')
    {
        c++;
    }
    if (c != str)
    {
        memmove(str, c, strlen(c)+1);
    }

    int i = static_cast<int>(strlen(str)) - 1;
    while (i >= 0 && str[i] == ' ')
    {
        str[i] = 0;
        i--;
    }
}

/************************************************************************/
/*                        RPFTOCRead()                                 */
/************************************************************************/

RPFToc* RPFTOCRead(const char* pszFilename, NITFFile* psFile)
{
    int nTRESize;
    const char* pachTRE = NITFFindTRE( psFile->pachTRE, psFile->nTREBytes,
                           "RPFHDR", &nTRESize );
    if (pachTRE == NULL)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid TOC file. Can't find RPFHDR." );
        return NULL;
    }

    if (nTRESize != 48)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "RPFHDR TRE wrong size." );
        return NULL;
    }

    return  RPFTOCReadFromBuffer(pszFilename, psFile->fp, pachTRE);
}

/* This function is directly inspired by function parse_toc coming from ogdi/driver/rpf/utils.c */

RPFToc* RPFTOCReadFromBuffer(const char* pszFilename, VSILFILE* fp, const char* tocHeader)
{
    tocHeader += 1; /* skip endian */
    tocHeader += 2; /* skip header length */
    tocHeader += 12; /* skip file name : this should be A.TOC (padded) */
    tocHeader += 1; /* skip new  */
    tocHeader += 15; /* skip standard_num  */
    tocHeader += 8; /* skip standard_date  */
    tocHeader += 1; /* skip classification  */
    tocHeader += 2; /* skip country  */
    tocHeader += 2; /* skip release  */

    unsigned int locationSectionPhysicalLocation;
    memcpy(&locationSectionPhysicalLocation, tocHeader, sizeof(unsigned int));
    CPL_MSBPTR32(&locationSectionPhysicalLocation);

    if( VSIFSeekL( fp, locationSectionPhysicalLocation, SEEK_SET ) != 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid TOC file. Unable to seek to locationSectionPhysicalLocation at offset %d.",
                   locationSectionPhysicalLocation );
        return NULL;
    }

    int nSections;
    NITFLocation* pasLocations = NITFReadRPFLocationTable(fp, &nSections);

    unsigned int boundaryRectangleSectionSubHeaderPhysIndex = 0;
    unsigned int boundaryRectangleTablePhysIndex = 0;
    unsigned int frameFileIndexSectionSubHeaderPhysIndex = 0;
    unsigned int frameFileIndexSubsectionPhysIndex = 0;

    for( int i = 0; i < nSections; i++ )
    {
        if (pasLocations[i].nLocId == LID_BoundaryRectangleSectionSubheader)
        {
            boundaryRectangleSectionSubHeaderPhysIndex = pasLocations[i].nLocOffset;
        }
        else if (pasLocations[i].nLocId == LID_BoundaryRectangleTable)
        {
            boundaryRectangleTablePhysIndex = pasLocations[i].nLocOffset;
        }
        else if (pasLocations[i].nLocId == LID_FrameFileIndexSectionSubHeader)
        {
            frameFileIndexSectionSubHeaderPhysIndex = pasLocations[i].nLocOffset;
        }
        else if (pasLocations[i].nLocId == LID_FrameFileIndexSubsection)
        {
            frameFileIndexSubsectionPhysIndex = pasLocations[i].nLocOffset;
        }
    }

    CPLFree(pasLocations);

    if (boundaryRectangleSectionSubHeaderPhysIndex == 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid TOC file. Can't find LID_BoundaryRectangleSectionSubheader." );
        return NULL;
    }
    if (boundaryRectangleTablePhysIndex == 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid TOC file. Can't find LID_BoundaryRectangleTable." );
        return NULL;
    }
    if (frameFileIndexSectionSubHeaderPhysIndex == 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid TOC file. Can't find LID_FrameFileIndexSectionSubHeader." );
        return NULL;
    }
    if (frameFileIndexSubsectionPhysIndex == 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid TOC file. Can't find LID_FrameFileIndexSubsection." );
        return NULL;
    }

    if( VSIFSeekL( fp, boundaryRectangleSectionSubHeaderPhysIndex, SEEK_SET ) != 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid TOC file. Unable to seek to boundaryRectangleSectionSubHeaderPhysIndex at offset %d.",
                   boundaryRectangleSectionSubHeaderPhysIndex );
        return NULL;
    }

    unsigned int boundaryRectangleTableOffset;
    bool bOK = VSIFReadL( &boundaryRectangleTableOffset, sizeof(boundaryRectangleTableOffset), 1, fp) == 1;
    CPL_MSBPTR32( &boundaryRectangleTableOffset );

    unsigned short boundaryRectangleCount;
    bOK &= VSIFReadL( &boundaryRectangleCount, sizeof(boundaryRectangleCount), 1, fp) == 1;
    CPL_MSBPTR16( &boundaryRectangleCount );

    if( !bOK || VSIFSeekL( fp, boundaryRectangleTablePhysIndex, SEEK_SET ) != 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid TOC file. Unable to seek to boundaryRectangleTablePhysIndex at offset %d.",
                   boundaryRectangleTablePhysIndex );
        return NULL;
    }

    RPFToc* toc = reinterpret_cast<RPFToc *>( CPLMalloc( sizeof( RPFToc ) ) );
    toc->nEntries = boundaryRectangleCount;
    toc->entries = reinterpret_cast<RPFTocEntry *>(
        CPLMalloc( boundaryRectangleCount * sizeof(RPFTocEntry) ) );
    memset(toc->entries, 0, boundaryRectangleCount * sizeof(RPFTocEntry));

    for( int i = 0; i < toc->nEntries; i++ )
    {
        toc->entries[i].isOverviewOrLegend = 0;

        bOK &= VSIFReadL( toc->entries[i].type, 1, 5, fp) == 5;
        toc->entries[i].type[5] = 0;
        RPFTOCTrim(toc->entries[i].type);

        bOK &= VSIFReadL( toc->entries[i].compression, 1, 5, fp) == 5;
        toc->entries[i].compression[5] = 0;
        RPFTOCTrim(toc->entries[i].compression);

        bOK &= VSIFReadL( toc->entries[i].scale, 1, 12, fp) == 12;
        toc->entries[i].scale[12] = 0;
        RPFTOCTrim(toc->entries[i].scale);
        if (toc->entries[i].scale[0] == '1' &&
            toc->entries[i].scale[1] == ':')
        {
            memmove(toc->entries[i].scale,
                    toc->entries[i].scale+2,
                    strlen(toc->entries[i].scale+2)+1);
        }

        bOK &= VSIFReadL( toc->entries[i].zone, 1, 1, fp) == 1;
        toc->entries[i].zone[1] = 0;
        RPFTOCTrim(toc->entries[i].zone);

        bOK &= VSIFReadL( toc->entries[i].producer, 1, 5, fp) == 5;
        toc->entries[i].producer[5] = 0;
        RPFTOCTrim(toc->entries[i].producer);

        bOK &= VSIFReadL( &toc->entries[i].nwLat, sizeof(double), 1, fp) == 1;
        CPL_MSBPTR64( &toc->entries[i].nwLat);

        bOK &= VSIFReadL( &toc->entries[i].nwLong, sizeof(double), 1, fp) == 1;
        CPL_MSBPTR64( &toc->entries[i].nwLong);

        bOK &= VSIFReadL( &toc->entries[i].swLat, sizeof(double), 1, fp) == 1;
        CPL_MSBPTR64( &toc->entries[i].swLat);

        bOK &= VSIFReadL( &toc->entries[i].swLong, sizeof(double), 1, fp) == 1;
        CPL_MSBPTR64( &toc->entries[i].swLong);

        bOK &= VSIFReadL( &toc->entries[i].neLat, sizeof(double), 1, fp) == 1;
        CPL_MSBPTR64( &toc->entries[i].neLat);

        bOK &= VSIFReadL( &toc->entries[i].neLong, sizeof(double), 1, fp) == 1;
        CPL_MSBPTR64( &toc->entries[i].neLong);

        bOK &= VSIFReadL( &toc->entries[i].seLat, sizeof(double), 1, fp) == 1;
        CPL_MSBPTR64( &toc->entries[i].seLat);

        bOK &= VSIFReadL( &toc->entries[i].seLong, sizeof(double), 1, fp) == 1;
        CPL_MSBPTR64( &toc->entries[i].seLong);

        bOK &= VSIFReadL( &toc->entries[i].vertResolution, sizeof(double), 1, fp) == 1;
        CPL_MSBPTR64( &toc->entries[i].vertResolution);

        bOK &= VSIFReadL( &toc->entries[i].horizResolution, sizeof(double), 1, fp) == 1;
        CPL_MSBPTR64( &toc->entries[i].horizResolution);

        bOK &= VSIFReadL( &toc->entries[i].vertInterval, sizeof(double), 1, fp) == 1;
        CPL_MSBPTR64( &toc->entries[i].vertInterval);

        bOK &= VSIFReadL( &toc->entries[i].horizInterval, sizeof(double), 1, fp) == 1;
        CPL_MSBPTR64( &toc->entries[i].horizInterval);

        bOK &= VSIFReadL( &toc->entries[i].nVertFrames, sizeof(int), 1, fp) == 1;
        CPL_MSBPTR32( &toc->entries[i].nVertFrames );

        bOK &= VSIFReadL( &toc->entries[i].nHorizFrames, sizeof(int), 1, fp) == 1;
        CPL_MSBPTR32( &toc->entries[i].nHorizFrames );

        if( !bOK )
        {
            CPLError(CE_Failure, CPLE_FileIO, "I/O error");
            toc->entries[i].nVertFrames = 0;
            toc->entries[i].nHorizFrames = 0;
            RPFTOCFree(toc);
            return NULL;
        }

        if( toc->entries[i].nHorizFrames == 0 ||
            toc->entries[i].nVertFrames == 0 ||
            toc->entries[i].nHorizFrames > INT_MAX / toc->entries[i].nVertFrames )
        {
            toc->entries[i].frameEntries = NULL;
        }
        else
        {
            toc->entries[i].frameEntries = reinterpret_cast<RPFTocFrameEntry*>(
                VSI_CALLOC_VERBOSE( toc->entries[i].nVertFrames * toc->entries[i].nHorizFrames,
                                    sizeof(RPFTocFrameEntry) ) );
        }
        if (toc->entries[i].frameEntries == NULL)
        {
            toc->entries[i].nVertFrames = 0;
            toc->entries[i].nHorizFrames = 0;
            RPFTOCFree(toc);
            return NULL;
        }

        CPLDebug("RPFTOC", "[%d] type=%s, compression=%s, scale=%s, zone=%s, producer=%s, nVertFrames=%d, nHorizFrames=%d",
                 i, toc->entries[i].type, toc->entries[i].compression, toc->entries[i].scale,
                 toc->entries[i].zone, toc->entries[i].producer, toc->entries[i].nVertFrames, toc->entries[i].nHorizFrames);
    }

    if( VSIFSeekL( fp, frameFileIndexSectionSubHeaderPhysIndex, SEEK_SET ) != 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid TOC file. Unable to seek to frameFileIndexSectionSubHeaderPhysIndex at offset %d.",
                   frameFileIndexSectionSubHeaderPhysIndex );
        RPFTOCFree(toc);
        return NULL;
    }

    /* Skip 1 byte security classification */
    bOK &= VSIFSeekL( fp, 1, SEEK_CUR ) == 0;

    unsigned int frameIndexTableOffset;
    bOK &= VSIFReadL( &frameIndexTableOffset, sizeof(frameIndexTableOffset), 1, fp) == 1;
    CPL_MSBPTR32( &frameIndexTableOffset );

    unsigned int nFrameFileIndexRecords;
    bOK &= VSIFReadL( &nFrameFileIndexRecords, sizeof(nFrameFileIndexRecords), 1, fp) == 1;
    CPL_MSBPTR32( &nFrameFileIndexRecords );

    unsigned short nFrameFilePathnameRecords;
    bOK &= VSIFReadL( &nFrameFilePathnameRecords, sizeof(nFrameFilePathnameRecords), 1, fp) == 1;
    CPL_MSBPTR16( &nFrameFilePathnameRecords );

    unsigned short frameFileIndexRecordLength;
    bOK &= VSIFReadL( &frameFileIndexRecordLength, sizeof(frameFileIndexRecordLength), 1, fp) == 1;
    CPL_MSBPTR16( &frameFileIndexRecordLength );

    if( !bOK )
    {
        CPLError(CE_Failure, CPLE_FileIO, "I/O error");
        RPFTOCFree(toc);
        return NULL;
    }

    int newBoundaryId = 0;

    for( int i = 0; i < static_cast<int>( nFrameFileIndexRecords ); i++ )
    {
        if( VSIFSeekL( fp, frameFileIndexSubsectionPhysIndex + frameFileIndexRecordLength * i, SEEK_SET ) != 0)
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                    "Invalid TOC file. Unable to seek to frameFileIndexSubsectionPhysIndex(%d) at offset %d.",
                     i, frameFileIndexSubsectionPhysIndex + frameFileIndexRecordLength * i);
            RPFTOCFree(toc);
            return NULL;
        }

        unsigned short boundaryId;
        if( VSIFReadL( &boundaryId, sizeof(boundaryId), 1, fp) != 1 )
        {
            CPLError(CE_Failure, CPLE_FileIO, "I/O error");
            RPFTOCFree(toc);
            return NULL;
        }
        CPL_MSBPTR16( &boundaryId );

        if (i == 0 && boundaryId == 0)
            newBoundaryId = 1;
        if (newBoundaryId == 0)
            boundaryId--;

        if (boundaryId >= toc->nEntries)
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                    "Invalid TOC file. Bad boundary id (%d) for frame file index %d.",
                     boundaryId, i);
            RPFTOCFree(toc);
            return NULL;
        }

        RPFTocEntry* entry = &toc->entries[boundaryId];
        entry->boundaryId = boundaryId;

        unsigned short frameRow;
        bOK &= VSIFReadL( &frameRow, sizeof(frameRow), 1, fp) == 1;
        CPL_MSBPTR16( &frameRow );

        unsigned short frameCol;
        bOK &= VSIFReadL( &frameCol, sizeof(frameCol), 1, fp) == 1;
        CPL_MSBPTR16( &frameCol );
        if( !bOK )
        {
            CPLError(CE_Failure, CPLE_FileIO, "I/O error");
            RPFTOCFree(toc);
            return NULL;
        }

        if (newBoundaryId == 0)
        {
            frameRow--;
            frameCol--;
        }
        else
        {
            /* Trick so that frames are numbered north to south */
            frameRow = (unsigned short)((entry->nVertFrames-1) - frameRow);
        }

        if (frameRow >= entry->nVertFrames)
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                        "Invalid TOC file. Bad row num (%d) for frame file index %d.",
                        frameRow, i);
            RPFTOCFree(toc);
            return NULL;
        }

        if (frameCol >= entry->nHorizFrames)
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                        "Invalid TOC file. Bad col num (%d) for frame file index %d.",
                        frameCol, i);
            RPFTOCFree(toc);
            return NULL;
        }

        RPFTocFrameEntry* frameEntry
            = &entry->frameEntries[frameRow * entry->nHorizFrames + frameCol ];
        frameEntry->frameRow = frameRow;
        frameEntry->frameCol = frameCol;

        if (frameEntry->exists)
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Frame entry(%d,%d) for frame file index %d was already found.",
                      frameRow, frameCol, i);
            CPLFree(frameEntry->directory);
            frameEntry->directory = NULL;
            CPLFree(frameEntry->fullFilePath);
            frameEntry->fullFilePath = NULL;
            frameEntry->exists = 0;
        }

        unsigned int offsetFrameFilePathName;
        bOK &= VSIFReadL( &offsetFrameFilePathName, sizeof(offsetFrameFilePathName), 1, fp) == 1;
        CPL_MSBPTR32( &offsetFrameFilePathName );

        bOK &= VSIFReadL( frameEntry->filename, 1, 12, fp) == 12;
        if( !bOK )
        {
            CPLError(CE_Failure, CPLE_FileIO, "I/O error");
            RPFTOCFree(toc);
            return NULL;
        }
        frameEntry->filename[12] = '\0';

        /* Check if the filename is an overview or legend */
        for( int j = 0; j < 12; j++ )
        {
            if (strcmp(&(frameEntry->filename[j]),".OVR") == 0 ||
                strcmp(&(frameEntry->filename[j]),".ovr") == 0 ||
                strcmp(&(frameEntry->filename[j]),".LGD") == 0 ||
                strcmp(&(frameEntry->filename[j]),".lgd") == 0)
            {
                entry->isOverviewOrLegend = TRUE;
                break;
            }
        }

        /* Extract series code */
        if (entry->seriesAbbreviation == NULL)
        {
            const NITFSeries* series = NITFGetSeriesInfo(frameEntry->filename);
            if (series)
            {
                entry->seriesAbbreviation = series->abbreviation;
                entry->seriesName = series->name;
            }
        }

        /* Get file geo reference */
        bOK &= VSIFReadL( frameEntry->georef, 1, 6, fp) == 6;
        frameEntry->georef[6] = '\0';

        /* Go to start of pathname record */
        /* New path_off offset from start of frame file index section of TOC?? */
        /* Add pathoffset wrt frame file index table subsection (loc[3]) */
        if( !bOK || VSIFSeekL( fp, frameFileIndexSubsectionPhysIndex + offsetFrameFilePathName, SEEK_SET ) != 0)
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "Invalid TOC file. Unable to seek to "
                      "frameFileIndexSubsectionPhysIndex + "
                      "offsetFrameFilePathName(%d) at offset %d.",
                     i, frameFileIndexSubsectionPhysIndex + offsetFrameFilePathName);
            RPFTOCFree(toc);
            return NULL;
        }

        unsigned short pathLength;
        bOK &= VSIFReadL( &pathLength, sizeof(pathLength), 1, fp) == 1;
        CPL_MSBPTR16( &pathLength );

        /* if nFrameFileIndexRecords == 65535 and pathLength == 65535 for each record,
           this leads to 4 GB allocation... Protect against this case */
        if (!bOK || pathLength > 256)
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "Path length is big : %d. Probably corrupted TOC file.",
                      static_cast<int>( pathLength ) );
            RPFTOCFree(toc);
            return NULL;
        }

        frameEntry->directory = reinterpret_cast<char *>( CPLMalloc(pathLength+1) );
        bOK &= VSIFReadL( frameEntry->directory, 1, pathLength, fp) == pathLength;
        if( !bOK )
        {
            CPLError(CE_Failure, CPLE_FileIO, "I/O error");
            RPFTOCFree(toc);
            return NULL;
        }
        frameEntry->directory[pathLength] = 0;
        if (pathLength > 0 && frameEntry->directory[pathLength-1] == '/')
            frameEntry->directory[pathLength-1] = 0;

        if (frameEntry->directory[0] == '.' && frameEntry->directory[1] == '/')
        {
            memmove(frameEntry->directory, frameEntry->directory+2, strlen(frameEntry->directory+2)+1);

            // Some A.TOC have subdirectory names like ".//X/" ... (#5979)
            // Check if it was not intended to be "./X/" instead.
            VSIStatBufL sStatBuf;
            if( frameEntry->directory[0] == '/' &&
                VSIStatL(CPLFormFilename(CPLGetDirname(pszFilename), frameEntry->directory+1, NULL), &sStatBuf) == 0 &&
                VSI_ISDIR(sStatBuf.st_mode) )
            {
                memmove(frameEntry->directory, frameEntry->directory+1, strlen(frameEntry->directory+1)+1);
            }
        }

        {
            char* baseDir = CPLStrdup(CPLGetDirname(pszFilename));
            VSIStatBufL sStatBuf;
            char* subdir = NULL;
            if (CPLIsFilenameRelative(frameEntry->directory) == FALSE)
                subdir = CPLStrdup(frameEntry->directory);
            else if (frameEntry->directory[0] == '.' && frameEntry->directory[1] == 0)
                subdir = CPLStrdup(baseDir);
            else
                subdir = CPLStrdup(CPLFormFilename(baseDir, frameEntry->directory, NULL));
#if !defined(_WIN32) && !defined(_WIN32_CE)
            if( VSIStatL( subdir, &sStatBuf ) != 0 && strlen(subdir) > strlen(baseDir) && subdir[strlen(baseDir)] != 0)
            {
                char* c = subdir + strlen(baseDir)+1;
                while(*c)
                {
                    if (*c >= 'A' && *c <= 'Z')
                        *c += 'a' - 'A';
                    c++;
                }
            }
#endif
            frameEntry->fullFilePath = CPLStrdup(CPLFormFilename(
                    subdir,
                    frameEntry->filename, NULL));
            if( VSIStatL( frameEntry->fullFilePath, &sStatBuf ) != 0 )
            {
#if !defined(_WIN32) && !defined(_WIN32_CE)
                char* c = frameEntry->fullFilePath + strlen(subdir)+1;
                while(*c)
                {
                    if (*c >= 'A' && *c <= 'Z')
                        *c += 'a' - 'A';
                    c++;
                }
                if( VSIStatL( frameEntry->fullFilePath, &sStatBuf ) != 0 )
#endif
                {
                    frameEntry->fileExists = 0;
                    CPLError( CE_Warning, CPLE_AppDefined,
                        "File %s does not exist.", frameEntry->fullFilePath );
                }
#if !defined(_WIN32) && !defined(_WIN32_CE)
                else
                {
                    frameEntry->fileExists = 1;
                }
#endif
            }
            else
            {
                frameEntry->fileExists = 1;
            }
            CPLFree(subdir);
            CPLFree(baseDir);
        }

        CPLDebug("RPFTOC", "Entry %d : %s,%s (%d, %d)", boundaryId, frameEntry->directory, frameEntry->filename, frameRow, frameCol);

        frameEntry->exists = 1;
    }

    return toc;
}

/************************************************************************/
/*                        RPFTOCFree()                                 */
/************************************************************************/

void RPFTOCFree(RPFToc*  toc)
{
    if (!toc)
        return;

    for( int i = 0; i < toc->nEntries; i++ )
    {
        for( int j = 0;
             j < static_cast<int>(toc->entries[i].nVertFrames
                       * toc->entries[i].nHorizFrames);
             j++)
        {
            CPLFree(toc->entries[i].frameEntries[j].fullFilePath);
            CPLFree(toc->entries[i].frameEntries[j].directory);
        }
        CPLFree(toc->entries[i].frameEntries);
    }

    CPLFree(toc->entries);
    CPLFree(toc);
}
