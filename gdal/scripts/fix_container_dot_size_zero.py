#!/usr/bin/env python
###############################################################################
# $Id$
#
#  Project:  GDAL samples
#  Purpose:  Replace container.size() by !container.empty(), and reverse
#  Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
#  Copyright (c) 2016, Even Rouault <even.rouault at spatialys.com>
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included
#  in all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
#  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#  DEALINGS IN THE SOFTWARE.
###############################################################################


import sys


def find_start_identifier_pos(content, pos_start_identifier):
    level_parenthesis = 0
    level_bracket = 0
    while True:
        c = content[pos_start_identifier]
        if level_parenthesis > 0 and c != '(' and c != ')':
            pos_start_identifier -= 1
        elif c == ')':
            level_parenthesis += 1
            pos_start_identifier -= 1
        elif c == '(':
            if level_parenthesis == 0:
                break
            level_parenthesis -= 1
            pos_start_identifier -= 1
        elif level_bracket > 0 and c != '[' and c != ']':
            pos_start_identifier -= 1
        elif c == ']':
            level_bracket += 1
            pos_start_identifier -= 1
        elif c == '[':
            if level_bracket == 0:
                break
            level_bracket -= 1
            pos_start_identifier -= 1
        elif c.isalnum() or c == '_' or c == '.':
            pos_start_identifier -= 1
        elif c == '>' and content[pos_start_identifier - 1] == '-':
            pos_start_identifier -= 2
        else:
            break
    pos_start_identifier += 1
    return pos_start_identifier


content = open(sys.argv[1], 'rb').read()
pos = 0
modified = False
while True:
    pos1 = content.find('.size()', pos)
    pos2 = content.find('->size()', pos)
    if pos1 < 0 and pos2 < 0:
        break
    separator = ''
    if pos1 >= 0 and (pos1 < pos2 or pos2 < 0):
        pos = pos1
        pos_after = pos + len('.size()')
        separator = '.'
    else:
        pos = pos2
        dot_variant = False
        pos_after = pos + len('->size()')
        separator = '->'

    if content[pos_after] == ' ':
        pos_after += 1
    extra_space = ''
    empty = False
    non_empty = False
    if content[pos_after:].startswith('== 0'):
        empty = True
        pos_after += len('== 0')
    elif content[pos_after:].startswith('==0'):
        empty = True
        pos_after += len('==0')
    elif content[pos_after:].startswith('!= 0'):
        non_empty = True
        pos_after += len('!= 0')
    elif content[pos_after:].startswith('!=0'):
        non_empty = True
        pos_after += len('!=0')
    elif content[pos_after:].startswith('> 0'):
        non_empty = True
        pos_after += len('> 0')
    elif content[pos_after:].startswith('>0'):
        non_empty = True
        pos_after += len('>0')

    if not empty and not non_empty and (
            content[pos_after:].startswith(' )') or
            content[pos_after:].startswith(')') or
            content[pos_after:].startswith(' &&') or
            content[pos_after:].startswith(' ||') or
            content[pos_after:].startswith('&&') or
            content[pos_after:].startswith('||') or
            content[pos_after:].startswith(' ?') or
            content[pos_after:].startswith('?')):
        if content[pos_after] != ' ':
            extra_space = ' '
        pos_cur = find_start_identifier_pos(content, pos - 1)
        pos_cur -= 1
        while content[pos_cur] in [' ', '\n', '\t']:
            pos_cur -= 1
        pos_cur += 1
        if (content[pos_after:].startswith(' ?') or
            content[pos_after:].startswith('?')) and \
           (content[pos_cur - 1] == '(' or content[pos_cur - 1] == ','):
            non_empty = True
        elif content[pos_cur - 3:pos_cur] == 'if(':
            non_empty = True
        elif content[pos_cur - 4:pos_cur] == 'if (':
            non_empty = True
        elif content[pos_cur - 2:pos_cur] == '&&':
            non_empty = True
        elif content[pos_cur - 2:pos_cur] == '||':
            non_empty = True

    if empty:
        modified = True
        content = content[0:pos] + separator + 'empty()' + content[pos_after:]
    elif non_empty:
        modified = True
        pos_start_identifier = find_start_identifier_pos(content, pos - 1)
        content = content[0:pos_start_identifier] + '!' + \
            content[pos_start_identifier:pos] + separator + 'empty()' + \
            extra_space + content[pos_after:]

    pos += 1

if modified:
    open(sys.argv[1], 'wb').write(content)

# sys.stdout.write(content)
