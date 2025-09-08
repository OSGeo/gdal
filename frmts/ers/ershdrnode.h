#ifndef ERSHDRNODE_H_INCLUDED
#define ERSHDRNODE_H_INCLUDED

#include "cpl_string.h"

class ERSHdrNode;

class ERSHdrNode
{
    CPLString osTempReturn{};

    void MakeSpace();

  public:
    int nItemMax = 0;
    int nItemCount = 0;
    char **papszItemName = nullptr;
    char **papszItemValue = nullptr;
    ERSHdrNode **papoItemChild = nullptr;

    ERSHdrNode() = default;
    ~ERSHdrNode();

    int ParseHeader(VSILFILE *fp);
    int ParseChildren(VSILFILE *fp, int nRecLevel = 0);
    int WriteSelf(VSILFILE *fp, int nIndent);

    const char *Find(const char *pszPath, const char *pszDefault = nullptr);
    const char *FindElem(const char *pszPath, int iElem,
                         const char *pszDefault = nullptr);
    ERSHdrNode *FindNode(const char *pszPath);

    void Set(const char *pszPath, const char *pszValue);

  private:
    static int ReadLine(VSILFILE *, CPLString &);

    CPL_DISALLOW_COPY_ASSIGN(ERSHdrNode)
};

#endif /* ERSHDRNODE_H_INCLUDED */
