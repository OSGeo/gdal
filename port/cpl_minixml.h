/**********************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Declarations for MiniXML Handler.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2001, Frank Warmerdam
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
 * Revision 1.8  2003/03/27 18:12:41  warmerda
 * Added NULL pszNameSpace support in namespace stripper (all namespaces).
 * Added XML file read/write functions.
 *
 * Revision 1.7  2003/03/24 16:46:48  warmerda
 * added CPLStripXMLNamespace
 *
 * Revision 1.6  2002/11/16 20:38:34  warmerda
 * added support for literals like DOCTYPE
 *
 * Revision 1.5  2002/05/24 04:09:10  warmerda
 * added clone and SetXMLValue functions
 *
 * Revision 1.4  2002/03/05 14:26:57  warmerda
 * expanded tabs
 *
 * Revision 1.3  2002/01/23 20:45:06  warmerda
 * handle <?...?> and comment elements
 *
 * Revision 1.2  2001/12/06 18:13:49  warmerda
 * added CPLAddXMLChild and CPLCreateElmentAndValue
 *
 * Revision 1.1  2001/11/16 15:39:48  warmerda
 * New
 *
 **********************************************************************/

#ifndef _CPL_MINIXML_H_INCLUDED
#define _CPL_MINIXML_H_INCLUDED

#include "cpl_port.h"

CPL_C_START

typedef enum 
{
    CXT_Element = 0,
    CXT_Text = 1,
    CXT_Attribute = 2,
    CXT_Comment = 3,
    CXT_Literal = 4
} CPLXMLNodeType;

typedef struct _CPLXMLNode
{
    CPLXMLNodeType      eType;
    
    char                *pszValue;

    struct _CPLXMLNode  *psNext;
    struct _CPLXMLNode  *psChild;
} CPLXMLNode;


CPLXMLNode CPL_DLL *CPLParseXMLString( const char * );
void       CPL_DLL  CPLDestroyXMLNode( CPLXMLNode * );
CPLXMLNode CPL_DLL *CPLGetXMLNode( CPLXMLNode *poRoot, 
                                   const char *pszPath );
const char CPL_DLL *CPLGetXMLValue( CPLXMLNode *poRoot, 
                                    const char *pszPath, 
                                    const char *pszDefault );
CPLXMLNode CPL_DLL *CPLCreateXMLNode( CPLXMLNode *poParent, 
                                      CPLXMLNodeType eType,
                                      const char *pszText );
char       CPL_DLL *CPLSerializeXMLTree( CPLXMLNode *psNode );
void       CPL_DLL  CPLAddXMLChild( CPLXMLNode *psParent,
                                    CPLXMLNode *psChild );
CPLXMLNode CPL_DLL *CPLCreateXMLElementAndValue( CPLXMLNode *psParent,
                                                 const char *pszName,
                                                 const char *pszValue );
CPLXMLNode CPL_DLL *CPLCloneXMLTree( CPLXMLNode *psTree );
int        CPL_DLL CPLSetXMLValue( CPLXMLNode *psRoot,  const char *pszPath,
                                   const char *pszValue );
void       CPL_DLL CPLStripXMLNamespace( CPLXMLNode *psRoot, 
                                         const char *pszNameSpace, 
                                         int bRecurse );

CPLXMLNode CPL_DLL *CPLParseXMLFile( const char *pszFilename );
int        CPL_DLL CPLSerializeXMLTreeToFile( CPLXMLNode *psTree, 
                                              const char *pszFilename );

CPL_C_END

#endif /* _CPL_MINIXML_H_INCLUDED */
