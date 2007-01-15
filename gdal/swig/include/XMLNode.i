/******************************************************************************
 * $Id$
 *
 * Name:     XMLNode.i
 * Project:  GDAL SWIG Interface
 * Purpose:  GDAL XML SWIG Interface declarations.
 * Author:   Tamas Szekeres (szekerest@gmeil.com)
 *

 *
 * $Log$
 * Revision 1.1  2006/11/25 21:24:25  tamas
 * Added XMLNode support for C#
 *
 *
*/

%rename (XMLNodeType) CPLXMLNodeType;
typedef enum 
{
    CXT_Element = 0,
    CXT_Text = 1,
    CXT_Attribute = 2,  
    CXT_Comment = 3,    
    CXT_Literal = 4     
} CPLXMLNodeType;

%rename (XMLNode) _CPLXMLNode;
%rename (Type) _CPLXMLNode::eType;
%rename (Value) _CPLXMLNode::pszValue;
%rename (Next) _CPLXMLNode::psNext;
%rename (Child) _CPLXMLNode::psChild;
typedef struct _CPLXMLNode
{
%immutable;
    CPLXMLNodeType      eType;       
    char                *pszValue;   
    struct _CPLXMLNode *psNext;     
    struct _CPLXMLNode *psChild;
%mutable;  
} CPLXMLNode;

%extend CPLXMLNode 
{
    CPLXMLNode(const char *pszString) 
    {
        return CPLParseXMLString( pszString );     
    }

    ~CPLXMLNode() 
    {
        CPLDestroyXMLNode( self );
    }
    
    char *SerializeXMLTree( )
    {
        return CPLSerializeXMLTree( self );
    }
    
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
    
    void AddXMLChild( CPLXMLNode *psChild )
    {
        return CPLAddXMLChild( self, psChild );
    }
    
    int RemoveXMLChild( CPLXMLNode *psChild )
    {
        return CPLRemoveXMLChild( self, psChild );
    }
    
    void AddXMLSibling( CPLXMLNode *psNewSibling )
    {
        return CPLAddXMLSibling( self, psNewSibling );
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
