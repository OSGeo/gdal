#ifndef __NETCDFLAYERSG_H__
#define __NETCDFLAYERSG_H__
#include "netcdfsg.h"
#include "ogr_core.h"

namespace nccfdriver
{
	OGRwkbGeometryType RawToOGR(geom_t type, int axis_count);

	geom_t OGRtoRaw(OGRwkbGeometryType type);

	bool OGRHasZandSupported(OGRwkbGeometryType type);

}

#endif
