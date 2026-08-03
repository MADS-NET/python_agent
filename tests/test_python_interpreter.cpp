// Unit tests for Mads::PythonInterpreter.
//
// IMPORTANT: each TEST_CASE below constructs at most one
// Mads::PythonInterpreter (so at most one cppy3::Interpreter, i.e. one
// Py_InitializeFromConfig()/Py_Finalize() cycle), and never inside a
// SECTION. A Catch2 SECTION re-runs its parent TEST_CASE's body from the
// top for every leaf section, so a PythonInterpreter built ahead of two
// sibling SECTIONs would be constructed and destroyed twice in the same
// process. That is not just wasteful here -- it crashes: NumPy's extension
// module is single-phase-init and refuses a second import after
// Py_Finalize(), throwing "ImportError: cannot load module more than once
// per process" (confirmed empirically while writing this suite). Each
// TEST_CASE gets its own process via Catch2's CTest integration
// (catch_discover_tests registers one ctest test per TEST_CASE, each a
// fresh invocation of this binary) -- that isolation is what makes a
// Python interpreter cycle safe at all, and it only holds at TEST_CASE
// granularity, not at SECTION granularity.
#include <python_interpreter.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <cstdlib>
#include <filesystem>

using namespace std;
using json = nlohmann::json;

namespace {

path fixture_dir() { return path(PYTHON_AGENT_TEST_FIXTURE_DIR); }

json base_settings() {
  json settings;
  settings["search_paths"] = json::array({fixture_dir().string()});
  settings["venv"] = nullptr;
  return settings;
}

void set_env(char const *name, string const &value) {
#ifdef _WIN32
  _putenv_s(name, value.c_str());
#else
  setenv(name, value.c_str(), 1);
#endif
}

void unset_env(char const *name) {
#ifdef _WIN32
  _putenv_s(name, "");
#else
  unsetenv(name);
#endif
}

} // namespace

TEST_CASE("filter agent: agent_type, params, and process() round-trip") {
  unset_env("VIRTUAL_ENV");
  auto settings = base_settings();
  // An apostrophe and a backslash: under the pre-v2-port code, both used to
  // be pasted into a Python string literal
  // ("mads.params = json.loads('" + dump + "')"), so either one broke the
  // literal and aborted the process before setup() ever ran.
  settings["label"] = "it's a \\path\\ label";

  Mads::PythonInterpreter py(settings, "agent_filter");

  CHECK(py.agent_type() == "filter");
  CHECK(py.python_module() == "agent_filter");
  CHECK(py.ns().eval("mads.params['label']").str() == "it's a \\path\\ label");

  // The same hostile characters, this time on the per-message path
  // (set_input(), the old topic/data exec() pair).
  json payload;
  payload["n"] = 21;
  payload["note"] = R"(don't "break" me \ here)";
  py.set_input("nasty'topic", payload.dump());

  CHECK(py.ns().eval("mads.topic").str() == "nasty'topic");
  CHECK(py.ns().eval("mads.data['note']").str() ==
        R"(don't "break" me \ here)");

  json out = json::parse(py.call("process").str());
  CHECK(out["n"] == 42);
  CHECK(out["processed"] == true);
  CHECK(out["topic_seen"] == "nasty'topic");
}

TEST_CASE("filter agent: mads.<name> aliases the bare state/data/topic/params/agent_type, not a copy") {
  unset_env("VIRTUAL_ENV");
  auto settings = base_settings();
  settings["label"] = "via-mads-check";

  Mads::PythonInterpreter py(settings, "agent_mads_alias");

  json payload;
  payload["n"] = 7;
  py.set_input("some-topic", payload.dump());

  json out = json::parse(py.call("process").str());
  CHECK(out["agent_type_via_mads"] == "filter");
  CHECK(out["topic_via_mads"] == "some-topic");
  CHECK(out["params_via_mads"] == "via-mads-check");
  // mads.state is state, not a copy: a write through the bare name in
  // setup() must be visible through the mads. prefix in process().
  CHECK(out["state_is_shared"] == true);
}

TEST_CASE("filter agent: calling a missing function throws cppy3::Error") {
  unset_env("VIRTUAL_ENV");
  Mads::PythonInterpreter py(base_settings(), "agent_filter");

  REQUIRE_THROWS_AS(py.call("no_such_function"), cppy3::Error);
  try {
    py.call("no_such_function");
    FAIL("expected cppy3::Error");
  } catch (cppy3::Error const &e) {
    CHECK_THAT(string(e.what()), Catch::Matchers::ContainsSubstring("AttributeError"));
  }
}

TEST_CASE("source agent: module-level state persists across get_output() calls") {
  unset_env("VIRTUAL_ENV");
  Mads::PythonInterpreter py(base_settings(), "agent_source");

  CHECK(py.agent_type() == "source");
  json a = json::parse(py.call("get_output").str());
  json b = json::parse(py.call("get_output").str());
  json c = json::parse(py.call("get_output").str());
  CHECK(a["n"] == 0);
  CHECK(b["n"] == 1);
  CHECK(c["n"] == 2);
}

TEST_CASE("a module with no agent_type declared defaults to source") {
  unset_env("VIRTUAL_ENV");
  Mads::PythonInterpreter py(base_settings(), "agent_no_type");
  CHECK(py.agent_type() == "source");
}

TEST_CASE("mads.params reflects the venv resolved from $VIRTUAL_ENV, not the constructor argument") {
  // setup_venv() only requires <venv>/lib/<name containing "python">/ to
  // exist -- it never checks that site-packages inside it is real -- so a
  // throwaway directory exercises the resolution path without depending on
  // this machine's actual virtualenv.
  auto fake_venv =
      filesystem::temp_directory_path() / "python_agent_test_fake_venv";
  filesystem::create_directories(fake_venv / "lib" / "python3.99");
  set_env("VIRTUAL_ENV", fake_venv.string());

  auto settings = base_settings(); // venv left null: must resolve from env

  Mads::PythonInterpreter py(settings, "agent_no_type");

  string seen = py.ns().eval("str(mads.params['venv'])").str();
  CHECK(seen == fake_venv.string());

  unset_env("VIRTUAL_ENV");
  filesystem::remove_all(fake_venv);
}

TEST_CASE("a missing venv directory throws instead of aborting the process") {
  unset_env("VIRTUAL_ENV");
  auto settings = base_settings();
  settings["venv"] = "/nonexistent/path/to/a/venv/that/does/not/exist";

  REQUIRE_THROWS_WITH(
      Mads::PythonInterpreter(settings, "agent_no_type"),
      Catch::Matchers::ContainsSubstring("does not exist"));
}

TEST_CASE("importing a nonexistent Python module throws cppy3::Error instead of exiting the process") {
  unset_env("VIRTUAL_ENV");
  REQUIRE_THROWS_AS(
      Mads::PythonInterpreter(base_settings(), "no_such_module_at_all"),
      cppy3::Error);
}
