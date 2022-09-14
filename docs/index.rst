.. title:: Home

.. raw:: html

    <style type="text/css">
      div#dtcontainer {
        display: flex;
        flex-direction: column;
        align-items: center;
      }
      div#badges a {
        padding: 1pt 0;
        text-align: right;
      }
      div#collage {
        height: 280px;
        margin: 20px;
        transform: matrix(1, 0, -0.29, 1, 0, 0);
        width: 395px;
      }
      div#collage a.box {
        border-radius: 5px;
        display: inline-block;
        font-family: Verdana;
        position: absolute;
        text-align: center;
        user-select: none;
      }
      div#collage a.box:hover {
        box-shadow: 2px 2px 2px #222;
      }
      div#collage a.box#box1 {
        background-color: #4684b6;
        color: white;
        height: 108pt;
        padding: 28pt 0;
        top: 0;
        width: 111pt;
      }
      a.box#box1 div:first-child { font-size: 22pt; }
      a.box#box1 div:last-child  { font-size: 16pt; }
      div#collage a.box#box2 {
        background-color: #2d659c;
        color: white;
        height: 50pt;
        left: 113pt;
        padding: 11pt 0;
        top: 58pt;
        width: 56pt;
      }
      a.box#box2 div:first-child { font-size: 11pt; }
      a.box#box2 div:last-child  { font-size: 10pt; }
      div#collage a.box#box3 {
        background-color: #a2c1da;
        color: black;
        height: 82pt;
        left: 171pt;
        padding: 10pt 0;
        top: 58pt;
        width: 93pt;
      }
      a.box#box3 div:first-child { font-size: 36pt; }
      a.box#box3 div:last-child  { font-size: 10pt; }
      div#collage a.box#box4 {
        background-color: #2c5735;
        color: white;
        font-size: 12pt;
        height: 22pt;
        left: 25pt;
        padding: 2pt;
        top: 129pt;
        transform: rotate(-90deg);
        width: 60pt;
      }
      div#collage a.box#box5 {
        background-color: #ffde56;
        color: black;
        height: 99pt;
        left: 68pt;
        padding: 17pt 0;
        top: 110pt;
        width: 101pt;
      }
      a.box#box5 div:first-child { font-size: 30pt; }
      a.box#box5 div:last-child  { font-size: 16pt; }
      div#collage a.box#box6 {
        background-color: #a2a84a;
        color: black;
        font-size: 14pt;
        height: 27pt;
        left: 171pt;
        padding: 3pt;
        top: 142pt;
        width: 125pt;
      }
    </style>

    <div id="dtcontainer">
      <div>
        <div id="description"><p>
          <b>Datatable</b> is a python library for manipulating tabular data.
          It supports out-of-memory datasets, multi-threaded data processing,
          and flexible API.
        </p></div>
        <div id="badges">
          <a href="https://pypi.org/project/datatable/">
            <img src="https://img.shields.io/pypi/v/datatable.svg">
          </a>
          <a href="https://ci.appveyor.com/project/h2oops/datatable">
            <img src="https://ci.appveyor.com/api/projects/status/github/h2oai/datatable?branch=main&svg=true">
          </a>
          <a href="https://datatable.readthedocs.io/en/latest/?badge=latest">
            <img src="https://readthedocs.org/projects/datatable/badge/?version=latest">
          </a>
          <a href="https://gitter.im/h2oai/datatable">
            <img src="https://badges.gitter.im/Join%20Chat.svg">
          </a>
        </div>
      </div>
      <div id="collage">
        <a class="box" id="box1" href="start/quick-start.html">
          <div>Getting</div><div>started</div>
        </a>
        <a class="box" id="box2" href="releases/index-releases.html">
          <div>Release</div><div>history</div>
        </a>
        <a class="box" id="box3" href="api/index-api.html">
          <div>API</div><div>reference</div>
        </a>
        <a class="box" id="box4" href="genindex.html">
          Index
        </a>
        <a class="box" id="box5" href="manual/index-manual.html">
          <div>User</div><div>Manual</div>
        </a>
        <a class="box" id="box6" href="develop/index-develop.html">
          Development
        </a>
      </div>
    </div>



.. toctree::
    :hidden:

    Getting Started  <start/index-start>
    User Guide       <manual/index-manual>
    API Reference    <api/index-api>
    Development      <develop/index-develop>
    Release History  <releases/index-releases>
