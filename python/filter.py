import sys
import json

# Speify that this is a filter agent
agent_type = "filter"

# This is an optional function that is called once, when the script is loaded.
# Use it for exampleto open a serial port connection.
# use the dictionary 'state' to store state objects for this script (e.g. port)
def setup():
  print("[Python] Setting up filter...")
  print("[Python] Parameters: " + json.dumps(params))
  print("[Python] Topic: " + topic)
  state["port"] = params["port"]
  print("[Python] Port: " + state["port"])


# This is a mandatory function that must be implemented in the script.
# The function must return a JSON string.
def process():
  print("[Python] Processing data from topic '" + topic + "'...")
  print("[Python] port: ", state["port"])
  data["processed"] = True
  return json.dumps(data)
