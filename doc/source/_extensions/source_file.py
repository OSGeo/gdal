import os

from docutils import nodes


def source_file(name, rawtext, text, lineno, inliner, options={}, content=[]):
    app = inliner.document.settings.env.app

    rootdir = app.config.source_file_root
    fname = text

    if not os.path.exists(os.path.join(rootdir, fname)):
        inliner.reporter.warning(
            f"Referenced source file {fname} not found", line=lineno
        )

    ref_node = nodes.reference(
        rawtext,
        os.path.basename(fname),
        refuri=app.config.source_file_url_template.format(fname),
        **options,
    )

    return [ref_node], []


def setup(app):
    app.add_config_value("source_file_root", None, "html")
    app.add_config_value("source_file_url_template", None, "html")

    app.add_role("source_file", source_file)

    return {"parallel_read_safe": True, "parallel_write_safe": True}
