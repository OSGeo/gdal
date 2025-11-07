/******************************************************************************
 *
 * Project:  GDAL SWIG Interface
 * Purpose:  GDAL XML SWIG Interface declarations.
 * Author:   Tamas Szekeres (szekerest@gmail.com)
 *
 ******************************************************************************
 * Copyright (c) 2005, Tamas Szekeres
 *
 * SPDX-License-Identifier: MIT
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

    CPLXMLNode(CPLXMLNodeType eType, const char *pszText )
    {
        return CPLCreateXMLNode(NULL, eType, pszText);
    }

    ~CPLXMLNode()
    {
        CPLDestroyXMLNode( self );
    }

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
