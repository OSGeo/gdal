/******************************************************************************
 *
 * Project:  GTM Driver
 * Purpose:  Class for reading, parsing and handling a gtmfile.
 * Author:   Leonardo de Paula Rosa Piga; http://lampiao.lsc.ic.unicamp.br/~piga
 *
 ******************************************************************************
 * Copyright (c) 2009, Leonardo de Paula Rosa Piga
 * Copyright (c) 2009-2010, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gtm.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*        Methods for dealing with write on files and buffers           */
/************************************************************************/
void appendDouble(void* pBuffer, double val)
{
    CPL_LSBPTR64(&val);
    memcpy(pBuffer, &val, 8);
}

void appendFloat(void* pBuffer, float val)
{
    CPL_LSBPTR32(&val);
    memcpy(pBuffer, &val, 4);
}

void appendInt(void* pBuffer, int val)
{
    CPL_LSBPTR32(&val);
    memcpy(pBuffer, &val, 4);
}

void appendUChar(void* pBuffer, unsigned char val)
{
    memcpy(pBuffer, &val, 1);
}

void appendUShort(void* pBuffer, unsigned short val)
{
    CPL_LSBPTR16(&val);
    memcpy(pBuffer, &val, 2);
}

void writeUChar(VSILFILE* fp, unsigned char val)
{
    VSIFWriteL(&val, 1, 1, fp);
}

void writeDouble(VSILFILE* fp, double val)
{
    CPL_LSBPTR64(&val);
    VSIFWriteL(&val, 1, 8, fp);
}

static double readDouble(VSILFILE* fp)
{
    double val;
    VSIFReadL( &val, 1, 8, fp );
    CPL_LSBPTR64(&val);
    return val;
}

static float readFloat(VSILFILE* fp)
{
    float val;
    VSIFReadL( &val, 1, 4, fp );
    CPL_LSBPTR32(&val);
    return val;
}

static int readInt(VSILFILE* fp)
{
    int val;
    VSIFReadL( &val, 1, 4, fp );
    CPL_LSBPTR32(&val);
    return val;
}

static unsigned char readUChar(VSILFILE* fp)
{
    unsigned char val;
    VSIFReadL( &val, 1, 1, fp );
    return val;
}

static unsigned short readUShort( VSILFILE* fp, bool *pbSuccess = NULL )
{
    unsigned short val;
    if (VSIFReadL( &val, 1, 2, fp ) != 2)
    {
        if (pbSuccess) *pbSuccess = false;
        return 0;
    }
    if (pbSuccess) *pbSuccess = true;
    CPL_LSBPTR16(&val);
    return val;
}

void writeFloat(VSILFILE* fp, float val)
{
    CPL_LSBPTR32(&val);
    VSIFWriteL(&val, 1, 4, fp);
}

void writeInt(VSILFILE* fp, int val)
{
    CPL_LSBPTR32(&val);
    VSIFWriteL(&val, 1, 4, fp);
}

void writeUShort(VSILFILE* fp, unsigned short val)
{
    CPL_LSBPTR16(&val);
    VSIFWriteL(&val, 1, 2, fp);
}

/************************************************************************/
/*             Implementation of Waypoint Function Members              */
/************************************************************************/
Waypoint::Waypoint( double latitudeIn,
                    double longitudeIn,
                    double altitudeIn,
                    const char* nameIn,
                    const char* commentIn,
                    int iconIn,
                    GIntBig wptdateIn ) :
    latitude(latitudeIn),
    longitude(longitudeIn),
    altitude(altitudeIn),
    name(CPLStrdup(nameIn)),
    comment(CPLStrdup(commentIn)),
    icon(iconIn),
    wptdate(wptdateIn)
{}

Waypoint::~Waypoint()
{
    CPLFree(name);
    CPLFree(comment);
}

double Waypoint::getLatitude() const
{
    return latitude;
}

double Waypoint::getLongitude() const
{
    return longitude;
}

double Waypoint::getAltitude() const
{
    return altitude;
}

const char* Waypoint::getName() const
{
    return name;
}

const char* Waypoint::getComment() const
{
    return comment;
}

int Waypoint::getIcon() const
{
    return icon;
}

GIntBig Waypoint::getDate() const
{
    return wptdate;
}

/************************************************************************/
/*               Implementation of Track Function Members               */
/************************************************************************/
Track::Track( const char* pszNameIn,
              unsigned char typeIn,
              int colorIn) :
    pszName(CPLStrdup(pszNameIn)),
    type(typeIn),
    color(colorIn),
    nPoints(0),
    pasTrackPoints(NULL)
{}

Track::~Track()
{
    CPLFree(pszName);
    pszName = NULL;
    CPLFree(pasTrackPoints);
}

const char* Track::getName() const
{
    return pszName;
}

unsigned char Track::getType() const
{
    return type;
}

int Track::getColor() const
{
    return color;
}

void Track::addPoint(double x, double y, GIntBig datetime, double altitude)
{
    pasTrackPoints = static_cast<TrackPoint*>(
        CPLRealloc(pasTrackPoints, (nPoints + 1) * sizeof(TrackPoint)) );
    pasTrackPoints[nPoints].x = x;
    pasTrackPoints[nPoints].y = y;
    pasTrackPoints[nPoints].datetime = datetime;
    pasTrackPoints[nPoints].altitude = altitude;
    nPoints ++;
}

int Track::getNumPoints() const
{
    return nPoints;
}

const TrackPoint* Track::getPoint(int pointNum) const
{
    if (pointNum >=0 && pointNum < nPoints)
        return &pasTrackPoints[pointNum];

    return NULL;
}

/************************************************************************/
/*                Implementation of GTM Function Members                */
/************************************************************************/
GTM::GTM() :
    pGTMFile(NULL),
    pszFilename(NULL),
    nwptstyles(0),
    nwpts(0),
    ntcks(0),
    n_tk(0),
    n_maps(0),
    headerSize(0),
    firstWaypointOffset(0),
    actualWaypointOffset(0),
    waypointFetched(0),
    firstTrackpointOffset(0),
    actualTrackpointOffset(0),
    trackpointFetched(0),
    firstTrackOffset(0),
    actualTrackOffset(0),
    trackFetched(0)
{}

GTM::~GTM()
{
    CPLFree(pszFilename);
    if (pGTMFile != NULL)
    {
        VSIFCloseL(pGTMFile);
        pGTMFile = NULL;
    }
}

bool GTM::Open(const char* pszFilenameIn)
{

    if (pGTMFile != NULL)
        VSIFCloseL(pGTMFile);

    CPLFree(pszFilename);
    pszFilename = CPLStrdup(pszFilenameIn);

    pGTMFile = VSIFOpenL( pszFilename, "r" );
    if (pGTMFile == NULL)
    {
        return false;
    }
    return true;
}

bool GTM::isValid()
{
/* -------------------------------------------------------------------- */
/*      If we aren't sure it is GTM, load a header chunk and check      */
/*      for signs it is GTM                                             */
/* -------------------------------------------------------------------- */
    //  2 bytes - version number
    // 10 bytes - "TrackMaker" string
    char buffer[13];

    const size_t nRead = VSIFReadL( buffer, 1, sizeof(buffer)-1, pGTMFile );
    if (nRead == 0)
    {
        VSIFCloseL( pGTMFile );
        pGTMFile = NULL;
        return false;
    }
    buffer[12] = '\0';

/* -------------------------------------------------------------------- */
/*      If it looks like a GZip header, this may be a .gtz file, so     */
/*      try opening with the /vsigzip/ prefix                           */
/* -------------------------------------------------------------------- */
    if (buffer[0] == 0x1f && ((unsigned char*)buffer)[1] == 0x8b &&
        !STARTS_WITH(pszFilename, "/vsigzip/"))
    {
        const size_t nLen = strlen("/vsigzip/") + strlen(pszFilename) + 1;
        char* pszGZIPFileName = static_cast<char *>(CPLMalloc(nLen));
        snprintf(pszGZIPFileName, nLen, "/vsigzip/%s", pszFilename);
        VSILFILE* fp = VSIFOpenL(pszGZIPFileName, "rb");
        if (fp)
        {
            VSILFILE* pGTMFileOri = pGTMFile;
            pGTMFile = fp;
            char* pszFilenameOri = pszFilename;
            pszFilename = pszGZIPFileName;
            const bool bRet = isValid();
            pszFilename = pszFilenameOri;
            if (bRet)
            {
                VSIFCloseL(pGTMFileOri);
                CPLFree(pszGZIPFileName);
                return true;
            }
            else
            {
                if (pGTMFile)
                    VSIFCloseL(pGTMFile);
                pGTMFile = pGTMFileOri;
            }
        }
        CPLFree(pszGZIPFileName);
    }

    const short version = CPL_LSBSINT16PTR(buffer);
    /*Skip string length */
    const char* szHeader = buffer + 2;
    if (version == 211 && strcmp(szHeader, "TrackMaker") == 0 )
    {
        return true;
    }
    return false;
}

bool GTM::readHeaderNumbers()
{
    if (pGTMFile == NULL)
        return false;

    /* I'm supposing that the user has already checked if the file is
       valid.  */
    /* Also, I'm ignoring some header parameters that are unnecessary
       for my purpose. If you want more features, implement it. :-P */

    /* Read Number of Waypoint Styles*/
    /* Seek file */
    if (VSIFSeekL(pGTMFile, NWPTSTYLES_OFFSET, SEEK_SET) != 0)
        return false;
    /* Read nwptstyles */
    nwptstyles = readInt(pGTMFile);
    if (nwptstyles < 0)
        return false;

    /* Read Number of Waypoints */
    /* Seek file */
    if ( VSIFSeekL(pGTMFile, NWPTS_OFFSET, SEEK_SET) != 0)
        return false;
    /* Read nwpts */
    nwpts = readInt(pGTMFile);
    if (nwpts < 0)
        return false;

    /* Read Number of Trackpoints */
    ntcks = readInt(pGTMFile);
    if (ntcks < 0)
        return false;

    /* Read Number of images */
    /* Seek file */
    if ( VSIFSeekL(pGTMFile, NMAPS_OFFSET, SEEK_SET) != 0)
        return false;
    /* read n_maps */
    n_maps = readInt(pGTMFile);
    if (n_maps < 0)
        return false;

    /* Read Number of Tracks */
    n_tk = readInt(pGTMFile);
    if (n_tk < 0)
        return false;

    /* Figure out the header size */
    headerSize = 99; // Constant size plus size of strings

    /* Read gradfont string size */
    if ( VSIFSeekL(pGTMFile, 99, SEEK_SET) != 0)
        return false;
    unsigned short stringSize = readUShort(pGTMFile);
    headerSize += stringSize + 2; // String + size field

    /* Read labelfont string size */
    if ( VSIFSeekL(pGTMFile, stringSize, SEEK_CUR) != 0)
        return false;
    stringSize = readUShort(pGTMFile);
    headerSize += stringSize + 2; // String + size field

    /* Read userfont string size */
    if ( VSIFSeekL(pGTMFile, stringSize, SEEK_CUR) != 0)
        return false;
    stringSize = readUShort(pGTMFile);
    headerSize += stringSize + 2; // String + size field

    /* Read newdatum string size */
    if ( VSIFSeekL(pGTMFile, stringSize, SEEK_CUR) != 0)
        return false;
    stringSize = readUShort(pGTMFile);
    headerSize += stringSize + 2; // String + size field

/* -------------------------------------------------------------------- */
/*                 Checks if it is using WGS84 datum                    */
/* -------------------------------------------------------------------- */
    /* Read newdatum string size */
    if ( VSIFSeekL(pGTMFile, headerSize + 34, SEEK_SET) != 0)
        return false;
    if (readInt(pGTMFile) != 217)
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "You are attempting to open a file that is not using "
                  "WGS84 datum.\n"
                  "Coordinates will be returned as if they were WGS84, "
                  "but no reprojection will be done." );
    }

    /* Look for the offsets */
    /* Waypoints */
    firstWaypointOffset = findFirstWaypointOffset();
    if (firstWaypointOffset == 0)
        return false;
    actualWaypointOffset = firstWaypointOffset;
    /* Trackpoints */
    firstTrackpointOffset = findFirstTrackpointOffset();
    if (firstTrackpointOffset == 0)
        return false;
    actualTrackpointOffset = firstTrackpointOffset;

    /* Tracks */
    firstTrackOffset = findFirstTrackOffset();
    if (firstTrackOffset == 0)
        return false;
    actualTrackOffset = firstTrackOffset;

    return true;
}

/************************************************************************/
/*                        Waypoint control functions                    */
/************************************************************************/
int GTM::getNWpts() const
{
    return nwpts;
}

bool GTM::hasNextWaypoint() const
{
    return waypointFetched < nwpts;
}

void GTM::rewindWaypoint()
{
    actualWaypointOffset = firstWaypointOffset;
    waypointFetched = 0;
}

Waypoint* GTM::fetchNextWaypoint()
{
    /* Point to the actual waypoint offset */
    if ( VSIFSeekL(pGTMFile, actualWaypointOffset, SEEK_SET) != 0)
        return NULL;

    const double latitude = readDouble(pGTMFile);
    const double longitude = readDouble(pGTMFile);

    char name[11];
    if ( !readFile( name, 1, 10 ) )
        return NULL;

    /* Trim string name */
    {
        int i = 9;  // Used after for.
        for( ; i >= 0; --i)
        {
            if (name[i] != ' ')
            {
                name[i+1] = '\0';
                break;
            }
        }
        if (i < 0)
            name[0] = '\0';
    }

    /* Read String Length */
    const unsigned short stringSize = readUShort(pGTMFile);
    /* Read Comment String */
    char* comment = static_cast<char *>(
        VSI_MALLOC2_VERBOSE(sizeof(char), stringSize+1) );
    if( comment == NULL )
        return NULL;
    if ( stringSize != 0 && !readFile( comment, 1, sizeof(char)*stringSize ) )
    {
        CPLFree(comment);
        return NULL;
    }
    comment[stringSize] = '\0';

    /* Read Icon */
    const unsigned short icon = readUShort(pGTMFile);

    /* Display number */
    readUChar(pGTMFile);

    /* Waypoint date */

    GIntBig wptdate = readInt(pGTMFile);
    if (wptdate != 0)
        wptdate += GTM_EPOCH;

    /* Rotation text angle */
    readUShort(pGTMFile);

    /* Altitude */
    const float altitude = readFloat(pGTMFile);

    Waypoint* poWaypoint = new Waypoint(latitude, longitude, altitude,
                                        name, comment, (int) icon, wptdate);

    /* Set actual waypoint offset to the next it there is one */
    ++waypointFetched;
    if (waypointFetched < nwpts)
    {
        actualWaypointOffset +=
            8 + 8 + 10 + 2 + stringSize + 2 + 1 + 4 + 2 + 4 + 2;
    }

    CPLFree(comment);
    return poWaypoint;
}

/************************************************************************/
/*                        Track control functions                    */
/************************************************************************/
int GTM::getNTracks() const
{
    return n_tk;
}

bool GTM::hasNextTrack() const
{
    return trackFetched < n_tk;
}

void GTM::rewindTrack()
{
    actualTrackpointOffset = firstTrackpointOffset;
    actualTrackOffset = firstTrackOffset;
    trackFetched = 0;
    trackpointFetched = 0;
}

Track* GTM::fetchNextTrack()
{
    /* Point to the actual track offset */
    if ( VSIFSeekL(pGTMFile, actualTrackOffset, SEEK_SET) != 0)
        return NULL;

    /* Read string length */
    const unsigned short stringSize = readUShort(pGTMFile);
    /* Read name string */
    char* pszName = (char*) VSI_MALLOC2_VERBOSE(sizeof(char), stringSize+1);
    if( pszName == NULL )
        return NULL;
    if ( stringSize != 0 && !readFile( pszName, 1, sizeof(char) * stringSize ) )
    {
        CPLFree(pszName);
        return NULL;
    }
    pszName[stringSize] = '\0';

    /* Read type */
    const unsigned char type = readUChar(pGTMFile);

    /* Read color */
    const int color = readInt(pGTMFile);

    Track* poTrack = new Track(pszName, type, color);

    CPLFree(pszName);
    /* Adjust actual Track offset */
    actualTrackOffset = VSIFTellL(pGTMFile) + 7;
    ++trackFetched;

    /* Now, We read all trackpoints for this track */
    double latitude = 0.0;
    double longitude = 0.0;
    GIntBig datetime = 0;
    unsigned char start = 0;
    float altitude = 0.0f;
    /* NOTE: Parameters are passed by reference */
    if ( !readTrackPoints(latitude, longitude, datetime, start, altitude) )
    {
        delete poTrack;
        return NULL;
    }

    /* Check if it is the start, if not we have done something wrong */
    if (start != 1)
    {
        delete poTrack;
        return NULL;
    }
    poTrack->addPoint(longitude, latitude, datetime, altitude);

    do
    {
        /* NOTE: Parameters are passed by reference */
        if ( !readTrackPoints(latitude, longitude, datetime, start, altitude) )
        {
            delete poTrack;
            return NULL;
        }
        if (start == 0)
            poTrack->addPoint(longitude, latitude, datetime, altitude);
    } while(start == 0 && trackpointFetched < ntcks);

    /* We have read one more than necessary, so we adjust the offset */
    if (trackpointFetched < ntcks)
    {
        actualTrackpointOffset -= 25;
        --trackpointFetched;
    }

    return poTrack;
}

/************************************************************************/
/*                        Private Methods Implementation                */
/************************************************************************/
vsi_l_offset GTM::findFirstWaypointOffset()
{
    /* Skip header and datum */
    if ( VSIFSeekL(pGTMFile, headerSize + DATUM_SIZE, SEEK_SET) != 0)
        return 0;

    /* Skip images */
    for (int i = 0; i < n_maps; ++i)
    {
        /* Read image name string size */
        unsigned short stringSize = readUShort(pGTMFile);

        /* skip image name string */
        if ( VSIFSeekL(pGTMFile, stringSize, SEEK_CUR) != 0)
            return 0;

        /* read image comment string size */
        stringSize = readUShort(pGTMFile);

        /* skip image comment string */
        if ( VSIFSeekL(pGTMFile, stringSize, SEEK_CUR) != 0)
            return 0;

        /* skip the others image parameters */
        if ( VSIFSeekL(pGTMFile, 30, SEEK_CUR) != 0)
            return 0;
    }
    return VSIFTellL(pGTMFile);
}

vsi_l_offset GTM::findFirstTrackpointOffset()
{
    if (firstWaypointOffset == 0)
    {
        firstWaypointOffset = findFirstWaypointOffset();
        if (firstWaypointOffset == 0)
            return 0;
    }

    /*---------------------------------------------*/
    /*       We are going to skip the waypoints    */
    /*---------------------------------------------*/
    /* Seek file to the first Waypoint */
    if (VSIFSeekL(pGTMFile, firstWaypointOffset, SEEK_SET) != 0)
        return 0;

    /* Skip waypoints */
    for (int i = 0; i < nwpts; ++i)
    {
        /* Seek file to the string size comment field */
        if (VSIFSeekL(pGTMFile, 26, SEEK_CUR) != 0)
            return 0;
        /* Read string comment size */
        bool bSuccess = false;
        const unsigned short stringSize = readUShort(pGTMFile, &bSuccess);

        /* Skip to the next Waypoint */
        if( !bSuccess || VSIFSeekL(pGTMFile, stringSize + 15, SEEK_CUR) != 0 )
            return 0;
    }

    /* Skip waypoint styles */
    /* If we don't have waypoints, we don't have waypoint styles, even
       though the nwptstyles is telling the contrary. */
    if (nwpts != 0)
    {
        for (int i = 0; i < nwptstyles; ++i)
        {
            /* Seek file to the string size facename field */
            if (VSIFSeekL(pGTMFile, 4, SEEK_CUR) != 0)
                return 0;

            /* Read string facename size */
            bool bSuccess = false;
            const unsigned short stringSize = readUShort(pGTMFile, &bSuccess);

            /* Skip to the next Waypoint Style*/
            if( !bSuccess ||
                VSIFSeekL(pGTMFile, stringSize + 24, SEEK_CUR) != 0 )
                return 0;
        }
    }
    /* We've found the first track. Return the offset*/
    return VSIFTellL(pGTMFile);
}

vsi_l_offset GTM::findFirstTrackOffset()
{
    if (firstTrackpointOffset == 0)
    {
        firstTrackpointOffset = findFirstTrackpointOffset();
        if (firstTrackpointOffset == 0)
            return 0;
    }
    /* First track offset is the first trackpoint offset plus number of
       trackpoints time size of a trackpoint*/
    return firstTrackpointOffset + static_cast<vsi_l_offset>(ntcks) * 25;
}

bool GTM::readTrackPoints( double& latitude, double& longitude,
                           GIntBig& datetime,
                           unsigned char& start, float& altitude)
{
    /* Point to the actual trackpoint offset */
    if ( VSIFSeekL(pGTMFile, actualTrackpointOffset, SEEK_SET) != 0)
        return false;

    /* Read latitude */
    latitude = readDouble(pGTMFile);

    /* Read longitude */
    longitude = readDouble(pGTMFile);

    /* Read trackpoint date */
    datetime = readInt(pGTMFile);
    if (datetime != 0)
        datetime += GTM_EPOCH;

    /* Read start flag */
    if ( !readFile( &start, 1, 1 ) )
        return false;

    /* Read altitude */
    altitude = readFloat(pGTMFile);

    ++trackpointFetched;
    if (trackpointFetched < ntcks)
    {
        actualTrackpointOffset += 8 + 8 + 4 + 1 + 4;
    }
    return true;
}

bool GTM::readFile(void* pBuffer, size_t nSize, size_t nCount)
{
    const size_t nRead = VSIFReadL( pBuffer, nSize, nCount, pGTMFile );
    if (nRead == 0)
    {
        VSIFCloseL( pGTMFile );
        pGTMFile = NULL;
        return false;
    }
    return true;
}
