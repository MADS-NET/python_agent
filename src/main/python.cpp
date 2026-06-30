/*
  ____        _   _                                          _
 |  _ \ _   _| |_| |__   ___  _ __     __ _  __ _  ___ _ __ | |_
 | |_) | | | | __| '_ \ / _ \| '_ \   / _` |/ _` |/ _ \ '_ \| __|
 |  __/| |_| | |_| | | | (_) | | | | | (_| | (_| |  __/ | | | |_
 |_|    \__, |\__|_| |_|\___/|_| |_|  \__,_|\__, |\___|_| |_|\__|
        |___/                               |___/
An agent that runs Python3 scripts
Author(s): Paolo Bosetti
*/
#include "../python_interpreter.hpp"
#include <agent.hpp>
#include <cppy3/cppy3.hpp>
#include <cppy3/utils.hpp>
#include <cstdlib>
#include <cxxopts.hpp>
#include <mads.hpp>
#include <agent_app.hpp>

using namespace std;
using namespace cxxopts;
using namespace Mads;
using json = nlohmann::json;

int main(int argc, char *argv[]) {
#ifdef PYTHON_AGENT_BUNDLED
  // When built with the bundled python-build-standalone distribution, point
  // PYTHONHOME at it (relative to this executable) BEFORE the interpreter is
  // initialized. cppy3::PythonVM's constructor calls Py_InitializeEx(), which
  // honors PYTHONHOME, and that member is default-constructed when the
  // PythonInterpreter object below is created -- so this must run first.
  {
    string bundled_home = exec_dir("../python-runtime");
#ifdef _WIN32
    _putenv_s("PYTHONHOME", bundled_home.c_str());
#else
    setenv("PYTHONHOME", bundled_home.c_str(), 1);
#endif
  }
#endif

  string settings_uri = SETTINGS_URI;
  chrono::milliseconds time{100};

  AgentApp agent{argv[0], settings_uri};
  // clang-format off
  agent.options()
    ("p,period", "Sampling period (default 100 ms)", value<size_t>())
    ("m,module", "Python module to load", value<string>());
  // clang-format on
  agent.add_agent_identity_options();
  agent.add_common_options();

  auto options_parsed = agent.parse_options(argc, argv);
  if (int rc = AgentApp::handle_standard_exit_options<AgentApp>(
          options_parsed, agent.raw_options(), argv);
      rc >= 0) {
    return rc;
  }

  if (options_parsed.count("name") != 0) {
    agent.set_agent_name(options_parsed["name"].as<string>());
  } else {
    agent.set_agent_name("python");
  }

  // Initialize agent ==========================================================
  try {
    agent.init(options_parsed);
  } catch (const std::exception &e) {
    std::cout << fg::red << "Error initializing agent: " << e.what()
              << fg::reset << endl;
    exit(EXIT_FAILURE);
  }
  agent.enable_remote_control();
  agent.enable_events();
  agent.connect();

  json settings = agent.get_settings();

  if (!agent.attachment_path().empty()) {
    auto dir = agent.attachment_path().parent_path();
    settings["search_paths"].push_back(dir.string());
    settings["python_module"] = agent.attachment_path().stem().string();
  }

  // CLI options overrides =====================================================
  if (options_parsed.count("module") != 0) {
    settings["python_module"] = options_parsed["module"].as<string>();
  }
  if (settings["python_module"].empty()) {
    cerr << fg::red << "Python module not specified in settings or command line"
         << fg::reset << endl;
    exit(EXIT_FAILURE);
  }
  if (!settings["period"].is_null()) {
    time = chrono::milliseconds(settings["period"].get<size_t>());
  }
  if (options_parsed.count("period") != 0) {
    time = chrono::milliseconds(options_parsed["period"].as<size_t>());
  }

  // Print info ================================================================
  agent.info(cerr);
  cerr << "  Loaded module:    " << style::bold
       << settings["python_module"].get<string>() << style::reset << endl;

  // Instantiate interpreter
  PythonInterpreter py(settings, settings["python_module"].get<string>());

  // Main loop =================================================================
  cout << fg::green << "Python process started" << fg::reset << endl;

  // SOURCE
  if (py.agent_type() == "source") {
    string result;
    json out;
    agent.loop([&]() -> chrono::milliseconds {
      try {
        result =
            cppy3::WideToUTF8(cppy3::eval("mads.get_output()").toString());
        out = json::parse(result);
      } catch (cppy3::PythonException &e) {
        cerr << fg::red << "Error running get_output(): " << e.what()
              << fg::reset << endl;
        return 0ms;
      } catch (json::parse_error &e) {
        cerr << fg::red << "Error parsing JSON: " << e.what() << fg::reset
              << endl
              << "JSON was: " << result << endl;
        return 0ms;
      }
      if (!out.empty()) {
        agent.publish(out);
      }
      return 0ms;
    }, time);

    // FILTER
  } else if (py.agent_type() == "filter") {
    message_type type;
    auto msg = agent.last_message();
    json out;
    string result;
    string load_topic, load_data;
    agent.loop([&]() -> chrono::milliseconds {
      try {
        type = agent.receive();
      } catch (const AgentError &e) {
        cerr << fg::red << "Error receiving message: " << e.what() << fg::reset
             << endl;
        return 0ms;
      }
      msg = agent.last_message();
      // agent.remote_control();
      if (type == message_type::json && agent.last_topic() != "control") {
        try {
          cppy3::exec("mads.topic = '" + agent.last_topic() + "'");
          cppy3::exec("mads.data = json.loads('" + get<1>(msg) + "')");
        } catch (cppy3::PythonException &e) {
          cerr << fg::red << "Error loading data: " << e.what() << fg::reset
               << endl;
          return 0ms;
        }
        try {
          result = cppy3::WideToUTF8(cppy3::eval("mads.process()").toString());
          out = json::parse(result);
        } catch (cppy3::PythonException &e) {
          cerr << fg::red << "Error running process(): " << e.what()
               << fg::reset << endl;
          return 0ms;
        } catch (json::parse_error &e) {
          cerr << fg::red << "Error parsing JSON: " << e.what() << fg::reset
               << endl
               << "JSON was: " << result << endl;
          return 0ms;
        }
        if (!out.empty()) {
          agent.publish(out);
        }
      }
      return 0ms;
    });

    // SINK
  } else if (py.agent_type() == "sink") {
    message_type type;
    auto msg = agent.last_message();
    json in;
    agent.loop([&]() -> chrono::milliseconds {
      try {
        type = agent.receive();
      } catch (const AgentError &e) {
        cerr << fg::red << "Error receiving message: " << e.what() << fg::reset
             << endl;
        return 0ms;
      }
      msg = agent.last_message();
      // agent.remote_control();
      if (type == message_type::json && agent.last_topic() != "control") {
        try {
          cppy3::exec("mads.topic = '" + agent.last_topic() + "'");
          cppy3::exec("mads.data = json.loads('" + get<1>(msg) + "')");
        } catch (cppy3::PythonException &e) {
          cerr << fg::red << "Error loading data: " << e.what() << fg::reset
               << endl;
          return 0ms;
        }
        try {
          cppy3::exec("mads.deal_with_data()");
        } catch (cppy3::PythonException &e) {
          cerr << fg::red << "Error running deal_with_data(): " << e.what()
               << fg::reset << endl;
          return 0ms;
        }
      }
      return 0ms;
    });
  }
  cout << fg::green << "Python process stopped" << fg::reset << endl;

  // Cleanup ===================================================================
  agent.disconnect();
  agent.restart_if_requested(argv);
  return 0;
}