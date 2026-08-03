import json

agent_type = "source"


def setup():
  state["n"] = 0


def get_output():
  out = {"n": state["n"]}
  state["n"] += 1
  return json.dumps(out)
