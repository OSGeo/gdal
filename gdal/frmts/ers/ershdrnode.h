
class ERSHdrNode;

class ERSHdrNode 
{
    CPLString osTempReturn;

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

private:
    int    ReadLine( FILE *, CPLString & );
};
