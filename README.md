# MADS Monolithic Python Interpreter

## Compilation

You need to have `python3` and `python3-dev` installed. Then:

```sh
python3 -m venv .venv
source .venv/bin/activate
pip install numpy
# also install other necessary Python libs

cmake -Bbuild -DCMAKE_INSTALL_PREFIX=$(mads -p)
cmake --build build -j6
sudo cmake --install build
```

The above is tested on MacOS and Ubuntu 22.04.

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