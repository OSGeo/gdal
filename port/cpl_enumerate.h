/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Collection enumerator
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_ENUMERATE_H
#define CPL_ENUMERATE_H

#include <cstddef>   // size_t
#include <iterator>  // std::begin(), std::end()
#include <utility>   // std::declval(), std::pair

namespace cpl
{

//! @cond Doxygen_Suppress

template <class T> class Enumerator
{
  public:
    using TIter = decltype(std::begin(std::declval<T &>()));

    class iterator
    {
      public:
        explicit inline iterator(const TIter &it) : m_iter(it)
        {
        }

        inline bool operator==(const iterator &other) const
        {
            return m_iter == other.m_iter;
        }

        inline bool operator!=(const iterator &other) const
        {
            return m_iter != other.m_iter;
        }

        inline iterator &operator++()
        {
            ++m_index;
            ++m_iter;
            return *this;
        }

        inline iterator operator++(int)
        {
            iterator before = *this;
            ++(*this);
            return before;
        }

        inline auto operator*() const
        {
            return std::pair<size_t, decltype(*m_iter) &>(m_index, *m_iter);
        }

      private:
        TIter m_iter;
        size_t m_index = 0;
    };

    explicit inline Enumerator(T &iterable) : m_iterable(iterable)
    {
    }

    inline iterator begin() const
    {
        return iterator(std::begin(m_iterable));
    }

    inline iterator end() const
    {
        // We could initialize the m_index to SIZE_T_MAX, but this does not
        // really matter as iterator comparison is done on the underlying
        // iterator and not the index.
        return iterator(std::end(m_iterable));
    }

  private:
    T &m_iterable;
};

//! @endcond

/** Function returning an eumerator whose values are
 * a std::pair of (index: size_t, value: iterable::value_type).
 *
 * This is similar to Python enumerate() function and C++23
 * std::ranges::enumerate() (https://en.cppreference.com/w/cpp/ranges/enumerate_view.html)
 *
 * \since GDAL 3.13
 */
template <class T> inline auto enumerate(T &iterable)
{
    return Enumerator<T>(iterable);
}

}  // namespace cpl

#endif
