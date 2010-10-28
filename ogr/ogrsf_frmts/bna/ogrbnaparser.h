/******************************************************************************
 * $Id$
 *
 * Project:  BNA Parser header
 * Purpose:  Definition of structures, enums and functions of BNA parser
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2007, Even Rouault
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


#ifndef _OGR_BNA_PARSER_INCLUDED
#define _OGR_BNA_PARSER_INCLUDED

#include "cpl_vsi.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
  BNA_POINT,
  BNA_POLYGON,
  BNA_POLYLINE,
  BNA_ELLIPSE,
/* The following values are not geometries, but flags for BNA_GetNextRecord */
  BNA_READ_ALL,
  BNA_READ_NONE,
} BNAFeatureType;

/* Standard BNA files should have 2 ids */
/* But some BNA files have a third ID, for example those produced by Atlas GIS(TM) 4.0... */
/* And BNA files found at ftp://ftp.ciesin.org/pub/census/usa/tiger/ have up to 4 ! */
#define NB_MIN_BNA_IDS    2
#define NB_MAX_BNA_IDS    4

typedef struct
{
  char*          ids[NB_MAX_BNA_IDS];
  int            nIDs;
  BNAFeatureType featureType;
  int            nCoords;
  double        (*tabCoords)[2];
} BNARecord;

/** Get the next BNA record in the file
   @param f open BNA files (VSI Large API handle)
   @param ok (out) set to TRUE if reading was OK (or EOF detected)
   @param curLine (in/out) incremenet number line
   @param verbose if TRUE, errors will be reported
   @param interestFeatureType if BNA_READ_ALL, any BNA feature will be parsed and read in details.
                              if BNA_READ_NONE, no BNA feature will be parsed and read in details.
                              Otherwise, if the read feature type doesn't match with the interestFeatureType,
                              the record will be parsed as fast as possible (tabCoords not allocated)
   @return the parsed BNA record or NULL if EOF or error
*/
BNARecord* BNA_GetNextRecord(VSILFILE* f, int* ok, int* curLine, int verbose, BNAFeatureType interestFeatureType);

/** Free a record obtained by BNA_GetNextRecord */
void BNA_FreeRecord(BNARecord* record);

const char* BNA_FeatureTypeToStr(BNAFeatureType featureType);

/** For debug purpose */
void BNA_Display(BNARecord* record);

#ifdef __cplusplus
}
#endif

#endif
