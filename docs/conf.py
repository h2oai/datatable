# -*- coding: utf-8 -*-
#
# Configuration file for the Sphinx documentation builder.
#
# This file does only contain a selection of the most common options. For a
# full list see the documentation:
# http://www.sphinx-doc.org/en/master/config

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
import docutils
import os
import subprocess
import sys
sys.path.insert(0, os.path.abspath('.'))


# -- Project information -----------------------------------------------------

project = 'datatable'
copyright = '2018-2020, H2O.ai'
author = 'Pasha Stetsenko'

try:
    import datatable
    version = datatable.__version__

except ImportError:
    verfile = os.path.abspath(os.path.join(os.path.dirname(__file__),
                              "../VERSION.txt"))
    if os.path.isfile(verfile):
        with open(verfile, "rt") as inp:
            version = inp.read().strip()
    else:
        version = ""


# -- General configuration ---------------------------------------------------

# If your documentation needs a minimal Sphinx version, state it here.
#
needs_sphinx = '1.8'

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [
    '_ext.xcode',
    '_ext.xcontributors',
    '_ext.xfunction',
    '_ext.xpython',
    '_ext.changelog',
    'sphinx.ext.mathjax',
    'sphinx.ext.napoleon',
    'sphinx.ext.intersphinx',  # links to external documentation
]

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# The suffix(es) of source filenames.
# You can specify multiple suffix as a list of string:
source_suffix = ['.rst', '.md']

# The main toctree document.
master_doc = 'index'

primary_domain = 'xpy'

# The language for content autogenerated by Sphinx. Refer to documentation
# for a list of supported languages.
#
# This is also used if you do content translation via gettext catalogs.
# Usually you set "language" from the command line for these cases.
language = None

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store',
                    '**.ipynb_checkpoints']

# The name of the Pygments (syntax highlighting) style to use.
pygments_style = 'sphinx'


# -- Options for intersphinx extension ---------------------------------------

intersphinx_mapping = {
    'python': ('https://docs.python.org/3', None),
    'IPython': ('https://ipython.readthedocs.io/en/stable/', None),
}


# -- Options for Changelog extension -----------------------------------------

changelog_issue_url = "https://github.com/h2oai/datatable/issues/{issue}"

changelog_user_url = "https://github.com/{name}"


# -- Options for XFunction extension -----------------------------------------

xf_module_name = "datatable"

xf_project_root = ".."

try:
    _ghcommit = subprocess.check_output(["git", "rev-parse", "HEAD"],
                                        universal_newlines=True).strip()
    xf_permalink_url0 = ("https://github.com/h2oai/datatable/blob/" +
                         _ghcommit + "/{filename}")
    xf_permalink_url2 = xf_permalink_url0 + "#L{line1}-L{line2}"

except subprocess.CalledProcessError:
    pass



# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
html_theme = 'wren'
# html_theme = 'sphinx_rtd_theme'

# Theme options are theme-specific and customize the look and feel of a theme
# further.  For a list of options available for each theme, see the
# documentation.
#
# See: https://sphinx-rtd-theme.readthedocs.io/en/latest/configuring.html
html_theme_options = {}
html_favicon = "_static/datatable_small_icon.ico"

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ['_static']



# -- Custom setup ------------------------------------------------------------
class TitleCollector(docutils.nodes.SparseNodeVisitor):
    def __init__(self, document):
        self.level = 0
        self.titles = []
        super().__init__(document)

    def visit_section(self, node):
        title_class = docutils.nodes.title
        self.level += 1
        if node.children and isinstance(node.children[0], title_class):
            title = node.children[0].astext()
            node_id = node.get("ids")[0]
            self.titles.append([title, node_id, self.level])

    def depart_section(self, node):
        self.level -= 1



def get_local_toc(document):
    if not document:
        return ""
    visitor = TitleCollector(document)
    document.walkabout(visitor)
    titles = visitor.titles
    if not titles:
        return ""

    levels = sorted(set(item[2] for item in titles))
    if levels.index(titles[0][2]) != 0:
        return document.reporter.error(
            "First title on the page is not <h1/>")
    del titles[0]  # remove the <h1> title

    h1_seen = False
    ul_level = 0
    html_text = "<div id='toc-local' class='list-group'>\n"
    html_text += " <b>Table of contents</b>\n"
    for title, node_id, level in titles:
        if level <= 1:
            return document.reporter.error("More than one <h1> title on the page")
        html_text += f"  <a href='#{node_id}' class='list-group-item level-{level-1}'>{title}</a>\n"
    html_text += "</div>\n"
    return html_text



# Emitted when the HTML builder has created a context dictionary to render
# a template with – this can be used to add custom elements to the context.
def on_html_page_context(app, pagename, templatename, context, doctree):
    context["get_local_toc"] = lambda: get_local_toc(doctree)


def patch_list_table():
    """
    Modifies docutils' builtin ListTable class so that the rendered table
    is wrapped in a div. This is necessary in order to be able to prevent
    a table from overflowing the width of the page.
    """
    from docutils.parsers.rst.directives.tables import ListTable
    from _ext.xnodes import div

    def new_run(self):
        ret = self._run()
        ret[0] = div(ret[0], classes=["list-table"])
        return ret

    ListTable._run = ListTable.run
    ListTable.run = new_run



def setup(app):
    patch_list_table()

    if html_theme == "wren":
        app.add_css_file("bootstrap.min.css")
        app.add_js_file("bootstrap.min.js")
        app.add_css_file("wren.css")

        thisdir = os.path.dirname(__file__)
        themedir = os.path.abspath(os.path.join(thisdir, "_theme"))
        app.add_html_theme("wren", themedir)

        app.connect("html-page-context", on_html_page_context)

