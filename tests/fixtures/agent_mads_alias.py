import json

agent_type = "filter"


def setup():
  # A write through the bare name; process() below reads the same slot
  # through the mads. prefix, to prove mads.state is state (the same dict
  # object), not a synced copy that could drift.
  state["seen_via_mads"] = False


def process():
  mads.state["seen_via_mads"] = True
  out = dict(mads.data)
  out["processed"] = True
  out["topic_via_mads"] = mads.topic
  out["agent_type_via_mads"] = mads.agent_type
  out["params_via_mads"] = mads.params.get("label")
  out["state_is_shared"] = state["seen_via_mads"] and mads.state is state
  return json.dumps(out)
