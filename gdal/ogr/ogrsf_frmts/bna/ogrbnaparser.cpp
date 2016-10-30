/******************************************************************************
 *
 * Project:  BNA Parser
 * Purpose:  Parse a BNA record
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
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

#include "ogrbnaparser.h"

#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

void BNA_FreeRecord(BNARecord* record)
{
  if (record)
  {
    for( int i = 0; i < NB_MAX_BNA_IDS; i++)
    {
        if (record->ids[i]) VSIFree(record->ids[i]);
        record->ids[i] = NULL;
    }
    CPLFree(record->tabCoords);
    record->tabCoords = NULL;
    CPLFree(record);
  }
}

const char* BNA_FeatureTypeToStr(BNAFeatureType featureType)
{
  switch (featureType)
  {
    case BNA_POINT:
      return "point";
    case BNA_POLYGON:
      return "polygon";
    case BNA_POLYLINE:
      return "polyline";
    case BNA_ELLIPSE:
      return "ellipse";
    default:
      return "unknown";
  }
}

void BNA_Display(BNARecord* record)
{
  fprintf(stderr, "\"%s\", \"%s\", \"%s\", %s\n",
          record->ids[0] ? record->ids[0] : "",
          record->ids[1] ? record->ids[1] : "",
          record->ids[2] ? record->ids[2] : "",
          BNA_FeatureTypeToStr(record->featureType));
  for( int i = 0; i < record->nCoords; i++ )
    fprintf( stderr, "%f, %f\n", record->tabCoords[i][0],
             record->tabCoords[i][1] );
}

/*
For a description of the format, see http://www.softwright.com/faq/support/boundary_file_bna_format.html
and http://64.145.236.125/forum/topic.asp?topic_id=1930&forum_id=1&Topic_Title=how+to+edit+*.bna+files%3F&forum_title=Surfer+Support&M=False
*/

/* The following lines are a valid BNA file (for this parser) :
"PID1","SID1",1
-0,0
"PID2","SID2",-2,   -1e-5,-1e2   ,-2,-2
"PID3""a","SID3",4
5,5
6,5
6,6
5,5
"PID4","SID4",2
10,10
5,4

"PID4","SID4",2

10,10
5,4
*/

/* The following lines are a valid BNA file (for this parser) :
  (small extract from ftp://ftp.ciesin.org/pub/census/usa/tiger/ct/bnablk/b09001.zip)
"090010000.00099A","099A","090010000.00099A","blk",21
-73.049337, 41.125000 -73.093761, 41.157780 -73.107900, 41.168300
-73.106878, 41.166459 -73.095800, 41.146500 -73.114553, 41.146331
-73.138922, 41.147993 -73.166200, 41.154200 -73.198497, 41.085988
-73.232241, 41.029986 -73.229548, 41.030507 -73.183922, 41.041955
-73.178678, 41.043239 -73.177951, 41.043417 -73.147888, 41.050781
-73.118658, 41.057942 -73.052399, 41.074174 -73.024976, 41.080892
-73.000000, 41.087010 -73.035597, 41.114420 -73.049337, 41.125000
*/

// We are (and must be) a bit tolerant : BNA files format has several variations
// and most don't follow strictly the 'specification'.
// Extra spaces, tabulations or line feed are accepted and ignored.
// We allow one line format and several line format in the same file.
// We allow from NB_MIN_BNA_IDS to NB_MAX_BNA_IDS ids.
// We allow that couples of coordinates on the same line may be separated only
// by spaces (instead of being separated by a comma).

static const char *const STRING_NOT_TERMINATED
    = "string not terminated when end of line occurred";
static const char *const MISSING_FIELDS            = "missing fields";
static const char *const BAD_INTEGER_NUMBER_FORMAT = "bad integer number format";
static const char *const BAD_FLOAT_NUMBER_FORMAT   = "bad float number format";
static const char *const STRING_EXPECTED           = "string expected";
static const char *const NUMBER_EXPECTED           = "number expected";
static const char *const INTEGER_NUMBER_EXPECTED   = "integer number expected";
static const char *const FLOAT_NUMBER_EXPECTED     = "float number expected";
static const char *const INVALID_GEOMETRY_TYPE     = "invalid geometry type";
static const char *const TOO_LONG_ID               = "too long id (> 256 characters)";
static const char *const MAX_BNA_IDS_REACHED       = "maximum number of IDs reached";
static const char *const NOT_ENOUGH_MEMORY
    = "not enough memory for request number of coordinates";
static const char *const LINE_TOO_LONG             = "line too long";

static const int TMP_BUFFER_SIZE = 256;
static const int LINE_BUFFER_SIZE = 1024;

enum
{
    BNA_LINE_OK,
    BNA_LINE_EOF,
    BNA_LINE_TOO_LONG
};

static int BNA_GetLine(char szLineBuffer[LINE_BUFFER_SIZE+1], VSILFILE* f)
{
    char* ptrCurLine = szLineBuffer;
    int nRead
        = static_cast<int>( VSIFReadL(szLineBuffer, 1, LINE_BUFFER_SIZE, f) );
    szLineBuffer[nRead] = 0;
    if (nRead == 0)
    {
        /* EOF */
        return BNA_LINE_EOF;
    }

    bool bFoundEOL = false;
    while (*ptrCurLine)
    {
        if (*ptrCurLine == 0x0d || *ptrCurLine == 0x0a)
        {
            bFoundEOL = true;
            break;
        }
        ptrCurLine ++;
    }
    if( !bFoundEOL )
    {
        if (nRead < LINE_BUFFER_SIZE)
            return BNA_LINE_OK;

        return BNA_LINE_TOO_LONG;
    }

    if (*ptrCurLine == 0x0d)
    {
        if (ptrCurLine == szLineBuffer + LINE_BUFFER_SIZE - 1)
        {
            char c = '\0';
            nRead = static_cast<int>(VSIFReadL(&c, 1, 1, f));
            if( nRead == 1 )
            {
                if( c == 0x0a )
                {
                    /* Do nothing */
                }
                else
                {
                    if( VSIFSeekL(f, VSIFTellL(f) - 1, SEEK_SET) != 0 )
                        return BNA_LINE_EOF;
                }
            }
        }
        else if (ptrCurLine[1] == 0x0a)
        {
            if( VSIFSeekL( f,
                           VSIFTellL(f) + ptrCurLine + 2
                           - (szLineBuffer + nRead),
                           SEEK_SET ) != 0 )
                return BNA_LINE_EOF;
        }
        else
        {
            if( VSIFSeekL( f,
                           VSIFTellL(f) + ptrCurLine + 1
                           - (szLineBuffer + nRead),
                           SEEK_SET ) != 0 )
                return BNA_LINE_EOF;
        }
    }
    else /* *ptrCurLine == 0x0a */
    {
        if( VSIFSeekL( f,
                       VSIFTellL(f) + ptrCurLine + 1 - (szLineBuffer + nRead),
                       SEEK_SET) != 0 )
            return BNA_LINE_EOF;
    }
    *ptrCurLine = 0;

    return BNA_LINE_OK;
}

BNARecord* BNA_GetNextRecord(VSILFILE* f,
                             int* ok,
                             int* curLine,
                             int verbose,
                             BNAFeatureType interestFeatureType)
{
    bool inQuotes = false;
    int numField = 0;
    char* ptrBeginningOfNumber = NULL;
    bool exponentFound = false;
    bool exponentSignFound = false;
    bool dotFound = false;
    int numChar = 0;
    const char* detailedErrorMsg = NULL;
    BNAFeatureType currentFeatureType = BNA_UNKNOWN;
    int nbExtraId = 0;
    char tmpBuffer[NB_MAX_BNA_IDS][TMP_BUFFER_SIZE+1];
    int  tmpBufferLength[NB_MAX_BNA_IDS] = {0, 0, 0};
    char szLineBuffer[LINE_BUFFER_SIZE + 1];

    BNARecord* record
        = static_cast<BNARecord *>( CPLMalloc(sizeof(BNARecord)) );
    memset(record, 0, sizeof(BNARecord));

    while( true )
    {
      numChar = 0;
      (*curLine)++;

      const int retGetLine = BNA_GetLine(szLineBuffer, f);
      if (retGetLine == BNA_LINE_TOO_LONG)
      {
          detailedErrorMsg = LINE_TOO_LONG;
          goto error;
      }
      else if (retGetLine == BNA_LINE_EOF)
      {
          break;
      }

      char* ptrCurLine = szLineBuffer;

      if (*ptrCurLine == 0)
        continue;

      const char* ptrBeginLine = szLineBuffer;

      while(1)
      {
        numChar = static_cast<int>(ptrCurLine - ptrBeginLine);
        char c = *ptrCurLine;
        if (c == 0) c = 10;
        if( inQuotes )
        {
          if (c == 10)
          {
            detailedErrorMsg = STRING_NOT_TERMINATED;
            goto error;
          }
          else if (c == '"' && ptrCurLine[1] == '"')
          {
            if (tmpBufferLength[numField] == TMP_BUFFER_SIZE)
            {
              detailedErrorMsg = TOO_LONG_ID;
              goto error;
            }
            tmpBuffer[numField][tmpBufferLength[numField]++] = c;

            ptrCurLine++;
          }
          else if (c == '"')
          {
            inQuotes = false;
          }
          else
          {
            if (tmpBufferLength[numField] == TMP_BUFFER_SIZE)
            {
              detailedErrorMsg = TOO_LONG_ID;
              goto error;
            }
            tmpBuffer[numField][tmpBufferLength[numField]++] = c;
          }
        }
        else if (c == ' ' || c == '\t')
        {
          if (numField > NB_MIN_BNA_IDS + nbExtraId && ptrBeginningOfNumber != NULL)
          {
            do
            {
              ptrCurLine++;
              numChar = static_cast<int>(ptrCurLine - ptrBeginLine);
              c = *ptrCurLine;
              if (!(c == ' ' || c == '\t'))
                break;
            } while(c);
            if (c == 0) c = 10;

            if (interestFeatureType == BNA_READ_ALL ||
                interestFeatureType == currentFeatureType)
            {
              char* pszComma = strchr(ptrBeginningOfNumber, ',');
              if (pszComma)
                  *pszComma = '\0';
              record->tabCoords[(numField - nbExtraId - NB_MIN_BNA_IDS - 1) / 2]
                               [1 - ((numField - nbExtraId) % 2)] =
                  CPLAtof(ptrBeginningOfNumber);
              if (pszComma)
                  *pszComma = ',';
            }
            if (numField == NB_MIN_BNA_IDS + 1 + nbExtraId + 2 * record->nCoords - 1)
            {
              if (c != 10)
              {
                if (verbose)
                {
                  CPLError( CE_Warning, CPLE_AppDefined,
                            "At line %d, at char %d, extra data will be "
                            "ignored",
                            *curLine, numChar+1 );
                }
              }
              *ok = 1;
              return record;
            }

            ptrBeginningOfNumber = NULL;
            exponentFound = false;
            exponentSignFound = false;
            dotFound = false;
            numField++;

            if (c == 10)
              break;

            if (c != ',')
            {
              // Do not increment ptrCurLine.
              continue;
            }
          }
          else
          {
            /* ignore */
          }
        }
        else if (c == 10 || c == ',')
        {
          /* Eat a comma placed at end of line */
          if (c == ',')
          {
              const char* ptr = ptrCurLine+1;
              while(*ptr)
              {
                  if (*ptr != ' ' && *ptr != '\t')
                      break;
                  ptr++;
              }
              if (*ptr == 0)
              {
                  c = 10;
              }
          }

          if (numField == 0)
          {
            // Maybe not so mandatory...
            // Atlas MapMaker(TM) exports BNA files with empty primaryID.
            /*
            if (record->primaryID == NULL || *(record->primaryID) == 0)
            {
              detailedErrorMsg = PRIMARY_ID_MANDATORY;
              goto error;
            }
            */
          }
          else if (numField == NB_MIN_BNA_IDS + nbExtraId)
          {
            int nCoords = 0;
            if (ptrBeginningOfNumber == NULL)
            {
              detailedErrorMsg = INTEGER_NUMBER_EXPECTED;
              goto error;
            }
            nCoords = atoi(ptrBeginningOfNumber);
            if (nCoords == 0 || nCoords == -1 || nCoords >= INT_MAX / 16 ||
                nCoords <= INT_MIN / 16 )
            {
              detailedErrorMsg = INVALID_GEOMETRY_TYPE;
              goto error;
            }
            else if (nCoords == 1)
            {
              currentFeatureType = BNA_POINT;
              record->featureType = BNA_POINT;
              record->nCoords = 1;
            }
            else if (nCoords == 2)
            {
              currentFeatureType = BNA_ELLIPSE;
              record->featureType = BNA_ELLIPSE;
              record->nCoords = 2;
            }
            else if (nCoords > 0)
            {
              currentFeatureType = BNA_POLYGON;
              record->featureType = BNA_POLYGON;
              record->nCoords = nCoords;
            }
            else
            {
              currentFeatureType = BNA_POLYLINE;
              record->featureType = BNA_POLYLINE;
              record->nCoords = -nCoords;
            }

            record->nIDs = NB_MIN_BNA_IDS + nbExtraId;

            if (interestFeatureType == BNA_READ_ALL ||
                interestFeatureType == currentFeatureType)
            {
              for( int i=0; i < NB_MAX_BNA_IDS; i++ )
              {
                if (tmpBufferLength[i] && tmpBuffer[i][0])
                {
                  record->ids[i] = static_cast<char *>(
                      CPLMalloc(tmpBufferLength[i] + 1));
                  tmpBuffer[i][tmpBufferLength[i]] = 0;
                  memcpy(record->ids[i], tmpBuffer[i], tmpBufferLength[i] + 1);
                }
              }

              record->tabCoords =
                  (double(*)[2])VSI_MALLOC2_VERBOSE(record->nCoords, 2 * sizeof(double));
              if (record->tabCoords == NULL)
              {
                  detailedErrorMsg = NOT_ENOUGH_MEMORY;
                  goto error;
              }
            }
          }
          else if (numField > NB_MIN_BNA_IDS + nbExtraId)
          {
            if (ptrBeginningOfNumber == NULL)
            {
              detailedErrorMsg = FLOAT_NUMBER_EXPECTED;
              goto error;
            }
            if (interestFeatureType == BNA_READ_ALL ||
                interestFeatureType == currentFeatureType)
            {
              char* pszComma = strchr(ptrBeginningOfNumber, ',');
              if (pszComma)
                  *pszComma = '\0';
              record->tabCoords[(numField - nbExtraId - NB_MIN_BNA_IDS - 1) / 2]
                               [1 - ((numField - nbExtraId) % 2)] =
                  CPLAtof(ptrBeginningOfNumber);
              if (pszComma)
                  *pszComma = ',';
            }
            if (numField ==
                NB_MIN_BNA_IDS + 1 + nbExtraId + 2 * record->nCoords - 1)
            {
              if (c != 10)
              {
                if (verbose)
                {
                  CPLError( CE_Warning, CPLE_AppDefined,
                            "At line %d, at char %d, extra data will be "
                            "ignored",
                            *curLine, numChar+1);
                }
              }
              *ok = 1;
              return record;
            }
          }

          ptrBeginningOfNumber = NULL;
          exponentFound = false;
          exponentSignFound = false;
          dotFound = false;
          numField++;

          if (c == 10)
            break;
        }
        else if (c == '"')
        {
          if (numField < NB_MIN_BNA_IDS)
          {
            inQuotes = true;
          }
          else if ( numField >= NB_MIN_BNA_IDS
                    && currentFeatureType == BNA_UNKNOWN)
          {
            if (ptrBeginningOfNumber == NULL)
            {
              if (nbExtraId == NB_MAX_BNA_IDS - NB_MIN_BNA_IDS)
              {
                detailedErrorMsg = MAX_BNA_IDS_REACHED;
                goto error;
              }
              nbExtraId ++;
              inQuotes = true;
            }
            else
            {
              detailedErrorMsg = BAD_INTEGER_NUMBER_FORMAT;
              goto error;
            }
          }
          else
          {
            detailedErrorMsg = NUMBER_EXPECTED;
            goto error;
          }
        }
        else
        {
          if( numField < NB_MIN_BNA_IDS
              || (numField == NB_MIN_BNA_IDS + nbExtraId - 1) )
          {
            detailedErrorMsg = STRING_EXPECTED;
            goto error;
          }
          else if (numField == NB_MIN_BNA_IDS + nbExtraId)
          {
            if (c >= '0' && c <= '9')
            {
            }
            else if (c == '+' || c == '-')
            {
              if (ptrBeginningOfNumber != NULL)
              {
                detailedErrorMsg = BAD_INTEGER_NUMBER_FORMAT;
                goto error;
              }
            }
            else
            {
              detailedErrorMsg = BAD_INTEGER_NUMBER_FORMAT;
              goto error;
            }
            if (ptrBeginningOfNumber == NULL)
              ptrBeginningOfNumber = ptrCurLine;
          }
          else
          {
            if (c >= '0' && c <= '9')
            {
            }
            else if (c == '.')
            {
              if (dotFound || exponentFound)
              {
                detailedErrorMsg = BAD_FLOAT_NUMBER_FORMAT;
                goto error;
              }
              dotFound = true;
            }
            else if (c == '+' || c == '-')
            {
              if (ptrBeginningOfNumber == NULL)
              {
              }
              else if (exponentFound)
              {
                if (!exponentSignFound && ptrCurLine > ptrBeginLine &&
                    (ptrCurLine[-1] == 'e' || ptrCurLine[-1] == 'E' ||
                     ptrCurLine[-1] == 'd' || ptrCurLine[-1] == 'D'))
                {
                  exponentSignFound = true;
                }
                else
                {
                  detailedErrorMsg = BAD_FLOAT_NUMBER_FORMAT;
                  goto error;
                }
              }
              else
              {
                detailedErrorMsg = BAD_FLOAT_NUMBER_FORMAT;
                goto error;
              }
            }
            else if (c == 'e' || c == 'E' || c == 'd' || c == 'D')
            {
              if (ptrBeginningOfNumber == NULL ||
                  !(ptrCurLine[-1] >= '0' && ptrCurLine[-1] <= '9') ||
                  exponentFound )
              {
                detailedErrorMsg = BAD_FLOAT_NUMBER_FORMAT;
                goto error;
              }
              exponentFound = true;
            }
            else
            {
              detailedErrorMsg = BAD_FLOAT_NUMBER_FORMAT;
              goto error;
            }
            if (ptrBeginningOfNumber == NULL)
              ptrBeginningOfNumber = ptrCurLine;
          }
        }
        ptrCurLine++;
      }
    }

    if (numField == 0)
    {
        /* End of file */
        *ok = 1;
        BNA_FreeRecord(record);
        return NULL;
    }
    else
    {
        detailedErrorMsg = MISSING_FIELDS;
        goto error;
    }
error:
    if (verbose)
    {
      if (detailedErrorMsg)
      {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Parsing failed at line %d, at char %d : %s",
                  *curLine, numChar+1, detailedErrorMsg );
      }
      else
      {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Parsing failed at line %d, at char %d",
                  *curLine, numChar+1 );
      }
    }
    BNA_FreeRecord(record);
    return NULL;
}
