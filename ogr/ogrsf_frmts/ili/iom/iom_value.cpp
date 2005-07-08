/** @file
 * implementation of object value
 * @defgroup value value related functions
 * @{
 */

#include <iom/iom_p.h>

iom_value::iom_value(IomObject value)
: str(0)
, obj(value)
{
}

iom_value::iom_value(const XMLCh *value)
: obj(0)
, str(value)
{
}


const XMLCh *iom_value::getStr()
{
	return str;
}

IomObject iom_value::getObj()
{
	return obj;
}

/** @}
 */
