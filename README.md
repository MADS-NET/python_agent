[![Build and Release](https://github.com/MADS-NET/python_agent/actions/workflows/release.yml/badge.svg)](https://github.com/MADS-NET/python_agent/actions/workflows/release.yml)

# MADS Monolithic Python Interpreter


## Compilation

> This plugin has been updated for MADS v2

Note that on linux this must be compiled with Clang, not with GCC. Be sure to `sudo apt install clang` and then set clang as default with `sudo update-alternatives --config c++` and select `/usr/bin/clang++`, then repeat with `sudo update-alternatives --config cc` and select `/usr/bin/clang`.

### UNIX

You need to have `python3` and `python3-dev` installed. Then:

```sh
python3 -m venv .venv
source .venv/bin/activate
pip install numpy
# also install other necessary Python libs

cmake -Bbuild"
cmake --build build -j6
sudo cmake --install build
```

The above is tested on MacOS and Ubuntu 22.04.

### Windows

Run the following from project root:

```powershell
python -m venv .venv
.venv\Scripts\activate.ps1
pip install numpy
```

Then: 

```powershell
cmake -Bbuild
cmake --build build --config Release
sudo cmake --install build
```

Or, if you have Ninja:

```powershell
cmake -Bbuild -G Ninja
cmake --build build -j6
cmake --install build
```

Then you can just type `mads-python -h`.

**Note**: for `sudo` to work on Windows, you need to enable it on *Settings > System > For Developers* and set *Enable sudo* to On.

### Bundled Python (portable builds)

By default the plugin links **dynamically** against whatever Python it finds on the
build machine (`find_package(Python3 ...)`). This is fine for builds made and used on
the same machine, but it makes the resulting `mads-python` binary **non-portable**: a
target machine needs a byte-compatible `libpython3.x`, the matching standard library,
and any packages used by your scripts (e.g. numpy) installed at the same locations.

For portable, self-contained packages there is an **opt-in** build mode that bundles a
relocatable [python-build-standalone](https://github.com/astral-sh/python-build-standalone)
CPython distribution inside the package, next to the executable. The binary then loads
its `libpython` and standard library from that bundled copy (via rpath and a
`PYTHONHOME` set relative to the executable at startup), so the package runs on a clean
machine with no system Python at all.

This mode is controlled by two CMake options:

| Option | Default | Meaning |
|--------|---------|---------|
| `PYTHON_AGENT_BUNDLE_PYTHON` | `OFF` | Enable bundling. When `OFF`, behaviour is exactly as described above (system/venv Python). |
| `PYTHON_BUNDLE_DIR` | *(empty)* | Path to an extracted, pip-populated python-build-standalone `install_only` distribution (the directory containing `bin/python3` on Unix or `python.exe` on Windows). **Required** when `PYTHON_AGENT_BUNDLE_PYTHON=ON`. |

To build in bundled mode, first download and prepare a distribution, then point the
build at it. For example on macOS (arm64):

```sh
# 1. Download + extract a matching python-build-standalone distribution
curl -fsSL "https://github.com/astral-sh/python-build-standalone/releases/download/20260623/cpython-3.12.13+20260623-aarch64-apple-darwin-install_only_stripped.tar.gz" -o cpython.tar.gz
mkdir -p bundle && tar xzf cpython.tar.gz -C bundle   # -> bundle/python/...

# 2. Install your script dependencies into the bundle, using ITS OWN pip
./bundle/python/bin/python3 -m pip install numpy

# 3. Configure + build with bundling enabled
cmake -Bbuild -G Ninja \
  -DPYTHON_AGENT_BUNDLE_PYTHON=ON \
  -DPYTHON_BUNDLE_DIR="$PWD/bundle/python"
cmake --build build -j6
cmake --install build
```

The installed package gains a `python-runtime/` directory (alongside `bin/` and
`lib/`) holding the bundled interpreter. Any Python packages your scripts need must be
`pip install`ed **into the bundle** (step 2) so they ship with it — the bundled
interpreter does not see the system/venv site-packages.

The GitHub Actions release workflow builds **all** published packages this way, using a
pinned python-build-standalone release (see `PBS_RELEASE` / `PBS_PYTHON_VERSION` in
`.github/workflows/release.yml`), so downloadable releases are self-contained.

> **Note:** this only makes the embedded Python portable. The `mads-python` binary still
> depends on a compatible system C/C++ runtime (glibc/libstdc++ on Linux) and on the
> MADS core libraries, exactly as before.

## Executing

Typically, to launch an agent named `python_source`, that gets its settings from a `python_source` section in `mads.ini`, and uses the Python module named `source` defined in the `source.py` file and that runs every 100 ms, the command is:

```sh
mads python -n python_source -m source -p100
```

## Python modules search paths

The Python modules are searched for in the following folders:

* `./python`
* `./scripts`
* `../python`
* `../scripts`
* `../../python`
* `../../scripts`
* `INSTALL_PREFIX + /python` 
* `INSTALL_PREFIX + /scripts`

plus any path listed in the `mads.ini` file under the `search_path` key (an array or a single string).

## `mads.ini` section

The following fields are typically used:

```ini
[python_source]
period = 200
venv = "/path/to/.venv"
python_module = "my_source"
search_paths = ["/path/to/python/folder"]
```

## Module Types

Python modules can be of type `source`, `filter`, or `sink`. The module type is defined by setting a top level variable like this, typically at the beginning of the script, just after the various `import`s:

```python
agent_type = "sink"
```

All the modules **must** implement a `setup()` function, which is expected to use the dictionary available in the module variable `params` (a dictionary) to do initial setup (opening ports or files, etc.)

**Source** modules **must** implement a `get_output()` function, that produces the JSON string that will be published.

**Filter** modules **must** implement a `process()` function, that is supposed to operate on the last received data dictionary (available as `data`, a module variable) and produce a JSON string that will be published.

**Sink** modules **must** implement a `deal_with_data()` function, that operates on the `data` dictionary, a module variable.

See examples in the `python` folder.
