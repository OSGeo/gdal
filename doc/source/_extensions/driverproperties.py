# From https://www.sphinx-doc.org/en/master/development/tutorials/todo.html

import docutils.statemachine
import sphinx.locale
from docutils import nodes
from sphinx.locale import _
from sphinx.util import logging
from sphinx.util.docutils import SphinxDirective

sphinx.locale.admonitionlabels["shortname"] = ""
sphinx.locale.admonitionlabels["built_in_by_default"] = ""  # 'Built-in by default'
sphinx.locale.admonitionlabels["supports_create"] = ""  # 'Supports Create()'
sphinx.locale.admonitionlabels["supports_createcopy"] = ""  # 'Supports CreateCopy()'
sphinx.locale.admonitionlabels["supports_georeferencing"] = (
    ""  # 'Supports georeferencing'
)
sphinx.locale.admonitionlabels["supports_virtualio"] = ""  # 'Supports VirtualIO'
sphinx.locale.admonitionlabels["supports_multidimensional"] = (
    ""  # 'Supports multidimensional'
)
sphinx.locale.admonitionlabels["deprecated_driver"] = (
    ""  # 'Driver is deprecated and marked for removal'
)


def setup(app):
    app.add_node(
        shortname,
        html=(visit_shortname_node_html, depart_node_html),
        latex=(visit_admonition_generic, depart_node_generic),
        text=(visit_admonition_generic, depart_node_generic),
    )
    app.add_directive("shortname", ShortName)

    app.add_node(
        built_in_by_default,
        html=(visit_built_in_by_default_node_html, depart_node_html),
        latex=(visit_admonition_generic, depart_node_generic),
        text=(visit_admonition_generic, depart_node_generic),
    )
    app.add_directive("built_in_by_default", BuiltInByDefault)

    app.add_node(
        build_dependencies,
        html=(visit_build_dependencies_node_html, depart_node_html),
        latex=(visit_admonition_generic, depart_node_generic),
        text=(visit_admonition_generic, depart_node_generic),
    )
    app.add_directive("build_dependencies", BuildDependencies)

    app.add_node(
        supports_create,
        html=(visit_supports_create_node_html, depart_node_html),
        latex=(visit_admonition_generic, depart_node_generic),
        text=(visit_admonition_generic, depart_node_generic),
    )
    app.add_directive("supports_create", CreateDirective)

    app.add_node(
        supports_createcopy,
        html=(visit_supports_createcopy_node_html, depart_node_html),
        latex=(visit_admonition_generic, depart_node_generic),
        text=(visit_admonition_generic, depart_node_generic),
    )
    app.add_directive("supports_createcopy", CreateCopyDirective)

    app.add_node(
        supports_georeferencing,
        html=(visit_supports_georeferencing_node_html, depart_node_html),
        latex=(visit_admonition_generic, depart_node_generic),
        text=(visit_admonition_generic, depart_node_generic),
    )
    app.add_directive("supports_georeferencing", GeoreferencingDirective)

    app.add_node(
        supports_virtualio,
        html=(visit_supports_virtualio_node_html, depart_node_html),
        latex=(visit_admonition_generic, depart_node_generic),
        text=(visit_admonition_generic, depart_node_generic),
    )
    app.add_directive("supports_virtualio", VirtualIODirective)

    app.add_node(
        supports_multidimensional,
        html=(visit_supports_multidimensional_node_html, depart_node_html),
        latex=(visit_admonition_generic, depart_node_generic),
        text=(visit_admonition_generic, depart_node_generic),
    )
    app.add_directive("supports_multidimensional", MultiDimensionalDirective)

    app.add_node(
        deprecated_driver,
        html=(visit_deprecated_driver_node_html, depart_node_html),
        latex=(visit_admonition_generic, depart_node_generic),
        text=(visit_admonition_generic, depart_node_generic),
    )
    app.add_directive("deprecated_driver", DeprecatedDriverDirective)

    app.connect("env-purge-doc", purge_driverproperties)
    app.connect("env-merge-info", merge_driverproperties)

    app.add_node(driver_index)
    app.add_directive("driver_index", DriverIndex)
    app.connect("doctree-resolved", create_driver_index)

    return {"parallel_read_safe": True, "parallel_write_safe": True}


def visit_admonition_generic(self, node):
    self.visit_admonition(node)


def depart_node_generic(self, node):
    self.depart_admonition(node)


def visit_admonition_html(self, node, name: str = ""):
    self.body.append(self.starttag(node, "div", CLASS=("admonition " + name)))


def depart_node_html(self, node):
    self.body.append("</div>\n")


class shortname(nodes.Admonition, nodes.Element):
    pass


def visit_shortname_node_html(self, node):
    self.body.append(self.starttag(node, "div", CLASS=("admonition shortname")))


class built_in_by_default(nodes.Admonition, nodes.Element):
    pass


def visit_built_in_by_default_node_html(self, node):
    self.body.append(
        self.starttag(node, "div", CLASS=("admonition built_in_by_default"))
    )


class build_dependencies(nodes.Admonition, nodes.Element):
    pass


def visit_build_dependencies_node_html(self, node):
    self.body.append(
        self.starttag(node, "div", CLASS=("admonition build_dependencies"))
    )


class supports_create(nodes.Admonition, nodes.Element):
    pass


def visit_supports_create_node_html(self, node):
    self.body.append(self.starttag(node, "div", CLASS=("admonition supports_create")))


class supports_createcopy(nodes.Admonition, nodes.Element):
    pass


def visit_supports_createcopy_node_html(self, node):
    self.body.append(
        self.starttag(node, "div", CLASS=("admonition supports_createcopy"))
    )


class supports_georeferencing(nodes.Admonition, nodes.Element):
    pass


def visit_supports_georeferencing_node_html(self, node):
    self.body.append(
        self.starttag(node, "div", CLASS=("admonition supports_georeferencing"))
    )


class supports_virtualio(nodes.Admonition, nodes.Element):
    pass


def visit_supports_virtualio_node_html(self, node):
    self.body.append(
        self.starttag(node, "div", CLASS=("admonition supports_virtualio"))
    )


class supports_multidimensional(nodes.Admonition, nodes.Element):
    pass


def visit_supports_multidimensional_node_html(self, node):
    self.body.append(
        self.starttag(node, "div", CLASS=("admonition supports_multidimensional"))
    )


class deprecated_driver(nodes.Admonition, nodes.Element):
    pass


def visit_deprecated_driver_node_html(self, node):
    self.body.append(self.starttag(node, "div", CLASS=("danger deprecated_driver")))


def finish_directive(_self, directive, node):

    env = _self.state.document.settings.env

    targetid = "%s-%d" % (directive, env.new_serialno(directive))
    targetnode = nodes.target("", "", ids=[targetid])

    _self.state.nested_parse(_self.content, _self.content_offset, node)

    if not hasattr(env, "all_" + directive):
        setattr(env, "all_" + directive, [])
    getattr(env, "all_" + directive).append(
        {
            "docname": env.docname,
            "lineno": _self.lineno,
            directive: node.deepcopy(),
            "target": targetnode,
        }
    )

    return [targetnode, node]


class DriverPropertyDirective(SphinxDirective):
    def driver_properties(self):

        if not hasattr(self.env, "gdal_drivers"):
            self.env.gdal_drivers = {}

        # Search for a .. shortname:: directive in the current document
        try:
            short_name_node = next(self.env.parser.document.findall(shortname))
            short_name = short_name_node.children[1].astext()
            if "raster" in self.env.docname:
                short_name += "_raster"
            else:
                short_name += "_vector"

        except StopIteration:
            if "plscenes" in self.env.docname:
                return {}

            logger = logging.getLogger(__name__)
            logger.warning(
                "Driver does not have a 'shortname' directive.",
                location=self.env.docname,
            )

            return {}

        if short_name not in self.env.gdal_drivers:
            # Initialize driver properties object
            try:
                long_name_node = next(self.env.parser.document.findall(nodes.title))
            except StopIteration:
                logger = logging.getLogger(__name__)
                logger.warning(
                    "Driver document does not have a title.",
                    location=self.env.docname,
                )

                return {}

            long_name = long_name_node.astext()
            if " -- " in long_name:
                long_name = long_name.split(" -- ")[1].strip()
            if (
                " - " in long_name
                and "OGC API" not in long_name
                and "NetCDF" not in long_name
            ):
                long_name = long_name.split(" - ")[1].strip()

            self.env.gdal_drivers[short_name] = {
                "docname": self.env.docname,
                "short_name": short_name,
                "long_name": long_name,
                "built_in_by_default": False,
                "supports_create": False,
                "supports_createcopy": False,
                "supports_georeferencing": False,
                "supports_virtualio": False,
                "supports_multidimensional": False,
                "deprecated": False,
                "requirements": "",
            }

        return self.env.gdal_drivers[short_name]


class ShortName(SphinxDirective):

    # this enables content in the directive
    has_content = True

    def run(self):

        node = shortname("\n".join(self.content))
        node += nodes.title(_("Driver short name"), _("Driver short name"))

        return finish_directive(self, "shortname", node)


class BuiltInByDefault(DriverPropertyDirective):

    # this enables content in the directive
    has_content = True

    def run(self):
        self.driver_properties()["built_in_by_default"] = True

        if not self.content:
            self.content = docutils.statemachine.StringList(
                ["This driver is built-in by default"]
            )
        node = built_in_by_default("\n".join(self.content))
        node += nodes.title(
            _("Driver built-in by default"), _("Driver built-in by default")
        )

        return finish_directive(self, "built_in_by_default", node)


class BuildDependencies(DriverPropertyDirective):

    # this enables content in the directive
    has_content = True

    def run(self):
        self.driver_properties()["requirements"] = ", ".join(self.content)

        assert (
            self.content
        ), "Content should be defined for build_dependencies directive"
        node = build_dependencies("\n".join(self.content))
        node += nodes.title(_("Build dependencies"), _("Build dependencies"))

        return finish_directive(self, "build_dependencies", node)


class CreateDirective(DriverPropertyDirective):

    # this enables content in the directive
    has_content = True

    def run(self):
        self.driver_properties()["supports_create"] = True

        if not self.content:
            self.content = docutils.statemachine.StringList(
                ["This driver supports the :cpp:func:`GDALDriver::Create` operation"]
            )
        node = supports_create("\n".join(self.content))
        node += nodes.title(_("Supports Create()"), _("Supports Create()"))

        return finish_directive(self, "supports_create", node)


class CreateCopyDirective(DriverPropertyDirective):

    # this enables content in the directive
    has_content = True

    def run(self):
        self.driver_properties()["supports_createcopy"] = True

        if not self.content:
            self.content = docutils.statemachine.StringList(
                [
                    "This driver supports the :cpp:func:`GDALDriver::CreateCopy` operation"
                ]
            )
        node = supports_createcopy("\n".join(self.content))
        node += nodes.title(_("Supports CreateCopy()"), _("Supports CreateCopy()"))

        return finish_directive(self, "supports_createcopy", node)


class GeoreferencingDirective(DriverPropertyDirective):

    # this enables content in the directive
    has_content = True

    def run(self):
        self.driver_properties()["supports_georeferencing"] = True

        if not self.content:
            self.content = docutils.statemachine.StringList(
                ["This driver supports georeferencing"]
            )
        node = supports_georeferencing("\n".join(self.content))
        node += nodes.title(_("Supports Georeferencing"), _("Supports Georeferencing"))

        return finish_directive(self, "supports_georeferencing", node)


class VirtualIODirective(DriverPropertyDirective):

    # this enables content in the directive
    has_content = True

    def run(self):
        self.driver_properties()["supports_virtualio"] = True

        if not self.content:
            self.content = docutils.statemachine.StringList(
                [
                    "This driver supports :ref:`virtual I/O operations (/vsimem/, etc.) <virtual_file_systems>`"
                ]
            )
        node = supports_virtualio("\n".join(self.content))
        node += nodes.title(_("Supports VirtualIO"), _("Supports VirtualIO"))

        return finish_directive(self, "supports_virtualio", node)


class MultiDimensionalDirective(DriverPropertyDirective):

    # this enables content in the directive
    has_content = True

    def run(self):
        self.driver_properties()["supports_multidimensional"] = True

        if not self.content:
            self.content = docutils.statemachine.StringList(
                ["This driver supports the :ref:`multidim_raster_data_model`"]
            )
        node = supports_virtualio("\n".join(self.content))
        node += nodes.title(
            _("Supports multidimensional API"), _("Supports multidimensional API")
        )

        return finish_directive(self, "supports_multidimensional", node)


class DeprecatedDriverDirective(DriverPropertyDirective):

    # this enables content in the directive
    has_content = True

    def run(self):
        self.driver_properties()["deprecated"] = True

        explanation = []
        version_targeted_for_removal = [
            c[len("version_targeted_for_removal:") :].strip()
            for c in self.content
            if c.startswith("version_targeted_for_removal:")
        ]
        if version_targeted_for_removal:
            explanation.append(
                "This driver is considered for removal in GDAL {}.".format(
                    version_targeted_for_removal[0]
                )
            )
        else:
            explanation.append(
                "This driver is considered for removal in a future GDAL release."
            )

        message = [
            c[len("message:") :].strip()
            for c in self.content
            if c.startswith("message:")
        ]
        if message:
            explanation.append(message[0])
        else:
            explanation.append(
                "You are invited to convert any dataset in that format to another more common one."
            )

        explanation.append(
            "If you need this driver in future GDAL versions, create a ticket at https://github.com/OSGeo/gdal "
            "(look first for an existing one first) to explain how critical it is for you "
            "(but the GDAL project may still remove it)."
        )

        env_variable = [
            c[len("env_variable:") :].strip()
            for c in self.content
            if c.startswith("env_variable:")
        ]
        if env_variable:
            explanation.append(
                "To enable use of the deprecated driver the {} configuration option /"
                " environment variable must be set to YES.".format(env_variable[0])
            )

        self.content = docutils.statemachine.StringList(explanation)

        node = deprecated_driver("\n".join(self.content))
        node += nodes.title(_("Deprecated"), _("Deprecated"))

        return finish_directive(self, "deprecated_driver", node)


### Driver Index


class driver_index(nodes.General, nodes.Element):
    pass


class DriverIndex(SphinxDirective):

    has_content = True
    required_arguments = 0
    option_spec = {"type": str}

    def run(self):
        index_placeholder = driver_index("")

        if "type" in self.options:
            index_placeholder["driver_type"] = self.options["type"]

        return [index_placeholder]


def create_driver_index(app, doctree, fromdocname):
    # This event handler is called on "doctree-resolved"
    # It replaces the driver_index placeholder(s) with
    # tables of drivers.

    env = app.builder.env

    if not hasattr(env, "gdal_drivers"):
        env.gdal_drivers = {}

    for node in doctree.findall(driver_index):

        if "driver_type" in node and node["driver_type"] == "vector":
            columns = {
                "short_name": {"name": "Short Name", "width": 10},
                "long_name": {"name": "Long Name", "width": 20},
                "supports_create": {"name": "Creation", "width": 10},
                "supports_georeferencing": {"name": "Georeferencing", "width": 10},
                "requirements": {"name": "Build Requirements", "width": 20},
            }
        else:
            columns = {
                "short_name": {"name": "Short Name", "width": 10},
                "long_name": {"name": "Long Name", "width": 35},
                "supports_create": {"name": "Creation (1)", "width": 10},
                "supports_createcopy": {"name": "Copy (2)", "width": 10},
                "supports_georeferencing": {"name": "Georeferencing", "width": 10},
                "requirements": {"name": "Build Requirements", "width": 25},
            }

        tgroup = nodes.tgroup(cols=len(columns))

        for col in columns:
            colspec = nodes.colspec(colwidth=columns[col]["width"])
            tgroup.append(colspec)

        header = nodes.row()
        for col in columns:
            header.append(nodes.entry("", nodes.paragraph(text=columns[col]["name"])))
        tgroup.append(nodes.thead("", header))

        tbody = nodes.tbody()

        for short_name in sorted(env.gdal_drivers, key=str.casefold):
            driver_properties = env.gdal_drivers[short_name]

            if (
                "driver_type" in node
                and node["driver_type"] not in driver_properties["docname"]
            ):
                continue

            row = nodes.row()

            for col in columns:
                cell = nodes.entry()
                row.append(cell)

                if col == "short_name":
                    ref = nodes.reference(
                        refuri=app.builder.get_relative_uri(
                            fromdocname, driver_properties["docname"]
                        ),
                        internal=True,
                    )
                    ref.append(
                        nodes.Text(
                            short_name.replace("_raster", "").replace("_vector", "")
                        )
                    )
                    para = nodes.paragraph()
                    para.append(ref)
                    cell.append(para)
                else:
                    value = driver_properties[col]

                    if col == "requirements" and not value:
                        if driver_properties["built_in_by_default"]:
                            value = "Built-in by default"

                    if type(value) is bool:
                        if value:
                            cell.append(nodes.strong("Yes", "Yes"))
                        else:
                            cell.append(nodes.Text("No"))
                    else:
                        cell.append(nodes.Text(value))

            tbody.append(row)

        tgroup.append(tbody)

        table = nodes.table("", tgroup)
        node.replace_self(table)


def merge_driverproperties(app, env, docnames, other):
    if not hasattr(env, "gdal_drivers"):
        env.gdal_drivers = {}

    if hasattr(other, "gdal_drivers"):
        for k, v in other.gdal_drivers.items():
            env.gdal_drivers[k] = v


def purge_driverproperties(app, env, docname):
    if hasattr(env, "gdal_drivers"):
        env.gdal_drivers = {
            k: v for k, v in env.gdal_drivers.items() if v["docname"] != docname
        }

    for directive in [
        "all_shortname",
        "all_built_in_by_default",
        "all_build_dependencies",
        "all_supports_create",
        "all_supports_createcopy",
        "all_supports_georeferencing",
        "all_supports_virtualio",
        "all_supports_multidimensional",
        "all_deprecated_driver",
    ]:
        if hasattr(env, directive):
            setattr(
                env,
                directive,
                [
                    embed
                    for embed in getattr(env, directive)
                    if embed["docname"] != docname
                ],
            )
