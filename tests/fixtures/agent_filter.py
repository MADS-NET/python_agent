import json

agent_type = "filter"


def setup():
  state["setup_ran"] = True


def process():
  out = dict(data)
  out["processed"] = True
  if "n" in out:
    out["n"] = out["n"] * 2
  out["topic_seen"] = topic
  return json.dumps(out)
