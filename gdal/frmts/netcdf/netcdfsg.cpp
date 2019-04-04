// Implementations of netCDF functions used for 
// implementing CF-1.8 convention awareness
//
// Author: wchen329
#include "netcdfsg.h"
#include "netcdf.h"
namespace nccfdriver
{
	Point::~Point()
	{
		delete[] this->values;
	}
}
