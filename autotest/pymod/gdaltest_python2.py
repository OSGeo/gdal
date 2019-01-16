# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Python Library supporting GDAL/OGR Test Suite
# Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################

import urllib2
import socket
import os
import subprocess
import shlex
import sys
from Queue import Queue
from threading import Thread


def run_func(func):
    try:
        result = func()
        print(result)
        return result
    except SystemExit as x:
        import traceback
        traceback.print_exc()

        raise x
    except Exception:  # pylint: disable=broad-except
        result = 'fail (blowup)'
        print(result)

        import traceback
        traceback.print_exc()
        return result


def urlescape(url):
    # Escape any non-ASCII characters
    try:
        import urllib
        url = urllib.quote(url)
    except ImportError:
        pass
    return url


def gdalurlopen(url, timeout=10):
    old_timeout = socket.getdefaulttimeout()
    socket.setdefaulttimeout(timeout)
    proxy = None

    if 'GDAL_HTTP_PROXY' in os.environ:
        proxy = os.environ['GDAL_HTTP_PROXY']
        protocol = 'http'

    if 'GDAL_HTTPS_PROXY' in os.environ and url.startswith('https'):
        proxy = os.environ['GDAL_HTTPS_PROXY']
        protocol = 'https'

    if proxy is not None:
        if 'GDAL_HTTP_PROXYUSERPWD' in os.environ:
            proxyuserpwd = os.environ['GDAL_HTTP_PROXYUSERPWD']
            proxyHandler = urllib2.ProxyHandler({"%s" % protocol:
                                                 "%s://%s@%s" % (protocol, proxyuserpwd, proxy)})
        else:
            proxyuserpwd = None
            proxyHandler = urllib2.ProxyHandler({"%s" % protocol:
                                                 "%s://%s" % (protocol, proxy)})

        opener = urllib2.build_opener(proxyHandler, urllib2.HTTPHandler)

        urllib2.install_opener(opener)

    try:
        handle = urllib2.urlopen(url)
        socket.setdefaulttimeout(old_timeout)
        return handle
    except urllib2.HTTPError as e:
        print('HTTP service for %s is down (HTTP Error: %d)' % (url, e.code))
        socket.setdefaulttimeout(old_timeout)
        return None
    except urllib2.URLError as e:
        print('HTTP service for %s is down (HTTP Error: %s)' % (url, e.reason))
        socket.setdefaulttimeout(old_timeout)
        return None
    except socket.timeout:
        print('HTTP service for %s is down (timeout)' % url)
        socket.setdefaulttimeout(old_timeout)
        return None


def warn_if_memleak(cmd, out_str):

    # If DEBUG_VSIMALLOC_STATS is defined, this is an easy way
    # to catch some memory leaks
    if cmd.find('--utility_version') == -1 and \
       out_str.find('VSIMalloc + VSICalloc - VSIFree') != -1 and \
       out_str.find('VSIMalloc + VSICalloc - VSIFree : 0') == -1:
        print('memory leak detected')
        print(out_str)


def spawn_async(cmd):
    command = shlex.split(cmd)
    try:
        process = subprocess.Popen(command, stdout=subprocess.PIPE)
        return (process, process.stdout)
    except OSError:
        return (None, None)


def wait_process(process):
    process.wait()


def runexternal(cmd, strin=None, check_memleak=True, display_live_on_parent_stdout=False, encoding=None):
    # pylint: disable=unused-argument
    command = shlex.split(cmd)
    command = [elt.replace('\x00', '') for elt in command]
    if strin is None:
        p = subprocess.Popen(command, stdout=subprocess.PIPE)
    else:
        p = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        p.stdin.write(strin)
        p.stdin.close()

    if p.stdout is not None:
        if display_live_on_parent_stdout:
            ret = ''
            ret_stdout = p.stdout
            while True:
                c = ret_stdout.read(1)
                if c == '':
                    break
                ret = ret + c
                sys.stdout.write(c)
        else:
            ret = p.stdout.read()
    else:
        ret = ''

    waitcode = p.wait()
    if waitcode != 0:
        ret = ret + '\nERROR ret code = %d' % waitcode

    if encoding is not None:
        ret = ret.decode(encoding)
    return ret


def read_in_thread(f, q):
    q.put(f.read())
    f.close()

# Compatible with Python 2.6 or above


def _runexternal_out_and_err_subprocess(cmd, check_memleak=True):
    # pylint: disable=unused-argument
    command = shlex.split(cmd)
    command = [elt.replace('\x00', '') for elt in command]
    p = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    if p.stdout is not None:
        q_stdout = Queue()
        t_stdout = Thread(target=read_in_thread, args=(p.stdout, q_stdout))
        t_stdout.start()
    else:
        q_stdout = None
        ret_stdout = ''

    if p.stderr is not None:
        q_stderr = Queue()
        t_stderr = Thread(target=read_in_thread, args=(p.stderr, q_stderr))
        t_stderr.start()
    else:
        q_stderr = None
        ret_stderr = ''

    if q_stdout is not None:
        ret_stdout = q_stdout.get()
    if q_stderr is not None:
        ret_stderr = q_stderr.get()

    waitcode = p.wait()
    if waitcode != 0:
        ret_stderr = ret_stderr + '\nERROR ret code = %d' % waitcode

    return (ret_stdout, ret_stderr)


def runexternal_out_and_err(cmd, check_memleak=True):
    return _runexternal_out_and_err_subprocess(cmd, check_memleak=check_memleak)
