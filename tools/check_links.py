#!/usr/bin/env python3
"""Check relative links, images and anchors in the tracked Markdown files. Exit 1 if any is broken."""
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import unquote

LINK = re.compile(r"!?\[[^\]]*\]\(\s*<?([^)>\s]+)>?(?:\s+\"[^\"]*\")?\s*\)")
HTML = re.compile(r"<(?:img|a)\b[^>]*?\b(?:src|href)=[\"']([^\"']+)[\"']", re.IGNORECASE)
REFERENCE = re.compile(r"^\s*\[[^\]]+\]:\s*<?(\S+?)>?(?:\s|$)", re.MULTILINE)
FENCE = re.compile(r"^(```|~~~).*?^\1", re.MULTILINE | re.DOTALL)


def anchors(path):
    found = {}
    text = FENCE.sub("", path.read_text(encoding="utf-8"))
    result = set()
    for title in re.findall(r"^#{1,6}\s+(.+?)\s*#*$", text, re.MULTILINE):
        title = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", title).lower()
        slug = re.sub(r"[^\w\- ]", "", title).replace(" ", "-")
        count = found.get(slug, 0)
        found[slug] = count + 1
        result.add(slug if count == 0 else f"{slug}-{count}")
    return result


def main():
    root = Path(subprocess.run(["git", "rev-parse", "--show-toplevel"], capture_output=True,
                               text=True, check=True).stdout.strip())
    names = subprocess.run(["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard", "*.md"],
                           cwd=root, capture_output=True, text=True, check=True).stdout.split("\0")
    broken = checked = 0
    for name in sorted(set(filter(None, names))):
        source = root / name
        if not source.is_file():
            continue
        text = FENCE.sub(lambda m: "\n" * m.group().count("\n"), source.read_text(encoding="utf-8"))
        plain = re.sub(r"`[^`\n]*`", lambda m: " " * len(m.group()), text)
        for pattern in (LINK, HTML, REFERENCE):
            for match in pattern.finditer(text if pattern is LINK else plain):
                target = unquote(match.group(1))
                if re.match(r"[a-z][a-z0-9+.-]*:|//", target, re.IGNORECASE):
                    continue
                checked += 1
                path, _, anchor = target.partition("#")
                base = root if path.startswith("/") else source.parent
                destination = (base / path.lstrip("/")).resolve() if path else source
                problem = None
                if not destination.exists():
                    problem = "missing file"
                elif anchor and destination.suffix == ".md" and anchor not in anchors(destination):
                    problem = "missing anchor"
                if problem:
                    broken += 1
                    print(f"{name}:{text.count(chr(10), 0, match.start()) + 1}: {problem}: {target}")
    print(f"{checked} relative links checked, {broken} broken")
    return 1 if broken else 0


if __name__ == "__main__":
    sys.exit(main())
