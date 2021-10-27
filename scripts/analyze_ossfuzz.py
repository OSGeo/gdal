#!/usr/bin/env python
# SPDX-License-Identifier: MIT
# Copyright 2021 Even Rouault

# Analyze git log for ossfuzz vulnerabilities

from subprocess import check_output
out = check_output("git log --no-merges --pretty=oneline".split(' ')).decode('utf-8')
result = list(filter(lambda x: 'chromium' in x, out.splitlines()))

categories = {}

def add_occurence(category_id):
    if category_id in categories:
        categories[category_id] += 1
    else:
        categories[category_id] = 1

for line in result:
    pos = line.find(' ')
    commit_id = line[0:pos]

    # Normalize
    msg = line[pos+1:].lower()
    msg = msg.replace('-', ' ')

    # Fix a few typos
    msg = msg.replace('oveflow', 'overflow')
    msg = msg.replace('overfow', 'overflow')
    msg = msg.replace('divison', 'division')
    msg = msg.replace('divizion', 'division')
    msg = msg.replace('divid', 'division')
    msg = msg.replace('use of initialized', 'uninitialized')
    msg = msg.replace('out of bond', 'out of bound')
    msg = msg.replace('procession', 'processing')
    msg = msg.replace('behaviour', 'behavior')
    msg = msg.replace('perforance', 'performance')

    # print(commit_id, msg)
    if 'overflow' in msg or 'out of bound' in msg or 'out of buffer' in msg or 'stack buffer' in msg:
        if 'stack' in msg:
            if 'read' in msg:
                add_occurence('stack_buffer_overflow_read')
            elif 'read' in msg:
                add_occurence('stack_buffer_overflow_write')
            elif 'buffer' in msg:
                add_occurence('stack_buffer_overflow_unspecified')
            else:
                add_occurence('stack_call_overflow')
        elif 'integer' in msg or 'int32' in msg or 'int64 ' in msg or 'int ' in msg or 'size_t' in msg or 'unsigned' in msg:
            if 'unsigned' in msg or 'size_t' in msg:
                if 'harmless' in msg:
                    add_occurence('integer_overflow_unsigned_harmless')
                else:
                    add_occurence('integer_overflow_unsigned')
            else:
                if 'harmless' in msg:
                    add_occurence('integer_overflow_harmless') # one occurrence due to atoi()
                else:
                    add_occurence('integer_overflow_signed')
        elif 'out of bound' in msg or 'heap' in msg or 'buffer' in msg or 'container overflow' in msg or 'array' in msg or 'read overflow' in msg:
            if 'read ' in msg:
                add_occurence('heap_buffer_overflow_read')
            elif 'write ' in msg:
                add_occurence('heap_buffer_overflow_write')
            else:
                add_occurence('buffer_overflow_unspecified')
        else:
            assert False, (commit_id, msg)
    elif 'unsigned integer underflow' in msg:
        add_occurence('unsigned_integer_underflow')
    elif 'oom' in msg or 'allocate' in msg or (('excessive' in msg or 'too large' in msg or 'too much' or 'huge' in msg or 'big' in msg) and ('memory' in msg or 'ram' in msg or 'allocation' in msg)):
        add_occurence('excessive_memory_use')
    elif 'shift' in msg:
        if 'left' in msg:
            add_occurence('invalid_shift_left')
        elif 'right' in msg:
            add_occurence('invalid_shift_right')
        else:
            add_occurence('invalid_shift_unspecified_dir')
    elif 'division' in msg:
        assert 'zero' in msg
        if 'float' in msg or 'harmless' in msg:
            if 'harmless' in msg:
                add_occurence('division_by_zero_floating_point_harmless')
            else:
                add_occurence('division_by_zero_floating_point_unknown_consequence')
        else:
            add_occurence('division_by_zero_integer')
    elif 'leak' in msg:
        if 'error' in msg or 'corrupted' in msg:
            add_occurence('memory_leak_error_code_path')
        else:
            add_occurence('memory_leak_unspecified')
    elif 'unhandled exception' in msg or 'avoid exception' in msg:
        add_occurence('unhandled_exception')
    elif 'null pointer' in msg or 'null ptr' in msg or 'nullptr' in msg or 'nullpointer' in msg or 'null deref' in msg or 'against null' in msg:
        add_occurence('null_pointer_dereference')
    elif 'reference' in msg:
        add_occurence('invalid_memory_dereference')
    elif 'infinite' in msg and 'loop' in msg:
        add_occurence('infinite_loop')
    elif 'double free' in msg:
        add_occurence('double_free')
    elif 'use after free' in msg:
        add_occurence('use_after_free')
    elif 'uninitialized' in msg:
        add_occurence('uninitialized_variable')
    elif 'assert' in msg:
        add_occurence('assertion')
    elif 'exponential' in msg or 'limit number' in msg or 'speed up' in msg or 'timeout' in msg or 'too long' in msg or \
         (('limit' in msg or 'long' in msg or 'large' in msg or 'slow' in msg) and ('loop' in msg or 'delay' in msg or 'time' in msg or 'processing' in msg)) or \
             ('excessive' in msg and 'time' in msg) or 'performance' in msg or 'big processing time' in msg:
        add_occurence('excessive_processing_time')
    elif 'recursion' in msg or 'recursive' in msg or 'deep call' in msg or 'cyclic call' in msg:
        add_occurence('stack_call_overflow')
    elif 'invalid enum' in msg:
        add_occurence('invalid_enum')
    elif 'cast' in msg:
        add_occurence('invalid_cast')
    elif 'negative size' in msg:
        add_occurence('negative_size_allocation')
    elif 'crash' in msg:
        add_occurence('unspecified_crash')
    elif 'ubsan' in msg or 'unspecified behavior' in msg:
        add_occurence('undefined_behavior_unspecified')
    else:
        add_occurence('other_issue')

ar = []
for k, v in categories.items():
    ar.append((k, v))
ar.sort(key=lambda x: x[1], reverse=True)
total = sum(v for k,v in ar)

print('%-55s %4s %4s' % ('Category', 'Count', 'Pct'))
print('-' * 70)
for k, v in ar:
    print('%-55s %4d  %5.02f %%' % (k, v, 100.0 * v / total))
print('-' * 70)
print('%-55s %4d' % ('Total', total))
