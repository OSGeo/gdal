/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Class for representing a whole feature, and layer schemas.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  1999/05/31 17:14:53  warmerda
 * New
 *
 */

#ifndef _OGR_FEATURE_H_INCLUDED
#define _OGR_FEATURE_H_INLLUDED

#include "ogr_geometry.h"

enum OGRFieldType
{
    OFTInteger = 0,
    OFTIntegerList = 1,
    OFTReal = 2,
    OFTRealList = 3,
    OFTString = 4,
    OFTStringList = 5,
    OFTWideString = 6,
    OFTWideStringList = 7,
    OFTBinary = 8,
};

enum OGRJustification
{
    OJUndefined = 0,
    OJLeft = 1,
    OJRight = 2
};

/************************************************************************/
/*                             OGRFieldDefn                             */
/************************************************************************/

/**
 * Definition of an attribute of a OGRFeatureDefn.
 */

class OGRFieldDefn
{
  private:
    OGRFieldType	eType;			
    OGRJustification	eJustify;		
    int			nWidth;			/* zero is variable */
    int			nPrecision;
    
  public:
    
};

/************************************************************************/
/*                            OGRFeatureDefn                            */
/************************************************************************/

/**
 * Definition of a feature class or feature table.
 */

class OGRFeatureDefn
{
  private:
    int		nFields;
    OGRFieldDefn *pasFieldDefn;
    
  public:
    
};

/************************************************************************/
/*                              OGRFeature                              */
/************************************************************************/

/**
 * A simple feature, including geometry and attributes.
 */

class OGRFeature
{
  private:
    
    OGRSchema	*poSchema;
    
  public:
};

#endif /* ndef _OGR_FEATURE_H_INCLUDED */

