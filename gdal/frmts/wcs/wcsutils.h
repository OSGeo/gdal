#include "cpl_string.h"
#include "cpl_minixml.h"
#include <vector>

CPLString String(const char *str);

CPLString URLEncode(CPLString str);

CPLString URLRemoveKey(const char *url, CPLString key);

std::vector<CPLString> Split(const char *value,
                             const char *delim,
                             bool swap_the_first_two = false);

CPLString Join(std::vector<CPLString> array,
               const char *delim,
               bool swap_the_first_two = false);

std::vector<double> Flist(std::vector<CPLString> array,
                          unsigned int from = 0,
                          unsigned int count = 2);

int IndexOf(std::vector<CPLString> array, CPLString str); // index of str in array

std::vector<int> IndexOf(std::vector<CPLString> array, std::vector<CPLString> str); // index of strs in array

bool Contains(std::vector<int> array, int value);

CPLString FromParenthesis(CPLString s);

std::vector<CPLString> ParseSubset(std::vector<CPLString>
                                   subset_array,
                                   CPLString dim);

bool FileIsReadable(CPLString filename);

bool MakeDir(CPLString dirname);
    
bool SetupCache(CPLString &cache_dir,
                bool clear);

bool DeleteEntryFromCache(CPLString cache_dir,
                          CPLString  key,
                          CPLString value);

int FromCache(CPLString cache_dir,
              CPLString &filename,
              CPLString url);

CPLXMLNode *AddSimpleMetaData(char ***metadata,
                              CPLXMLNode *node,
                              CPLString &path,
                              CPLString from,
                              std::vector<CPLString> keys);

CPLString GetKeywords(CPLXMLNode *root,
                      CPLString path,
                      CPLString kw);

CPLString ParseCRS(CPLXMLNode *node);

bool CRS2Projection(CPLString crs,
                    OGRSpatialReference &oSRS,
                    char **projection);

bool CRSImpliesAxisOrderSwap(CPLString crs, bool &swap, char **projection = NULL);
    
std::vector<std::vector<int>> ParseGridEnvelope(CPLXMLNode *node,
                                                bool swap_the_first_two = false);

std::vector<CPLString> ParseBoundingBox(CPLXMLNode *node);
