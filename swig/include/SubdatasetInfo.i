

%{
#include "gdalsubdatasetinfo.h"
%}

%rename (SubdatasetInfo) GDALSubdatasetInfoShadow;

class GDALSubdatasetInfoShadow {

    public:

%extend {

        GDALSubdatasetInfoShadow(){
            return new GDALSubdatasetInfo();
        }

        ~GDALSubdatasetInfoShadow() {
            GDALDestroySubdatasetInfo(reinterpret_cast<GDALSubdatasetInfoH>(self));
        }

        const char* GetFilenameFromSubdatasetName(const char *pszFileName)
        {
            return GDALSubdatasetInfoGetFileName(reinterpret_cast<GDALSubdatasetInfoH>(self), pszFileName );
        }

        bool IsSubdatasetSyntax(const char *pszFileName)
        {
            return GDALSubdatasetInfoIsSubdatasetSyntax(reinterpret_cast<GDALSubdatasetInfoH>(self), pszFileName );
        }
}
};

%inline %{
GDALSubdatasetInfoShadow* GetSubdatasetInfo(const char *pszFileName)
{
    GDALSubdatasetInfoH info { GDALGetSubdatasetInfo(pszFileName) };

    if( ! info )
    {
      return nullptr;
    }

    return (GDALSubdatasetInfoShadow*)( info );
};
%}
