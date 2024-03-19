#ifndef JSON_UTILS_H_INCLUDED
#define JSON_UTILS_H_INCLUDED

#include "cpl_json.h"

/**
 * Get or create CPLJSONObject.
 * @param  oParent Parent CPLJSONObject.
 * @param  osKey  Key name.
 * @return         CPLJSONObject class instance.
 */
static CPLJSONObject GetOrCreateJSONObject(CPLJSONObject &oParent,
                                           const std::string &osKey)
{
    CPLJSONObject oChild = oParent[osKey];
    if (oChild.IsValid() && oChild.GetType() != CPLJSONObject::Type::Object)
    {
        oParent.Delete(osKey);
        oChild.Deinit();
    }

    if (!oChild.IsValid())
    {
        oChild = CPLJSONObject();
        oParent.Add(osKey, oChild);
    }
    return oChild;
}

#endif /* JSON_UTILS_H_INCLUDED */
