/**********************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  String functions for MacOS (pre MacOS X).
 * Author:   Dennis Christopher, dennis@avenza.com
 *
 **********************************************************************
 * Copyright (c) 2001, Avenza Systems, Inc.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************
 *
 * $Log$
 * Revision 1.4  2002/03/05 14:26:57  warmerda
 * expanded tabs
 *
 * Revision 1.3  2001/07/18 04:00:49  warmerda
 * added CPL_CVSID
 *
 * Revision 1.2  2001/04/30 18:16:16  warmerda
 * added big pre10 ifdef
 *
 * Revision 1.1  2001/04/30 18:15:39  warmerda
 * New
 *
 *
 **********************************************************************/

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
 
#include "cpl_port.h"

#ifdef macos_pre10
 
CPL_CVSID("$Id$");

int strcasecmp(char * str1, char * str2)
{
    int i;
    char * temp1, *temp2;
        
    for(i=0;str1[i]!='\0';i++)
        temp1[i]=tolower(str1[i]);
    temp1[i]='\0';
        
    for(i=0;str2[i]!='\0';i++)
        temp2[i]=tolower(str2[i]);
    temp2[i]='\0';
        
    return (strcmp( temp1, temp2) );

 
}

int strncasecmp(char * str1, char * str2, int len)
{
    int i;
    char * temp1, *temp2;
        
    for(i=0;str1[i]!='\0';i++)
        temp1[i]=tolower(str1[i]);
    temp1[i]='\0';
        
    for(i=0;str2[i]!='\0';i++)
        temp2[i]=tolower(str2[i]);
    temp2[i]='\0';
        
    return (strncmp( temp1, temp2, len) );
 
}
 
char * strdup (char *instr)
{
    char * temp = calloc(strlen(instr)+1, 1);
         
    if (temp)
    {
        strcpy(temp, instr);
    }
         
    return temp;
}

#endif /* defined(macos_pre10) */
