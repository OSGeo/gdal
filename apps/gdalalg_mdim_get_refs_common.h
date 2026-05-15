/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Shared helpers for "gdal mdim get-refs"
 * Author:   Michael Sumner <mdsumner at gmail.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Michael Sumner <mdsumner at gmail.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef GDALALG_MDIM_GET_REFS_COMMON_INCLUDED
#define GDALALG_MDIM_GET_REFS_COMMON_INCLUDED

#include <cstddef>
#include <cstdint>
#include <vector>

#include "cpl_error.h"

//! @cond Doxygen_Suppress

namespace get_refs
{

/************************************************************************/
/*                          LinearToCoords()                            */
/************************************************************************/
/**
 * Decode a linear chunk index into per-dimension chunk coordinates,
 * using row-major (last dimension varies fastest) ordering.
 *
 * Given a chunk grid of shape n_chunks = [N_0, N_1, ..., N_{k-1}] and a
 * linear index iLinear in [0, product(n_chunks)), this fills coords with
 * the [c_0, c_1, ..., c_{k-1}] such that
 *     iLinear = (((c_0 * N_1) + c_1) * N_2 + c_2) * ... + c_{k-1}.
 *
 * The output is uint64_t to match GDALMDArray::GetRawBlockInfo()'s
 * coordinate-pointer parameter directly — no further conversion needed
 * at the call site.
 *
 * @param iLinear  The flat chunk index. Must be < product(n_chunks);
 *                 callers should bound this with nTotalChunks. No
 *                 internal range check (this is on the hot path).
 * @param n_chunks Per-dimension chunk count. Must be non-empty and have
 *                 no zero entries.
 * @param coords   Output buffer; checked to match size of n_chunks, and filled.
 */
inline void LinearToCoords(size_t iLinear, const std::vector<size_t> &n_chunks,
                           std::vector<uint64_t> &coords)
{
    CPLAssert(coords.size() == n_chunks.size());
    size_t remaining = iLinear;
    for (size_t iDim = n_chunks.size(); iDim-- > 0;)
    {
        coords[iDim] = static_cast<uint64_t>(remaining % n_chunks[iDim]);
        remaining /= n_chunks[iDim];
    }
}

/************************************************************************/
/*                          CoordsToLinear()                            */
/************************************************************************/
/**
 * Encode per-dimension chunk coordinates into a linear chunk index,
 * inverse of LinearToCoords().
 *
 * Currently unused by RunImpl which only needs the decoder direction,
 * this is the pair to LinearToCoords.
 *
 * @param coords    Per-dimension chunk coordinates. Each coords[i] must
 *                  be < n_chunks[i]; not checked.
 * @param n_chunks  Per-dimension chunk count. Same size as coords.
 * @return          The linear index in [0, product(n_chunks)).
 */
inline size_t CoordsToLinear(const std::vector<uint64_t> &coords,
                             const std::vector<size_t> &n_chunks)
{
    CPLAssert(coords.size() == n_chunks.size());
    size_t iLinear = 0;
    for (size_t iDim = 0; iDim < n_chunks.size(); ++iDim)
    {
        iLinear = iLinear * n_chunks[iDim] + static_cast<size_t>(coords[iDim]);
    }
    return iLinear;
}

/************************************************************************/
/*                          ComputeChunkGrid()                          */
/************************************************************************/
/**
 * Compute per-dimension chunk counts by ceil-division:
 *     n_chunks[i] = (dim_size[i] + block[i] - 1) / block[i].
 *
 * Confirmed correct against ZARR, netCDF, and HDF5 drivers (Phase 0
 * evidence log Q1; HDFEOS addendum). Caller must have already verified
 * that all block[i] > 0; this function will divide by zero otherwise.
 *
 * @param dim_size  Per-dimension array size.
 * @param block     Per-dimension block size. block.size() == dim_size.size().
 * @param n_chunks  Output; resized to dim_size.size() and filled.
 * @return          The total chunk count, product(n_chunks).
 */
inline size_t ComputeChunkGrid(const std::vector<uint64_t> &dim_size,
                               const std::vector<uint64_t> &block,
                               std::vector<size_t> &n_chunks)
{
    CPLAssert(dim_size.size() == block.size());
    n_chunks.resize(dim_size.size());
    size_t nTotal = 1;
    for (size_t i = 0; i < dim_size.size(); ++i)
    {
        CPLAssert(block[i] > 0);
        n_chunks[i] =
            static_cast<size_t>((dim_size[i] + block[i] - 1) / block[i]);
        nTotal *= n_chunks[i];
    }
    return nTotal;
}

}  // namespace get_refs

//! @endcond

#endif  // GDALALG_MDIM_GET_REFS_COMMON_INCLUDED
