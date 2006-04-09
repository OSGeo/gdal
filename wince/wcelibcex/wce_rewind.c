/*
 * $Id$
 *
 * Defines rewind() function.
 *
 * Created by Mateusz Loskot, mloskot@taxussi.com.pl
 *
 * Copyright (c) 2006 Taxus SI Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom 
 * the Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * MIT License:
 * http://opensource.org/licenses/mit-license.php
 *
 * Contact:
 * Taxus SI Ltd.
 * http://www.taxussi.com.pl
 *
 */

#include <windows.h>

/*******************************************************************************
* wceex_rewind - Reset the file position indicator in a stream
*
* Description:
*
*   The call rewind(stream) shall be equivalent to:
*   (void) fseek(stream, 0L, SEEK_SET)
*
*   Internally, rewind() function uses SetFilePointer call from 
*   Windows CE API.
*
*   Windows CE specific:
*   On Windows CE HANDLE type is defined as typedef void *HANDLE
*   and FILE type is declared as typedef void FILE.
*
* Return:
*
*   No return value.
*       
* Reference:
*
*   IEEE 1003.1, 2004 Edition
*
*******************************************************************************/
void wceex_rewind(FILE * fp)
{
    /* HANDLE is a typedef of void* */
    HANDLE hFile = (void*)fp;
	
    if (0xFFFFFFFF == SetFilePointer(hFile,0,0,FILE_BEGIN))
    {
    	; /* No return */
    }
}

