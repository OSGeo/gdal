import re

from docutils import nodes
from sphinx.addnodes import pending_xref
from sphinx.errors import NoUri
from sphinx.transforms.post_transforms import SphinxPostTransform
from sphinx.util import logging
from sphinx.util.docutils import SphinxDirective

### Utility functions


def option_key(*, option_type, option_name, docname):
    # Options of type "config" are considered to be global
    # in scope, while other types are associated with
    # a specific driver. Use docname as a proxy for driver.
    if option_type == "config":
        return f"{option_type}-{option_name}"
    else:
        return f"{docname}-{option_type}-{option_name}"


def split_option_key(key):
    """
    Return option_type, name
    """
    return key.split("-")[-2:]


def parse_choices(text):
    # split on commas, except those that are escaped with \
    # strip whitespace and remove escape character \
    return [x.replace("\\,", ",").strip() for x in re.split(r"(?<![\\]),", text)]


def version_at_least(a, b):
    a_parts = [int(x) for x in a.split(".")]
    b_parts = [int(x) for x in b.split(".")]

    while len(a_parts) < len(b_parts):
        a_parts.append(0)

    while len(b_parts) < len(a_parts):
        b_parts.append(0)

    return a_parts >= b_parts


class Option:
    # Class to store properties of an option and where it was
    # defined.

    def __init__(
        self,
        *,
        option_type,
        option_name,
        docname,
        lineno,
        required,
        choices,
        since_ver,
        default,
        content_node,
    ):
        self.option_type = option_type
        self.option_name = option_name
        self.docname = docname
        self.lineno = lineno
        self.required = required
        self.since_ver = since_ver
        self.content_node = content_node
        self.choices = choices
        self.default = default

        self.para = None

    def __eq__(self, other):
        return (
            self.option_type == other.option_type
            and self.option_name == other.option_name
            and self.docname == other.docname
            and self.lineno == other.lineno
        )

    def key(self):
        return option_key(
            option_type=self.option_type,
            option_name=self.option_name,
            docname=self.docname,
        )

    def render(self, app):
        if self.para:
            return self.para

        para = nodes.paragraph()

        # Option name and choices
        text = self.option_name

        if self.choices is None:
            text += "=value: "
        elif len(self.choices) == 1:
            text += f"={self.choices[0]}: "
        else:
            text += f'=[{"/".join(self.choices)}]: '

        para += nodes.strong(text, text)

        caveats = []

        # Required flag
        if self.required:
            para += caveats.append(nodes.Text("required"))

        if self.since_ver is not None:
            min_since_ver = app.config.options_since_ignore_before

            try:
                if not min_since_ver or version_at_least(self.since_ver, min_since_ver):
                    caveats.append(
                        nodes.Text(f"{app.config.project} >= {self.since_ver}")
                    )
            except ValueError:
                logger = logging.getLogger(__name__)
                logger.warning(
                    f"Option {self.option_name} :since: should be a sequence of integers and periods (got {self.since_ver})",
                    location=self.env.docname,
                )

        if caveats:
            para += nodes.Text(" (")
            for i, caveat in enumerate(caveats):
                if i > 0:
                    para += nodes.Text(", ")
                para += caveat
            para += nodes.Text(") ")

        if self.default:
            # Process argument like "0 (lossless)" so that only
            # the value is rendered as a literal.
            default_split = self.default.split(" ", 1)
            default_value = default_split[0]

            para += nodes.Text(" Defaults to ")
            para += nodes.literal(default_value, default_value)

            if len(default_split) > 1:
                para += nodes.Text(f" {default_split[1]}")

            para += nodes.Text(". ")

        # We need to copy the content in case this option block contains
        # references and is rendered in multiple locations. If we do
        # not copy, the cross-references in the second rendering will
        # not be resolved.
        contents = self.content_node.deepcopy()

        if len(contents.children) > 0:
            para += contents.children[0].children
        if len(contents.children) > 1:
            para += contents.children[1:]

        self.para = para

        return para


def register_option(env, opt):
    if not hasattr(env, "gdal_options"):
        env.gdal_options = {}

    orig_opt = env.gdal_options.get(opt.key(), None)
    if orig_opt is not None and orig_opt != opt:
        # Ignore exact duplicates that may arise during parallel processing.
        # If the options differ in any way (docname, lineno, etc.)
        # then raise a warning.
        logger = logging.getLogger(__name__)
        logger.warning(
            f"Duplicate definition of {opt.option_name} (previously defined at {orig_opt.docname}:{orig_opt.lineno})",
            location=(opt.docname, opt.lineno),
        )
    else:
        env.gdal_options[opt.key()] = opt


### Directives for declaring configuration options


class BaseConfigOption(SphinxDirective):

    has_content = True
    required_arguments = 1
    option_spec = {
        "since": str,
        "choices": parse_choices,
        "default": str,
        "required": str,
    }

    def run(self):
        option_name = self.arguments[0]

        if "required" in self.options:
            if self.options["required"].upper() in {"TRUE", "YES"}:
                required = True
            elif self.options["required"].upper() in {"FALSE", "NO"}:
                required = False
            else:
                required = False
                logger = logging.getLogger(__name__)
                logger.warning(
                    f"Option {option_name} :required: should be YES or NO)",
                    location=self.env.docname,
                )
        else:
            required = False

        ref_key = option_key(
            option_type=self.opt_type,
            option_name=option_name,
            docname=self.env.docname,
        )

        target_node = nodes.target("", "", ids=[ref_key])

        # Parse the option description into a throwaway node.  This lets us
        # flatten the parsed content so that the first piece of text appears on
        # the same line as the config option signature.
        content_node = nodes.Element()
        self.state.nested_parse(self.content, self.content_offset, content_node)
        # Record information about this option and where it was
        # defined in our environment, so we can process cross
        # references later.
        opt = Option(
            option_type=self.opt_type,
            option_name=option_name,
            docname=self.env.docname,
            lineno=self.lineno,
            required=required,
            choices=self.options.get("choices", None),
            default=self.options.get("default", None),
            since_ver=self.options.get("since", None),
            content_node=content_node,
        )

        register_option(self.env, opt)

        return [target_node, opt.render(self.env.app)]


class ConfigOption(BaseConfigOption):
    opt_type = "config"


class CreationOption(BaseConfigOption):
    opt_type = "co"


class DatasetCreationOption(BaseConfigOption):
    opt_type = "dsco"


class LayerCreationOption(BaseConfigOption):
    opt_type = "lco"


class OpenOption(BaseConfigOption):
    opt_type = "oo"


### Placeholder node for a copy of a config definition that will be filled in
### later by WriteOptionDefinitions.


def visit_config_definition(self, node):
    pass


def depart_config_definition(self, node):
    pass


class config_definition(nodes.General, nodes.Element):
    pass


### Roles to create either a reference to an option's definition or a
### copy of the definition itself.


def config_ref(opt_type, *, copy_definition=False):
    def role(name, rawtext, text, lineno, inliner, options={}, content=[]):
        env = inliner.document.settings.env

        # Role content is expected to be of the following format:
        # :role:`[document ]option_name[=VALUE]`
        #
        # If we don't specify [document], assume it is on the current page.
        # This only matters for driver-specific options (open options, creation
        # options, etc.) For configuration options the document is ignored.
        if " " in text:
            docname, text = text.split(" ", 1)
        else:
            docname = env.docname

        # Parse the option name, stripping a supplied value (if any).
        # This allows a reference like :co:`APPEND_SUBDATASET=YES`
        # to link to APPEND_SUBDATASET
        split_text = text.rsplit("=", 1)
        option = split_text[0]

        # Record the document from which this reference was
        # used. This lets us build a reverse index showing
        # where each configuration option is used.
        if not hasattr(env, "gdal_option_refs"):
            env.gdal_option_refs = {}

        ref_key = option_key(option_type=opt_type, option_name=option, docname=docname)

        if ref_key not in env.gdal_option_refs:
            env.gdal_option_refs[ref_key] = []

        env.gdal_option_refs[ref_key].append({"document": env.docname})

        # Emit a placeholder node that describes the config
        # option we're trying to reference. After all files
        # have been parsed and we've discovered where each
        # option is defined, we can go back and replace the
        # placeholder nodes with actual references.
        ref_text = nodes.literal(text, text)

        if copy_definition:
            opt_placeholder = config_definition("", ref_text)
            opt_placeholder["key"] = ref_key

            return [opt_placeholder], []
        else:
            ref_node = pending_xref(
                "",
                ref_text,
                reftype=opt_type,
                refdomain="std",
                reftarget=ref_key,
            )

            return [ref_node], []

    return role


### Functions to update the cached environment when source files
### change, or when environments of parallel build workers are
### combined.


def purge_option_defs(app, env, docname):
    if not hasattr(env, "gdal_options"):
        return

    env.gdal_options = {
        k: v for k, v in env.gdal_options.items() if v.docname != docname
    }


def purge_option_refs(app, env, docname):
    if not hasattr(env, "gdal_option_refs"):
        return

    for key in env.gdal_option_refs:
        env.gdal_option_refs[key] = [
            x for x in env.gdal_option_refs[key] if x["document"] != docname
        ]


def merge_option_defs(app, env, docnames, other):
    if hasattr(other, "gdal_options"):
        for opt in other.gdal_options.values():
            register_option(env, opt)


def merge_option_refs(app, env, docnames, other):
    if not hasattr(env, "gdal_option_refs"):
        env.gdal_option_refs = {}
    if hasattr(other, "gdal_option_refs"):
        for k, v in other.gdal_option_refs.items():
            if k in env.gdal_option_refs:
                env.gdal_option_refs[k] += v
            else:
                env.gdal_option_refs[k] = v.copy()


### Code to support construction of a config option index


class ConfigIndex(SphinxDirective):

    has_content = True
    required_arguments = 0
    option_spec = {"types": str}

    def run(self):
        if "types" not in self.options:
            types = set(option_classes.keys())
        else:
            types = {x.strip() for x in self.options["types"].split(",")}

        # TODO use proper constructor for config_index(?)
        index_placeholder = config_index("")
        index_placeholder["types"] = types

        return [index_placeholder]


class config_index(nodes.General, nodes.Element):
    """
    Placeholder class marking the location of a config option index, to be created later.
    """

    pass


def create_config_index(app, doctree, fromdocname):
    # This event handler is called on "doctree-resolved"
    # It replaces the config_index placeholder(s) with
    # lists of config options and the documents that
    # reference them.

    env = app.builder.env

    if not hasattr(env, "gdal_option_refs"):
        env.gdal_option_refs = {}

    if not hasattr(env, "gdal_options"):
        env.gdal_options = {}

    for node in doctree.findall(config_index):
        # Filter out the options that will be included
        # in this index.

        options = [
            opt for opt in env.gdal_options.values() if opt.option_type in node["types"]
        ]
        options.sort(key=lambda x: x.option_name)

        list_node = nodes.bullet_list()

        for opt in options:
            refs = env.gdal_option_refs.get(opt.key(), [])

            # Include a link to the definition of
            # the option along with usages.
            refs.append({"document": opt.docname})

            if refs:
                para = nodes.paragraph()

                # Link the option back to its definition.
                opt_name = nodes.literal(opt.option_name, opt.option_name)
                def_ref = nodes.reference(
                    "",
                    "",
                    refuri=app.builder.get_relative_uri(fromdocname, opt.docname)
                    + "#"
                    + opt.key(),
                    internal=True,
                )
                def_ref.append(opt_name)
                para += def_ref

                para += nodes.Text(": ")

                # Create a link for each unique page referencing the option.
                # TODO sort by document title instead of document name?
                ref_docs = sorted({ref["document"] for ref in refs})

                bullets_for_references = len(ref_docs) > 1

                if bullets_for_references:
                    ref_node_parent = nodes.bullet_list()
                    para.append(ref_node_parent)
                else:
                    ref_node_parent = para

                for ref_doc in ref_docs:
                    ref_title = str(env.titles[ref_doc].children[0])

                    try:
                        ref_node = nodes.reference(
                            "",
                            ref_title,
                            refuri=app.builder.get_relative_uri(fromdocname, ref_doc),
                            internal=True,
                        )

                        if bullets_for_references:
                            ref_para = nodes.paragraph()
                            ref_para += ref_node
                            ref_li = nodes.list_item("", ref_para)
                            ref_node_parent.append(ref_li)
                        else:
                            ref_node_parent.append(ref_node)
                    except NoUri:
                        # this is thrown when linking to a document that is not
                        # included in output for a certain format, e.g. RFC
                        # documents not included in the PDF build.
                        pass

                li_node = nodes.list_item("", para)
                list_node.append(li_node)

        node.replace_self(list_node)


### Code to resolve references to config options, either
### by creating a link or reproducing the entire definition
### of the config option.


def link_option_refs(app, env, node, contnode):
    # This event handler is called on "missing-reference"
    # It resolves the pending_xrefs created by e.g.,
    # :config:`GDAL_CACHEMAX` into actual references.
    if node["reftype"] not in option_classes.keys():
        return

    # logger = logging.getLogger(__name__)
    # logger.info(
    #    f"Linking {node['reftarget']}",
    #    location=env.docname,
    # )

    matched_opt = env.gdal_options.get(node["reftarget"], None)

    if matched_opt is None:
        logger = logging.getLogger(__name__)
        option_type, option_name = split_option_key(node["reftarget"])

        logger.warning(
            f"Can't find option {option_name} of type {option_type}",
            location=node,
        )

        return contnode

    from_doc = node["refdoc"]
    to_doc = matched_opt.docname

    refuri = app.builder.get_relative_uri(from_doc, to_doc)

    ref_node = nodes.reference(
        "", "", refuri=refuri + "#" + node["reftarget"], internal=True
    )

    ref_node += node.children

    return ref_node


class WriteOptionDefinitions(SphinxPostTransform):
    # Post-transformation that replaces references to
    # config options created by e.g. :copy-config:`PG_USE_COPY`
    # with duplicates of the option definitions.

    default_priority = 4

    def run(self, **kwargs):
        for node in self.document.findall(config_definition):
            key = node["key"]

            opt = self.env.gdal_options.get(key, None)

            if opt:
                def_block = opt.render(self.app)
                node.replace_self(def_block)
            else:
                option_type, option_name = split_option_key(node["key"])

                logger = logging.getLogger(__name__)
                logger.warning(
                    f"Can't find option {option_name} of type {option_type}",
                    location=node,
                )

                node.replace_self(node.children[0])


def log_options(app, env):
    # This event handler is called on "env-updated"

    logger = logging.getLogger(__name__)

    logger.info(
        f"Identified {len(env.gdal_options)} GDAL options with {len(env.gdal_option_refs)} references."
    )


option_classes = {
    "config": ConfigOption,
    "co": CreationOption,
    "dsco": DatasetCreationOption,
    "lco": LayerCreationOption,
    "oo": OpenOption,
}


def setup(app):
    app.add_node(
        config_definition,
        html=(visit_config_definition, depart_config_definition),
        latex=(visit_config_definition, depart_config_definition),
    )
    app.add_node(config_index)

    app.add_config_value("options_since_ignore_before", None, "html")

    for opt_type, opt_directive in option_classes.items():
        app.add_directive(f"{opt_type}", opt_directive)
        app.add_role(opt_type, config_ref(opt_type))
        app.add_role(f"copy-{opt_type}", config_ref(opt_type, copy_definition=True))

    app.add_directive("config_index", ConfigIndex)

    app.connect("doctree-resolved", create_config_index)
    app.connect("env-purge-doc", purge_option_defs)
    app.connect("env-merge-info", merge_option_defs)
    app.connect("env-purge-doc", purge_option_refs)
    app.connect("env-merge-info", merge_option_refs)

    app.connect("missing-reference", link_option_refs)

    app.add_post_transform(WriteOptionDefinitions)

    app.connect("env-updated", log_options)

    return {"version": "0.1", "parallel_read_safe": True, "parallel_write_safe": True}
