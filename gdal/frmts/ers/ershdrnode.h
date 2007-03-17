
class ERSHdrNode;

class ERSHdrNode 
{
    CPLString osTempReturn;

    void      MakeSpace();

public:
    int    nItemMax;
    int    nItemCount;
    char   **papszItemName;
    char   **papszItemValue;
    ERSHdrNode **papoItemChild;

    ERSHdrNode();
    ~ERSHdrNode();

    int    ParseChildren( FILE *fp );
    int    WriteSelf( FILE *fp, int nIndent );

    const char *Find( const char *pszPath, const char *pszDefault = NULL );
    const char *FindElem( const char *pszPath, int iElem, 
                          const char *pszDefault = NULL );
    ERSHdrNode *FindNode( const char *pszPath );

    void   Set( const char *pszPath, const char *pszValue );

private:
    int    ReadLine( FILE *, CPLString & );
};
