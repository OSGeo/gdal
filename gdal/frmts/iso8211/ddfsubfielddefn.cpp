/******************************************************************************
 *
 * Project:  ISO 8211 Access
 * Purpose:  Implements the DDFSubfieldDefn class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_port.h"
#include "iso8211.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          DDFSubfieldDefn()                           */
/************************************************************************/

DDFSubfieldDefn::DDFSubfieldDefn() :
    pszName(NULL),
    pszFormatString(CPLStrdup("")),
    eType(DDFString),
    eBinaryFormat(NotBinary),
    bIsVariable(TRUE),
    chFormatDelimiter(DDF_UNIT_TERMINATOR),
    nFormatWidth(0),
    nMaxBufChars(0),
    pachBuffer(NULL)
{}

/************************************************************************/
/*                          ~DDFSubfieldDefn()                          */
/************************************************************************/

DDFSubfieldDefn::~DDFSubfieldDefn()

{
    CPLFree( pszName );
    CPLFree( pszFormatString );
    CPLFree( pachBuffer );
}

/************************************************************************/
/*                              SetName()                               */
/************************************************************************/

void DDFSubfieldDefn::SetName( const char * pszNewName )

{
    int         i;

    CPLFree( pszName );

    pszName = CPLStrdup( pszNewName );

    for( i = static_cast<int>(strlen(pszName))-1; i > 0 && pszName[i] == ' '; i-- )
        pszName[i] = '\0';
}

/************************************************************************/
/*                             SetFormat()                              */
/*                                                                      */
/*      While interpreting the format string we don't support:          */
/*                                                                      */
/*       o Passing an explicit terminator for variable length field.    */
/*       o 'X' for unused data ... this should really be filtered       */
/*         out by DDFFieldDefn::ApplyFormats(), but isn't.              */
/*       o 'B' bitstrings that aren't a multiple of eight.              */
/************************************************************************/

int DDFSubfieldDefn::SetFormat( const char * pszFormat )

{
    CPLFree( pszFormatString );
    pszFormatString = CPLStrdup( pszFormat );

/* -------------------------------------------------------------------- */
/*      These values will likely be used.                               */
/* -------------------------------------------------------------------- */
    if( pszFormatString[1] == '(' )
    {
        nFormatWidth = atoi(pszFormatString+2);
        if( nFormatWidth < 0 )
        {
             CPLError( CE_Failure, CPLE_AppDefined,
                       "Format width %s is invalid.",
                       pszFormatString+2 );
            return FALSE;
        }
        bIsVariable = nFormatWidth == 0;
    }
    else
        bIsVariable = TRUE;

/* -------------------------------------------------------------------- */
/*      Interpret the format string.                                    */
/* -------------------------------------------------------------------- */
    switch( pszFormatString[0] )
    {
      case 'A':
      case 'C':         // It isn't clear to me how this is different than 'A'
        eType = DDFString;
        break;

      case 'R':
        eType = DDFFloat;
        break;

      case 'I':
      case 'S':
        eType = DDFInt;
        break;

      case 'B':
      case 'b':
        // Is the width expressed in bits? (is it a bitstring)
        bIsVariable = FALSE;
        if( pszFormatString[1] == '\0' )
            return FALSE;

        if( pszFormatString[1] == '(' )
        {
            nFormatWidth = atoi(pszFormatString+2);
            if( nFormatWidth < 0 || nFormatWidth % 8 != 0 )
            {
                 CPLError( CE_Failure, CPLE_AppDefined,
                           "Format width %s is invalid.",
                           pszFormatString+2 );
                return FALSE;
            }

            nFormatWidth = nFormatWidth / 8;
            eBinaryFormat = SInt; // good default, works for SDTS.

            if( nFormatWidth < 5 )
                eType = DDFInt;
            else
                eType = DDFBinaryString;
        }

        // or do we have a binary type indicator? (is it binary)
        else
        {
            eBinaryFormat = (DDFBinaryFormat) (pszFormatString[1] - '0');
            nFormatWidth = atoi(pszFormatString+2);
            if( nFormatWidth < 0 )
            {
                 CPLError( CE_Failure, CPLE_AppDefined,
                           "Format width %s is invalid.",
                           pszFormatString+2 );
                return FALSE;
            }

            if( eBinaryFormat == SInt || eBinaryFormat == UInt )
                eType = DDFInt;
            else
                eType = DDFFloat;
        }
        break;

      case 'X':
        // 'X' is extra space, and should not be directly assigned to a
        // subfield ... I have not encountered it in use yet though.
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Format type of `%c' not supported.\n",
                  pszFormatString[0] );

        return FALSE;

      default:
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Format type of `%c' not recognised.\n",
                  pszFormatString[0] );

        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/

/**
 * Write out subfield definition info to debugging file.
 *
 * A variety of information about this field definition is written to the
 * give debugging file handle.
 *
 * @param fp The standard IO file handle to write to.  i.e. stderr
 */

void DDFSubfieldDefn::Dump( FILE * fp )

{
    fprintf( fp, "    DDFSubfieldDefn:\n" );
    fprintf( fp, "        Label = `%s'\n", pszName );
    fprintf( fp, "        FormatString = `%s'\n", pszFormatString );
}

/************************************************************************/
/*                           GetDataLength()                            */
/*                                                                      */
/*      This method will scan for the end of a variable field.          */
/************************************************************************/

/**
 * Scan for the end of variable length data.  Given a pointer to the data
 * for this subfield (from within a DDFRecord) this method will return the
 * number of bytes which are data for this subfield.  The number of bytes
 * consumed as part of this field can also be fetched.  This number may
 * be one longer than the length if there is a terminator character
 * used.<p>
 *
 * This method is mainly for internal use, or for applications which
 * want the raw binary data to interpret themselves.  Otherwise use one
 * of ExtractStringData(), ExtractIntData() or ExtractFloatData().
 *
 * @param pachSourceData The pointer to the raw data for this field.  This
 * may have come from DDFRecord::GetData(), taking into account skip factors
 * over previous subfields data.
 * @param nMaxBytes The maximum number of bytes that are accessible after
 * pachSourceData.
 * @param pnConsumedBytes Pointer to an integer into which the number of
 * bytes consumed by this field should be written.  May be NULL to ignore.
 *
 * @return The number of bytes at pachSourceData which are actual data for
 * this record (not including unit, or field terminator).
 */

int DDFSubfieldDefn::GetDataLength( const char * pachSourceData,
                                    int nMaxBytes, int * pnConsumedBytes )

{
    if( !bIsVariable )
    {
        if( nFormatWidth > nMaxBytes )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Only %d bytes available for subfield %s with\n"
                      "format string %s ... returning shortened data.",
                      nMaxBytes, pszName, pszFormatString );

            if( pnConsumedBytes != NULL )
                *pnConsumedBytes = nMaxBytes;

            return nMaxBytes;
        }
        else
        {
            if( pnConsumedBytes != NULL )
                *pnConsumedBytes = nFormatWidth;

            return nFormatWidth;
        }
    }
    else
    {
        int     nLength = 0;
        int     bAsciiField = TRUE;
        int     extraConsumedBytes = 0;

        /* We only check for the field terminator because of some buggy
         * datasets with missing format terminators.  However, we have found
         * the field terminator and unit terminators are legal characters
         * within the fields of some extended datasets (such as JP34NC94.000).
         * So we don't check for the field terminator and unit terminators as
         * a single byte if the field appears to be multi-byte which we
         * establish by checking for the buffer ending with 0x1e 0x00 (a
         * two byte field terminator).
         *
         * In the case of S57, the subfield ATVL of the NATF field can be
         * encoded in lexical level 2 (see S57 specification, Edition 3.1,
         * paragraph 2.4 and 2.5). In that case the Unit Terminator and Field
         * Terminator are followed by the NULL character.
         * A better fix would be to read the NALL tag in the DSSI to check
         * that the lexical level is 2, instead of relying on the value of
         * the first byte as we are doing - but that is not information
         * that is available at the libiso8211 level (bug #1526)
         */

        // If the whole field ends with 0x1e 0x00 then we assume this
        // field is a double byte character set.
        if( nMaxBytes > 1
            && (pachSourceData[nMaxBytes-2] == chFormatDelimiter
                || pachSourceData[nMaxBytes-2] == DDF_FIELD_TERMINATOR)
            && pachSourceData[nMaxBytes-1] == 0x00 )
            bAsciiField = FALSE;

//        if( !bAsciiField )
//            CPLDebug( "ISO8211", "Non-ASCII field detected." );

        while( nLength < nMaxBytes)
        {
            if (bAsciiField)
            {
                if (pachSourceData[nLength] == chFormatDelimiter ||
                    pachSourceData[nLength] == DDF_FIELD_TERMINATOR)
                    break;
            }
            else
            {
                if (nLength > 0
                    && (pachSourceData[nLength-1] == chFormatDelimiter
                        || pachSourceData[nLength-1] == DDF_FIELD_TERMINATOR)
                    && pachSourceData[nLength] == 0)
                {
                    // Suck up the field terminator if one follows
                    // or else it will be interpreted as a new subfield.
                    // This is a pretty ugly counter-intuitive hack!
                    if (nLength+1 < nMaxBytes &&
                        pachSourceData[nLength+1] == DDF_FIELD_TERMINATOR)
                        extraConsumedBytes++;
                    break;
                }
            }

            nLength++;
        }

        if( pnConsumedBytes != NULL )
        {
            if( nMaxBytes == 0 )
                *pnConsumedBytes = nLength + extraConsumedBytes;
            else
                *pnConsumedBytes = nLength + extraConsumedBytes + 1;
        }

        return nLength;
    }
}

/************************************************************************/
/*                         ExtractStringData()                          */
/************************************************************************/

/**
 * Extract a zero terminated string containing the data for this subfield.
 * Given a pointer to the data
 * for this subfield (from within a DDFRecord) this method will return the
 * data for this subfield.  The number of bytes
 * consumed as part of this field can also be fetched.  This number may
 * be one longer than the string length if there is a terminator character
 * used.<p>
 *
 * This function will return the raw binary data of a subfield for
 * types other than DDFString, including data past zero chars.  This is
 * the standard way of extracting DDFBinaryString subfields for instance.<p>
 *
 * @param pachSourceData The pointer to the raw data for this field.  This
 * may have come from DDFRecord::GetData(), taking into account skip factors
 * over previous subfields data.
 * @param nMaxBytes The maximum number of bytes that are accessible after
 * pachSourceData.
 * @param pnConsumedBytes Pointer to an integer into which the number of
 * bytes consumed by this field should be written.  May be NULL to ignore.
 * This is used as a skip factor to increment pachSourceData to point to the
 * next subfields data.
 *
 * @return A pointer to a buffer containing the data for this field.  The
 * returned pointer is to an internal buffer which is invalidated on the
 * next ExtractStringData() call on this DDFSubfieldDefn().  It should not
 * be freed by the application.
 *
 * @see ExtractIntData(), ExtractFloatData()
 */

const char *
DDFSubfieldDefn::ExtractStringData( const char * pachSourceData,
                                    int nMaxBytes, int * pnConsumedBytes )

{
    int         nLength = GetDataLength( pachSourceData, nMaxBytes,
                                         pnConsumedBytes );

/* -------------------------------------------------------------------- */
/*      Do we need to grow the buffer.                                  */
/* -------------------------------------------------------------------- */
    if( nMaxBufChars < nLength+1 )
    {
        CPLFree( pachBuffer );

        nMaxBufChars = nLength+1;
        pachBuffer = (char *) CPLMalloc(nMaxBufChars);
    }

/* -------------------------------------------------------------------- */
/*      Copy the data to the buffer.  We use memcpy() so that it        */
/*      will work for binary data.                                      */
/* -------------------------------------------------------------------- */
    memcpy( pachBuffer, pachSourceData, nLength );
    pachBuffer[nLength] = '\0';

    return pachBuffer;
}

/************************************************************************/
/*                          ExtractFloatData()                          */
/************************************************************************/

/**
 * Extract a subfield value as a float.  Given a pointer to the data
 * for this subfield (from within a DDFRecord) this method will return the
 * floating point data for this subfield.  The number of bytes
 * consumed as part of this field can also be fetched.  This method may be
 * called for any type of subfield, and will return zero if the subfield is
 * not numeric.
 *
 * @param pachSourceData The pointer to the raw data for this field.  This
 * may have come from DDFRecord::GetData(), taking into account skip factors
 * over previous subfields data.
 * @param nMaxBytes The maximum number of bytes that are accessible after
 * pachSourceData.
 * @param pnConsumedBytes Pointer to an integer into which the number of
 * bytes consumed by this field should be written.  May be NULL to ignore.
 * This is used as a skip factor to increment pachSourceData to point to the
 * next subfields data.
 *
 * @return The subfield's numeric value (or zero if it isn't numeric).
 *
 * @see ExtractIntData(), ExtractStringData()
 */

double
DDFSubfieldDefn::ExtractFloatData( const char * pachSourceData,
                                   int nMaxBytes, int * pnConsumedBytes )

{
    switch( pszFormatString[0] )
    {
      case 'A':
      case 'I':
      case 'R':
      case 'S':
      case 'C':
        return CPLAtof(ExtractStringData(pachSourceData, nMaxBytes,
                                      pnConsumedBytes));

      case 'B':
      case 'b':
      {
          unsigned char   abyData[8];
          void* pabyData = abyData;

          if( nFormatWidth > nMaxBytes )
          {
              CPLError( CE_Warning, CPLE_AppDefined,
                        "Attempt to extract float subfield %s with format %s\n"
                        "failed as only %d bytes available.  Using zero.",
                        pszName, pszFormatString, nMaxBytes );
              return 0;
          }

          if( pnConsumedBytes != NULL )
              *pnConsumedBytes = nFormatWidth;

          // Byte swap the data if it isn't in machine native format.
          // In any event we copy it into our buffer to ensure it is
          // word aligned.
#ifdef CPL_LSB
          if( pszFormatString[0] == 'B' )
#else
              if( pszFormatString[0] == 'b' )
#endif
              {
                  for( int i = 0; i < nFormatWidth; i++ )
                      abyData[nFormatWidth-i-1] = pachSourceData[i];
              }
              else
              {
                  memcpy( abyData, pachSourceData, nFormatWidth );
              }

          // Interpret the bytes of data.
          switch( eBinaryFormat )
          {
            case UInt:
              if( nFormatWidth == 1 )
                  return abyData[0];
              else if( nFormatWidth == 2 )
                  return *((GUInt16 *) pabyData);
              else if( nFormatWidth == 4 )
                  return *((GUInt32 *) pabyData);
              else
              {
                  // CPLAssert( false );
                  return 0.0;
              }

            case SInt:
              if( nFormatWidth == 1 )
                  return *((signed char *) abyData);
              else if( nFormatWidth == 2 )
                  return *((GInt16 *) pabyData);
              else if( nFormatWidth == 4 )
                  return *((GInt32 *) pabyData);
              else
              {
                  // CPLAssert( false );
                  return 0.0;
              }

            case FloatReal:
              if( nFormatWidth == 4 )
                  return *((float *) pabyData);
              else if( nFormatWidth == 8 )
                  return *((double *) pabyData);
              else
              {
                  // CPLAssert( false );
                  return 0.0;
              }

            case NotBinary:
            case FPReal:
            case FloatComplex:
              // CPLAssert( false );
              return 0.0;
          }
          break;
          // end of 'b'/'B' case.
      }

      default:
        // CPLAssert( false );
        return 0.0;
    }

    // CPLAssert( false );
    return 0.0;
}

/************************************************************************/
/*                           ExtractIntData()                           */
/************************************************************************/

/**
 * Extract a subfield value as an integer.  Given a pointer to the data
 * for this subfield (from within a DDFRecord) this method will return the
 * int data for this subfield.  The number of bytes
 * consumed as part of this field can also be fetched.  This method may be
 * called for any type of subfield, and will return zero if the subfield is
 * not numeric.
 *
 * @param pachSourceData The pointer to the raw data for this field.  This
 * may have come from DDFRecord::GetData(), taking into account skip factors
 * over previous subfields data.
 * @param nMaxBytes The maximum number of bytes that are accessible after
 * pachSourceData.
 * @param pnConsumedBytes Pointer to an integer into which the number of
 * bytes consumed by this field should be written.  May be NULL to ignore.
 * This is used as a skip factor to increment pachSourceData to point to the
 * next subfields data.
 *
 * @return The subfield's numeric value (or zero if it isn't numeric).
 *
 * @see ExtractFloatData(), ExtractStringData()
 */

int
DDFSubfieldDefn::ExtractIntData( const char * pachSourceData,
                                 int nMaxBytes, int * pnConsumedBytes )

{
    switch( pszFormatString[0] )
    {
      case 'A':
      case 'I':
      case 'R':
      case 'S':
      case 'C':
        return atoi(ExtractStringData(pachSourceData, nMaxBytes,
                                      pnConsumedBytes));

      case 'B':
      case 'b':
      {
          unsigned char   abyData[8];
          void* pabyData = abyData;

          if( nFormatWidth > nMaxBytes || nFormatWidth >= (int)sizeof(abyData) )
          {
              CPLError(CE_Warning, CPLE_AppDefined,
                       "Attempt to extract int subfield %s with format %s\n"
                       "failed as only %d bytes available.  Using zero.",
                       pszName, pszFormatString,
                       std::min(nMaxBytes, static_cast<int>(sizeof(abyData))));
              return 0;
          }

          if( pnConsumedBytes != NULL )
              *pnConsumedBytes = nFormatWidth;

          // Byte swap the data if it isn't in machine native format.
          // In any event we copy it into our buffer to ensure it is
          // word aligned.
#ifdef CPL_LSB
          if( pszFormatString[0] == 'B' )
#else
              if( pszFormatString[0] == 'b' )
#endif
              {
                  for( int i = 0; i < nFormatWidth; i++ )
                      abyData[nFormatWidth-i-1] = pachSourceData[i];
              }
              else
              {
                  memcpy( abyData, pachSourceData, nFormatWidth );
              }

          // Interpret the bytes of data.
          switch( eBinaryFormat )
          {
            case UInt:
              if( nFormatWidth == 4 )
                  return (int) *((GUInt32 *) pabyData);
              else if( nFormatWidth == 1 )
                  return abyData[0];
              else if( nFormatWidth == 2 )
                  return *((GUInt16 *) pabyData);
              else
              {
                  // CPLAssert( false );
                  return 0;
              }

            case SInt:
              if( nFormatWidth == 4 )
                  return *((GInt32 *) pabyData);
              else if( nFormatWidth == 1 )
                  return *((signed char *) abyData);
              else if( nFormatWidth == 2 )
                  return *((GInt16 *) pabyData);
              else
              {
                  // CPLAssert( false );
                  return 0;
              }

            case FloatReal:
              if( nFormatWidth == 4 )
                  return (int) *((float *) pabyData);
              else if( nFormatWidth == 8 )
                  return (int) *((double *) pabyData);
              else
              {
                  // CPLAssert( false );
                  return 0;
              }

            case NotBinary:
            case FPReal:
            case FloatComplex:
              // CPLAssert( false );
              return 0;
          }
          break;
          // end of 'b'/'B' case.
      }

      default:
        // CPLAssert( false );
        return 0;
    }

    // CPLAssert( false );
    return 0;
}

/************************************************************************/
/*                              DumpData()                              */
/*                                                                      */
/*      Dump the instance data for this subfield from a data            */
/*      record.  This fits into the output dump stream of a DDFField.   */
/************************************************************************/

/**
 * Dump subfield value to debugging file.
 *
 * @param pachData Pointer to data for this subfield.
 * @param nMaxBytes Maximum number of bytes available in pachData.
 * @param fp File to write report to.
 */

void DDFSubfieldDefn::DumpData( const char * pachData, int nMaxBytes,
                                FILE * fp )

{
    if( nMaxBytes < 0 )
    {
        fprintf( fp, "      Subfield `%s' = {invalid length}\n", pszName );
        return;
    }
    if( eType == DDFFloat )
        fprintf( fp, "      Subfield `%s' = %f\n",
                 pszName,
                 ExtractFloatData( pachData, nMaxBytes, NULL ) );
    else if( eType == DDFInt )
        fprintf( fp, "      Subfield `%s' = %d\n",
                 pszName,
                 ExtractIntData( pachData, nMaxBytes, NULL ) );
    else if( eType == DDFBinaryString )
    {
        int nBytes = 0;
        GByte *pabyBString =
            (GByte *) ExtractStringData( pachData, nMaxBytes, &nBytes );

        fprintf( fp, "      Subfield `%s' = 0x", pszName );
        for( int i = 0; i < std::min(nBytes, 24); i++ )
            fprintf( fp, "%02X", pabyBString[i] );

        if( nBytes > 24 )
            fprintf( fp, "%s", "..." );

        fprintf( fp, "\n" );
    }
    else
        fprintf( fp, "      Subfield `%s' = `%s'\n",
                 pszName,
                 ExtractStringData( pachData, nMaxBytes, NULL ) );
}

/************************************************************************/
/*                          GetDefaultValue()                           */
/************************************************************************/

/**
 * Get default data.
 *
 * Returns the default subfield data contents for this subfield definition.
 * For variable length numbers this will normally be "0<unit-terminator>".
 * For variable length strings it will be "<unit-terminator>".  For fixed
 * length numbers it is zero filled.  For fixed length strings it is space
 * filled.  For binary numbers it is binary zero filled.
 *
 * @param pachData the buffer into which the returned default will be placed.
 * May be NULL if just querying default size.
 * @param nBytesAvailable the size of pachData in bytes.
 * @param pnBytesUsed will receive the size of the subfield default data in
 * bytes.
 *
 * @return TRUE on success or FALSE on failure or if the passed buffer is too
 * small to hold the default.
 */

int DDFSubfieldDefn::GetDefaultValue( char *pachData, int nBytesAvailable,
                                      int *pnBytesUsed )

{
    int nDefaultSize;

    if( !bIsVariable )
        nDefaultSize = nFormatWidth;
    else
        nDefaultSize = 1;

    if( pnBytesUsed != NULL )
        *pnBytesUsed = nDefaultSize;

    if( pachData == NULL )
        return TRUE;

    if( nBytesAvailable < nDefaultSize )
        return FALSE;

    if( bIsVariable )
    {
        pachData[0] = DDF_UNIT_TERMINATOR;
    }
    else
    {
        char chFillChar;
        if( GetBinaryFormat() == NotBinary )
        {
            if( GetType() == DDFInt || GetType() == DDFFloat )
                chFillChar = '0'; /* ASCII zero intended */
            else
                chFillChar = ' ';
        }
        else
            chFillChar = 0;
        memset( pachData, chFillChar, nDefaultSize );
    }

    return TRUE;
}

/************************************************************************/
/*                         FormatStringValue()                          */
/************************************************************************/

/**
 * Format string subfield value.
 *
 * Returns a buffer with the passed in string value reformatted in a way
 * suitable for storage in a DDFField for this subfield.
 */

int DDFSubfieldDefn::FormatStringValue( char *pachData, int nBytesAvailable,
                                        int *pnBytesUsed,
                                        const char *pszValue,
                                        int nValueLength )

{
    int nSize;

    if( nValueLength == -1 )
        nValueLength = static_cast<int>(strlen(pszValue));

    if( bIsVariable )
    {
        nSize = nValueLength + 1;
    }
    else
    {
        nSize = nFormatWidth;
    }

    if( pnBytesUsed != NULL )
        *pnBytesUsed = nSize;

    if( pachData == NULL )
        return TRUE;

    if( nBytesAvailable < nSize )
        return FALSE;

    if( bIsVariable )
    {
        strncpy( pachData, pszValue, nSize-1 );
        pachData[nSize-1] = DDF_UNIT_TERMINATOR;
    }
    else
    {
        if( GetBinaryFormat() == NotBinary )
        {
            memset( pachData, ' ', nSize );
            // cppcheck-suppress redundantCopy
            memcpy( pachData, pszValue, std::min(nValueLength, nSize) );
        }
        else
        {
            memset( pachData, 0, nSize );
            // cppcheck-suppress redundantCopy
            memcpy( pachData, pszValue, std::min(nValueLength, nSize) );
        }
    }

    return TRUE;
}

/************************************************************************/
/*                           FormatIntValue()                           */
/************************************************************************/

/**
 * Format int subfield value.
 *
 * Returns a buffer with the passed in int value reformatted in a way
 * suitable for storage in a DDFField for this subfield.
 */

int DDFSubfieldDefn::FormatIntValue( char *pachData, int nBytesAvailable,
                                     int *pnBytesUsed, int nNewValue )

{
    int nSize;
    char szWork[30];

    snprintf( szWork, sizeof(szWork), "%d", nNewValue );

    if( bIsVariable )
    {
        nSize = static_cast<int>(strlen(szWork)) + 1;
    }
    else
    {
        nSize = nFormatWidth;

        if( GetBinaryFormat() == NotBinary && (int) strlen(szWork) > nSize )
            return FALSE;
    }

    if( pnBytesUsed != NULL )
        *pnBytesUsed = nSize;

    if( pachData == NULL )
        return TRUE;

    if( nBytesAvailable < nSize )
        return FALSE;

    if( bIsVariable )
    {
        strncpy( pachData, szWork, nSize-1 );
        pachData[nSize-1] = DDF_UNIT_TERMINATOR;
    }
    else
    {
        GUInt32 nMask = 0xff;
        int i;

        switch( GetBinaryFormat() )
        {
          case NotBinary:
          {
            char chFillChar = '0'; /* ASCII zero intended */
            memset( pachData, chFillChar, nSize );
            strncpy( pachData + nSize - strlen(szWork), szWork,
                     strlen(szWork) );
            break;
          }

          case UInt:
          case SInt:
            for( i = 0; i < nFormatWidth; i++ )
            {
                int iOut;

                // big endian required?
                if( pszFormatString[0] == 'B' )
                    iOut = nFormatWidth - i - 1;
                else
                    iOut = i;

                pachData[iOut] = (char)((nNewValue & nMask) >> (i*8));
                nMask *= 256;
            }
            break;

          case FloatReal:
            CPLAssert( false );
            break;

          default:
            CPLAssert( false );
            break;
        }
    }

    return TRUE;
}

/************************************************************************/
/*                          FormatFloatValue()                          */
/************************************************************************/

/**
 * Format float subfield value.
 *
 * Returns a buffer with the passed in float value reformatted in a way
 * suitable for storage in a DDFField for this subfield.
 */

int DDFSubfieldDefn::FormatFloatValue( char *pachData, int nBytesAvailable,
                                       int *pnBytesUsed, double dfNewValue )

{
    int nSize;
    char szWork[120];

    CPLsnprintf( szWork, sizeof(szWork), "%.16g", dfNewValue );

    if( bIsVariable )
    {
        nSize = static_cast<int>(strlen(szWork)) + 1;
    }
    else
    {
        nSize = nFormatWidth;

        if( GetBinaryFormat() == NotBinary && (int) strlen(szWork) > nSize )
            return FALSE;
    }

    if( pnBytesUsed != NULL )
        *pnBytesUsed = nSize;

    if( pachData == NULL )
        return TRUE;

    if( nBytesAvailable < nSize )
        return FALSE;

    if( bIsVariable )
    {
        strncpy( pachData, szWork, nSize-1 );
        pachData[nSize-1] = DDF_UNIT_TERMINATOR;
    }
    else
    {
        if( GetBinaryFormat() == NotBinary )
        {
            const char chFillZeroASCII = '0'; /* ASCII zero intended */
            /* coverity[bad_memset] */
            memset( pachData, chFillZeroASCII, nSize );
            strncpy( pachData + nSize - strlen(szWork), szWork,
                     strlen(szWork) );
        }
        else
        {
            CPLAssert( false );
            /* implement me */
        }
    }

    return TRUE;
}
