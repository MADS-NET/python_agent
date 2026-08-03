/*
   ____
  / ___|___  _ __ ___  _ __ ___   ___  _ __
 | |   / _ \| '_ ` _ \| '_ ` _ \ / _ \| '_ \
 | |__| (_) | | | | | | | | | | | (_) | | | |
  \____\___/|_| |_| |_|_| |_| |_|\___/|_| |_|

Common header for source, filter and sink python plugins
*/
#include <cppy3/cppy3.hpp>
#include <cppy3/utils.hpp>
#include <cstdlib>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <iostream>
#include <exec_path.hpp>
#include <rang.hpp>

using namespace std;
using namespace filesystem;
using namespace rang;
using json = nlohmann::json;

namespace Mads {

class PythonInterpreter {
public:
  PythonInterpreter(json const &params, string module)
      : _params(params), _python_module(module) {
    setup_venv();

    // One append_sys_path() call replaces "import sys" plus a
    // sys.path.append('...') exec per entry. It takes real path values and
    // skips duplicates, so a Windows path is no longer pasted into a Python
    // string literal where its backslashes read as escape sequences.
    vector<path> sys_paths = _default_paths;
    for (auto const &entry : _params["search_paths"]) {
      if (entry.is_string()) {
        sys_paths.push_back(entry.get<string>());
      } else if (entry.is_array()) {
        for (auto const &p : entry) {
          sys_paths.push_back(p.get<string>());
        }
      }
    }
    _python.append_sys_path(sys_paths);

    try {
      _mads = _python.import(_python_module);
    } catch (cppy3::Error const &e) {
      cerr << "Error loading module: " << e.format();
      exit(EXIT_FAILURE);
    }
    // Keep the module reachable as `mads` from __main__ as well, as the
    // "import <module> as mads" this replaces did.
    _ns.set("mads", _mads);

    // The module's mailbox, set as real attributes on the module object
    // rather than by exec'ing assignments against it.
    _json_loads = _python.import("json").attr("loads");
    _mads.set_attr("data", cppy3::Dict::create());
    _mads.set_attr("state", cppy3::Dict::create());
    _mads.set_attr("topic", cppy3::to_var(""));
    // params reaches json.loads() as a Python string argument, so a quote or
    // a backslash anywhere in the settings stays data instead of terminating
    // the literal it used to be pasted into.
    _mads.set_attr("params", _json_loads(params.dump()));

    try {
      call("setup");
    } catch (cppy3::Error const &e) {
      cerr << "[Python] running setup(): " << e.format();
    }
    // Get Agent type. A module that declares no agent_type is an ordinary,
    // expected case, so ask rather than catch the AttributeError.
    if (_mads.has_attr("agent_type")) {
      _agent_type = _mads.attr("agent_type").str();
    } else {
      cerr << fg::yellow << "[Python] Agent type unspecified, using default"
           << fg::reset << endl;
    }
    if (_agent_type != "source" && _agent_type != "filter" &&
        _agent_type != "sink") {
      cerr << fg::yellow
                << "[Python] Agent type not recognized, using default"
                << fg::reset << endl;
      _agent_type = "source";
    }
    cerr << "[Python] Agent type: " << _agent_type << endl;
  }

  void setup_venv() {
    if (_params["venv"].is_null() ||
        _params["venv"].get<string>().empty()) {
      const char *venv_path = getenv("VIRTUAL_ENV");
      if (venv_path && *venv_path) {
        cerr
            << "[Python] Using virtual environment from VIRTUAL_ENV shell var: "
            << venv_path << endl;
        _params["venv"] = string(venv_path);
      } else {
        cerr << "[Python] No virtual environment specified, using system "
                     "Python libraries"
                  << endl;
        return;
      }
    }

    if (!exists(_params["venv"].get<path>())) {
      throw runtime_error("Virtual environment does not exist: " +
                               _params["venv"].get<string>());
    } else {
      path venv = _params["venv"].get<path>();
      path lib = venv / "lib";
      path site_packages;
      for (auto &entry : directory_iterator(lib)) {
        if (entry.is_directory() && entry.path().filename().string().find(
                                        "python") != string::npos) {
          site_packages = entry.path() / "site-packages";
          break;
        }
      }
      if (site_packages.empty()) {
        throw runtime_error(
            "Could not find site-packages in virtual environment: " +
            venv.string());
      }
      // Still addsitedir() rather than a plain sys.path entry, since it also
      // processes the .pth files a venv relies on -- but as a call argument,
      // which keeps the path's backslashes intact on Windows.
      (void)_python.import("site").method("addsitedir", site_packages.string());
    }
  }

  string agent_type() const { return _agent_type; }
  string python_module() const { return _python_module; }
  cppy3::Namespace const &ns() const { return _ns; }

  // Calls mads.<name>() and returns its result. Throws cppy3::Error if the
  // attribute is missing or the call raises. Unlike the eval("mads.<name>()")
  // it replaces, this compiles no Python source per call.
  cppy3::Var call(string const &name) const { return _mads.method(name); }

  // Binds an incoming message onto the module as mads.topic and mads.data.
  // The payload is handed to json.loads() as a Python string argument, so it
  // cannot break out of -- or inject into -- the code that consumes it.
  void set_input(string const &topic, string const &payload) const {
    _mads.set_attr("topic", cppy3::to_var(topic));
    _mads.set_attr("data", _json_loads(payload));
  }

protected:
  // Config is v2's supported way to point CPython at a specific installation:
  // PyConfig replaced the PYTHONHOME environment variable this used to set
  // from main(), which CPython 3.12 stopped honoring through the old
  // Py_SetPythonHome() path.
  static cppy3::Config make_config() {
    cppy3::Config config;
    config.program_name = "mads-python";
#ifdef PYTHON_AGENT_BUNDLED
    // The relocatable python-build-standalone distribution ships next to the
    // executable.
    config.home = path(exec_dir("../python-runtime"));
#endif
    return config;
  }

  json _params;
  cppy3::Interpreter _python{make_config()};
  cppy3::Namespace _ns = _python.main();
  cppy3::Var _mads;       // the user module, also bound as `mads`
  cppy3::Var _json_loads; // json.loads, resolved once
  string _python_module;
  string _agent_type = "source";
  // clang-format off
  vector<path> _default_paths = {
      "./python",
      "./scripts",
      "../python",
      "../scripts",
      "../../python",
      "../../scripts",
      Mads::prefix() + "/python",
      Mads::prefix() + "/scripts"};
  // clang-format on
};

} // namespace Mads
