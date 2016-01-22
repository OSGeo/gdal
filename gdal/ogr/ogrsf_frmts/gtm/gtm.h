/******************************************************************************
 * $Id$
 *
 * Project:  GTM Driver
 * Purpose:  Class for reading, parsing and handling a gtm file.
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
#ifndef OGR_GTM_GTM_H_INCLUDED
#define OGR_GTM_GTM_H_INCLUDED

#include <iostream>
#include <vector>
#include <string>

#include <time.h>

#include "ogrsf_frmts.h"
#include "cpl_conv.h"
#include "cpl_string.h"


#ifndef FILE_OFFSETS
#define FILE_OFFSETS
#define NWPTSTYLES_OFFSET 27
#define NWPTSTYLES_SIZE 4

#define NWPTS_OFFSET 35
#define NWPTS_SIZE 4

#define NTRCKS_OFFSET 39
#define NTRCKS_SIZE 4

#define NMAPS_OFFSET 63
#define NMAPS_SIZE 4

#define NTK_OFFSET 67
#define NTK_SIZE 4

#define BOUNDS_OFFSET 47

#define DATUM_SIZE 58

/* GTM_EPOCH is defined as the unix time for the 31 dec 1989 00:00:00 */
#define GTM_EPOCH 631065600
    
#endif


void appendDouble(void* pBuffer, double val);
void appendFloat(void* pBuffer, float val);
void appendInt(void* pBuffer, int val);
void appendUChar(void* pBuffer, unsigned char val);
void appendUShort(void* pBuffer, unsigned short val);

void writeDouble(VSILFILE* fp, double val);
void writeFloat(VSILFILE* fp, float val);
void writeInt(VSILFILE* fp, int val);
void writeUChar(VSILFILE* fp, unsigned char val);
void writeUShort(VSILFILE* fp, unsigned short);


class Waypoint
{
public:
    Waypoint(double latitude,
             double longitude,
             double altitude,
             const char* name,
             const char* comment,
             int icon,
             GIntBig wptdate
             );
    ~Waypoint();
    double getLatitude();
    double getLongitude();
    double getAltitude();
    const char* getName();
    const char* getComment();
    int getIcon();
    GIntBig getDate(); /* 0 if invalid */
private:
    double latitude;
    double longitude;
    double altitude;
    char* name;
    char* comment;
    int icon;
    GIntBig wptdate;
};

typedef struct
{
    double x;
    double y;
    GIntBig datetime;
    double altitude;
} TrackPoint;

class Track
{
public:
    Track(const char* pszName,
          unsigned char type,
          int color);
    ~Track();
  
    const char* getName();
    unsigned char getType();
    int getColor();

    void addPoint(double x, double y, GIntBig datetime, double altitude);  
    int getNumPoints();
    const TrackPoint* getPoint(int pointNum);

private: 
    char* pszName;
    unsigned char type;
    int color;
    int nPoints;
    TrackPoint* pasTrackPoints;

};


class GTM
{
public:
    GTM();
    ~GTM();
    bool Open(const char* pszFilename);


    // Check wheater it is a valid GTM file or not
    bool isValid();
    bool readHeaderNumbers();

    // Waypoint control functions
    Waypoint* fetchNextWaypoint();
    int getNWpts();
    bool hasNextWaypoint();
    void rewindWaypoint();

    int getNTracks();
    bool hasNextTrack();
    void rewindTrack();
    Track* fetchNextTrack();

private:
    // File descriptor
    VSILFILE* pGTMFile;
    char* pszFilename;

    // GTM Header Parameters
    int nwptstyles;
    int nwpts;
    int ntcks;
    int n_tk;
    int n_maps;
    int headerSize;

    // Waypoint controller
    vsi_l_offset firstWaypointOffset;
    vsi_l_offset actualWaypointOffset;
    int waypointFetched;

    // Trackpoint controller
    vsi_l_offset firstTrackpointOffset;
    vsi_l_offset actualTrackpointOffset;
    int trackpointFetched;

    // Track controller
    vsi_l_offset firstTrackOffset;
    vsi_l_offset actualTrackOffset;
    int trackFetched;

    vsi_l_offset findFirstWaypointOffset();
    vsi_l_offset findFirstTrackpointOffset();
    vsi_l_offset findFirstTrackOffset();

    bool readTrackPoints(double& latitude, double& longitude, GIntBig& datetime,
                         unsigned char& start, float& altitude);
    bool readFile(void* pBuffer, size_t nSize, size_t nCount);

};

#endif // OGR_GTM_GTM
