/**********************************************************************
 * $Id: mitab_utils.h,v 1.5 1999/12/14 02:08:16 daniel Exp $
 *
 * Name:     mitab_utils.h
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Header file containing definitions of misc. util functions.
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
 * $Log: mitab_utils.h,v $
 * Revision 1.5  1999/12/14 02:08:16  daniel
 * Added TABGetBasename() + TAB_CSLLoad()
 *
 * Revision 1.4  1999/09/28 13:33:32  daniel
 * Moved definition for PI to mitab.h
 *
 * Revision 1.3  1999/09/26 14:59:38  daniel
 * Implemented write support
 *
 * Revision 1.2  1999/09/16 02:39:17  daniel
 * Completed read support for most feature types
 *
 * Revision 1.1  1999/07/12 04:18:25  daniel
 * Initial checkin
 *
 **********************************************************************/

#ifndef _MITAB_UTILS_H_INCLUDED_
#define _MITAB_UTILS_H_INCLUDED_

#include "ogr_geometry.h"

#define COLOR_R(color) ((color&0xff0000)/0x10000)
#define COLOR_G(color) ((color&0xff00)/0x100)
#define COLOR_B(color) (color&0xff)

/*=====================================================================
                        Function prototypes
 =====================================================================*/

int TABGenerateArc(OGRLineString *poLine, int numPoints, 
                   double dCenterX, double dCenterY,
                   double dXRadius, double dYRadius,
                   double dStartAngle, double dEndAngle);
int TABCloseRing(OGRLineString *poRing);


GBool TABAdjustFilenameExtension(char *pszFname);
char *TABGetBasename(const char *pszFname);
char **TAB_CSLLoad(const char *pszFname);

#endif /* _MITAB_UTILS_H_INCLUDED_ */


