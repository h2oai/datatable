
.. xdata:: datatable.build_info
    :src: --

    This is a python struct that contains information about the installed
    datatable module. The following fields are available:

    .. xparam:: .version: str

        The version string of the current build. Several formats of
        the version string are possible:

        - ``{MAJOR}.{MINOR}.{MICRO}`` -- the release version string,
          such as ``"0.11.0"``.

        - ``{RELEASE}a{DEVNUM}`` -- version string for the development
          build of datatable, where ``{RELEASE}`` is the normal release
          string and ``{DEVNUM}`` is an integer that is incremented with
          each build. For example: ``"0.11.0a1776"``.

        - ``{RELEASE}a0+{SUFFIX}`` -- version string for a PR build of
          datatable, where the ``{SUFFIX}`` is formed from the PR
          number and the build sequence number. For example,
          ``"0.11.0a0+pr2602.13"``.

        - ``{RELEASE}a0+{FLAVOR}.{TIMESTAMP}.{USER}`` -- version string
          used for local builds. This contains the "flavor" of the build,
          such as normal build, or debug, or coverage, etc; the unix
          timestamp of the build; and lastly the system user name of the
          user who made the build.


    .. xparam:: .build_date: str

        UTC timestamp (date + time) of the build.

    .. xparam:: .build_mode: str

        The type of datatable build. Usually this will be ``"release"``, but
        may also be ``"debug"`` if datatable was built in debug mode. Other
        build modes exist, or may be added in the future.

    .. xparam:: .compiler: str

        The version of the compiler used to build the C++ datatable extension.
        This will include both the name and the version of the compiler.

    .. xparam:: .git_revision: str

        Git-hash of the revision from which the build was made, as
        obtained from ``git rev-parse HEAD``.


    .. xparam:: .git_branch: str

        Name of the git branch from where the build was made. This will
        be obtained from environment variable ``CHANGE_BRANCH`` if defined,
        or from command ``git rev-parse --abbrev-ref HEAD`` otherwise.


    .. xparam:: .git_date: str

        .. x-version-added:: 0.11

        Timestamp of the git commit from which the build was made.


    .. xparam:: .git_diff: str

        .. x-version-added:: 0.11

        If the source tree contains any uncommitted changes (compared
        to the checked out git revision), then the summary of these
        changes will be in this field, as reported by
        ``git diff HEAD --stat --no-color``. Otherwise, this field
        is an empty string.
