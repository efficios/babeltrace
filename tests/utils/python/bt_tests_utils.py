# SPDX-FileCopyrightText: 2025 Philippe Proulx <pproulx@efficios.com>
# SPDX-License-Identifier: MIT

# pyright: strict, reportMissingTypeStubs=false, reportPrivateUsage=false

import os
import sys
import enum
import json
import time
import shlex
import typing
import logging
import pathlib
import platform
import tempfile
import textwrap
import itertools
import subprocess
from typing import (
    Any,
    Dict,
    List,
    Type,
    Tuple,
    Union,
    Iterable,
    Optional,
)

import bt2

_logger = logging.getLogger(__name__)


class OsType(enum.Enum):
    # MinGW (Windows)
    MINGW = "mingw"

    # macOS
    DARWIN = "darwin"

    # Linux
    LINUX = "linux"

    # Cygwin (Windows)
    CYGWIN = "cygwin"

    # Anything else
    UNSUPPORTED = "unsupported"


# Returns the normalized OS type.
def os_type() -> OsType:
    system = platform.system().lower()

    if system.startswith("mingw") or "windows" in system:
        return OsType.MINGW
    elif system.startswith("cygwin"):
        return OsType.CYGWIN
    elif "darwin" in system:
        return OsType.DARWIN
    elif "linux" in system:
        return OsType.LINUX
    else:
        return OsType.UNSUPPORTED


# Returns the directory containing the source file `source_file`.
#
# Typical usage:
#
#     this_src_dir(__file__)
def this_src_dir(source_file: Union[pathlib.Path, str]) -> pathlib.Path:
    return pathlib.Path(source_file).resolve().parent


# Returns the executable file path from `path`, appending `.exe`
# on MinGW if not already present.
def exe_path(path: pathlib.Path) -> pathlib.Path:
    if os_type() == OsType.MINGW and not path.name.endswith(".exe"):
        return path.with_suffix(f"{path.suffix}.exe")

    return path


# Returns the shared library file path from `path`, appending `.dll`
# on MinGW and Cygwin, or `.so` on other platforms if not already present.
def shared_lib_path(path: pathlib.Path) -> pathlib.Path:
    if os_type() in (OsType.MINGW, OsType.CYGWIN):
        return path.with_suffix(f"{path.suffix}.dll")
    else:
        return path.with_suffix(f"{path.suffix}.so")


# Cached set of standard plugins, loaded lazily and only once
# by plugin_set().
_plugin_set: Optional[bt2._PluginSet] = None


# Returns the cached set of standard plugins, loading it lazily (and
# only once) with bt2.find_plugins() on the first call.
#
# Pass the result to any `bt2.TraceCollectionMessageIterator` or other
# TCMI interface accepting a `plugin_set` parameter to avoid reloading
# the standard plugins on each call.
def plugin_set() -> bt2._PluginSet:
    global _plugin_set

    if _plugin_set is None:
        _logger.info("Loading standard plugins with bt2.find_plugins()")
        _plugin_set = bt2.find_plugins(find_in_user_dir=False, find_in_sys_dir=False)

        if _plugin_set is None:
            raise RuntimeError("cannot find any plugin")

    return _plugin_set


# Cached set of test utility plugins (from `tests/utils/python`), loaded
# lazily and only once by _test_utils_plugin_set().
_test_utils_plugin_set_cache: Optional[bt2._PluginSet] = None


# Returns the cached set of test utility plugins, loading it lazily (and
# only once) with bt2.find_plugins_in_path() on the first call.
#
# `tests/utils/python` isn't part of `BABELTRACE_PLUGIN_PATH` (see
# `utils/env.sh`): otherwise, each `babeltrace2` CLI run would load the
# Python plugin provider only to find those plugins.
def _test_utils_plugin_set() -> bt2._PluginSet:
    global _test_utils_plugin_set_cache

    if _test_utils_plugin_set_cache is None:
        path = this_src_dir(__file__)
        _logger.info(f"Loading test utility plugins from `{path}`")
        _test_utils_plugin_set_cache = bt2.find_plugins_in_path(str(path))

        if _test_utils_plugin_set_cache is None:
            raise RuntimeError(f"cannot find any plugin in `{path}`")

    return _test_utils_plugin_set_cache


# Returns the standard or test utility plugin named `name`, or `None` if
# there's no such plugin.
#
# Unlike bt2.find_plugin(), this function loads the plugins lazily (and
# only once) through plugin_set() and _test_utils_plugin_set(), getting
# `name` from the cached sets instead of reloading the plugins on
# each call.
def plugin_by_name(name: str) -> Optional[bt2._Plugin]:
    for plugin in plugin_set():
        if plugin.name == name:
            return plugin

    for plugin in _test_utils_plugin_set():
        if plugin.name == name:
            return plugin


# Logs a command to be run.
def _log_run_cmd(
    env: Dict[str, str],
    exec_path: pathlib.Path,
    args: List[str],
    extra_env: Optional[Dict[str, str]],
) -> None:
    env_vars_to_log = {
        "ASAN_OPTIONS",
        "BABELTRACE_DISABLE_STD_PLUGIN_DIRS",
        "BABELTRACE_PLUGIN_PATH",
        "BABELTRACE_PLUGIN_PROVIDER_PATH",
        "BT_TESTS_BUILDDIR",
        "BT_TESTS_PYTHON_BIN",
        "BT_TESTS_PYTHON_CONFIG_BIN",
        "BT_TESTS_SRCDIR",
        "DYLD_LIBRARY_PATH",
        "LD_LIBRARY_PATH",
        "LD_PRELOAD",
        "LIBBABELTRACE2_DISABLE_STD_PLUGIN_PROVIDER_DIRS",
        "PYTHONPATH",
        "BABELTRACE_TERM_COLOR",
        "NO_COLOR",
    }

    # Collect all environment variables to log
    env_to_log: Dict[str, str] = {}

    for var_name in env_vars_to_log:
        if var_name in env:
            env_to_log[var_name] = env[var_name]

    if extra_env is not None:
        for var_name, var_val in extra_env.items():
            if var_name not in env_vars_to_log:
                env_to_log[var_name] = var_val

    # Build command parts, sorting environment variables by name
    cmd_parts: List[str] = []

    for var_name in sorted(env_to_log.keys()):
        cmd_parts.append(f"{var_name}={shlex.quote(env_to_log[var_name])}")

    cmd_parts.append(shlex.quote(str(exec_path)))
    cmd_parts.extend(shlex.quote(arg) for arg in args)

    # Log
    _logger.info(f"Running: {' '.join(cmd_parts)}")


# Like subprocess.run(), but always captures the standard streams.
#
# `exec_path` is the path to the executable to use: don't include it in
# `args`. This function automatically adds the `.exe` suffix on Windows
# if `windows_safe` is `True`.
#
# `build_root_dir` is the build directory containing the
# `tests` directory
#
# This function adds the `extra_env` environment dictionary to the
# current environment before running the subprocess.
#
# `check` and `timeout` are subprocess.run() parameters.
def run(
    build_root_dir: pathlib.Path,
    exec_path: pathlib.Path,
    args: List[str],
    check: bool = False,
    timeout: Optional[float] = None,
    windows_safe: bool = True,
    extra_env: Optional[Dict[str, str]] = None,
) -> "subprocess.CompletedProcess[Any]":
    # Add `.exe` suffix on Windows (MinGW)
    if windows_safe:
        exec_path = exe_path(exec_path)

    env = os.environ.copy()

    # Strip `NO_COLOR` so that the subprocess (the `babeltrace2` CLI, a
    # Catch2 test binary, and the rest) doesn't honor it: tests need
    # deterministic output regardless of the user's terminal preference.
    #
    # `os.environ` itself is left intact, therefore pytest's own
    # terminal writer keeps honoring the user's preference.
    env.pop("NO_COLOR", None)

    if extra_env is not None:
        env.update(extra_env)

    _log_run_cmd(env, exec_path, args, extra_env)

    try:
        res: "subprocess.CompletedProcess[Any]" = subprocess.run(
            [str(exec_path)] + args,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=env,
            timeout=timeout,
            encoding="utf-8",
        )
    except subprocess.TimeoutExpired as exc:
        if exc.stderr is not None:
            sys.stderr.write(exc.stderr.decode("utf-8"))

        raise

    # Write captured standard error so that pytest includes it in the
    # test report on failure.
    sys.stderr.write(res.stderr)

    if check:
        res.check_returncode()

    _logger.info(f"  Exit status: {res.returncode}")
    return res


# Parses `params`, a string of `NAME=VAL` lines, into a component
# initialization parameter dictionary suitable for convert() and
# similar functions.
#
# If `params` starts with `{`, then this function decodes it as a JSON
# object and returns the result as is.
#
# Otherwise, each non-empty line must have the form `NAME=VAL`, where
# `VAL` is one of:
#
# `yes` or `no`:
#     A boolean.
#
# A string which parses as an integer with int():
#     An integer.
#
# Anything else:
#     A plain string.
def params_from_part(params: str) -> Dict[str, Any]:
    params = params.strip()

    if params.startswith("{"):
        return typing.cast(Dict[str, Any], json.loads(params))

    result: Dict[str, Any] = {}

    for line in params.splitlines():
        line = line.strip()

        if not line or line.startswith("#"):
            continue

        name, sep, val = line.partition("=")
        assert sep == "="
        name = name.strip()
        val = val.strip()

        if val == "yes":
            result[name] = True
        elif val == "no":
            result[name] = False
        else:
            try:
                result[name] = int(val)
            except ValueError:
                result[name] = val

    return result


# Any component type
_AnyCompT = Union[
    bt2._GenericSourceComponentConst,
    bt2._GenericFilterComponentConst,
    bt2._GenericSinkComponentConst,
]

# Any component class type (user class or wrapper)
_AnyCompClsT = Union[
    bt2._SourceComponentClassConst,
    bt2._FilterComponentClassConst,
    bt2._SinkComponentClassConst,
    Type[bt2._UserSourceComponent],
    Type[bt2._UserFilterComponent],
    Type[bt2._UserSinkComponent],
]


# Sink component specification to be used with convert().
#
# Similar to `bt2.ComponentSpec` but for sink component classes.
class SinkComponentSpec:
    def __init__(
        self,
        component_class: Union[
            bt2._SinkComponentClassConst,
            Type[bt2._UserSinkComponent],
        ],
        params: bt2._ComponentParams = None,
        obj: object = None,
        logging_level: Optional[bt2.LoggingLevel] = None,
    ):
        self._component_class = component_class
        self._params = bt2.create_value(params)
        self._obj = obj
        self._logging_level = (
            logging_level if logging_level is not None else bt2.LoggingLevel.NONE
        )

    @property
    def component_class(
        self,
    ) -> Union[
        bt2._SinkComponentClassConst,
        Type[bt2._UserSinkComponent],
    ]:
        return self._component_class

    @property
    def params(self) -> Optional[bt2._Value]:
        return self._params

    @property
    def obj(self) -> object:
        return self._obj

    @property
    def logging_level(self) -> bt2.LoggingLevel:
        return self._logging_level


# Returns a string representation of a component class spec for logging.
def _fmt_comp_cls_spec(comp_cls: _AnyCompClsT) -> str:
    # Determine the component type prefix
    if isinstance(comp_cls, bt2._SourceComponentClassConst) or (
        isinstance(comp_cls, type) and issubclass(comp_cls, bt2._UserSourceComponent)
    ):
        type_prefix = "src"
    elif isinstance(comp_cls, bt2._FilterComponentClassConst) or (
        isinstance(comp_cls, type) and issubclass(comp_cls, bt2._UserFilterComponent)
    ):
        type_prefix = "flt"
    else:
        assert isinstance(comp_cls, bt2._SinkComponentClassConst) or (
            isinstance(comp_cls, type) and issubclass(comp_cls, bt2._UserSinkComponent)
        )
        type_prefix = "sink"

    return f"{type_prefix}.?.{comp_cls.name}"


# Returns a string representation of a component spec for logging.
def _log_comp_spec(comp_spec: Union[bt2.ComponentSpec, SinkComponentSpec], indent: int):
    ind = " " * indent
    _logger.info("%s`%s`:", ind, _fmt_comp_cls_spec(comp_spec.component_class))
    _logger.info("%s  Params: %r", ind, comp_spec.params)


# Returns a unique component name for a component class and the updated
# next suffix.
#
# `all_comps` is a list of all components created so far.
#
# `next_suffix` is the next suffix to use if a name collision occurs.
def _get_unique_comp_name(
    comp_cls: _AnyCompClsT,
    all_comps: List[_AnyCompT],
    next_suffix: int,
) -> Tuple[str, int]:
    name = comp_cls.name

    if name in [comp.name for comp in all_comps]:
        name += f"-{next_suffix}"
        next_suffix += 1

    return name, next_suffix


# Creates a component from a component spec.
#
# `all_comps` is a list of all components created so far.
#
# `next_suffix` is the next suffix to use if a name collision occurs.
#
# Returns the created component and the updated next suffix.
def _create_comp(
    graph: bt2.Graph,
    comp_spec: Union[bt2.ComponentSpec, SinkComponentSpec],
    all_comps: List[_AnyCompT],
    next_suffix: int,
) -> Tuple[_AnyCompT, int]:
    comp_cls = comp_spec.component_class
    comp_name, next_suffix = _get_unique_comp_name(comp_cls, all_comps, next_suffix)
    _logger.info(
        f"  Instantiating `{_fmt_comp_cls_spec(comp_cls)}` as component `{comp_name}`"
    )
    comp = graph.add_component(
        comp_cls,
        comp_name,
        typing.cast(bt2._ComponentParams, comp_spec.params),
        comp_spec.obj,
        (
            bt2.LoggingLevel.NONE
            if comp_spec.logging_level is None
            else comp_spec.logging_level
        ),
    )
    all_comps.append(comp)
    return comp, next_suffix


# Returns the next free input port of an `flt.utils.muxer` component.
def _get_free_muxer_input_port(
    muxer_comp: bt2._GenericFilterComponentConst,
) -> Optional[bt2._InputPortConst]:
    for port in muxer_comp.input_ports.values():
        if not port.is_connected:
            return port


# Logs and connects ports.
def _connect_ports(
    graph: bt2.Graph,
    out_port: bt2._OutputPortConst,
    out_comp: _AnyCompT,
    in_port: bt2._InputPortConst,
    in_comp: _AnyCompT,
) -> None:
    _logger.info(
        f"  Connecting ports: `{out_port.name}` of `{out_comp.name}` → `{in_port.name}` of `{in_comp.name}`"
    )
    graph.connect_ports(out_port, in_port)


# Runs a graph to completion with the given component specifications.
#
# Similar to `bt2.TraceCollectionMessageIterator`, but:
#
# • Not an iterator (runs to completion).
# • Accepts a single sink component spec.
# • No stream intersection feature.
# • No `flt.utils.trimmer` feature.
# • No custom plugin set.
#
# `src_component_specs` and `flt_component_specs` are like
# their bt2.TraceCollectionMessageIterator.__init__() equivalent.
def convert(
    src_component_specs: Union[
        str,
        pathlib.Path,
        bt2.AutoSourceComponentSpec,
        bt2.ComponentSpec,
        Iterable[
            Union[str, pathlib.Path, bt2.AutoSourceComponentSpec, bt2.ComponentSpec]
        ],
    ],
    sink_component_spec: SinkComponentSpec,
    flt_component_specs: Optional[
        Union[bt2.ComponentSpec, Iterable[bt2.ComponentSpec]]
    ] = None,
    mip_version: Optional[int] = None,
) -> None:
    _logger.info("btu.convert():")

    # Normalize `src_component_specs` to a list
    if isinstance(
        src_component_specs,
        (str, pathlib.Path, bt2.AutoSourceComponentSpec, bt2.ComponentSpec),
    ):
        src_component_specs = [src_component_specs]

    # Convert strings to `AutoSourceComponentSpec`
    src_component_specs = [
        (
            bt2.AutoSourceComponentSpec(str(spec))
            if isinstance(spec, (str, pathlib.Path))
            else spec
        )
        for spec in src_component_specs
    ]

    # Normalize `flt_component_specs` to a list
    if isinstance(flt_component_specs, bt2.ComponentSpec):
        flt_component_specs = [flt_component_specs]

    if flt_component_specs is None:
        flt_component_specs = []

    # Combine all source component specs
    all_src_component_specs = [
        spec for spec in src_component_specs if type(spec) is bt2.ComponentSpec
    ] + bt2.source_component_specs_from_auto_source_component_specs(
        [
            spec
            for spec in src_component_specs
            if type(spec) is bt2.AutoSourceComponentSpec
        ],
        plugin_set(),
    )

    # Validate filter component specs and convert to list
    flt_component_specs = list(flt_component_specs)

    for comp_spec in flt_component_specs:
        assert isinstance(comp_spec, bt2.ComponentSpec)

    # Log source component specs
    _logger.info(f"  Source component specs ({len(all_src_component_specs)})")

    for comp_spec in all_src_component_specs:
        _log_comp_spec(comp_spec, 4)

    # Log filter component specs
    _logger.info(f"  Filter component specs ({len(flt_component_specs)})")

    for comp_spec in flt_component_specs:
        _log_comp_spec(comp_spec, 4)

    # Log sink component spec
    _logger.info("  Sink component spec:")
    _log_comp_spec(sink_component_spec, 4)

    # State for unique component naming
    next_suffix = 1
    all_comps: List[_AnyCompT] = []

    # Compute greatest operative MIP version if not provided
    if mip_version is None:
        _logger.info("  Computing greatest operative MIP version")
        descriptors: List[bt2.ComponentDescriptor] = []

        for comp_spec in itertools.chain(
            all_src_component_specs, flt_component_specs, [sink_component_spec]
        ):
            descriptors.append(
                bt2.ComponentDescriptor(
                    comp_spec.component_class,
                    typing.cast(bt2._ComponentParams, comp_spec.params),
                )
            )

        mip_version = bt2.get_greatest_operative_mip_version(descriptors)

        if mip_version is None:
            raise RuntimeError("failed to find an operative MIP version")

    # Build graph
    _logger.info(f"  Creating graph with MIP {mip_version}")
    graph = bt2.Graph(mip_version)

    # Create muxer
    plugin = plugin_by_name("utils")

    if plugin is None:
        raise RuntimeError(
            "cannot find `utils` plugin (needed for the `flt.utils.muxer`)"
        )

    if "muxer" not in plugin.filter_component_classes:
        raise RuntimeError("cannot find `flt.utils.muxer` within the `utils` plugin")

    _logger.info("  Instantiating `flt.utils.muxer` as component `muxer`")
    muxer_comp = graph.add_component(plugin.filter_component_classes["muxer"], "muxer")
    all_comps.append(muxer_comp)

    # Track the last output port and its component in the chain
    last_out_port = muxer_comp.output_ports["out"]
    last_out_comp = muxer_comp

    # Create filter components (chained)
    for comp_spec in flt_component_specs:
        flt_comp, next_suffix = _create_comp(graph, comp_spec, all_comps, next_suffix)
        flt_comp = typing.cast(bt2._GenericFilterComponentConst, flt_comp)
        out_port = list(flt_comp.output_ports.values())[0]
        in_port = list(flt_comp.input_ports.values())[0]
        _connect_ports(graph, last_out_port, last_out_comp, in_port, flt_comp)
        last_out_port = out_port
        last_out_comp = flt_comp

    # Create source components
    src_comps: List[_AnyCompT] = []

    for comp_spec in all_src_component_specs:
        src_comp, next_suffix = _create_comp(graph, comp_spec, all_comps, next_suffix)
        src_comps.append(src_comp)

    # Connect source output ports to muxer input ports
    for src_comp in src_comps:
        src_comp = typing.cast(bt2._GenericSourceComponentConst, src_comp)

        for out_port in src_comp.output_ports.values():
            muxer_in_port = _get_free_muxer_input_port(muxer_comp)
            assert muxer_in_port is not None
            _connect_ports(graph, out_port, src_comp, muxer_in_port, muxer_comp)

    # Create sink component and connect it
    sink_comp, next_suffix = _create_comp(
        graph, sink_component_spec, all_comps, next_suffix
    )
    sink_comp = typing.cast(bt2._GenericSinkComponentConst, sink_comp)
    sink_in_port = list(sink_comp.input_ports.values())[0]
    _connect_ports(graph, last_out_port, last_out_comp, sink_in_port, sink_comp)

    # Run graph to completion, retrying on `bt2.TryAgain` (like the
    # CLI does).
    _logger.info("  Running graph")

    while True:
        try:
            graph.run()
            break
        except bt2.TryAgain:
            _logger.debug("  Got `bt2.TryAgain`: sleeping 0.1 s")
            time.sleep(0.1)

    _logger.info("  Graph completed")


# Runs a graph with `sink.text.details` using convert() and compares
# the output to the expectation `expect` (a string or the path of an
# expectation file to open).
#
# `src_component_specs` and `flt_component_specs` are like
# their convert() equivalent.
#
# `expect` is the expected output, either:
#
# • A `pathlib.Path` object (expectation file path).
# • A string (dedented before comparison).
#
# The `sink.text.details` component receives its `color` initialization
# parameter set to `never`, therefore the expectation string must not
# contain terminal color codes.
#
# `details_params` is an optional dictionary of initialization
# parameters to pass to the `sink.text.details` component.
#
# `mip_version` is like its convert() equivalent.
#
# This function asserts that the output matches the expectation
# (both stripped of leading/trailing whitespace).
def convert_sink_text_details_test(
    src_component_specs: Union[
        str,
        pathlib.Path,
        bt2.AutoSourceComponentSpec,
        bt2.ComponentSpec,
        Iterable[
            Union[str, pathlib.Path, bt2.AutoSourceComponentSpec, bt2.ComponentSpec]
        ],
    ],
    expect: Union[pathlib.Path, str],
    flt_component_specs: Optional[
        Union[bt2.ComponentSpec, Iterable[bt2.ComponentSpec]]
    ] = None,
    details_params: Optional[Dict[str, Any]] = None,
    mip_version: Optional[int] = None,
) -> None:
    if details_params is None:
        details_params = {}

    # Find the `text` plugin and get `details` sink component class
    plugin = plugin_by_name("text")

    if plugin is None:
        raise RuntimeError("cannot find `text` plugin")

    if "details" not in plugin.sink_component_classes:
        raise RuntimeError("cannot find `sink.text.details` within the `text` plugin")

    with tempfile.TemporaryDirectory(prefix="bt-test-sink.text.details-") as temp_dir:
        # Output file path
        temp_path = pathlib.Path(temp_dir) / "output.txt"

        # Add the path to the details params
        full_params = dict(details_params)
        full_params["path"] = str(temp_path)
        full_params["color"] = "never"

        # Create sink component spec
        sink_spec = SinkComponentSpec(
            plugin.sink_component_classes["details"], full_params
        )

        # Run the conversion
        convert(
            src_component_specs,
            sink_spec,
            flt_component_specs,
            mip_version=mip_version,
        )

        # Read the output
        output = temp_path.read_text(encoding="utf-8").strip()

        # Get expected output
        if isinstance(expect, pathlib.Path):
            expected = expect.read_text(encoding="utf-8").strip()
        else:
            expected = textwrap.dedent(expect).strip()

        # Compare
        if output != expected:
            old_expected = expected

            if (
                isinstance(expect, pathlib.Path)
                and os.environ.get("BT_TESTS_WRITE_EXPECTED") == "1"
            ):
                expect.write_text(f"{output}\n", encoding="utf-8")

            assert output == old_expected


# Returns the root build directory (the build directory containing the
# `tests` directory) which the root `conftest.py` sets on the pytest
# configuration object `config`.
def config_build_root_dir(config: object) -> pathlib.Path:
    return getattr(config, "build_root_dir")


# Returns the build directory equivalent to the source directory
# containing `source_file`.
#
# `build_root_dir` is the root build directory (the build directory
# containing the `tests` directory).
#
# Typical usage examples:
#
#     build_dir_of_source_file(config_build_root_dir(config), __file__)
#
# For example, if `source_file` is
# `/path/to/babeltrace/tests/meow/mix/test_lel.py`, then this function
# returns `/path/to/build/tests/meow/mix`.
def build_dir_of_source_file(
    build_root_dir: pathlib.Path, source_file: Union[pathlib.Path, str]
) -> pathlib.Path:
    source_file = pathlib.Path(source_file).resolve()

    # Find root source dir. by walking up until we find `pyproject.toml`
    src_root_dir = source_file.parent

    while not (src_root_dir / "pyproject.toml").exists():
        assert src_root_dir.parent != src_root_dir
        src_root_dir = src_root_dir.parent

    # Equivalent path in the build directory
    return build_root_dir / source_file.parent.relative_to(src_root_dir)


# Builds a `bt2.TraceCollectionMessageIterator` with `args` and
# `kwargs`, collects all the event messages until the end, and returns
# the list.
def tcmi_events(*args: Any, **kwargs: Any) -> List[Any]:
    assert "plugin_set" not in kwargs
    kwargs["plugin_set"] = plugin_set()
    msgs = typing.cast(
        Iterable[bt2._MessageConst],
        bt2.TraceCollectionMessageIterator(*args, **kwargs),
    )

    return [m.event for m in msgs if isinstance(m, bt2._EventMessageConst)]
