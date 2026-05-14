#!/usr/bin/env python3

import glob
import os

options = {}


def collect_config_options(filename):
    lines = open(filename, "rt").readlines()
    i = -1
    len_lines = len(lines)

    func_names = ["CPLGetConfigOption"]
    if os.path.basename(filename) == "gt_overview.cpp":
        func_names.append("GetOptionValue")

    while True:
        i += 1
        if i >= len_lines:
            break
        l = lines[i][0:-1].strip()
        if l.startswith("/*"):
            continue

        found = False
        for func_name in func_names:
            pos = l.find(func_name + "(")
            if pos >= 0:
                pos_start = pos + len(func_name + "(")
                option = None
                if pos_start < len(l) and l[pos_start] == '"':
                    pos_end = l.find('"', pos_start + 1)
                    if pos_end + 1 < len(l) and l[pos_end + 1] == ",":
                        option = l[pos_start + 1 : pos_end]
                    else:
                        print("Skipping", l)
                elif pos_start == len(l):
                    l = lines[i + 1][0:-1].strip()
                    pos_start = l.find('"')
                    assert pos_start >= 0, l
                    pos_end = l.find('"', pos_start + 1)
                    if pos_end + 1 < len(l) and l[pos_end + 1] == ",":
                        option = l[pos_start + 1 : pos_end]
                    elif (
                        l
                        != """("OGR_" + GetDriverUCName() + "_USE_BBOX").c_str(), "YES")));"""
                    ):
                        print("Skipping", l)
                if option:
                    if func_name == "GetOptionValue":
                        option += "_OVERVIEW"
                    if option not in options:
                        options[option] = set([os.path.basename(filename)])
                    else:
                        options[option].add(os.path.basename(filename))

                found = True
                break

        if found:
            continue

        pos = l.find("VSIGetPathSpecificOption(")
        if pos >= 0 and l.find("VSIGetPathSpecificOption()") < 0:
            pos_start = pos + len("VSIGetPathSpecificOption(")
            option = None
            if pos_start == len(l):
                l += lines[i + 1][0:-1].strip()
                i += 1

            pos_start2 = l.find(', "', pos_start)
            if pos_start2 < 0:
                l += " " + lines[i + 1][0:-1].strip()
                i += 1

            pos_start2 = l.find(', "', pos_start)
            if pos_start2 < 0:
                if (
                    l.endswith(', std::string("VSI")')
                    or l.endswith("VSIGetPathSpecificOption(pszFilename, pszFilename,")
                    or l.find("VSIGetPathSpecificOption(pszPath, pszKey, pszDefault)")
                    >= 0
                    or l.find(
                        "VSIGetPathSpecificOption(osPathForOption.c_str(), osPathForOption.c_str(),"
                    )
                    >= 0
                    or l.find(
                        "VSIGetPathSpecificOption(pszFilename, sTuple.pszEnvVar, nullptr)"
                    )
                    >= 0
                    or l.find(
                        "VSIGetPathSpecificOption(osNonStreamingFilename.c_str(), sTuple.pszEnvVar, nullptr)"
                    )
                    >= 0
                    or l.find(
                        "const char *VSIGetPathSpecificOption(const char *pszPath, const char *pszKey, const char *pszDefault)"
                    )
                    >= 0
                ):
                    continue
            assert pos_start2 >= 0, l
            pos_end = l.find('"', pos_start2 + 3)
            if pos_end + 1 < len(l) and l[pos_end + 1] == ",":
                option = l[pos_start2 + 3 : pos_end]
            else:
                if (
                    l.find(
                        'VSIGetPathSpecificOption(osPathForOption.c_str(), pszOptionKey, "")'
                    )
                    >= 0
                    or l.find(
                        'VSIGetPathSpecificOption(osPathForOption.c_str(), pszURLKey, "")'
                    )
                    >= 0
                    or l.find(
                        'VSIGetPathSpecificOption(osPathForOption.c_str(), pszUserKey, "")'
                    )
                    >= 0
                    or l.find(
                        'VSIGetPathSpecificOption(osPathForOption.c_str(), pszPasswordKey, "")'
                    )
                    >= 0
                ):
                    continue
                print("Skipping", l)

            if option:
                if option not in options:
                    options[option] = set([os.path.basename(filename)])
                else:
                    options[option].add(os.path.basename(filename))

        pos = l.find("alt_config_option='")
        if pos >= 0:
            pos_start = pos + len("alt_config_option='")
            pos_end = l.find("'", pos_start)
            option = l[pos_start:pos_end]
            if option not in options:
                options[option] = set([os.path.basename(filename)])
            else:
                options[option].add(os.path.basename(filename))


def explore(dirname):
    for filename in glob.glob(dirname + "/*"):
        if os.path.isdir(filename):
            explore(filename)
        if filename.endswith(".c") or filename.endswith(".cpp"):
            collect_config_options(filename)


for dirname in ("alg", "port", "gcore", "frmts", "ogr", "apps", "swig"):
    explore(os.path.join(os.path.dirname(__file__), "..", dirname))

c_file = "// This file was automatically generated by collect_config_options.py\n"
c_file += "// DO NOT EDIT\n\n"
c_file += "// clang-format off\n"
c_file += "constexpr static const char* const apszKnownConfigOptions[] =\n"
c_file += "{\n"
for option in sorted(options, key=str.lower):
    filename = ", ".join(sorted(options[option]))
    c_file += f'   "{option}", // from {filename}\n'
c_file += "};\n"
c_file += "// clang-format on\n"
# print(c_file)

open(
    os.path.join(os.path.dirname(__file__), "..", "port", "cpl_known_config_options.h"),
    "wt",
).write(c_file)
