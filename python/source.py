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
# use the dictionary 'state' to store state objects for this script (e.g. port)
def setup():
  print("[Python] Setting up source...")
  print("[Python] Parameters: " + json.dumps(params))

# This is a mandatory function that must be implemented in the script.
# The function must return a JSON string.
def get_output():
  data = {"list": [1, 2, 3, 4]}
  data["processed"] = True
  return json.dumps(data)
