#!/usr/bin/env python3
"""
check_text.py  -  Mud Cats

Compares the text keys the game's code asks for with the ones the text
file has, so a line that's missing, or one nobody uses any more, shows up
before anyone sees "[some.key]" on screen.

    python3 tools/text/check_text.py              # assets/mudcats/text/en.json
    python3 tools/text/check_text.py fr           # another language's file

It finds keys written out whole in the code: tr("..."), pick("..."),
item("...", n), list("...") and has_text("..."), and the shout(s, "...")
barks (barks.<side>.<key>). Keys the code builds from pieces, like
"levels." + level_key(level) + ".won.", can't be checked this way; for
those, run the game with MUDCATS_TEXT_CHECK=1 and it reports every key it
looks up and doesn't find. Exits 1 if anything the code asks for is missing.
"""

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CODE = ROOT / "examples" / "mudcats"


def flatten(node, key, out):
    if isinstance(node, dict):
        for name, child in node.items():
            flatten(child, f"{key}.{name}" if key else name, out)
    elif isinstance(node, list):
        out.add(key)
        for i, child in enumerate(node):
            if isinstance(child, (dict, list)):
                flatten(child, f"{key}.{i}", out)
    else:
        out.add(key)


def main():
    language = sys.argv[1] if len(sys.argv) > 1 else "en"
    path = ROOT / "assets" / "mudcats" / "text" / f"{language}.json"
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as e:
        print(f"can't read {path}: {e}")
        return 1
    have = set()
    flatten(data, "", have)

    call = re.compile(r'\b(?:tr|pick|item|list|has_text|level_text)\(\s*"([a-z0-9_.]+)"')
    shout = re.compile(r'\bshout\(\s*[^,]+,\s*"([a-z0-9_]+)"')
    table = re.compile(r'"((?:title\.menu|debrief|briefing)\.[a-z0-9_.]+)"')
    used = {}
    for source in sorted(CODE.glob("*.[ch]pp")):
        for n, line in enumerate(source.read_text(encoding="utf-8").splitlines(), 1):
            for m in call.finditer(line):
                key = m.group(1)
                if line[m.start():].startswith("level_text("):
                    continue
                used.setdefault(key, f"{source.name}:{n}")
            for m in table.finditer(line):
                used.setdefault(m.group(1), f"{source.name}:{n}")
            for m in shout.finditer(line):
                # Each side's own line if it has one, else the Monteran one.
                key = m.group(1)
                if f"barks.rechivii.{key}" not in have:
                    used.setdefault(f"barks.monteran.{key}", f"{source.name}:{n}")

    missing = sorted(k for k in used if k not in have and not k.endswith("."))
    if missing:
        print("Asked for in the code, not in the file:")
        for k in missing:
            print(f"  {k:50s} {used[k]}")
    else:
        print(f"Every key the code names is in {path.name} ({len(used)} keys).")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
