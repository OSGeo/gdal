#ifndef HAZARD_H
#define HAZARD_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "meta.h"

void FreeHazardString (HazardStringType * haz);

void ParseHazardString (HazardStringType * haz, char *data, int simpleVer);

void PrintHazardString (HazardStringType * haz);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
