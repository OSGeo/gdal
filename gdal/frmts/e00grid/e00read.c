/**********************************************************************
 * $Id: e00read.c,v 1.10 2009-02-24 20:03:50 aboudreault Exp $
 *
 * Name:     e00read.c
 * Project:  Compressed E00 Read/Write library
 * Language: ANSI C
 * Purpose:  Functions to read Compressed E00 files and return a stream 
 *           of uncompressed lines.
 * Author:   Daniel Morissette, dmorissette@mapgears.com
 *
 * $Log: e00read.c,v $
 * Revision 1.10  2009-02-24 20:03:50  aboudreault
 * Added a short manual pages (#1875)
 * Updated documentation and code examples (#247)
 *
 * Revision 1.9  2005-09-17 14:22:05  daniel
 * Switch to MIT license, update refs to website and email address, and
 * prepare for 1.0.0 release.
 *
 * Revision 1.8  1999/02/25 18:45:56  daniel
 * Now use CPL for Error handling, Memory allocation, and File access
 *
 * Revision 1.7  1999/01/08 17:39:08  daniel
 * Added E00ReadCallbackOpen()
 *
 * Revision 1.6  1998/11/13 16:34:08  daniel
 * Fixed '\r' problem when reading E00 files from a PC under Unix
 *
 * Revision 1.5  1998/11/13 15:48:08  daniel
 * Simplified the decoding of the compression codes for numbers
 * (use a logical rule instead of going case by case)
 *
 * Revision 1.4  1998/11/02 18:34:29  daniel
 * Added E00ErrorReset() calls.  Replace "EXP  1" by "EXP  0" on read.
 *
 * Revision 1.1  1998/10/29 13:26:00  daniel
 * Initial revision
 *
 **********************************************************************
 * Copyright (c) 1998-2005, Daniel Morissette
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
 * 
 **********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "e00compr.h"

static void _ReadNextSourceLine(E00ReadPtr psInfo);
static const char *_UncompressNextLine(E00ReadPtr psInfo);

/**********************************************************************
 *                          _E00ReadTestOpen()
 *
 * Given a pre-initialized E00ReadPtr, this function will make sure
 * that the file is really a E00 file, and also establish if it is
 * compressed or not... setting the structure members by the same way.
 *
 * Returns NULL (and destroys the E00ReadPtr) if the file does not
 * appear to be a valid E00 file.
 **********************************************************************/
static E00ReadPtr  _E00ReadTestOpen(E00ReadPtr psInfo)
{

    /* Check that the file is in E00 format.
     */
    _ReadNextSourceLine(psInfo);
    if (!psInfo->bEOF && strncmp(psInfo->szInBuf, "EXP ", 4) == 0)
    {
        /* We should be in presence of a valid E00 file... 
         * Is the file compressed or not?
         *
         * Note: we cannot really rely on the number that follows the EXP to
         * establish if the file is compressed since we sometimes encounter
         * uncompressed files that start with a "EXP 1" line!!!
         *
         * The best test is to read the first non-empty line: if the file is
         * compressed, the first line of data should be 79 or 80 characters 
         * long and contain several '~' characters.
         */
        do
        {
            _ReadNextSourceLine(psInfo);
        }while(!psInfo->bEOF && 
               (psInfo->szInBuf[0] == '\0' || isspace(psInfo->szInBuf[0])) );

         if (!psInfo->bEOF && 
             (strlen(psInfo->szInBuf)==79 || strlen(psInfo->szInBuf)==80) &&
             strchr(psInfo->szInBuf, '~') != NULL )
             psInfo->bIsCompressed = 1;

         /* Move the Read ptr ready to read at the beginning of the file
          */
         E00ReadRewind(psInfo);
    }
    else
    {
        CPLFree(psInfo);
        psInfo = NULL;
    }

    return psInfo;
}

/**********************************************************************
 *                          E00ReadOpen()
 *
 * Try to open a E00 file given its filename and return a E00ReadPtr handle.
 *
 * Returns NULL if the file could not be opened or if it does not 
 * appear to be a valid E00 file.
 **********************************************************************/
E00ReadPtr  E00ReadOpen(const char *pszFname)
{
    E00ReadPtr  psInfo = NULL;
    FILE        *fp;

    CPLErrorReset();

    /* Open the file 
     */
    fp = VSIFOpen(pszFname, "rt");
    if (fp == NULL)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Failed to open %s: %s", pszFname, strerror(errno));
        return NULL;
    }

    /* File was succesfully opened, allocate and initialize a 
     * E00ReadPtr handle and check that the file is valid.
     */
    psInfo = (E00ReadPtr)CPLCalloc(1, sizeof(struct _E00ReadInfo));

    psInfo->fp = fp;

    psInfo = _E00ReadTestOpen(psInfo);

    if (psInfo == NULL)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "%s is not a valid E00 file.", pszFname);
    }

    return psInfo;
}

/**********************************************************************
 *                          E00ReadCallbackOpen()
 *
 * This is an alternative to E00ReadOpen() for cases where you want to
 * do all the file management yourself.  You open/close the file yourself
 * and provide 2 callback functions: to read from the file and rewind the
 * file pointer.  pRefData is your handle on the physical file and can
 * be whatever you want... it is not used by the library, it will be
 * passed directly to your 2 callback functions when they are called.
 *
 * The callback functions must have the following C prototype:
 *
 *   const char *myReadNextLine(void *pRefData);
 *   void        myReadRewind(void *pRefData);
 *
 *   myReadNextLine() should return a reference to its own internal 
 *   buffer, or NULL if an error happens or EOF is reached.
 *
 * E00ReadCallbackOpen() returns a E00ReadPtr handle or NULL if the file
 * does not appear to be a valid E00 file.
 **********************************************************************/
E00ReadPtr  E00ReadCallbackOpen(void *pRefData,
                                const char * (*pfnReadNextLine)(void *),
                                void (*pfnReadRewind)(void *))
{
    E00ReadPtr  psInfo = NULL;

    CPLErrorReset();

    /* Make sure we received valid function pointers
     */
    if (pfnReadNextLine == NULL || pfnReadRewind == NULL)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "Invalid function pointers!");
        return NULL;
    }

    /* Allocate and initialize a 
     * E00ReadPtr handle and check that the file is valid.
     */
    psInfo = (E00ReadPtr)CPLCalloc(1, sizeof(struct _E00ReadInfo));

    psInfo->pRefData = pRefData;
    psInfo->pfnReadNextLine = pfnReadNextLine;
    psInfo->pfnReadRewind = pfnReadRewind;

    psInfo = _E00ReadTestOpen(psInfo);

    if (psInfo == NULL)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "This is not a valid E00 file.");
    }

    return psInfo;
}

/**********************************************************************
 *                          E00ReadClose()
 *
 * Close input file and release any memory used by the E00ReadPtr.
 **********************************************************************/
void    E00ReadClose(E00ReadPtr psInfo)
{
    CPLErrorReset();

    if (psInfo)
    {
        if (psInfo->fp)
            VSIFClose(psInfo->fp);
        CPLFree(psInfo);
    }
}

/**********************************************************************
 *                          E00ReadRewind()
 *
 * Rewind the E00ReadPtr.  Allows starting another read pass on the 
 * input file.
 **********************************************************************/
void    E00ReadRewind(E00ReadPtr psInfo)
{
    CPLErrorReset();

    psInfo->szInBuf[0] = psInfo->szOutBuf[0] = '\0';
    psInfo->iInBufPtr = 0;

    psInfo->nInputLineNo = 0;

    if (psInfo->pfnReadRewind == NULL)
        VSIRewind(psInfo->fp);
    else
        psInfo->pfnReadRewind(psInfo->pRefData);

    psInfo->bEOF = 0;
}

/**********************************************************************
 *                          E00ReadNextLine()
 *
 * Return the next line of input from the E00 file or NULL if we reached EOF.
 *
 * Returns a reference to an internal buffer whose contents will be valid
 * only until the next call to this function.
 **********************************************************************/
const char *E00ReadNextLine(E00ReadPtr psInfo)
{
    const char *pszLine = NULL;
    char *pszPtr;

    CPLErrorReset();

    if (psInfo && !psInfo->bEOF)
    {
        if (!psInfo->bIsCompressed)
        {
            /* Uncompressed file... return line directly. 
             */
            _ReadNextSourceLine(psInfo);
            pszLine = psInfo->szInBuf;
        }
        else if (psInfo->bIsCompressed && psInfo->nInputLineNo == 0)
        {
            /* Header line in a compressed file... return line 
             * after replacing "EXP  1" with "EXP  0".  E00ReadOpen()
             * has already verified that this line starts with "EXP "
             */
            _ReadNextSourceLine(psInfo);
            if ( (pszPtr = strstr(psInfo->szInBuf, " 1")) != NULL)
                pszPtr[1] = '0';
            pszLine = psInfo->szInBuf;
        }
        else
        {
            if (psInfo->nInputLineNo == 1)
            {
                /* We just read the header line... reload the input buffer
                 */
                _ReadNextSourceLine(psInfo);
            }

            /* Uncompress the next line of input and return it 
             */
            pszLine = _UncompressNextLine(psInfo);
        }

        /* If we just reached EOF then make sure we don't add an extra
         * empty line at the end of the uncompressed oputput.
         */
        if (psInfo->bEOF && strlen(pszLine) == 0)
            pszLine = NULL;
    }

    return pszLine;
}

/**********************************************************************
 *                          _ReadNextSourceLine()
 *
 * Loads the next line from the source file in psInfo.
 *
 * psInfo->bEOF should be checked after this call.
 **********************************************************************/
static void _ReadNextSourceLine(E00ReadPtr psInfo)
{
    if (!psInfo->bEOF)
    {
        psInfo->iInBufPtr = 0;
        psInfo->szInBuf[0] = '\0';

        /* Read either using fgets() or psInfo->pfnReadNextLine() 
         * depending on the way the file was opened...
         */
        if (psInfo->pfnReadNextLine == NULL)
        {
            if (VSIFGets(psInfo->szInBuf,E00_READ_BUF_SIZE,psInfo->fp) == NULL)
            {
                /* We reached EOF
                 */
                psInfo->bEOF = 1;
            }
        }
        else
        {
            const char *pszLine;
            pszLine = psInfo->pfnReadNextLine(psInfo->pRefData);
            if (pszLine)
            {
                strncpy(psInfo->szInBuf, pszLine, E00_READ_BUF_SIZE);
            }
            else
            {
                /* We reached EOF
                 */
                psInfo->bEOF = 1;
            }
        }

        if (!psInfo->bEOF)
        {
            /* A new line was succesfully read.  Remove trailing '\n' if any.
             * (Note: For Unix systems, we also have to check for '\r')
             */
            int nLen;
            nLen = strlen(psInfo->szInBuf);
            while(nLen > 0 && (psInfo->szInBuf[nLen-1] == '\n' ||
                               psInfo->szInBuf[nLen-1] == '\r'   ) )
            {
                nLen--;
                psInfo->szInBuf[nLen] = '\0';
            }

            psInfo->nInputLineNo++;
        }
    }
}


/**********************************************************************
 *                          _GetNextSourceChar()
 *
 * Returns the next char from the source file input buffer... and 
 * reload the input buffer when necessary... this function makes the
 * whole input file appear as one huge null-terminated string with
 * no line delimiters.
 *
 * Will return '\0' when EOF is reached.
 **********************************************************************/
static char _GetNextSourceChar(E00ReadPtr psInfo)
{
    char c = '\0';

    if (!psInfo->bEOF)
    {
        if (psInfo->szInBuf[psInfo->iInBufPtr] == '\0')
        {
            _ReadNextSourceLine(psInfo);
            c = _GetNextSourceChar(psInfo);
        }
        else
        {
            c = psInfo->szInBuf[psInfo->iInBufPtr++];
        }
    }

    return c;
}

/**********************************************************************
 *                          _UngetSourceChar()
 *
 * Reverse the effect of the previous call to _GetNextSourceChar() by
 * moving the input buffer pointer back 1 character.
 *
 * This function can be called only once per call to _GetNextSourceChar()
 * (i.e. you cannot unget more than one character) otherwise the pointer
 * could move before the beginning of the input buffer.
 **********************************************************************/
static void _UngetSourceChar(E00ReadPtr psInfo)
{
    if (psInfo->iInBufPtr > 0)
        psInfo->iInBufPtr--;
    else
    {
        /* This error can happen only if _UngetSourceChar() is called
         * twice in a row (which should never happen!).
         */
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "UNEXPECTED INTERNAL ERROR: _UngetSourceChar() "
                      "failed while reading line %d.", psInfo->nInputLineNo);
    }
}

/**********************************************************************
 *                          _UncompressNextLine()
 *
 * Uncompress one line of input and return a reference to an internal
 * buffer containing the uncompressed output.
 **********************************************************************/
static const char *_UncompressNextLine(E00ReadPtr psInfo)
{
    char    c;
    int     bEOL = 0;   /* Set to 1 when End of Line reached */
    int     iOutBufPtr = 0, i, n;
    int     iDecimalPoint, bOddNumDigits, iCurDigit;
    char    const *pszExp;
    int     bPreviousCodeWasNumeric = 0;

    while(!bEOL && (c=_GetNextSourceChar(psInfo)) != '\0')
    {
        if (c != '~')
        {
            /* Normal character... just copy it */
            psInfo->szOutBuf[iOutBufPtr++] = c;
            bPreviousCodeWasNumeric = 0;
        }
        else /* c == '~' */
        {
            /* ========================================================
             * Found an encoded sequence.
             * =======================================================*/
            c = _GetNextSourceChar(psInfo);

            /* --------------------------------------------------------
             * Compression level 1: only spaces, '~' and '\n' are encoded
             * -------------------------------------------------------*/
            if (c == ' ')
            {
                /* "~ " followed by number of spaces
                 */
                c = _GetNextSourceChar(psInfo);
                n = c - ' ';
                for(i=0; i<n; i++)
                    psInfo->szOutBuf[iOutBufPtr++] = ' ';
                bPreviousCodeWasNumeric = 0;
            }
            else if (c == '}')
            {
                /* "~}" == '\n'
                 */
                bEOL = 1;
                bPreviousCodeWasNumeric = 0;
            }
            else if (bPreviousCodeWasNumeric)
            {
                /* If the previous code was numeric, then the only valid code
                 * sequences are the ones above: "~ " and "~}".  If we end up
                 * here, it is because the number was followed by a '~' but
                 * this '~' was not a code, it only marked the end of a
                 * number that was not followed by any space.
                 *
                 * We should simply ignore the '~' and return the character
                 * that follows it directly.
                 */
                psInfo->szOutBuf[iOutBufPtr++] = c;
                bPreviousCodeWasNumeric = 0;
            }
            else if (c == '~' || c == '-')
            {
                /* "~~" and "~-" are simple escape sequences for '~' and '-'
                 */
                psInfo->szOutBuf[iOutBufPtr++] = c;
            }
            /* --------------------------------------------------------
             * Compression level 2: numeric values are encoded.
             *
             * All codes for this level are in the form "~ c0 c1 c2 ... cn"
             * where:
             *
             *  ~             marks the beginning of a new code sequence
             *
             *  c0            is a single character code defining the format
             *                of the number (decimal position, exponent, 
             *                and even or odd number of digits)
             *
             *  c1 c2 ... cn  each of these characters represent a pair of
             *                digits of the encoded value with '!' == 00
             *                values 92..99 are encoded on 2 chars that
             *                must be added to each other
             *                (i.e. 92 == }!, 93 == }", ...)
             *
             *  The sequence ends with a ' ' or a '~' character
             * -------------------------------------------------------*/
            else if (c >= '!' && c <= 'z')
            {
                /* The format code defines 3 characteristics of the final number:
                 * - Presence of a decimal point and its position
                 * - Presence of an exponent, and its sign
                 * - Odd or even number of digits
                 */
                n = c - '!';
                iDecimalPoint = n % 15; /* 0 = no decimal point         */
                bOddNumDigits = n / 45; /* 0 = even num.digits, 1 = odd */
                n = n / 15;
                if ( n % 3 == 1 )
                    pszExp = "E+";
                else if (n % 3 == 2 )
                    pszExp = "E-";
                else
                    pszExp = NULL;

                /* Decode the c1 c2 ... cn value and apply the format.
                 * Read characters until we encounter a ' ' or a '~'
                 */
                iCurDigit = 0;
                while((c=_GetNextSourceChar(psInfo)) != '\0' &&
                      c != ' ' && c != '~')
                {
                    n = c - '!';
                    if (n == 92 && (c=_GetNextSourceChar(psInfo)) != '\0')
                        n += c - '!';

                    psInfo->szOutBuf[iOutBufPtr++] = '0' + n/10;

                    if (++iCurDigit == iDecimalPoint)
                        psInfo->szOutBuf[iOutBufPtr++] = '.';

                    psInfo->szOutBuf[iOutBufPtr++] = '0' + n%10;

                    if (++iCurDigit == iDecimalPoint)
                        psInfo->szOutBuf[iOutBufPtr++] = '.';
                }

                if (c == '~' || c == ' ')
                {
                    bPreviousCodeWasNumeric = 1;
                    _UngetSourceChar(psInfo);
                }

                /* If odd number of digits, then flush the last one
                 */
                if (bOddNumDigits)
                    iOutBufPtr--;

                /* Insert the exponent string before the 2 last digits
                 * (we assume the exponent string is 2 chars. long)
                 */
                if (pszExp)
                {
                    for(i=0; i<2;i++)
                    {
                        psInfo->szOutBuf[iOutBufPtr] =
                                   psInfo->szOutBuf[iOutBufPtr-2];
                        psInfo->szOutBuf[iOutBufPtr-2] = pszExp[i];
                        iOutBufPtr++;
                    }
                }
            }
            else
            {
                /* Unsupported code sequence... this is a possibility
                 * given the fact that this library was written by
                 * reverse-engineering the format!
                 *
                 * Send an error to the user and abort.
                 *
                 * If this error ever happens, and you are convinced that
                 * the input file is not corrupted, then please report it to
                 * me at dmorissette@mapgears.com, quoting the section of the input
                 * file that produced it, and I'll do my best to add support
                 * for this code sequence.
                 */
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Unexpected code \"~%c\" encountered in line %d.",
                          c, psInfo->nInputLineNo);
    
                /* Force the program to abort by simulating a EOF 
                 */
                psInfo->bEOF = 1;
                bEOL = 1;
            }

        }/* if c == '~' */

        /* E00 lines should NEVER be longer than 80 chars.  if we passed
         * that limit, then the input file is likely corrupt.
         */
         if (iOutBufPtr > 80)
         {
            CPLError(CE_Failure, CPLE_FileIO,
                      "Uncompressed line longer than 80 chars. "
                      "Input file possibly corrupt around line %d.",
                      psInfo->nInputLineNo);
            /* Force the program to abort by simulating a EOF 
             */
            psInfo->bEOF = 1;
            bEOL = 1;
         }

    }/* while !EOL */

    psInfo->szOutBuf[iOutBufPtr++] = '\0';

    return psInfo->szOutBuf;
}
