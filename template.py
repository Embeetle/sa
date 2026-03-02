# Copyright © 2018-2026 Johan Cockx
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

# SPDX-License-Identifier: GPL-3.0-or-later


# This file is a example of how the source analyzer can be used in Python.
# The idea is to use this as template for the real implementation.
# It is also used in regression tests.

import source_analyzer as ce
import LineOffsetTable
import sys
import os
import subprocess
import pathlib
import re
import shlex
import time
import threading
import platform
import inspect
import shutil
import traceback

silent = 0

def eprint(*args, **kwargs):
    if not silent:
        ce._eprint(*args, **kwargs)

# OS name (linux or windows).
# In MSYS UCRT64, platform.system() is 'MINGW64_NT-10.0-19045'
# In a cmd shell and in MSYS MINGW64,  it is 'Windows'
# On Linux, it is 'Linux'
_osname = "linux" if platform.system().lower() == "linux" else "windows"

# Normalize architecture name so it matches your sys folder naming convention.
# Example: amd64/x64 -> x86_64
def _normalize_arch(arch: str) -> str:
    a = (arch or "").lower()
    if a in ("amd64", "x64"):
        return "x86_64"
    return a or "unknown"

_arch = _normalize_arch(platform.machine())

def flag_list_repr(flags):
    return ' '.join([f'"{flag}"' if ' ' in flag else flag for flag in flags])

def stdpath(path, workdir='.'):

    # Use the source_analyzer path normalizer:
    #  - expands env vars and ~
    #  - uses '/' separators even on Windows
    #  - resolves relative paths relative to workdir
    return ce._standard_path(path, workdir)

def _normslashes(p: str) -> str:
    return p.replace('\\', '/')

def is_nested_in(path, folder):
    return path.startswith(folder) and (
        len(path) == len(folder) or path[len(folder)] == '/'
    )

def get_rel_path(path, folder):
    return os.path.relpath(path, folder).replace('\\', '/')

def insert_after(list, elem, after):
    if not after:
        return [elem] + list
    out = []
    for item in list:
        out.append(item)
        if item == after:
            out.append(elem)
    return out

def remove(list, elem):
    out = []
    for item in list:
        if item != elem:
            out.append(item)
    return out


# -----------------------------------------------------------------------------
# Sys layout resolution
# -----------------------------------------------------------------------------
#
# We support multiple sys layouts (because the Makefile was changed):
#
#   1) old layout:
#        sys/<os>/lib/libsource_analyzer.so
#
#   2) new layout (flattened, named by OS+arch):
#        sys-<os>-<arch>/lib/libsource_analyzer.so
#
#   3) flattened but still called 'sys':
#        sys/lib/libsource_analyzer.so
#
# Additionally, tests often run from a per-test build directory like:
#     .../bld/sa/test/pic32
# while sys lives one or more parents higher:
#     .../bld/sa/sys-windows-x86_64
#
# Therefore we search *upward* through parent directories until root.
#
# You can override detection by setting:
#     SA_SYS=<path-or-name>
# where SA_SYS may be absolute or relative. Relative is tried at each search root.
#

def _iter_search_roots(base_dir: str) -> list[pathlib.Path]:
    """
    Return a list of directories to search for sys layouts:
    base_dir, base_dir/.., base_dir/../.., ... up to filesystem root.
    """
    p = pathlib.Path(stdpath(base_dir)).resolve()
    roots = [p]
    while True:
        parent = p.parent
        if parent == p:
            break
        roots.append(parent)
        p = parent
    return roots

def _resolve_sa_sys_layout(base_dir: str = '.') -> tuple[str, str, str, str]:
    """
    Resolve paths for:
      - sys root directory (folder containing 'esa' and 'lib' (new) or '<os>/lib' (old))
      - resource dir (..../esa)
      - lib dir (..../lib OR ..../<os>/lib)
      - full path to the source analyzer shared library

    Supports these layouts:
      - old: sys/<os>/lib/libsource_analyzer.so
      - new: sys-<os>-<arch>/lib/libsource_analyzer.so  (flattened)
      - flattened named 'sys': sys/lib/libsource_analyzer.so
    Also allows override via env var SA_SYS, which may be absolute or relative.
    """

    # Candidate shared library filenames we might encounter.
    # (Some builds use .so even on Windows under MSYS; keep both here.)
    libnames = [
        "libsource_analyzer.so",
        "libsource_analyzer.dll",
        "source_analyzer.dll",
    ]

    # Candidate sys folder names (relative names).
    # Order matters: prefer the most specific first.
    rel_candidates = [
        f"sys-{_osname}-{_arch}",
        f"sys-{_osname}",
        "sys",
    ]

    env_sa_sys = os.environ.get("SA_SYS")

    tried: list[str] = []
    search_roots = _iter_search_roots(base_dir)

    def try_libdir(rootdir: pathlib.Path, libdir: pathlib.Path) -> tuple[str, str, str, str] | None:
        # Try all known library filenames inside libdir
        for name in libnames:
            so = libdir / name
            tried.append(_normslashes(str(so)))
            if so.is_file():
                root = _normslashes(str(rootdir))
                resdir = _normslashes(str(rootdir / "esa"))
                libd = _normslashes(str(libdir))
                sop = _normslashes(str(so))
                return root, resdir, libd, sop
        return None

    def check_candidate_path(p: pathlib.Path) -> tuple[str, str, str, str] | None:
        """
        Given a candidate path p (which can be sys root, sys/<os>, sys/lib,
        sys/<os>/lib, etc), infer rootdir and libdir(s) to try.
        """
        if not p.exists():
            return None

        # If SA_SYS points directly to a lib directory: .../lib
        if p.is_dir() and p.name == "lib":
            libdir = p
            rootdir = libdir.parent

            # old layout: sys/<os>/lib, where sys root is parent of <os>
            if rootdir.name == _osname and (rootdir.parent / "esa").is_dir():
                real_root = rootdir.parent
                found = try_libdir(real_root, libdir)
                if found:
                    return found

            # flattened layout: sys/lib or sys-.../lib
            found = try_libdir(rootdir, libdir)
            if found:
                return found
            return None

        # If SA_SYS points to sys/<os> directory (old layout)
        if p.is_dir() and p.name == _osname and (p.parent / "esa").is_dir():
            rootdir = p.parent
            libdir = p / "lib"
            found = try_libdir(rootdir, libdir)
            if found:
                return found
            return None

        # If SA_SYS points to sys root directory (or sys-... root)
        if p.is_dir():
            rootdir = p

            # Support both:
            #   sys/lib/libsource_analyzer.*
            # and:
            #   sys/<os>/lib/libsource_analyzer.*
            libdirs = [
                rootdir / "lib",              # flattened layout (including sys/lib)
                rootdir / _osname / "lib",    # old layout
            ]
            for libdir in libdirs:
                found = try_libdir(rootdir, libdir)
                if found:
                    return found
            return None

        return None

    # 1) If SA_SYS is absolute, try it first (no need to walk parents)
    if env_sa_sys:
        env_path = pathlib.Path(env_sa_sys)
        if env_path.is_absolute():
            found = check_candidate_path(env_path)
            if found:
                return found

    # 2) Search upward through parents: at each root, try SA_SYS (if relative) and default candidates
    for root in search_roots:

        # SA_SYS relative: interpret relative to each search root
        if env_sa_sys:
            env_path = pathlib.Path(env_sa_sys)
            if not env_path.is_absolute():
                found = check_candidate_path(root / env_path)
                if found:
                    return found

        # Default relative candidates
        for cand in rel_candidates:
            found = check_candidate_path(root / cand)
            if found:
                return found

        # 3) Last-resort (per root): glob sys*/**/... under that root.
        # This catches odd layouts without baking in more assumptions.
        glob_hits = []
        for pat in (
            "sys*/**/libsource_analyzer.so",
            "sys*/**/libsource_analyzer.dll",
            "sys*/**/source_analyzer.dll",
        ):
            try:
                glob_hits.extend(list(root.glob(pat)))
            except Exception:
                pass

        if glob_hits:
            # Prefer the most specific match (sys-<os>-<arch>) if present.
            def score(p: pathlib.Path) -> tuple[int, int]:
                s = _normslashes(str(p))
                bonus = 0
                if f"sys-{_osname}-{_arch}" in s:
                    bonus -= 100
                elif f"sys-{_osname}" in s:
                    bonus -= 50
                elif "/sys/" in s:
                    bonus -= 10
                return (bonus, len(s))

            best = sorted(glob_hits, key=score)[0]
            libdir = best.parent

            # Infer rootdir:
            # - if .../<os>/lib then root is .../sys
            # - if .../lib then root is parent of libdir
            if libdir.parent.name == _osname:
                rootdir = libdir.parent.parent
            else:
                rootdir = libdir.parent

            root_s = _normslashes(str(rootdir))
            resdir_s = _normslashes(str(rootdir / "esa"))
            libd_s = _normslashes(str(libdir))
            so_s = _normslashes(str(best))
            return root_s, resdir_s, libd_s, so_s

    # Fail with helpful diagnostics
    roots_str = "\n  - ".join(_normslashes(str(r)) for r in search_roots[:8])
    if len(search_roots) > 8:
        roots_str += "\n  - ..."

    tried_str = "\n  - ".join(tried) if tried else "(no existing candidate directories found under search roots)"

    raise FileNotFoundError(
        "Could not locate source analyzer shared library.\n"
        "Searched upwards from:\n  - "
        + _normslashes(str(pathlib.Path(stdpath(base_dir)).resolve())) + "\n"
        "Search roots (first ones):\n  - " + roots_str + "\n"
        "Tried these library paths:\n  - " + tried_str + "\n"
        "If needed, set SA_SYS to the sys folder name/path "
        "(e.g. sys-windows-x86_64, sys, or an absolute path)."
    )


# -----------------------------------------------------------------------------
# Locate our development folder and test folder
# -----------------------------------------------------------------------------

dev_dir = os.path.dirname(
    os.path.realpath(inspect.getfile(inspect.currentframe()))
).replace('\\', '/')

test_dir = dev_dir + '/test'


# -----------------------------------------------------------------------------
# Toolchain setup for tests
# -----------------------------------------------------------------------------

toolchain_dir = stdpath(os.environ['TOOLCHAIN_DIR'])
eprint(f'toolchain dir: {toolchain_dir}')

build_env = os.environ.copy()
build_env['PATH'] = toolchain_dir + '/bin' + os.pathsep + build_env['PATH']
PATH = build_env['PATH']
eprint(f'PATH={PATH}')


# -----------------------------------------------------------------------------
# Initialize the source analyzer shared library (once per Python process)
# -----------------------------------------------------------------------------
#
# We resolve the sys layout relative to the current working directory.
# This is typically the top-level build directory (e.g. ~/bld/sa), but during
# some test runs it may be a per-test build directory; the resolver searches
# upward to find sys.
#

_sa_sys_root, _sa_resource_dir, _sa_lib_dir, _sa_so_path = _resolve_sa_sys_layout(os.getcwd())
eprint(f"SA sys root:     {_sa_sys_root}")
eprint(f"SA resource dir: {_sa_resource_dir}")
eprint(f"SA lib dir:      {_sa_lib_dir}")
eprint(f"SA library:      {_sa_so_path}")

ce.init(_sa_so_path, debug=True)
ce.set_number_of_workers(4)

trace = False


# -----------------------------------------------------------------------------
# Entity kinds used in tests
# -----------------------------------------------------------------------------

entity_kind_global_function = ce.entity_kind('global function')
entity_kind_global_variable = ce.entity_kind('global variable')
entity_kind_static_variable = ce.entity_kind('static variable')
entity_kind_parameter = ce.entity_kind('parameter')
entity_kind_automatic_variable = ce.entity_kind('automatic variable')
entity_kind_local_static_variable = ce.entity_kind('local static variable')
entity_kind_local_function = ce.entity_kind('local function')
entity_kind_type = ce.entity_kind('type')
entity_kind_macro = ce.entity_kind('macro')

cleared_cache_dirs = set()

def _get_rel_path(path, folder):
    return os.path.relpath(path, folder).replace('\\', '/')


# -----------------------------------------------------------------------------
# Project wrapper used in regression tests
# -----------------------------------------------------------------------------

class Project(ce.Project):
    ignore_undefined_globals = False

    def __init__(
            self,
            build_dir,
            makefile,
            source_dir=None,
            cache_dir=None,
            env={},
    ):
        build_dir = stdpath(build_dir)
        eprint(f"Build dir:  {build_dir}")
        os.makedirs(build_dir, exist_ok=True)

        # Resolve sys layout relative to THIS build_dir.
        # Important: build_dir is often a per-test directory under .../test/<name>,
        # while sys lives at .../bld/sa/sys-... . The resolver searches upward.
        sa_sys_root, sa_resource_dir, sa_lib_dir, sa_so_path = _resolve_sa_sys_layout(build_dir)
        eprint(f"SA sys root (for this project): {sa_sys_root}")
        eprint(f"SA resource dir:               {sa_resource_dir}")
        eprint(f"SA lib dir:                    {sa_lib_dir}")
        eprint(f"SA library:                    {sa_so_path}")

        if source_dir:
            self.source_dir = stdpath(source_dir)
        else:
            self.source_dir = stdpath('../source', build_dir)
        eprint(f"Source dir: {self.source_dir}")

        if cache_dir:
            cache_dir = stdpath(cache_dir)
        else:
            cache_dir = build_dir + '.cache'
        eprint(f"Cache dir:  {cache_dir}")
        os.makedirs(cache_dir, exist_ok=True)

        # Clear cache directory once per test run
        if cache_dir not in cleared_cache_dirs:
            eprint(f"Clear cache dir {cache_dir} (first use in this test run)")
            shutil.rmtree(cache_dir)
            cleared_cache_dirs.add(cache_dir)

        # Makefile path relative to build directory
        makefile = _get_rel_path(
            os.path.join(build_dir, makefile), build_dir
        )
        eprint(f"Makefile:  {makefile}")

        self.make_command = [
            'make', '-f', makefile,
            f'TOOLPREFIX={toolchain_dir}/bin/arm-none-eabi-',
            f'TOOLCHAIN={toolchain_dir}/bin/',  # For backward compatibility
        ]
        self.env = {**build_env, **env}

        # Initialize C++ project
        super().__init__(
            project_path=self.source_dir,
            cache_path=cache_dir,
            resource_path=sa_resource_dir,
            lib_path=sa_lib_dir,
        )
        self.set_make_config(self.make_command, self.env)
        self.set_build_path(build_dir)

        # Status bookkeeping for tests
        self.fail_count = 0  # Count analysis failures (file not found, crash)
        self.pending = 0
        self.included_files = set()
        self.linked_files = set()
        self.used_hdirs = set()
        self.warning_count = 0
        self.error_count = 0
        self.set_diagnostic_limit(ce.Severity.WARNING, 30)
        self.set_diagnostic_limit(ce.Severity.ERROR, 30)
        self._analysis_status = {}
        self.project_status = ce.ProjectStatus.READY
        self.linker_status = ce.LinkerStatus.ERROR
        self.tracked = set()
        self.offsets = {}
        self.diagnostics = []

    def offset(self, path, line, column):
        #eprint(f'{path} ---> {self.abs_source_path(path)}')
        table = self.offsets.get(path)
        if not table:
            eprint(f'Load line offsets for {path}')
            with open(self.abs_source_path(path), mode='rb') as file:
                table = LineOffsetTable.LineOffsetTable(file.read())
            self.offsets[path] = table
        return table.offset(line, column)

    def reload_file(self, path):
        print(f'Reload {path}')
        self.offsets.pop(path, None)
        super().reload_file(path)

    def srcpath(self, path):
        return self.standard_source_path(path)

    def hdir_used(self, hdir):
        return stdpath(hdir, self.source_dir) in self.used_hdirs

    def file_included(self, file):
        return self.standard_source_path(file) in self.included_files

    def file_linked(self, file):
        return self.standard_source_path(file) in self.linked_files

    def get_diagnostics(self):
        return [
            (f'{d.path}:{d.offset}:' if d.path else '')
            + f' {ce.severity_name(d.severity)}: {d.message}'
            for d in self.diagnostics
        ]

    def print_diagnostics(self):
        eprint("diagnostics ({}): \n{}".format(
            self.get_diagnostic_count(),
            '\n'.join(self.get_diagnostics())
        ))

    def get_diagnostic_count(self):
        return len(self.diagnostics)
        #return sum([1 for d in self.diagnostic_set])

    def report_project_status(self, status):
        eprint(
            f'Change project status to {ce.project_status_name(status)}'
        )
        self.project_status = status

    # Callback from source analyzer when the linker status changes. The initial
    # status is 'error', because initially no main function is defined (before
    # files are added). All changes are reported.
    def report_linker_status(self, status):
        eprint(f"Change linker status to {ce.linker_status_name(status)}")
        self.linker_status = status

    # Callback from source analyzer when an hdir changes from being used to
    # unused or vice versa. The source analyzer assumes that all hdirs are
    # initially unused and reports all changes.
    def report_hdir_usage(self, path, used):
        eprint(f'Hdir used={used}: {path}')
        assert isinstance(used, bool)
        if used:
            self.used_hdirs.add(path)
        else:
            self.used_hdirs.remove(path)

    # Callback from source analyzer to add a diagnostic. The source analyzer
    # assumes that initially, there are no diagnostics. All changes are
    # reported using callbacks to add and remove diagnostics.
    def add_diagnostic(self, message, severity, category, path, offset, after):
        srcpath = self.srcpath(path) if path else None
        # Report location in emacs-style: 1-based line, 0-based column
        eprint(
            f"Insert {ce.severity_name(severity)}: "
            f"[{ce.category_name(category)}] {message} at "
            f"{srcpath}:{offset} after {after}"
        )
        if severity is ce.Severity.WARNING:
            self.warning_count = self.warning_count + 1
        if severity is ce.Severity.ERROR:
            self.error_count = self.error_count + 1
        diagnostic = Diagnostic(message, severity, srcpath, offset)
        self.diagnostics = insert_after(self.diagnostics, diagnostic, after)
        return diagnostic

    # Callback from source analyzer to remove a diagnostic previously added by
    # the add diagnostic callback.
    def remove_diagnostic(self, diagnostic):
        # Report location in emacs-style: 1-based line, 0-based column
        eprint(
            f"Remove {ce.severity_name(diagnostic.severity)}: "
            f"{diagnostic.message} at "
            f"{diagnostic.path}:{diagnostic.offset}"
        )
        self.diagnostics = remove(self.diagnostics, diagnostic)
        if diagnostic.severity is ce.Severity.WARNING:
            self.warning_count = self.warning_count - 1
        if diagnostic.severity is ce.Severity.ERROR:
            self.error_count = self.error_count - 1

    def report_more_diagnostics(self, severity, count):
        eprint(f'{count} hidden {ce.severity_name(severity)}s')

    def add_occurrence(self, data, scope):
        occurrence = Occurrence(data, scope)
        eprint(f'track {occurrence}')
        self.tracked.add(occurrence)
        return occurrence

    # Callback from source analyzer to remove a tracked occurrence previously
    # added by the add occurrence callback.
    def remove_occurrence(self, occurrence):
        eprint(f'untrack {occurrence}')
        self.tracked.remove(occurrence)

    # Callback from source analyzer when the inclusion status of a file in this
    # project changes.
    def report_file_inclusion_status(self, raw_path, status):
        path = self.standard_source_path(raw_path)
        eprint(f"Change inclusion status to {status} for {path}")
        if status:
            self.included_files.add(path)
        else:
            self.included_files.discard(path)

    # Callback from source analyzer when the link status of a file in this
    # project changes.
    def report_file_link_status(self, raw_path, status):
        path = self.standard_source_path(raw_path)
        eprint(f"Change link inclusion status to {status} for {path}")
        if status:
            self.linked_files.add(path)
        else:
            self.linked_files.discard(path)

    def get_file_inclusion_status(self, raw_path):
        path = self.standard_source_path(raw_path)
        if path in self.included_files or path in self.linked_files:
            return ce.InclusionStatus.INCLUDED
        return ce.InclusionStatus.EXCLUDED

    # Callback when the analysis progress of the project changes.
    # Progress is current/total*100%. When current is equal to total, the total
    # will be reset before the next call.
    #
    # Parameters:
    #  - "current": the number of files analyzed
    #  - "total": the total number of files to be analyzed
    #
    def report_progress(self, current, total):
        eprint("progress {}/{}".format(current, total))
        self.pending = total - current

    # Callback from source analyzer when the analysis status of a file in this
    # project changes.
    def report_file_analysis_status(self, raw_path, status):
        path = self.standard_source_path(raw_path)
        eprint(
            f'Change analysis status to {ce.analysis_status_name(status)}'
            f' for {path}'
        )
        old_status = self._analysis_status.get(path, ce.AnalysisStatus.NONE)
        self._analysis_status[path] = status
        if old_status == ce.AnalysisStatus.FAILED:
            self.fail_count -= 1
        if status == ce.AnalysisStatus.FAILED:
            self.fail_count += 1

    def get_file_analysis_status(self, raw_path):
        path = self.standard_source_path(raw_path)
        return self._analysis_status.get(path, ce.AnalysisStatus.NONE)

    def report_compilation_settings(self, file, compiler, flags):
        if False:
            path = self.standard_source_path(file)
            eprint(
                f"Compilation settings for {path}:\n"
                f"    compiler: {compiler}\n"
                f"    flags: {flag_list_repr(flags)}"
            )

    def report_embeetle_version_in_makefile(self, version):
        if False:
            eprint(f"Embeetle version in makefile: {version}")

    def report_internal_error(self, message):
        """Callback called when an internal error occurs in a background thread

        When this callback is called,  the source analyzer will no longer work
        and cannot recover.  It is advisable to save all edits and restart
        Embeetle.

        """
        eprint("SA FATAL: internal error - save changes and restart Embeetle")
        eprint(f"Details: {message}")
        #os._exit(1)

    # Aux functions for testing below

    # Add a file and return its path. A relative path is relative to the source
    # directory.
    def get_file(self, path, user_data=None):
        self.add_file(path, ce.file_mode_automatic, user_data)
        return path

    # Click at position in emacs coordinates (one-based line, zero-based column)
    # and return occurrence found.
    def click(self, file, line, column, verbose=True):
        offset = self.offset(file, line-1, column)
        if verbose:
            eprint(f"click at {file}:{line}:{column} (offset={offset})")
        ref = self.find_occurrence(file, offset)
        if not ref:
            if verbose:
                eprint(" `-> no entity found")
        else:
            if verbose:
                eprint(f' `-> found {ref}')
        return ref

    def wait_for_analysis(self):
        eprint(f"Wait for project analysis (status={self.project_status})")
        start = time.time()
        # Make sure source analysis is running. If compiled with NO_THREADS (for
        # debugging), this runs the analysis.
        ce.start()
        while self.project_status == ce.ProjectStatus.BUSY:
            time.sleep(0.1)
        eprint(f"... ready after {time.time()-start:.1f} seconds")

    # Return a user-friendly path for a given source path. The normalized path
    # is relative to the project when project-local.
    def standard_source_path(self, path):
        project_path = ce._standard_path(
            self.source_path, self.build_path
        )
        src_path = ce._standard_path(path, project_path)
        if is_nested_in(src_path, project_path):
            return get_rel_path(src_path, project_path)
        return src_path

    # Return an absolute path for a given source path
    def abs_source_path(self, path):
        project_path = ce._standard_path(
            self.source_path, self.build_path
        )
        return ce._standard_path(path, project_path)


class Diagnostic:
    def __init__(self, message, severity, path, offset):
        self.message = message
        self.severity = severity
        self.path = path
        self.offset = offset

    def __del__(self):
        #eprint("delete diagnostic")
        pass

    def __str__(self):
        return (
            f'{ce.severity_name(self.severity)}: {self.message}'
            f' at {self.path}:{self.offset}'
        )


class Occurrence:
    def __init__(self, data, scope):
        self.data = data
        self.scope = scope

    def __del__(self):
        #eprint("delete diagnostic")
        pass

    def __str__(self):
        return f'{self.data} in scope of {self.scope}'