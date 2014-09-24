/******************************************************************************
 * $Id$
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
    CPL_LSBPTR32(&val)
    memcpy(pBuffer, &val, 4);
}

void appendInt(void* pBuffer, int val)
{
    CPL_LSBPTR32(&val)
    memcpy(pBuffer, &val, 4);
}

void appendUChar(void* pBuffer, unsigned char val)
{
    memcpy(pBuffer, &val, 1);
}

void appendUShort(void* pBuffer, unsigned short val)
{
    CPL_LSBPTR16(&val)
    memcpy(pBuffer, &val, 2);
}

void writeUChar(VSILFILE* fp, unsigned char val)
{
    VSIFWriteL(&val, 1, 1, fp);
}

void writeDouble(VSILFILE* fp, double val)
{
    CPL_LSBPTR64(&val)
    VSIFWriteL(&val, 1, 8, fp);
}

static double readDouble(VSILFILE* fp)
{
    double val;
    VSIFReadL( &val, 1, 8, fp );
    CPL_LSBPTR64(&val)
    return val;
}

static float readFloat(VSILFILE* fp)
{
    float val;
    VSIFReadL( &val, 1, 4, fp );
    CPL_LSBPTR32(&val)
    return val;
}

static int readInt(VSILFILE* fp)
{
    int val;
    VSIFReadL( &val, 1, 4, fp );
    CPL_LSBPTR32(&val)
    return val;
}

static unsigned char readUChar(VSILFILE* fp)
{
    unsigned char val;
    VSIFReadL( &val, 1, 1, fp );
    return val;
}

static unsigned short readUShort(VSILFILE* fp, int *pbSuccess = NULL)
{
    unsigned short val;
    if (VSIFReadL( &val, 1, 2, fp ) != 2)
    {
        if (pbSuccess) *pbSuccess = FALSE;
        return 0;
    }
    if (pbSuccess) *pbSuccess = TRUE;
    CPL_LSBPTR16(&val)
    return val;
}

void writeFloat(VSILFILE* fp, float val)
{
    CPL_LSBPTR32(&val)
    VSIFWriteL(&val, 1, 4, fp);
}

void writeInt(VSILFILE* fp, int val)
{
    CPL_LSBPTR32(&val)
    VSIFWriteL(&val, 1, 4, fp);
}

void writeUShort(VSILFILE* fp, unsigned short val)
{
    CPL_LSBPTR16(&val)
    VSIFWriteL(&val, 1, 2, fp);
}


/************************************************************************/
/*             Implementation of Waypoint Function Members              */
/************************************************************************/
Waypoint::Waypoint(double latitude,
                   double longitude,
                   double altitude,
                   const char* name,
                   const char* comment,
                   int icon,
                   GIntBig wptdate)
{
    this->latitude = latitude;
    this->longitude = longitude;
    this->altitude = altitude;
    this->name = CPLStrdup(name);
    this->comment = CPLStrdup(comment);
    this->icon = icon;
    this->wptdate = wptdate;
}

Waypoint::~Waypoint()
{
    CPLFree(name);
    CPLFree(comment);
}

double Waypoint::getLatitude() 
{
    return latitude;
}

double Waypoint::getLongitude()
{
    return longitude;
}

double Waypoint::getAltitude()
{
    return altitude;
}

const char* Waypoint::getName()
{
    return name;
}

const char* Waypoint::getComment()
{
    return comment;
}

int Waypoint::getIcon()
{
    return icon;
}

GIntBig Waypoint::getDate()
{
    return wptdate;
}

/************************************************************************/
/*               Implementation of Track Function Members               */
/************************************************************************/
Track::Track(const char* pszName,
             unsigned char type,
             int color)
{
    this->pszName = CPLStrdup(pszName);
    this->type = type;
    this->color = color;
    nPoints = 0;
    pasTrackPoints = NULL;
}

Track::~Track()
{
    CPLFree(pszName);
    pszName = NULL;
    CPLFree(pasTrackPoints);
}

const char* Track::getName() {
    return pszName;
}

unsigned char Track::getType()
{
    return type;
}


int Track::getColor()
{
    return color;
}

void Track::addPoint(double x, double y, GIntBig datetime, double altitude)
{
    pasTrackPoints = (TrackPoint*)
        CPLRealloc(pasTrackPoints, (nPoints + 1) * sizeof(TrackPoint));
    pasTrackPoints[nPoints].x = x;
    pasTrackPoints[nPoints].y = y;
    pasTrackPoints[nPoints].datetime = datetime;
    pasTrackPoints[nPoints].altitude = altitude;
    nPoints ++;
}

int Track::getNumPoints()
{
    return nPoints;
}

const TrackPoint* Track::getPoint(int pointNum)
{
    if (pointNum >=0 && pointNum < nPoints)
        return &pasTrackPoints[pointNum];
    else
        return NULL;
}


/************************************************************************/
/*                Implementation of GTM Function Members                */
/************************************************************************/
GTM::GTM()
{
    pGTMFile = NULL;
    pszFilename = NULL;

    nwptstyles = 0;
    nwpts = 0;
    ntcks = 0;
    n_tk = 0;
    n_maps = 0;
    headerSize = 0;

    firstWaypointOffset = 0;
    actualWaypointOffset = 0;
    waypointFetched = 0;
  
    firstTrackpointOffset = 0;
    actualTrackpointOffset = 0;
    trackpointFetched = 0;

    firstTrackOffset = 0;
    actualTrackOffset = 0;
    trackFetched = 0;
}

GTM::~GTM()
{
    CPLFree(pszFilename);
    if (pGTMFile != NULL)
    {
        VSIFCloseL(pGTMFile);
        pGTMFile = NULL;
    }
}

bool GTM::Open(const char* pszFilename)
{

    if (pGTMFile != NULL)
        VSIFCloseL(pGTMFile);
        
    CPLFree(this->pszFilename);
    this->pszFilename = CPLStrdup(pszFilename);

    pGTMFile = VSIFOpenL( pszFilename, "r" );
    if (pGTMFile == NULL)
    {
        return FALSE;
    }
    return TRUE;
}


bool GTM::isValid()
{
    //  2 bytes - version number
    // 10 bytes - "TrackMaker" string
    char buffer[13];

    char* szHeader;
    short version;

/* -------------------------------------------------------------------- */
/*      If we aren't sure it is GTM, load a header chunk and check      */
/*      for signs it is GTM                                             */
/* -------------------------------------------------------------------- */
    size_t nRead = VSIFReadL( buffer, 1, sizeof(buffer)-1, pGTMFile );
    if (nRead <= 0)
    {
        VSIFCloseL( pGTMFile );
        pGTMFile = NULL;
        return FALSE;
    }
    buffer[12] = '\0';
    
/* -------------------------------------------------------------------- */
/*      If it looks like a GZip header, this may be a .gtz file, so     */
/*      try opening with the /vsigzip/ prefix                           */
/* -------------------------------------------------------------------- */
    if (buffer[0] == 0x1f && ((unsigned char*)buffer)[1] == 0x8b &&
        strncmp(pszFilename, "/vsigzip/", strlen("/vsigzip/")) != 0)
    {
        char* pszGZIPFileName = (char*)CPLMalloc(
                           strlen("/vsigzip/") + strlen(pszFilename) + 1);
        sprintf(pszGZIPFileName, "/vsigzip/%s", pszFilename);
        VSILFILE* fp = VSIFOpenL(pszGZIPFileName, "rb");
        if (fp)
        {
            VSILFILE* pGTMFileOri = pGTMFile;
            pGTMFile = fp;
            if (isValid())
            {
                VSIFCloseL(pGTMFileOri);
                CPLFree(pszGZIPFileName);
                return TRUE;
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
    
    version = CPL_LSBINT16PTR(buffer);
    /*Skip string length */
    szHeader = buffer + 2;
    if (version == 211 && strcmp(szHeader, "TrackMaker") == 0 )
    {
        return TRUE;
    }
    return FALSE;
}

bool GTM::readHeaderNumbers()
{
    if (pGTMFile == NULL)
        return FALSE;

   
    /* I'm supposing that the user has already checked if the file is
       valid.  */
    /* Also, I'm ignoring some header parameters that are unnecessary
       for my purpose. If you want more features, implement it. :-P */

    /* Read Number of Waypoint Styles*/
    /* Seek file */
    if (VSIFSeekL(pGTMFile, NWPTSTYLES_OFFSET, SEEK_SET) != 0)
        return FALSE;
    /* Read nwptstyles */
    nwptstyles = readInt(pGTMFile);
    if (nwptstyles < 0)
        return FALSE;

    /* Read Number of Waypoints */
    /* Seek file */
    if ( VSIFSeekL(pGTMFile, NWPTS_OFFSET, SEEK_SET) != 0)
        return FALSE;
    /* Read nwpts */
    nwpts = readInt(pGTMFile);
    if (nwpts < 0)
        return FALSE;

    /* Read Number of Trackpoints */
    ntcks = readInt(pGTMFile);
    if (ntcks < 0)
        return FALSE;

    /* Read Number of images */
    /* Seek file */
    if ( VSIFSeekL(pGTMFile, NMAPS_OFFSET, SEEK_SET) != 0)
        return FALSE;
    /* read n_maps */
    n_maps = readInt(pGTMFile);
    if (n_maps < 0)
        return FALSE;

    /* Read Number of Tracks */
    n_tk = readInt(pGTMFile);
    if (n_tk < 0)
        return FALSE;

    /* Figure out the header size */
    headerSize = 99; // Constant size plus size of strings
    unsigned short stringSize;

    /* Read gradfont string size */
    if ( VSIFSeekL(pGTMFile, 99, SEEK_SET) != 0)
        return FALSE;
    stringSize = readUShort(pGTMFile);
    headerSize += stringSize + 2; // String + size field

    /* Read labelfont string size */
    if ( VSIFSeekL(pGTMFile, stringSize, SEEK_CUR) != 0)
        return FALSE;
    stringSize = readUShort(pGTMFile);
    headerSize += stringSize + 2; // String + size field


    /* Read userfont string size */
    if ( VSIFSeekL(pGTMFile, stringSize, SEEK_CUR) != 0)
        return FALSE;
    stringSize = readUShort(pGTMFile);
    headerSize += stringSize + 2; // String + size field

    /* Read newdatum string size */
    if ( VSIFSeekL(pGTMFile, stringSize, SEEK_CUR) != 0)
        return FALSE;
    stringSize = readUShort(pGTMFile);
    headerSize += stringSize + 2; // String + size field



/* -------------------------------------------------------------------- */
/*                 Checks if it is using WGS84 datum                    */
/* -------------------------------------------------------------------- */
    /* Read newdatum string size */
    if ( VSIFSeekL(pGTMFile, headerSize + 34, SEEK_SET) != 0)
        return FALSE;
    if (readInt(pGTMFile) != 217)
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "You are attempting to open a file that is not using WGS84 datum.\n"
                  "Coordinates will be returned as if they were WGS84, but no reprojection will be done.");
    }

    /* Look for the offsets */
    /* Waypoints */
    firstWaypointOffset = findFirstWaypointOffset();
    if (firstWaypointOffset == 0)
        return FALSE;
    actualWaypointOffset = firstWaypointOffset;
    /* Trackpoints */
    firstTrackpointOffset = findFirstTrackpointOffset();
    if (firstTrackpointOffset == 0)
        return FALSE;
    actualTrackpointOffset = firstTrackpointOffset;

    /* Tracks */
    firstTrackOffset = findFirstTrackOffset();
    if (firstTrackOffset == 0)
        return FALSE;
    actualTrackOffset = firstTrackOffset;

    return TRUE;
}

/************************************************************************/
/*                        Waypoint control functions                    */
/************************************************************************/
int GTM::getNWpts()
{
    return nwpts;
}

bool GTM::hasNextWaypoint()
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
    unsigned short stringSize;

    double latitude, longitude;
    char name[11];
    char* comment;
    unsigned short icon;
    int i;
    float altitude;
    GIntBig wptdate;

    /* Point to the actual waypoint offset */
    if ( VSIFSeekL(pGTMFile, actualWaypointOffset, SEEK_SET) != 0)
        return NULL;

    latitude = readDouble(pGTMFile);
    longitude = readDouble(pGTMFile);

    if ( !readFile( name, 1, 10 ) )
        return NULL;
    /* Trim string name */
    for (i = 9; i >= 0; --i)
    {
        if (name[i] != ' ')
        {
            name[i+1] = '\0';
            break;
        }
    }
    if (i < 0)
        name[0] = '\0';

    /* Read String Length */
    stringSize = readUShort(pGTMFile);
    /* Read Comment String */
    comment = (char*) VSIMalloc2(sizeof(char), stringSize+1);
    if ( stringSize != 0 && !readFile( comment, 1, sizeof(char)*stringSize ) )
    {
        CPLFree(comment);
        return NULL;
    }
    comment[stringSize] = '\0';

    /* Read Icon */
    icon = readUShort(pGTMFile);
    
    /* Display number */
    readUChar(pGTMFile);
    
    /* Waypoint date */
    
    wptdate = readInt(pGTMFile);
    if (wptdate != 0)
        wptdate += GTM_EPOCH;
    
    /* Rotation text angle */
    readUShort(pGTMFile);
    
    /* Altitude */
    altitude = readFloat(pGTMFile);
  
    Waypoint* poWaypoint = new Waypoint(latitude, longitude, altitude,
                                        name, comment, (int) icon, wptdate);


    /* Set actual waypoint offset to the next it there is one */
    ++waypointFetched;
    if (waypointFetched < nwpts)
    {
        actualWaypointOffset += 8 + 8 + 10 + 2 + stringSize + 2 + 1 + 4 + 2 + 4 + 2;
    }

    CPLFree(comment);
    return poWaypoint;
}


/************************************************************************/
/*                        Track control functions                    */
/************************************************************************/
int GTM::getNTracks()
{
    return n_tk;
}

bool GTM::hasNextTrack()
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
    unsigned short stringSize;

    char* pszName;
    unsigned char type;
    int color;
  

    /* Point to the actual track offset */
    if ( VSIFSeekL(pGTMFile, actualTrackOffset, SEEK_SET) != 0)
        return NULL;


    /* Read string length */
    stringSize = readUShort(pGTMFile);
    /* Read name string */
    pszName = (char*) VSIMalloc2(sizeof(char), stringSize+1);
    if ( stringSize != 0 && !readFile( pszName, 1, sizeof(char) * stringSize ) )
    {
        CPLFree(pszName);
        return NULL;
    }
    pszName[stringSize] = '\0';

    /* Read type */
    type = readUChar(pGTMFile);
  
    /* Read color */
    color = readInt(pGTMFile);
    
    Track* poTrack = new Track(pszName, type, color);

    CPLFree(pszName);
    /* Adjust actual Track offset */
    actualTrackOffset = VSIFTellL(pGTMFile) + 7;
    ++trackFetched;

    /* Now, We read all trackpoints for this track */
    double latitude, longitude;
    GIntBig datetime;
    unsigned char start;
    float altitude;
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
    unsigned short stringSize;
    for (int i = 0; i < n_maps; ++i)
    {
        /* Read image name string size */
        stringSize = readUShort(pGTMFile);

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
  
    unsigned short stringSize;
    int bSuccess;
    /* Skip waypoints */
    for (int i = 0; i < nwpts; ++i)
    {
        /* Seek file to the string size comment field */
        if (VSIFSeekL(pGTMFile, 26, SEEK_CUR) != 0)
            return 0;
        /* Read string comment size */
        stringSize = readUShort(pGTMFile, &bSuccess);
    
        /* Skip to the next Waypoint */
        if (bSuccess == FALSE || VSIFSeekL(pGTMFile, stringSize + 15, SEEK_CUR) != 0)
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
            stringSize = readUShort(pGTMFile, &bSuccess);

            /* Skip to the next Waypoint Style*/
            if (bSuccess == FALSE || VSIFSeekL(pGTMFile, stringSize + 24, SEEK_CUR) != 0)
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
    return (vsi_l_offset) (firstTrackpointOffset + ntcks * 25);
}

bool GTM::readTrackPoints(double& latitude, double& longitude, GIntBig& datetime,
                          unsigned char& start, float& altitude) {
    /* Point to the actual trackpoint offset */
    if ( VSIFSeekL(pGTMFile, actualTrackpointOffset, SEEK_SET) != 0)
        return FALSE;

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
        return FALSE;
        
    /* Read altitude */
    altitude = readFloat(pGTMFile);

    ++trackpointFetched;
    if (trackpointFetched < ntcks)
    {
        actualTrackpointOffset += 8 + 8 + 4 + 1 + 4;
    }
    return TRUE;
}

bool GTM::readFile(void* pBuffer, size_t nSize, size_t nCount)
{
    size_t nRead;
    nRead = VSIFReadL( pBuffer, nSize, nCount, pGTMFile );
    if (nRead <= 0)
    {
        VSIFCloseL( pGTMFile );
        pGTMFile = NULL;
        return FALSE;
    }
    return TRUE;
}



