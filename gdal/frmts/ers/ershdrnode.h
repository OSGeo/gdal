
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

    int    ParseChildren( VSILFILE *fp, int nRecLevel = 0 );
    int    WriteSelf( VSILFILE *fp, int nIndent );

    const char *Find( const char *pszPath, const char *pszDefault = nullptr );
    const char *FindElem( const char *pszPath, int iElem,
                          const char *pszDefault = nullptr );
    ERSHdrNode *FindNode( const char *pszPath );

    void   Set( const char *pszPath, const char *pszValue );

private:
    static int    ReadLine( VSILFILE *, CPLString & );
};
