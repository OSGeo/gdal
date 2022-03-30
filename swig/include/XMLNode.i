/******************************************************************************
 * $Id$
 *
 * Project:  GDAL SWIG Interface
 * Purpose:  GDAL XML SWIG Interface declarations.
 * Author:   Tamas Szekeres (szekerest@gmail.com)
 *
 ******************************************************************************
 * Copyright (c) 2005, Tamas Szekeres
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
 *****************************************************************************/

%rename (XMLNodeType) CPLXMLNodeType;
typedef enum
{
    CXT_Element = 0,
    CXT_Text = 1,
    CXT_Attribute = 2,
    CXT_Comment = 3,
    CXT_Literal = 4
} CPLXMLNodeType;

%rename (XMLNode) CPLXMLNode;
%rename (Type) CPLXMLNode::eType;
%rename (Value) CPLXMLNode::pszValue;
%rename (Next) CPLXMLNode::psNext;
%rename (Child) CPLXMLNode::psChild;
typedef struct CPLXMLNode
{
%immutable;
    CPLXMLNodeType      eType;
    char                *pszValue;
    struct CPLXMLNode *psNext;
    struct CPLXMLNode *psChild;
%mutable;
} CPLXMLNode;

%extend CPLXMLNode
{
    CPLXMLNode(const char *pszString)
    {
        return CPLParseXMLString( pszString );
    }

    /* Interface method added for GDAL 1.7.0 */
    CPLXMLNode(CPLXMLNodeType eType, const char *pszText )
    {
        return CPLCreateXMLNode(NULL, eType, pszText);
    }

    ~CPLXMLNode()
    {
        CPLDestroyXMLNode( self );
    }

    /* Interface method added for GDAL 1.7.0 */
#ifdef SWIGJAVA
    %newobject ParseXMLFile;
    static CPLXMLNode* ParseXMLFile( const char *pszFilename )
    {
        return CPLParseXMLFile(pszFilename);
    }
#endif

#if defined(SWIGJAVA) || defined(SWIGCSHARP)
    retStringAndCPLFree *SerializeXMLTree( )
#else
    char *SerializeXMLTree( )
#endif
    {
        return CPLSerializeXMLTree( self );
    }

    /* Interface method added for GDAL 1.7.0 */
#if defined(SWIGJAVA) || defined(SWIGCSHARP)
    retStringAndCPLFree * toString()
    {
        return CPLSerializeXMLTree( self );
    }
#endif

    CPLXMLNode *SearchXMLNode( const char *pszElement )
    {
        return CPLSearchXMLNode(self, pszElement);
    }

    CPLXMLNode *GetXMLNode( const char *pszPath )
    {
        return CPLGetXMLNode( self, pszPath );
    }

    const char *GetXMLValue( const char *pszPath,
                            const char *pszDefault )
    {
        return CPLGetXMLValue( self, pszPath, pszDefault );
    }

    // For Java, I don't want to deal with ownership issues,
    // so I just clone.
#ifdef SWIGJAVA
    %apply Pointer NONNULL {CPLXMLNode *psChild};
    void AddXMLChild( CPLXMLNode *psChild )
    {
        CPLAddXMLChild( self, CPLCloneXMLTree(psChild) );
    }
    %clear CPLXMLNode *psChild;

    %apply Pointer NONNULL {CPLXMLNode *psNewSibling};
    void AddXMLSibling( CPLXMLNode *psNewSibling )
    {
        CPLAddXMLSibling( self, CPLCloneXMLTree(psNewSibling) );
    }
    %clear CPLXMLNode *psNewSibling;
#else
    void AddXMLChild( CPLXMLNode *psChild )
    {
        CPLAddXMLChild( self, psChild );
    }

    int RemoveXMLChild( CPLXMLNode *psChild )
    {
        return CPLRemoveXMLChild( self, psChild );
    }

    void AddXMLSibling( CPLXMLNode *psNewSibling )
    {
        CPLAddXMLSibling( self, psNewSibling );
    }

    CPLXMLNode *CreateXMLElementAndValue( const char *pszName,
                                         const char *pszValue )
    {
        return CPLCreateXMLElementAndValue( self, pszName, pszValue );
    }

    %newobject CloneXMLTree;
    CPLXMLNode *CloneXMLTree( CPLXMLNode *psTree )
    {
        return CPLCloneXMLTree( psTree );
    }
#endif

    /* Interface method added for GDAL 1.7.0 */
    %newobject Clone;
    CPLXMLNode *Clone()
    {
        return CPLCloneXMLTree( self );
    }

    int SetXMLValue( const char *pszPath,
                    const char *pszValue )
    {
        return CPLSetXMLValue( self,  pszPath, pszValue );
    }

    void StripXMLNamespace( const char *pszNamespace,
                           int bRecurse )
    {
        CPLStripXMLNamespace( self, pszNamespace, bRecurse );
    }
}
