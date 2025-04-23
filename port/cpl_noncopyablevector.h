/******************************************************************************
 *
 * Project:  CPL
 * Purpose:  Implementation of a std::vector<> without copy capabilities
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_NONCOPYABLEVECOTR_H_INCLUDED
#define CPL_NONCOPYABLEVECOTR_H_INCLUDED

#include <vector>

namespace cpl
{

/** Derived class of std::vector<> without copy capabilities */
template <class T> struct NonCopyableVector : public std::vector<T>
{
    /** Constructor
     * @param siz Initial size of vector.
     */
    explicit inline NonCopyableVector(size_t siz = 0) : std::vector<T>(siz)
    {
    }

    /** Move constructor */
    NonCopyableVector(NonCopyableVector &&) = default;

    /** Move assignment operator */
    NonCopyableVector &operator=(NonCopyableVector &&) = default;

  private:
    NonCopyableVector(const NonCopyableVector &) = delete;
    NonCopyableVector &operator=(const NonCopyableVector &) = delete;
};

}  // namespace cpl

#endif  // CPL_NONCOPYABLEVECOTR_H_INCLUDED
