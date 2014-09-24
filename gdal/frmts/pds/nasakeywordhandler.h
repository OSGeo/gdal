/************************************************************************/
/* ==================================================================== */
/*                          NASAKeywordHandler                          */
/* ==================================================================== */
/************************************************************************/

class NASAKeywordHandler
{
    char     **papszKeywordList;

    CPLString osHeaderText;
    const char *pszHeaderNext;

    void    SkipWhite();
    int     ReadWord( CPLString &osWord );
    int     ReadPair( CPLString &osName, CPLString &osValue );
    int     ReadGroup( const char *pszPathPrefix );

public:
    NASAKeywordHandler();
    ~NASAKeywordHandler();

    int     Ingest( VSILFILE *fp, int nOffset );

    const char *GetKeyword( const char *pszPath, const char *pszDefault );
    char **GetKeywordList();
};
