#ifndef WEATHER_H
#define WEATHER_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "meta.h"

void FreeUglyString (UglyStringType * ugly);

int ParseUglyString (UglyStringType * ugly, char *wxData, int simpleVer);

void PrintUglyString (UglyStringType *ugly);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* WEATHER_H */
