import json

# Specify that this is a sink agent
agent_type = "sink"

# This is an optional function that is called once, when the script is loaded.
# Use it for exampleto open a serial port connection.
# use the dictionary 'state' to store state objects for this script (e.g. port)
def setup():
  print("[Python] Setting up sink...")
  print("[Python] Parameters: " + json.dumps(params))
  print("[Python] Topic: " + topic)

# This is a mandatory function that must be implemented in the script.
# The function needs to deal with the global variable 'data', 
# which is a dictionary containing the data received from the source.
# The topic variable is also available, containing the topic of the data.
# The function does not need to return anything.
def deal_with_data():
  print("[Python] topic: '" + topic + "'...")
  print("[Python] Data: ", data)