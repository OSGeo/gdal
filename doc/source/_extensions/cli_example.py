from docutils import nodes
from sphinx.addnodes import pending_xref
from sphinx.util import logging
from sphinx.util.docutils import SphinxDirective, SphinxRole


class Example:
    # class to store properties of an example and where it was
    # defined

    def __init__(self, *, example_id, example_num, docname, lineno):
        self.example_id = example_id
        self.example_num = example_num
        self.docname = docname
        self.lineno = lineno

    def __eq__(self, other):
        if not isinstance(other, Example):
            return NotImplemented

        return self.__dict__ == other.__dict__


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


# Computes an example number for each use of the directive in a
# given document.
# If the example is assigned a reference id (:id:) then store
# (docname, example_num) under that id in env.gdal_examples.
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

        if not hasattr(self.env, "gdal_examples_for_doc"):
            self.env.gdal_examples_for_doc = {}

        if docname not in self.env.gdal_examples_for_doc:
            self.env.gdal_examples_for_doc[docname] = []

        example_num = len(self.env.gdal_examples_for_doc[docname]) + 1

        title_content = self.options.get("title", None)
        if title_content:
            title_content = f"Example {example_num}: {title_content}"
        else:
            title_content = f"Example {example_num}"

        example_id = self.options.get("id")
        if example_id is not None:
            if example_id in self.env.gdal_examples:
                logger = logging.getLogger(__name__)

                prev_example = self.env.gdal_examples[example_id]

                logger.warning(
                    f"Example id '{example_id}' has already been defined at {prev_example.docname}:{prev_example.lineno}",
                    location=(self.env.docname, self.lineno),
                )
                example_id = None
            else:
                self.env.gdal_examples[example_id] = Example(
                    example_id=example_id,
                    docname=docname,
                    lineno=self.lineno,
                    example_num=example_num,
                )

        if example_id is None:
            # nodes.section must always have an id
            example_id = f"{docname}-{example_num}"

        self.env.gdal_examples_for_doc[docname].append(example_id)

        retnodes = []
        section = nodes.section(ids=[example_id])
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

    example_id = node["reftarget"]

    if hasattr(env, "gdal_examples"):
        examples = env.gdal_examples
    else:
        examples = {}

    if example_id not in examples:
        logger = logging.getLogger(__name__)

        logger.warning(
            f"Can't find example {example_id}",
            location=node,
        )

        return contnode

    example = examples[example_id]

    ref_node = nodes.reference("", "", refid=node["reftarget"], internal=True)

    # Set the link text to the example number (e.g, "Example 5") if the example
    # body is in the same document as the reference. Otherwise, set the link
    # text to the program/driver name and example number ("gdalwarp example 5").
    if node["refdoc"] == example.docname:
        link_text = f"Example {example.example_num}"
    else:
        example_doc_title = str(env.titles[example.docname].children[0])

        # TODO: trim the example_doc name, if it's a really long one like
        # WMO General Regularly-distributed Information in Binary form ?
        link_text = f"{example_doc_title} example {example.example_num}"

    ref_node.append(nodes.Text(link_text))

    return ref_node


def purge_example_defs(app, env, docname):
    if not hasattr(env, "gdal_examples"):
        return

    if not hasattr(env, "gdal_examples_for_doc"):
        return

    if docname not in env.gdal_examples_for_doc:
        return

    for example_id in env.gdal_examples_for_doc[docname]:
        if (
            example_id in env.gdal_examples
            and env.gdal_examples[example_id].docname == docname
        ):
            del env.gdal_examples[example_id]

    del env.gdal_examples_for_doc[docname]


def merge_example_defs(app, env, docnames, other):
    if not hasattr(env, "gdal_examples"):
        env.gdal_examples = {}

    if hasattr(other, "gdal_examples"):
        for example_id, example_props in other.gdal_examples.items():
            if example_id in env.gdal_examples:
                logger = logging.getLogger(__name__)

                orig_example = env.gdal_examples[example_id]
                new_example = other.gdal_examples[example_id]

                if new_example != orig_example:
                    logger.warning(
                        f"Example id '{example_id}' has already been defined at {orig_example.docname}:{orig_example.lineno}",
                        location=(new_example.docname, new_example.lineno),
                    )
            else:
                env.gdal_examples[example_id] = example_props

    if not hasattr(env, "gdal_examples_for_doc"):
        env.gdal_examples_for_doc = {}

    if hasattr(other, "gdal_examples_for_doc"):
        env.gdal_examples_for_doc.update(other.gdal_examples_for_doc)


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
