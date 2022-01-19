#!/usr/bin/env python
# SPDX-License-Identifier: MIT
# Copyright (C) 2021 Even Rouault

# One-time script to extract GRIB2 tables from degrib metaname.c[pp] file

import shutil

lines = open('degrib/metaname.cpp', 'rt', encoding='utf-8').readlines()
lines = [l[0:-1] for l in lines]

def get_utf8(l):
    l = l.replace('\\xC3\\xA9" "', 'é')
    l = l.replace('\\xC3\\xB6" "', 'ö')
    l = l.replace('\\xC3\\xBC" "', 'ü')
    l = l.replace('\\xC3\\xA4" "', 'ä')
    assert '\\x' not in l, l
    return l

def format_csv_line(l):
    out = ''
    in_quotes = False
    for c in l:
        assert c != '\\'
        if c == '"':
            out += c
            in_quotes = not in_quotes
        elif c != ' ' or in_quotes:
            out += c
    return out


def strip_comments(l):
    out = ''
    pos = 0
    while True:
        pos1 = l.find("/*", pos)
        if pos1 < 0:
            out += l[pos:]
            break
        else:
            out += l[pos:pos1]
        pos2 = l.find("*/", pos1)
        assert pos2 > 0, l
        pos = pos2 + 2
    pos = out.find('//')
    if pos >= 0:
        out = out[0:pos]
    return out

def extract_table(lines, out, start_pattern, add_code=False):
    found = False
    code = 0
    line_idx = 0
    line_count = len(lines)
    while line_idx < line_count:
        l = lines[line_idx]
        line_idx += 1
        l = get_utf8(l)
        if start_pattern in l:
            found = True
        elif found:
            while '/*' in l and '*/' not in l:
                l += lines[line_idx]
                line_idx += 1
            l = strip_comments(l).strip()
            if len(l) == 0:
                continue
            if '};' in l:
                break

            while l.endswith('"'):
                next_l = lines[line_idx]
                line_idx += 1
                next_l = strip_comments(next_l).strip()
                assert next_l.startswith('"')
                l = l[0:-1] + next_l[1:]

            while not l.endswith('},') and l.endswith(','):
                next_l = lines[line_idx]
                line_idx += 1
                next_l = strip_comments(next_l).strip()
                assert next_l.endswith('},') or next_l.endswith('}'), next_l
                l += next_l

            pos = l.find("}, {")
            if pos > 0:
                sublines = [ l[0:pos+2] ]
                l = l[pos+3:]
                while True:
                    pos = l.find("}, {")
                    if pos > 0:
                        sublines.append(l[0:pos+2])
                        l = l[pos+3:]
                    else:
                        sublines.append(l)
                        break
            else:
                sublines = [ l ]
            for l in sublines:
                assert l.startswith('{'), l
                l = l[1:]
                if l.endswith('},'):
                    l = l[0:-2]
                elif l.endswith('}'):
                    l = l[0:-1]
                else:
                    assert False, l
                assert '{' not in l, l
                assert '}' not in l, l
                l = format_csv_line(l)
                if add_code:
                    out.write('%d,' % code)
                    code += 1
                out.write(l + '\n')

def extract_center():
    out = open('grib2_center.csv', 'wt', encoding='utf-8')
    out.write("code,name\n")
    extract_table(lines, out, 'Center[]')
    out.close()

extract_center()


def extract_subcenter():
    out = open('grib2_subcenter.csv', 'wt', encoding='utf-8')
    out.write("center_code,subcenter_code,name\n")
    extract_table(lines, out, 'SubCenter[]')
    out.close()

extract_subcenter()

def extract_process():
    out = open('grib2_process.csv', 'wt', encoding='utf-8')
    out.write("center_code,process_code,name\n")
    extract_table(lines, out, 'Process[]')
    out.close()

extract_process()


def extract_table_4_2(filename_suffix, table_name_pattern):
    out = open('grib2_table_4_2_%s.csv' % filename_suffix, 'wt', encoding='utf-8')
    out.write("subcat,short_name,name,unit,unit_conv\n")
    extract_table(lines, out, table_name_pattern, add_code=True)
    out.close()

extract_table_4_2("0_0", "MeteoTemp[]")
extract_table_4_2("0_1", "MeteoMoist[]")
extract_table_4_2("0_2", "MeteoMoment[]")
extract_table_4_2("0_3", "MeteoMass[]")
extract_table_4_2("0_4", "MeteoShortRadiate[]")
extract_table_4_2("0_5", "MeteoLongRadiate[]")
extract_table_4_2("0_6", "MeteoCloud[]")
extract_table_4_2("0_7", "MeteoStability[]")
extract_table_4_2("0_13", "MeteoAerosols[]")
extract_table_4_2("0_14", "MeteoGases[]")
extract_table_4_2("0_15", "MeteoRadar[]")
extract_table_4_2("0_16", "MeteoRadarImagery[]")
extract_table_4_2("0_17", "MeteoElectro[]")
extract_table_4_2("0_18", "MeteoNuclear[]")
extract_table_4_2("0_19", "MeteoAtmos[]")
extract_table_4_2("0_20", "MeteoAtmoChem[]")
extract_table_4_2("0_190", "MeteoText[]")
# DEGRIB has the following non standard 253 code
shutil.copy('grib2_table_4_2_0_190.csv', # CCITT IA5 string
            'grib2_table_4_2_0_253.csv') # METEO_CCITT2
extract_table_4_2("0_191", "MeteoMisc[]")
extract_table_4_2("1_0", "HydroBasic[]")
extract_table_4_2("1_1", "HydroProb[]")
extract_table_4_2("2_0", "LandVeg[]")
extract_table_4_2("2_3", "LandSoil[]")
extract_table_4_2("3_0", "SpaceImage[]")
extract_table_4_2("3_1", "SpaceQuantitative[]")
extract_table_4_2("10_0", "OceanWaves[]")
extract_table_4_2("10_1", "OceanCurrents[]")
extract_table_4_2("10_2", "OceanIce[]")
extract_table_4_2("10_3", "OceanSurface[]")
extract_table_4_2("10_4", "OceanSubSurface[]")
extract_table_4_2("10_191", "OceanMisc[]")

def extract_NDFD_Override():
    out = open('grib2_NDFD_Override.csv', 'wt', encoding='utf-8')
    out.write("grib2_name,NDFD_name\n")
    extract_table(lines, out, 'NDFD_Override[]')
    out.close()

extract_NDFD_Override()


def extract_local_table(name):
    out = open('grib2_table_4_2_local_%s.csv' % name, 'wt', encoding='utf-8')
    out.write("prod,cat,subcat,short_name,name,unit,unit_conv\n")
    extract_table(lines, out, '%s_LclTable[]' % name)
    out.close()

extract_local_table('NDFD')
extract_local_table('HPC')
extract_local_table('Canada')
extract_local_table('MRMS')
extract_local_table('NCEP')


