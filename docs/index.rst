.. raw:: html

  <div id="first-section">
    <div class="part1">
      <img src="_static/py_datatable_logo.png" id="dt-logo" />
    </div>
    <div class="part2">

.. image:: https://img.shields.io/pypi/v/datatable.svg
   :target: https://pypi.org/project/datatable/

.. image:: https://img.shields.io/pypi/l/datatable.svg
   :target: https://github.com/h2oai/datatable/blob/master/LICENSE

.. image:: https://travis-ci.org/h2oai/datatable.svg?branch=master
   :target: https://travis-ci.org/h2oai/datatable

.. raw:: html

      <div class="chyron">
        Python library for efficient multi-threaded data processing, with
        the support for out-of-memory datasets.
      </div>
      <div class="main-toc">
        <div class="large-links">
          <a class="reference internal" href="quick-start.html">
            <i class="fa fa-chevron-circle-right"></i> Getting Started
          </a>
          <a class="reference internal" href="user-guide.html">
            <i class="fa fa-chevron-circle-right"></i> User Guide
          </a>
          <a class="reference internal" href="api-reference.html">
            <i class="fa fa-chevron-circle-right"></i> API Reference
          </a>
        </div>

        <div class="small-links">
          <div><a class="reference internal" href="install.html">
           &#x25AA; Installation
          </a></div>
          <div><a class="reference internal" href="contrib.html">
           &#x25AA; Development
          </a></div>
          <div><a class="reference internal" href="genindex.html">
           &#x25AA; Index
          </a></div>
        </div>
      </div>
    </div>  <!-- div.part2 -->
    </div>  <!-- #first-section -->



Introduction
============

Data is everywhere. From the smallest photon interactions to galaxy collisions,
from mouse movements on a screen to economic developments of countries, we are
surrounded by the sea of information. Human mind cannot comprehend this data in
all its complexity; since ancient times people found it much easier to reduce
the dimensionality, to impose a strict order, to arrange the data points neatly
on a rectangular grid: to make a **data table**.

But once the data has been collected into a table, it has been tamed. It may
still need some grooming and exercise, essentially it is no longer scary. Even
if it is really Big Data, with the **right tools** you can approach it, play
with it, bend to your will, *master*.

Python ``datatable`` module is the right tool for the task. It is a library that
implements a wide (and growing) range of operators for manipulating
two-dimensional data frames. It focuses on: big data support, high performance,
both in-memory and out-of-memory datasets, and multi-threaded algorithms. In
addition, ``datatable`` strives to achieve good user experience, helpful error
messages, and powerful API similar to R ``data.table``'s.


**Contributing**

``datatable`` is an open source project released under the Mozilla Public Licence v2. Open Source projects live by their user and developer communities. We welcome and encourage your contributions of any kind!

No matter what your skill set or level of engagement is with ``datatable``, you can help others by improving the ecosystem of documentation, bug report and feature request tickets, and code.

We invite anyone who is interested to contribute, whether through pull requests, or tests, or GitHub issues, API suggestions, or generic discussion.

**Have Questions?**

If you have questions about using ``datatable``, post them on Stack Overflow using the ``[datatable] [python]`` tags at http://stackoverflow.com/questions/tagged/datatable+python.


.. toctree::
   :maxdepth: 2
   :caption: Installation

   install

.. toctree::
   :maxdepth: 2
   :caption: Common Tasks

   using-datatable

.. toctree::
   :maxdepth: 2
   :caption: Python API

   frame.rst

.. toctree::
   :maxdepth: 2
   :caption: Models

   ftrl.rst
