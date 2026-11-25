#!/usr/bin/env python3
"""The host's side of the mascot, as the skill's tools read it: `.agents/project.py` → `MASCOT` where the host's kit
keeps its facts in one Python file, else `.agents/project/config.json` → `mascot`. One loader, so the preview, the model
sheet and render_sounds.sh agree (SKILL.md → The host's side; references/build.md → The tools).

  python3 host_config.py <key>   prints the key's value, empty when the host leaves it out or sets it None

Python 3.9+, standard library only.
"""
import importlib.util
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]


def load():
    """The host's mascot settings as a dict; SystemExit when the host names none."""
    py = ROOT / ".agents/project.py"
    if py.exists():
        spec = importlib.util.spec_from_file_location("_mascot_host_project", py)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        if getattr(mod, "MASCOT", None):
            return dict(mod.MASCOT)
    js = ROOT / ".agents/project/config.json"
    if js.exists():
        cfg = json.loads(js.read_text()).get("mascot")
        if cfg:
            return dict(cfg)
    raise SystemExit("the host names no mascot: add MASCOT to .agents/project.py, or mascot to "
                     ".agents/project/config.json (the skill's SKILL.md → The host's side)")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(2)
    value = load().get(sys.argv[1])
    print("" if value is None else value)
