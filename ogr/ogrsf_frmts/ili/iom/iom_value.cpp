/* This file is part of the iom project.
 * For more information, please see <http://www.interlis.ch>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


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
: str(value)
, obj(0)
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
