from docutils import nodes
from sphinx.addnodes import pending_xref
from sphinx.util import logging
from sphinx.util.docutils import SphinxDirective, SphinxRole


class ExampleRole(SphinxRole):
    def run(self):
        return [
            pending_xref(
                "",
                nodes.Text("placeholder"),
                reftype="example",
                refdomain="std",
                reftarget=self.text,
                refdoc=self.env.docname,
            )
        ], []


class ExampleDirective(SphinxDirective):

    required_arguments = 0
    has_content = True

    option_spec = {
        "title": str,
        "id": str,
    }

    def run(self):
        docname = self.env.docname

        if not hasattr(self.env, "gdal_examples"):
            self.env.gdal_examples = {}

        if docname not in self.env.gdal_examples:
            self.env.gdal_examples[docname] = {}

        examples = self.env.gdal_examples[docname]

        example_num = len(examples) + 1

        title_content = self.options.get("title", None)
        if title_content:
            title_content = f"Example {example_num}: {title_content}"
        else:
            title_content = f"Example {example_num}"

        id = self.options.get("id", f"{docname}-{example_num}")
        examples[id] = example_num

        retnodes = []
        section = nodes.section(ids=[id])
        section.document = self.state.document

        title = nodes.title()
        title += self.parse_inline(title_content)[0]
        section.append(title)
        self.state.nested_parse(self.content, self.content_offset, section)

        retnodes.append(section)

        return retnodes


def link_example_refs(app, env, node, contnode):
    if node["reftype"] != "example":
        return

    example_name = node["reftarget"]

    if hasattr(env, "gdal_examples"):
        examples = env.gdal_examples.get(node["refdoc"], {})
    else:
        examples = {}

    example_num = examples.get(example_name, None)

    if not example_num:
        logger = logging.getLogger(__name__)

        logger.warning(
            f"Can't find example {example_name}",
            location=node,
        )

        return contnode

    ref_node = nodes.reference("", "", refid=node["reftarget"], internal=True)
    ref_node.append(nodes.Text(f'Example {examples.get(node["reftarget"])}'))

    return ref_node


def purge_example_defs(app, env, docname):
    if not hasattr(env, "gdal_examples"):
        return

    if docname in env.gdal_examples:
        del env.gdal_examples[docname]


def merge_example_defs(app, env, docnames, other):
    if not hasattr(env, "gdal_examples"):
        env.gdal_examples = {}

    if hasattr(other, "gdal_examples"):
        env.gdal_examples.update(other.gdal_examples)


def setup(app):
    app.add_directive("example", ExampleDirective)
    app.add_role("example", ExampleRole())

    app.connect("env-purge-doc", purge_example_defs)
    app.connect("env-merge-info", merge_example_defs)

    app.connect("missing-reference", link_example_refs)

    return {
        "version": "0.1",
        "parallel_read_safe": True,
        "parallel_write_safe": True,
    }
