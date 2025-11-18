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

#include "gdal_jit_cpp.h"

#include "cpl_error.h"

//! @cond Doxygen_Suppress

#if !defined(HAVE_JIT)

std::vector<std::string> GDALGetJITEngines()
{
    return {};
}

std::shared_ptr<void> GDALCompileAndLoad(const std::string &,
                                         const std::string &, uint64_t &,
                                         std::string *, bool *)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "GDALCompileAndLoad() not available due to GDAL not being built "
             "against libclang-cpp and libLLVM");
    return nullptr;
}

#else

#include "cpl_string.h"

#include <sstream>

#ifdef HAVE_GNU_LIBC_VERSION_H
#include <gnu/libc-version.h>
#endif

#define ENABLE_DISASSEMBLY

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#pragma GCC diagnostic ignored "-Wextra-semi"
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wredundant-move"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcomma"
#pragma clang diagnostic ignored "-Wdeprecated-copy-with-dtor"
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
#pragma clang diagnostic ignored "-Winconsistent-missing-destructor-override"
#pragma clang diagnostic ignored "-Wshadow-field"
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#pragma clang diagnostic ignored "-Wsuggest-destructor-override"
#pragma clang diagnostic ignored "-Wweak-vtables"
#else
// LLVM-17 on /usr/lib/llvm-17/include/clang/AST/TypeLoc.h
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif
#endif

#ifdef _MSC_VER
// This is a stripped down version of clang/Driver/Options.h that avoids
// including Options.in, on which the compiler breaks on the use of the OPTION macro
#define LLVM_CLANG_DRIVER_OPTIONS_H
#include <llvm/Option/OptTable.h>
#include <llvm/Option/Option.h>
const llvm::opt::OptTable &getDriverOptTable();

#pragma warning(push)
#pragma warning(disable : 4267)

#endif

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <clang/CodeGen/CodeGenAction.h>  // EmitLLVMOnlyAction()
#include <clang/Driver/Compilation.h>
#include <clang/Driver/Driver.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>  // ThreadSafeContext, ThreadSafeModule
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#if LLVM_VERSION_MAJOR >= 16
#include <llvm/TargetParser/Host.h>
#else
#include <llvm/Support/Host.h>
#endif
#include "llvm/Support/MemoryBuffer.h"
#include <llvm/Support/TargetSelect.h>  // InitializeNativeTarget(), etc.

#ifdef ENABLE_DISASSEMBLY

// Below is for disassembling only
#include "llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/ObjectTransformLayer.h"
#include "llvm/ExecutionEngine/Orc/Layer.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Object/ObjectFile.h"
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Support/raw_ostream.h>

#endif  // ENABLE_DISASSEMBLY

#ifdef _MSC_VER
#pragma warning(pop)
#endif

/************************************************************************/
/*                       GDALCompileCCodeToIR()                         */
/************************************************************************/

// Given C code (not using any #include), return a LLVM Intermediate
// Representation (IR) module, or nullptr in case of error.

static std::unique_ptr<llvm::Module>
GDALCompileCCodeToIR(llvm::LLVMContext &ctx, bool bHasVeclib,
                     const std::string &cCode, bool bDebug)
{
    auto invocation = std::make_shared<clang::CompilerInvocation>();

#if LLVM_VERSION_MAJOR >= 21
    clang::CompilerInstance compInst(invocation);
    clang::DiagnosticOptions diagOpts;
#else
    clang::CompilerInstance compInst;
    compInst.setInvocation(invocation);
    auto diagOpts = llvm::makeIntrusiveRefCnt<clang::DiagnosticOptions>();
#endif

    auto inMemFS = llvm::makeIntrusiveRefCnt<llvm::vfs::InMemoryFileSystem>();
    inMemFS->addFile("input.c", 0, llvm::MemoryBuffer::getMemBuffer(cCode));

    auto fileMgr = std::make_unique<clang::FileManager>(
        clang::FileSystemOptions(), inMemFS);
    compInst.setFileManager(fileMgr.release());

    // Create DiagnosticsEngine instance
    auto diags = clang::CompilerInstance::createDiagnostics(
#if LLVM_VERSION_MAJOR >= 20
        *inMemFS,
#endif
#if LLVM_VERSION_MAJOR >= 21
        diagOpts
#else
        diagOpts.get()
#endif
    );

    compInst.setDiagnostics(diags.get());

    const std::string hostCPU(llvm::sys::getHostCPUName());
    const std::string tripleStr(llvm::sys::getProcessTriple());
    if (bDebug)
    {
        CPLErrorOnce(CE_Warning, CPLE_AppDefined,
                     "GDAL_JIT: hostCPU = %s, triple = %s", hostCPU.c_str(),
                     tripleStr.c_str());
    }
    CPLDebugOnce("GDAL_JIT", "hostCPU = %s, triple = %s", hostCPU.c_str(),
                 tripleStr.c_str());

    clang::driver::Driver driver("clang", tripleStr, compInst.getDiagnostics(),
                                 "clang LLVM compiler", inMemFS.get());

    // clang-format off
    std::vector<const char*> args {
        "clang",
        "-O2",
        "-emit-llvm",
        "-Xclang",          "-target-cpu",
        "-Xclang",          hostCPU.c_str(),
        "-x",               "c",
        "-c",               "input.c",
        "-fno-math-errno",
    };
    // clang-format on

    if (CPLIsDebugEnabled())
    {
        args.push_back("-Wall");
        args.push_back("-Wextra");
    }
    std::string osFVecLib;  // keep in that scope
    if (bHasVeclib)
    {
        const char *pszVecLib =
            CPLGetConfigOption("GDAL_JIT_VECLIB_TYPE", nullptr);
        if (pszVecLib)
        {
            osFVecLib = "-fveclib=";
            osFVecLib += pszVecLib;
            args.push_back(osFVecLib.c_str());
        }
        else
        {
#ifdef ACCELERATE_LIBRARY
            args.push_back("-fveclib=Accelerate");
#elif defined(_WIN32)
            args.push_back("-fveclib=SVML");
#else
            args.push_back("-fveclib=libmvec");
#endif
        }
    }

    auto compilation = std::unique_ptr<clang::driver::Compilation>(
        driver.BuildCompilation(llvm::ArrayRef<const char *>(args)));
    if (!compilation)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to build compilation");
        return nullptr;
    }

    const auto &jobs = compilation->getJobs();
    if (jobs.size() != 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "compilation->getJobs() did no return a single job");
        return nullptr;
    }

    const auto &job = *jobs.begin();

    {
        std::string osFullCompLine;
        for (const auto *arg : job.getArguments())
        {
            if (!osFullCompLine.empty())
                osFullCompLine += ' ';
            osFullCompLine += arg;
        }
        if (bDebug)
        {
            CPLErrorOnce(CE_Warning, CPLE_AppDefined, "GDAL_JIT: %s",
                         osFullCompLine.c_str());
        }
#ifdef DEBUG_VERBOSE
        CPLDebug("GDAL_JIT", "%s", osFullCompLine.c_str());
#endif
    }

    clang::CompilerInvocation::CreateFromArgs(*invocation, job.getArguments(),
                                              compInst.getDiagnostics());

    clang::EmitLLVMOnlyAction action(&ctx);
    if (!compInst.ExecuteAction(action))
        return nullptr;

    return action.takeModule();
}

/************************************************************************/
/*                     GDALGetObjectDisassembly()                       */
/************************************************************************/

#ifdef ENABLE_DISASSEMBLY

// Given a memory buffer containing a linked object, return a string with
// its disassembly.

static std::string
GDALGetObjectDisassembly(const llvm::MemoryBuffer &objectBuffer)
{
    // Stongly inspired from https://github.com/llvm/llvm-project/blob/4822f4986fae9bb212e2f35e29839bbd9fb26bea/llvm/lib/MC/MCDisassembler/Disassembler.cpp

    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllDisassemblers();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    auto objectFilePtr = llvm::object::ObjectFile::createObjectFile(
        objectBuffer.getMemBufferRef());
    if (!objectFilePtr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "createObjectFile() failed: %s",
                 llvm::toString(objectFilePtr.takeError()).c_str());
        return {};
    }
    const auto &objectFile = **objectFilePtr;
    const auto triple = objectFile.makeTriple();

#if LLVM_VERSION_MAJOR >= 22
    const auto &tripleOrStr = triple;
#else
    const std::string tripleStr = triple.str();
    const auto &tripleOrStr = tripleStr;
#endif

    std::string err;
    const llvm::Target *target =
        llvm::TargetRegistry::lookupTarget(tripleOrStr, err);
    if (!target)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "lookupTarget() failed: %s",
                 err.c_str());
        return {};
    }

    auto regInfo = std::unique_ptr<llvm::MCRegisterInfo>(
        target->createMCRegInfo(tripleOrStr));
    if (!regInfo)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "createMCRegInfo() failed");
        return {};
    }

    llvm::MCTargetOptions targetOptions;
    auto asmInfo = std::unique_ptr<llvm::MCAsmInfo>(
        target->createMCAsmInfo(*regInfo, tripleOrStr, targetOptions));
    if (!asmInfo)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "createMCAsmInfo() failed");
        return {};
    }

    auto instrInfo =
        std::unique_ptr<llvm::MCInstrInfo>(target->createMCInstrInfo());
    if (!instrInfo)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "createMCInstrInfo() failed");
        return {};
    }

    auto subtargetInfo =
        std::unique_ptr<llvm::MCSubtargetInfo>(target->createMCSubtargetInfo(
            tripleOrStr, llvm::StringRef(), llvm::StringRef()));
    if (!subtargetInfo)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "createMCSubtargetInfo() failed");
        return {};
    }

    llvm::MCContext ctx(triple, asmInfo.get(), regInfo.get(),
                        subtargetInfo.get(),
                        /* SourceMgr */ nullptr, &targetOptions,
                        /* DoAutoReset */ false,
                        /* CompilationDir */ llvm::StringRef());

    auto disassembler = std::unique_ptr<llvm::MCDisassembler>(
        target->createMCDisassembler(*subtargetInfo, ctx));
    if (!disassembler)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "createMCDisassembler() failed");
        return {};
    }

    auto relocInfo = std::unique_ptr<llvm::MCRelocationInfo>(
        target->createMCRelocationInfo(tripleOrStr, ctx));
    if (!relocInfo)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "createMCRelocationInfo() failed");
        return {};
    }

    auto symbolizer =
        std::unique_ptr<llvm::MCSymbolizer>(target->createMCSymbolizer(
            tripleOrStr, /* GetOpInfo = */ nullptr,
            /* SymbolLookUp = */ nullptr, /* DisInfo = */ nullptr, &ctx,
            std::move(relocInfo)));
    if (!symbolizer)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "createMCSymbolizer() failed");
        return {};
    }

    disassembler->setSymbolizer(std::move(symbolizer));

    auto instrPrinter = std::unique_ptr<llvm::MCInstPrinter>(
        target->createMCInstPrinter(triple, asmInfo->getAssemblerDialect(),
                                    *asmInfo, *instrInfo, *regInfo));
    if (!instrPrinter)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "createMCInstPrinter() failed");
        return {};
    }

    std::stringstream ss;
    llvm::raw_os_ostream rawOsOstream(ss);

    for (const auto &section : objectFile.sections())
    {
        if (!section.isText())
            continue;

        auto contentsOrErr = section.getContents();
        if (!contentsOrErr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "section.getContents() failed with %s",
                     llvm::toString(contentsOrErr.takeError()).c_str());
            return {};
        }

        const llvm::StringRef bytesAsStr = *contentsOrErr;
        const uint64_t sectionAddress = section.getAddress();
        const llvm::ArrayRef<uint8_t> bytes(
            reinterpret_cast<const uint8_t *>(bytesAsStr.data()),
            bytesAsStr.size());

        // Iterate over bytes in the section and dump instructions
        // Inspired from https://github.com/llvm/llvm-project/blob/4822f4986fae9bb212e2f35e29839bbd9fb26bea/llvm/lib/MC/MCDisassembler/Disassembler.cpp#L194
        size_t idx = 0;
        while (idx < bytes.size())
        {
            llvm::MCInst instruction;
            uint64_t instructionSize = 0;

            const auto instructionAddr = sectionAddress + idx;
            const auto status = disassembler->getInstruction(
                instruction, instructionSize, bytes.slice(idx), instructionAddr,
                llvm::nulls());
            if (status == llvm::MCDisassembler::Success)
            {
                rawOsOstream.write_hex(instructionAddr);
                rawOsOstream << ":\t";

                instrPrinter->printInst(&instruction, instructionAddr, "",
                                        *subtargetInfo, rawOsOstream);

#if defined(__x86_64__) || defined(_M_X64)
                constexpr bool isX86 = true;
#else
                constexpr bool isX86 = false;
#endif
                if constexpr (isX86)
                {
                    // Display absolute address for jumps and calls
                    // Note: that would build for other architectures,
                    // but this is really x86 specific
                    const unsigned opCode = instruction.getOpcode();
                    const llvm::StringRef instName = instrInfo->getName(opCode);

                    if (cpl::starts_with(instName.str(), "J") ||
                        cpl::starts_with(instName.str(), "CALL"))
                    {
                        if (instruction.getNumOperands() > 0)
                        {
                            const auto &operand = instruction.getOperand(0);
                            if (operand.isImm())
                            {
                                const int64_t nJumpOffset = operand.getImm();
                                const uint64_t targetAddr = instructionAddr +
                                                            instructionSize +
                                                            nJumpOffset;
                                rawOsOstream << " <0x"
                                             << llvm::format_hex(targetAddr, 10)
                                             << ">";
                            }
                        }
                    }
                }

                rawOsOstream << "\n";

                idx += instructionSize;
            }
            else
            {
                rawOsOstream << "Could not disassemble one instruction. "
                                "Interrupting disassembly\n";
                break;
            }
        }
    }

    rawOsOstream.flush();
    return ss.str();
}

#endif  // ENABLE_DISASSEMBLY

/************************************************************************/
/*                        GDALGetJITEngines()                           */
/************************************************************************/

std::vector<std::string> GDALGetJITEngines()
{
    return {std::string("LLVM")};
}

/************************************************************************/
/*                        GDALCompileAndLoad()                          */
/************************************************************************/

std::shared_ptr<void> GDALCompileAndLoad(const std::string &cCode,
                                         const std::string &functionName,
                                         uint64_t &functionAddress,
                                         std::string *posDisassembledCode,
                                         bool *pbHasVeclib)
{
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    if (posDisassembledCode)
        posDisassembledCode->clear();
    if (pbHasVeclib)
        *pbHasVeclib = false;

#ifdef ENABLE_DISASSEMBLY
    // When disassembly is requeted, collect the linked object file in a memory
    // buffer. There should normally be only one.
    std::vector<std::unique_ptr<llvm::MemoryBuffer>> capturedObjectBuffers;

    // Wrap a regular llvm::orc::ObjectLinkingLayer instance in a specialized
    // ObjectTransformLayer to capture the linked object.
    const auto objectLinkingLayerCreator =
        [&capturedObjectBuffers](
            llvm::orc::ExecutionSession &executionSession
#if LLVM_VERSION_MAJOR < 21
            ,
            const llvm::Triple &
#endif
            ) -> llvm::Expected<std::unique_ptr<llvm::orc::ObjectLayer>>
    {
        auto baseLinkingLayer =
            std::make_shared<llvm::orc::ObjectLinkingLayer>(executionSession);
        return std::make_unique<llvm::orc::ObjectTransformLayer>(
            executionSession, *(baseLinkingLayer.get()),
            [&capturedObjectBuffers,
             baseLinkingLayer](std::unique_ptr<llvm::MemoryBuffer> memBuffer)
                -> llvm::Expected<std::unique_ptr<llvm::MemoryBuffer>>
            {
                capturedObjectBuffers.push_back(
                    llvm::MemoryBuffer::getMemBufferCopy(memBuffer->getBuffer(),
                                                         "<captured-obj>"));
                return std::move(memBuffer);
            });
    };
#endif  // ENABLE_DISASSEMBLY

    auto jitBuilder = llvm::orc::LLJITBuilder();

#ifdef ENABLE_DISASSEMBLY
#ifdef _WIN32
    // We could potentially make that working but there is a subtelty on
    // how objectLinkingLayerCreator should be done.
    // Cf https://github.com/llvm/llvm-project/blob/93b20e7d1f1d72c19c450a81ef5d84376e474b77/llvm/lib/ExecutionEngine/Orc/LLJIT.cpp#L815
    // and https://github.com/llvm/llvm-project/blob/93b20e7d1f1d72c19c450a81ef5d84376e474b77/llvm/lib/ExecutionEngine/Orc/LLJIT.cpp#L840
    constexpr bool bDisassemblerWorking = false;
#else
    constexpr bool bDisassemblerWorking = true;
#endif
    if constexpr (bDisassemblerWorking)
    {
        if (posDisassembledCode)
        {
            jitBuilder.setObjectLinkingLayerCreator(objectLinkingLayerCreator);
        }
    }
#endif  // ENABLE_DISASSEMBLY

    // Instantiate a just-in-time compiler
    auto jitOrErr = jitBuilder.create();
    if (!jitOrErr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Failed to create LLJIT: %s",
                 llvm::toString(jitOrErr.takeError()).c_str());
        return nullptr;
    }
    auto jit = std::shared_ptr<llvm::orc::LLJIT>(std::move(*jitOrErr));

    const bool bDebug = CPLTestBool(CPLGetConfigOption("GDAL_JIT_DEBUG", "NO"));

    // Detect if a math vectorizing library is available
    bool bHasVeclib = false;
#if defined(ACCELERATE_LIBRARY) || defined(_WIN32) ||                          \
    (defined(__linux) && defined(HAVE_GNU_LIBC_VERSION_H) &&                   \
     (defined(__x86_64__) || defined(__i386__) ||                              \
      (defined(__aarch64__) && defined(USE_NEON_OPTIMIZATIONS))))

#if defined(HAVE_GNU_LIBC_VERSION_H) && defined(__aarch64__) &&                \
    defined(USE_NEON_OPTIMIZATIONS)
    // From clang documentation (and experimentation), -fveclib=libmvec
    // is only compatible of glibc >= 2.40 on aarch64
    static const bool bCompatibleOfVecLib = [bDebug]()
    {
        const char *pszGLibCVersion = gnu_get_libc_version();
        const CPLStringList aosVersion(
            CSLTokenizeString2(pszGLibCVersion, ".", 0));
        const bool bRet =
            (aosVersion.size() >= 2 &&
             (atoi(aosVersion[0]) > 2 ||
              (atoi(aosVersion[0]) == 2 && atoi(aosVersion[1]) >= 40)));
        if (!bRet)
        {
            if (bDebug)
            {
                CPLErrorOnce(CE_Warning, CPLE_AppDefined,
                             "GDAL_JIT: glibc version = %s, but "
                             "-fveclib=libmvec only "
                             "compatible of glibc >= 2.40",
                             pszGLibCVersion);
            }
            CPLDebugOnce("GDAL_JIT",
                         "glibc version = %s, but -fveclib=libmvec only "
                         "compatible of glibc >= 2.40",
                         pszGLibCVersion);
        }
        return bRet;
    }();
#else
    constexpr bool bCompatibleOfVecLib = true;
#endif

    // cppcheck-suppress knownConditionTrueFalse
    const char *pszUseVecLib =
        bCompatibleOfVecLib ? CPLGetConfigOption("GDAL_JIT_USE_VECLIB", "AUTO")
                            : "";
    if (bCompatibleOfVecLib &&
        (EQUAL(pszUseVecLib, "AUTO") || CPLTestBool(pszUseVecLib)))
    {
#ifdef ACCELERATE_LIBRARY
        // On MacOS
        const char *pszName = ACCELERATE_LIBRARY;
#elif defined(_WIN32)
        const char *pszName = "svml_dispmd.dll";
#else
        // On GNU/Linux systems
        const char *pszName = "libmvec.so.1";
#endif
        auto dllGenerator = llvm::orc::DynamicLibrarySearchGenerator::Load(
            CPLGetConfigOption("GDAL_JIT_VECLIB_PATH", pszName),
            jit->getDataLayout().getGlobalPrefix());
        if (dllGenerator)
        {
            if (bDebug)
            {
                CPLErrorOnce(CE_Warning, CPLE_AppDefined,
                             "GDAL_JIT: Vector math library \"%s\" loaded",
                             pszName);
            }
            CPLDebugOnce("GDAL_JIT", "Vector math library \"%s\" loaded",
                         pszName);
            jit->getMainJITDylib().addGenerator(std::move(*dllGenerator));
            bHasVeclib = true;
        }
        else
        {
            if (bDebug)
            {
                CPLErrorOnce(CE_Warning, CPLE_AppDefined,
                             "GDAL_JIT: Cannot load \"%s\": %s", pszName,
                             llvm::toString(dllGenerator.takeError()).c_str());
            }
            CPLDebugOnce("GDAL_JIT", "Cannot load \"%s\": %s", pszName,
                         llvm::toString(dllGenerator.takeError()).c_str());
        }
    }
#endif

    if (pbHasVeclib)
        *pbHasVeclib = bHasVeclib;

    auto llvmContext = std::make_unique<llvm::LLVMContext>();

    // Compile the c code
    auto IRModule =
        GDALCompileCCodeToIR(*(llvmContext.get()), bHasVeclib, cCode, bDebug);
    if (!IRModule)
    {
        // Error message emitted by GDALCompileCCodeToIR()
        return nullptr;
    }

    // and add it to the JIT
    if (auto err = jit->addIRModule(llvm::orc::ThreadSafeModule(
            std::move(IRModule),
            llvm::orc::ThreadSafeContext(std::move(llvmContext)))))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "jit->addIRModule() failed: %s",
                 llvm::toString(std::move(err)).c_str());
        return nullptr;
    }

    // Get the symbol
    auto symbol = jit->lookup(functionName);
    if (!symbol)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "jit->lookup(\"%s\") failed",
                 functionName.c_str());
        return nullptr;
    }

    // Extract the symbol address
#if LLVM_VERSION_MAJOR >= 15
    functionAddress = symbol->getValue();
#else
    functionAddress = symbol->getAddress();
#endif

#ifdef ENABLE_DISASSEMBLY
    if constexpr (bDisassemblerWorking)
    {
        if (posDisassembledCode && capturedObjectBuffers.size() == 1)
        {
            *posDisassembledCode =
                GDALGetObjectDisassembly(*capturedObjectBuffers[0]);
        }
        else if (posDisassembledCode)
        {
            CPLDebug("GDAL_JIT", "Got %d captured object buffers",
                     static_cast<int>(capturedObjectBuffers.size()));
        }
    }
#endif  // ENABLE_DISASSEMBLY

    // Return a type-erased shared_ptr<llvm::orc::LLJIT>
    return std::static_pointer_cast<GDALJIT>(jit);
}

#if defined(__GNUC__)
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#pragma GCC diagnostic pop
#endif

#endif  // HAVE_JIT

//! @endcond
