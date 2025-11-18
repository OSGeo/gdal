/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Just-in-time compilation of C code
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_JIT_CPP_H
#define GDAL_JIT_CPP_H

// This header requires C++17
// _MSC_VER >= 1920 : Visual Studio >= 2019
#if defined(__cplusplus) && !defined(CPL_SUPRESS_CPLUSPLUS) &&                 \
    (__cplusplus >= 201703L || _MSC_VER >= 1920)

#include "cpl_port.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

//! @cond Doxygen_Suppress
// Not directly aimed at being used. Use the GDALGetJITFunction() helper
// instead
typedef void GDALJIT;
std::shared_ptr<GDALJIT> CPL_DLL
GDALCompileAndLoad(const std::string &cCode, const std::string &functionName,
                   uint64_t &functionAddress, std::string *posDisassembledCode,
                   bool *pbHasVeclib);

//! @endcond

/** Return which JIT engines are available.
 *
 * At time of writing, the return value may be an empty vector or a vector
 * with "LLVM".
 *
 * @since 3.13
 */
std::vector<std::string> CPL_DLL GDALGetJITEngines();

/** Returns an executable function from the provided C code.
 *
 * @param cCode Valid C code that has a function called functionName and
 *              whose signature must be FunctionSignature.
 *              The C code must not use any \#include statement.
 * @param functionName Entry point in the C code
 * @param papszOptions NULL-terminated list of options, or NULL. Unused for now.
 * @param[out] posDisassembledCode Pointer to a string that must receive the
 *                                 disassembly of the compiled code, or nullptr
 *                                 if not useful. Some targets (like Windows)
 *                                 do not currently support disassembling, and
 *                                 will set it to empty.
 * @param[out] pbHasVeclib Pointer to a boolean to indicate if a math vector lib
 *                         has been found, or nullptr if not useful.
 * @return a std::function of signature FunctionSignature corresponding to the
 *         entry point in the C code (may be invalid in case of error.)
 * @since 3.13
 */
template <typename FunctionSignature>
std::function<FunctionSignature> CPL_DLL GDALGetJITFunction(
    const std::string &cCode, const std::string &functionName,
    CSLConstList papszOptions = nullptr,
    std::string *posDisassembledCode = nullptr, bool *pbHasVeclib = nullptr)
{
    (void)papszOptions;

    uint64_t functionAddress = 0;
    std::shared_ptr<GDALJIT> jitHolder = GDALCompileAndLoad(
        cCode, functionName, functionAddress, posDisassembledCode, pbHasVeclib);
    // cppcheck-suppress knownConditionTrueFalse
    if (!jitHolder || !functionAddress)
        return {};

    using FnPtrType = typename std::add_pointer<FunctionSignature>::type;
    FnPtrType fnPtr = reinterpret_cast<FnPtrType>(functionAddress);

    // We capture the jitHolder by value, because, as it is a shared_ptr, the
    // returned std::function will keep it alive. Which is very important
    // because the raw pointer fnPtr is only valid while *jitHolder is.
    return [fnPtr, jitHolder](auto &&...args)
    { return fnPtr(std::forward<decltype(args)>(args)...); };
}

#endif

#endif  // GDAL_JIT_CPP_H
