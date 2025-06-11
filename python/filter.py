import sys
import json

# Speify that this is a filter agent
agent_type = "filter"

# This is an optional function that is called once, when the script is loaded.
# Use it for exampleto open a serial port connection.
# Use the dictionary 'state' to store state objects for this script (e.g. port);
# the state dictionary is visible to all functions in this script
# The 'params' dictionary contains the parameters passed to the filter, 
# obtained from the configuration file.
def setup():
  print("[Python] Setting up filter...")
  print("[Python] Parameters: " + json.dumps(params))
  print("[Python] Topic: " + topic)
  state["port"] = params["port"]
  print("[Python] Port: " + state["port"])

# This is a mandatory function that must be implemented in the script.
# The function must return a JSON string.
# It has access to the dictionaries `data` , `state` and `params`, and to the
# string `topic`.
def process():
  # print("[Python] Processing data from topic '" + topic + "'...")
  # print("[Python] port: ", state["port"])
  # print("[Python] data:", data)
  data["processed"] = True
  data["n"] = data["n"] * 2
  # print("[Python] data:", data)
  return json.dumps(data)
