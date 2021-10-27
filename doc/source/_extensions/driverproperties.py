# From https://www.sphinx-doc.org/en/master/development/tutorials/todo.html

import docutils.statemachine
import sphinx.locale

sphinx.locale.admonitionlabels['shortname'] = ''
sphinx.locale.admonitionlabels['built_in_by_default'] = ''  # 'Built-in by default'
sphinx.locale.admonitionlabels['supports_create'] = ''  # 'Supports Create()'
sphinx.locale.admonitionlabels['supports_createcopy'] = ''  # 'Supports CreateCopy()'
sphinx.locale.admonitionlabels['supports_georeferencing'] = ''  # 'Supports georeferencing'
sphinx.locale.admonitionlabels['supports_virtualio'] = ''  # 'Supports VirtualIO'
sphinx.locale.admonitionlabels['supports_multidimensional'] = ''  # 'Supports multidimensional'
sphinx.locale.admonitionlabels['deprecated_driver'] = ''  # 'Driver is deprecated and marked for removal'

def setup(app):
    app.add_node(shortname,
                 html=(visit_shortname_node, depart_node),
                 latex=(visit_admonition, depart_node),
                 text=(visit_admonition, depart_node))
    app.add_directive('shortname', ShortName)

    app.add_node(built_in_by_default,
                 html=(visit_built_in_by_default_node, depart_node),
                 latex=(visit_admonition, depart_node),
                 text=(visit_admonition, depart_node))
    app.add_directive('built_in_by_default', BuiltInByDefault)

    app.add_node(build_dependencies,
                 html=(visit_build_dependencies_node, depart_node),
                 latex=(visit_admonition, depart_node),
                 text=(visit_admonition, depart_node))
    app.add_directive('build_dependencies', BuildDependencies)

    app.add_node(supports_create,
                 html=(visit_supports_create_node, depart_node),
                 latex=(visit_admonition, depart_node),
                 text=(visit_admonition, depart_node))
    app.add_directive('supports_create', CreateDirective)

    app.add_node(supports_createcopy,
                 html=(visit_supports_createcopy_node, depart_node),
                 latex=(visit_admonition, depart_node),
                 text=(visit_admonition, depart_node))
    app.add_directive('supports_createcopy', CreateCopyDirective)

    app.add_node(supports_georeferencing,
                 html=(visit_supports_georeferencing_node, depart_node),
                 latex=(visit_admonition, depart_node),
                 text=(visit_admonition, depart_node))
    app.add_directive('supports_georeferencing', GeoreferencingDirective)

    app.add_node(supports_virtualio,
                 html=(visit_supports_virtualio_node, depart_node),
                 latex=(visit_admonition, depart_node),
                 text=(visit_admonition, depart_node))
    app.add_directive('supports_virtualio', VirtualIODirective)

    app.add_node(supports_multidimensional,
                 html=(visit_supports_multidimensional_node, depart_node),
                 latex=(visit_admonition, depart_node),
                 text=(visit_admonition, depart_node))
    app.add_directive('supports_multidimensional', MultiDimensionalDirective)

    app.add_node(deprecated_driver,
                 html=(visit_deprecated_driver_node, depart_node),
                 latex=(visit_admonition, depart_node),
                 text=(visit_admonition, depart_node))
    app.add_directive('deprecated_driver', DeprecatedDriverDirective)

    app.connect('env-purge-doc', purge_driverproperties)

    return { 'parallel_read_safe': True, 'parallel_write_safe': True }

from docutils import nodes

def visit_admonition(self, node):
    self.visit_admonition(node)

def depart_node(self, node):
    self.depart_admonition(node)

class shortname(nodes.Admonition, nodes.Element):
    pass

def visit_shortname_node(self, node):
    self.body.append(self.starttag(
            node, 'div', CLASS=('admonition shortname')))

class built_in_by_default(nodes.Admonition, nodes.Element):
    pass

def visit_built_in_by_default_node(self, node):
    self.body.append(self.starttag(
            node, 'div', CLASS=('admonition built_in_by_default')))

class build_dependencies(nodes.Admonition, nodes.Element):
    pass

def visit_build_dependencies_node(self, node):
    self.body.append(self.starttag(
            node, 'div', CLASS=('admonition build_dependencies')))

class supports_create(nodes.Admonition, nodes.Element):
    pass

def visit_supports_create_node(self, node):
    self.body.append(self.starttag(
            node, 'div', CLASS=('admonition supports_create')))

class supports_createcopy(nodes.Admonition, nodes.Element):
    pass

def visit_supports_createcopy_node(self, node):
    self.body.append(self.starttag(
            node, 'div', CLASS=('admonition supports_createcopy')))

class supports_georeferencing(nodes.Admonition, nodes.Element):
    pass

def visit_supports_georeferencing_node(self, node):
    self.body.append(self.starttag(
            node, 'div', CLASS=('admonition supports_georeferencing')))

class supports_virtualio(nodes.Admonition, nodes.Element):
    pass

def visit_supports_virtualio_node(self, node):
    self.body.append(self.starttag(
            node, 'div', CLASS=('admonition supports_virtualio')))

class supports_multidimensional(nodes.Admonition, nodes.Element):
    pass

def visit_supports_multidimensional_node(self, node):
    self.body.append(self.starttag(
            node, 'div', CLASS=('admonition supports_multidimensional')))

class deprecated_driver(nodes.Admonition, nodes.Element):
    pass

def visit_deprecated_driver_node(self, node):
    self.body.append(self.starttag(
            node, 'div', CLASS=('danger deprecated_driver')))

from docutils.parsers.rst import Directive


from sphinx.locale import _


def finish_directive(_self, directive, node):

    env = _self.state.document.settings.env

    targetid = "%s-%d" % (directive, env.new_serialno(directive))
    targetnode = nodes.target('', '', ids=[targetid])

    _self.state.nested_parse(_self.content, _self.content_offset, node)

    if not hasattr(env, 'all_' + directive):
        setattr(env, 'all_' + directive, [])
    getattr(env, 'all_' + directive).append({
        'docname': env.docname,
        'lineno': _self.lineno,
        directive: node.deepcopy(),
        'target': targetnode,
    })

    return [targetnode, node]


class ShortName(Directive):

    # this enables content in the directive
    has_content = True

    def run(self):

        node = shortname('\n'.join(self.content))
        node += nodes.title(_('Driver short name'), _('Driver short name'))

        return finish_directive(self, 'shortname', node)


class BuiltInByDefault(Directive):

    # this enables content in the directive
    has_content = True

    def run(self):

        if not self.content:
            self.content = docutils.statemachine.StringList(['This driver is built-in by default'])
        node = built_in_by_default('\n'.join(self.content))
        node += nodes.title(_('Driver built-in by default'), _('Driver built-in by default'))

        return finish_directive(self, 'built_in_by_default', node)


class BuildDependencies(Directive):

    # this enables content in the directive
    has_content = True

    def run(self):

        assert self.content, "Content should be defined for build_dependencies directive"
        node = build_dependencies('\n'.join(self.content))
        node += nodes.title(_('Build dependencies'), _('Build dependencies'))

        return finish_directive(self, 'build_dependencies', node)


class CreateDirective(Directive):

    # this enables content in the directive
    has_content = True

    def run(self):

        if not self.content:
            self.content = docutils.statemachine.StringList(['This driver supports the :cpp:func:`GDALDriver::Create` operation'])
        node = supports_create('\n'.join(self.content))
        node += nodes.title(_('Supports Create()'), _('Supports Create()'))

        return finish_directive(self, 'supports_create', node)

class CreateCopyDirective(Directive):

    # this enables content in the directive
    has_content = True

    def run(self):

        if not self.content:
            self.content = docutils.statemachine.StringList(['This driver supports the :cpp:func:`GDALDriver::CreateCopy` operation'])
        node = supports_createcopy('\n'.join(self.content))
        node += nodes.title(_('Supports CreateCopy()'), _('Supports CreateCopy()'))

        return finish_directive(self, 'supports_createcopy', node)


class GeoreferencingDirective(Directive):

    # this enables content in the directive
    has_content = True

    def run(self):

        if not self.content:
            self.content = docutils.statemachine.StringList(['This driver supports georeferencing'])
        node = supports_georeferencing('\n'.join(self.content))
        node += nodes.title(_('Supports Georeferencing'), _('Supports Georeferencing'))

        return finish_directive(self, 'supports_georeferencing', node)


class VirtualIODirective(Directive):

    # this enables content in the directive
    has_content = True

    def run(self):

        if not self.content:
            self.content = docutils.statemachine.StringList(['This driver supports :ref:`virtual I/O operations (/vsimem/, etc.) <virtual_file_systems>`'])
        node = supports_virtualio('\n'.join(self.content))
        node += nodes.title(_('Supports VirtualIO'), _('Supports VirtualIO'))

        return finish_directive(self, 'supports_virtualio', node)


class MultiDimensionalDirective(Directive):

    # this enables content in the directive
    has_content = True

    def run(self):

        if not self.content:
            self.content = docutils.statemachine.StringList(['This driver supports the :ref:`multidim_raster_data_model`'])
        node = supports_virtualio('\n'.join(self.content))
        node += nodes.title(_('Supports multidimensional API'), _('Supports multidimensional API'))

        return finish_directive(self, 'supports_multidimensional', node)


class DeprecatedDriverDirective(Directive):

    # this enables content in the directive
    has_content = True

    def run(self):

        explanation = []
        version_targeted_for_removal = [c[len('version_targeted_for_removal:'):].strip() for c in self.content if
                                        c.startswith('version_targeted_for_removal:')]
        if version_targeted_for_removal:
            explanation.append(
                "This driver is considered for removal in GDAL {}.".format(version_targeted_for_removal[0]))
        else:
            explanation.append("This driver is considered for removal in a future GDAL release.")

        message = [c[len('message:'):].strip() for c in self.content if
                                        c.startswith('message:')]
        if message:
            explanation.append(message[0])
        else:
            explanation.append("You are invited to convert any dataset in that format to another more common one.")

        explanation.append("If you need this driver in future GDAL versions, create a ticket at https://github.com/OSGeo/gdal "
                           "(look first for an existing one first) to explain how critical it is for you "
                           "(but the GDAL project may still remove it).")

        env_variable = [c[len('env_variable:'):].strip() for c in self.content if
                        c.startswith('env_variable:')]
        if env_variable:
            explanation.append("To enable use of the deprecated driver the {} configuration option /"
                               " environment variable must be set to YES.".format(env_variable[0]))

        self.content = docutils.statemachine.StringList(explanation)

        node = deprecated_driver('\n'.join(self.content))
        node += nodes.title(_('Deprecated'), _('Deprecated'))

        return finish_directive(self, 'deprecated_driver', node)


def purge_driverproperties(app, env, docname):
    for directive in ['all_shortname',
                      'all_built_in_by_default',
                      'all_build_dependencies',
                      'all_supports_create',
                      'all_supports_createcopy',
                      'all_supports_georeferencing',
                      'all_supports_virtualio',
                      'all_supports_multidimensional',
                      'all_deprecated_driver']:
        if hasattr(env, directive):
            setattr(env, directive, [ embed for embed in getattr(env, directive) if embed['docname'] != docname])
