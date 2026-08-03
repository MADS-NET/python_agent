import sys
import json

# Specify that this is a filter agent
mads.agent_type = "filter"

# This is an optional function that is called once, when the script is loaded.
# Use it for exampleto open a serial port connection.
# Use the dictionary 'state' to store state objects for this script (e.g. port);
# the state dictionary is visible to all functions in this script
# The 'params' dictionary contains the parameters passed to the filter, 
# obtained from the configuration file.
def setup():
  print("[Python] Setting up filter...")
  print("[Python] Parameters: " + json.dumps(mads.params))
  print("[Python] Topic: " + mads.topic)
  mads.state["port"] = mads.params["port"]
  print("[Python] Port: " + mads.state["port"])

# This is a mandatory function that must be implemented in the script.
# The function must return a JSON string.
# It has access to the dictionaries `data` , `state` and `params`, and to the
# string `topic`.
def process():
  # print("[Python] Processing data from topic '" + topic + "'...")
  # print("[Python] port: ", state["port"])
  # print("[Python] data:", data)
  mads.data["processed"] = True
  mads.data["n"] = mads.data["n"] * 2
  # print("[Python] data:", data)
  return json.dumps(mads.data)
