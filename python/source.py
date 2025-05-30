import json
import random 
import time
from datetime import datetime
import serial
# import mfrc522

# Specify that this is a source agent
agent_type = "source"

# This is an optional function that is called once, when the script is loaded.
# Use it for exampleto open a serial port connection.
# Use the dictionary 'state' to store state objects for this script (e.g. port);
# the state dictionary is visible to all functions in this script
# The 'params' dictionary contains the parameters passed to the filter, 
# obtained from the configuration file.
def setup():
  print("[Python] Setting up source...")
  print("[Python] Parameters: " + json.dumps(params))


# This is a mandatory function that must be implemented in the script.
# The function must return a JSON string.
# It has access to the dictionaries `state` and `params`, and to the
# string `topic`.
def get_output():
  data = {"list": [1, 2, 3, 4]}
  data["processed"] = False
  return json.dumps(data)
