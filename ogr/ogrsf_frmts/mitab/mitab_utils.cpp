/**********************************************************************
 * $Id: mitab_utils.cpp,v 1.7 1999/12/16 06:10:24 daniel Exp $
 *
 * Name:     mitab_utils.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Misc. util. functions for the library
 * Author:   Daniel Morissette, danmo@videotron.ca
 *
 **********************************************************************
 * Copyright (c) 1999, Daniel Morissette
 *
 * All rights reserved.  This software may be copied or reproduced, in
 * all or in part, without the prior written consent of its author,
 * Daniel Morissette (danmo@videotron.ca).  However, any material copied
 * or reproduced must bear the original copyright notice (above), this 
 * original paragraph, and the original disclaimer (below).
 * 
 * The entire risk as to the results and performance of the software,
 * supporting text and other information contained in this file
 * (collectively called the "Software") is with the user.  Although 
 * considerable efforts have been used in preparing the Software, the 
 * author does not warrant the accuracy or completeness of the Software.
 * In no event will the author be liable for damages, including loss of
 * profits or consequential damages, arising out of the use of the 
 * Software.
 * 
 **********************************************************************
 *
 * $Log: mitab_utils.cpp,v $
 * Revision 1.7  1999/12/16 06:10:24  daniel
 * TABGetBasename(): make sure last '/' of path is removed
 *
 * Revision 1.6  1999/12/14 02:08:37  daniel
 * Added TABGetBasename() + TAB_CSLLoad()
 *
 * Revision 1.5  1999/11/08 04:30:59  stephane
 * Modify TABGenerateArc()
 *
 * Revision 1.4  1999/09/29 17:59:21  daniel
 * Definition for PI was gone on Windows
 *
 * Revision 1.3  1999/09/16 02:39:17  daniel
 * Completed read support for most feature types
 *
 * Revision 1.2  1999/07/12 05:44:59  daniel
 * Added include math.h for VC++
 *
 * Revision 1.1  1999/07/12 04:18:25  daniel
 * Initial checkin
 *
 **********************************************************************/

#include "mitab.h"
#include "mitab_utils.h"
#include "cpl_conv.h"

#include <math.h>       /* sin()/cos() */
#include <ctype.h>      /* toupper()/tolower() */

/**********************************************************************
 *                       TABGenerateArc()
 *
 * Generate the coordinates for an arc and ADD the coordinates to the 
 * geometry object.  If the geometry already contains some points then
 * these won't be lost.
 *
 * poLine can be a OGRLineString or one of its derived classes, such as 
 *        OGRLinearRing
 * numPoints is the number of points to generate.
 * Angles are specified in radians, valid values are in the range [0..2*PI]
 *
 * Arcs are always generated counterclockwise, even if StartAngle > EndAngle
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABGenerateArc(OGRLineString *poLine, int numPoints, 
                   double dCenterX, double dCenterY,
                   double dXRadius, double dYRadius,
                   double dStartAngle, double dEndAngle)
{
    double dX, dY, dAngleStep, dAngle;
    int i;

    // Adjust angles to go counterclockwise
    if (dEndAngle < dStartAngle)
        dEndAngle += 2.0*PI;

    dAngleStep = (dEndAngle-dStartAngle)/(numPoints-1.0);

    for(i=0; i<numPoints; i++)
    {
        dAngle = (dStartAngle + (double)i*dAngleStep);
        dX = dCenterX + dXRadius*cos(dAngle);
        dY = dCenterY + dYRadius*sin(dAngle);
        poLine->addPoint(dX, dY);
    }

    // Complete the arc with the last EndAngle, to make sure that 
    // the arc is correcly close.

    dX = dCenterX + dXRadius*cos(dAngle);
    dY = dCenterY + dYRadius*sin(dAngle);
    poLine->addPoint(dX,dY);


    return 0;
}


/**********************************************************************
 *                       TABCloseRing()
 *
 * Check if a ring is closed, and add a point to close it if necessary.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABCloseRing(OGRLineString *poRing)
{
    if ( poRing->getNumPoints() > 0 && !poRing->get_IsClosed() )
    {
        poRing->addPoint(poRing->getX(0), poRing->getY(0));
    }

    return 0;
}


/**********************************************************************
 *                       TABAdjustFilenameExtension()
 *
 * Because Unix filenames are case sensitive and MapInfo datasets often have
 * mixed cases filenames, we use this function to find the right filename
 * to use ot open a specific file.
 *
 * This function works directly on the source string, so the filename it
 * contains at the end of the call is the one that should be used.
 *
 * Returns TRUE if one of the extensions worked, and FALSE otherwise.
 * If none of the extensions worked then the original extension will NOT be
 * restored.
 **********************************************************************/
GBool TABAdjustFilenameExtension(char *pszFname)
{
    FILE        *fp;
    int         i;
    
    /*-----------------------------------------------------------------
     * First try using filename as provided
     *----------------------------------------------------------------*/
    if ((fp=VSIFOpen(pszFname, "r")) != NULL)
    {
        VSIFClose(fp);
        return TRUE;
    }     

    /*-----------------------------------------------------------------
     * Try using uppercase extension (we assume that fname contains a '.')
     *----------------------------------------------------------------*/
    for(i = strlen(pszFname)-1; i >= 0 && pszFname[i] != '.'; i--)
    {
        pszFname[i] = toupper(pszFname[i]);
    }

    if ((fp=VSIFOpen(pszFname, "r")) != NULL)
    {
        VSIFClose(fp);
        return TRUE;
    }     
    
    /*-----------------------------------------------------------------
     * Try using lowercase extension
     *----------------------------------------------------------------*/
    for(i = strlen(pszFname)-1; i >= 0 && pszFname[i] != '.'; i--)
    {
        pszFname[i] = tolower(pszFname[i]);
    }

    if ((fp=VSIFOpen(pszFname, "r")) != NULL)
    {
        VSIFClose(fp);
        return TRUE;
    }     

    /*-----------------------------------------------------------------
     * None of the extensions worked!
     *----------------------------------------------------------------*/

    return FALSE;
}



/**********************************************************************
 *                       TABGetBasename()
 *
 * Extract the basename part of a complete file path.
 *
 * Returns a newly allocated string without the leading path (dirs) and
 * the extenstion.  The returned string should be freed using CPLFree().
 **********************************************************************/
char *TABGetBasename(const char *pszFname)
{
    const char *pszTmp = NULL;

    /*-----------------------------------------------------------------
     * Skip leading path
     *----------------------------------------------------------------*/
    if ((pszTmp = strrchr(pszFname, '/')) != NULL ||
        (pszTmp = strrchr(pszFname, '\\')) != NULL)
    {
        pszTmp++; // Skip the last '/' of the path as well.
    }
    else
    {
        pszTmp = pszFname;  // No path to skip
    }

    /*-----------------------------------------------------------------
     * Now allocate our own copy and remove extension
     *----------------------------------------------------------------*/
    char *pszBasename = CPLStrdup(pszTmp);
    int i;
    for(i=0; pszBasename[i] != '\0'; i++)
    {
        if (pszBasename[i] == '.')
        {
            pszBasename[i] = '\0';
            break;
        }
    }

    return pszBasename;
}



/**********************************************************************
 *                       TAB_CSLLoad()
 *
 * Same as CSLLoad(), but does not produce an error if it fails... it
 * just returns NULL silently instead.
 *
 * Load a test file into a stringlist.
 *
 * Lines are limited in length by the size fo the CPLReadLine() buffer.
 **********************************************************************/
char **TAB_CSLLoad(const char *pszFname)
{
    FILE        *fp;
    const char  *pszLine;
    char        **papszStrList=NULL;

    fp = VSIFOpen(pszFname, "rt");

    if (fp)
    {
        while(!VSIFEof(fp))
        {
            if ( (pszLine = CPLReadLine(fp)) != NULL )
            {
                papszStrList = CSLAddString(papszStrList, pszLine);
            }
        }

        VSIFClose(fp);
    }

    return papszStrList;
}
