# Licensed under a 3-clause BSD style license - see LICENSE.rst
"""
This is a set of three directives that allow us to insert metadata
about doctests into the .rst files so the testing framework knows
which tests to skip.

This is quite different from the doctest extension in Sphinx itself,
which actually does something.  For astropy, all of the testing is
centrally managed from py.test and Sphinx is not used for running
tests.
"""

import re

from docutils.nodes import literal_block
from docutils.parsers.rst import Directive


class DoctestSkipDirective(Directive):
    has_content = True

    def run(self):
        # Check if there is any valid argument, and skip it. Currently only
        # 'win32' is supported.
        if re.match("win32", self.content[0]):
            self.content = self.content[2:]
        code = "\n".join(self.content)
        return [literal_block(code, code)]


class DoctestOmitDirective(Directive):
    has_content = True

    def run(self):
        # Simply do not add any content when this directive is encountered
        return []


class DoctestRequiresDirective(DoctestSkipDirective):
    # This is silly, but we really support an unbounded number of
    # optional arguments
    optional_arguments = 64


def setup(app):

    app.add_directive("doctest-requires", DoctestRequiresDirective)
    app.add_directive("doctest-requires-all", DoctestRequiresDirective)
    app.add_directive("doctest-skip", DoctestSkipDirective)
    app.add_directive("doctest-skip-all", DoctestSkipDirective)
    app.add_directive("doctest", DoctestSkipDirective, override=True)
    app.add_directive("doctest-remote-data", DoctestSkipDirective)
    app.add_directive("doctest-remote-data-all", DoctestSkipDirective)
    # Code blocks that use this directive will not appear in the generated
    # documentation. This is intended to hide boilerplate code that is only
    # useful for testing documentation using doctest, but does not actually
    # belong in the documentation itself.
    app.add_directive("testsetup", DoctestOmitDirective, override=True)
    app.add_directive("testcleanup", DoctestOmitDirective, override=True)

    return {"parallel_read_safe": True, "parallel_write_safe": True}
