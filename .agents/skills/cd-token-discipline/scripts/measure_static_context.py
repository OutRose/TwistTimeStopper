#!/usr/bin/env python3
"""Measure repository-owned static Codex context without external dependencies."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any

FRONTMATTER = re.compile(
    r"\A---\r?\n(?P<meta>.*?)\r?\n---(?:\r?\n|\Z)(?P<body>.*)\Z",
    re.DOTALL,
)


def _frontmatter(text: str) -> tuple[dict[str, str], str]:
    match = FRONTMATTER.fullmatch(text)
    if match is None:
        raise ValueError("frontmatter delimiters are missing")
    metadata: dict[str, str] = {}
    for line in match.group("meta").splitlines():
        key, separator, value = line.partition(":")
        if not separator or not key.strip() or not value.strip():
            raise ValueError(f"invalid frontmatter line: {line!r}")
        normalized = value.strip()
        if len(normalized) >= 2 and normalized[0] == normalized[-1] and normalized[0] in {"'", '"'}:
            normalized = normalized[1:-1]
        metadata[key.strip()] = normalized
    return metadata, match.group("body")


def _file_record(path: Path, repo_root: Path) -> dict[str, Any]:
    raw = path.read_bytes()
    return {
        "path": path.relative_to(repo_root).as_posix(),
        "utf8_bytes": len(raw),
        "code_points": len(raw.decode("utf-8")),
    }


def measure(repo_root: Path) -> dict[str, Any]:
    repo_root = repo_root.resolve(strict=True)
    if not repo_root.is_dir():
        raise ValueError("repo root must be a directory")

    agent_files: list[dict[str, Any]] = []
    for candidate in (repo_root / "AGENTS.md", repo_root / ".codex" / "AGENTS.md"):
        if candidate.is_file() and not candidate.is_symlink():
            agent_files.append(_file_record(candidate, repo_root))

    skill_items: list[dict[str, Any]] = []
    skills_root = repo_root / ".agents" / "skills"
    if skills_root.is_dir() and not skills_root.is_symlink():
        for skill_file in sorted(skills_root.glob("*/SKILL.md"), key=lambda item: item.as_posix()):
            if not skill_file.is_file() or skill_file.is_symlink():
                continue
            raw = skill_file.read_bytes()
            text = raw.decode("utf-8")
            try:
                metadata, body = _frontmatter(text)
                error = None
            except ValueError as exc:
                metadata, body = {}, ""
                error = str(exc)
            skill_items.append(
                {
                    "path": skill_file.relative_to(repo_root).as_posix(),
                    "name": metadata.get("name"),
                    "description_code_points": len(metadata.get("description", "")),
                    "body_utf8_bytes": len(body.encode("utf-8")),
                    "file_utf8_bytes": len(raw),
                    "metadata_error": error,
                }
            )

    return {
        "schema_version": 1,
        "status": "PASS",
        "repo_root": str(repo_root),
        "agents": {
            "files": agent_files,
            "total_utf8_bytes": sum(item["utf8_bytes"] for item in agent_files),
        },
        "repo_skills": {
            "discovered_count": len(skill_items),
            "description_code_points": sum(item["description_code_points"] for item in skill_items),
            "body_utf8_bytes": sum(item["body_utf8_bytes"] for item in skill_items),
            "items": skill_items,
            "enabled_state": "unknown; obtain the live runtime catalog separately",
        },
        "runtime": {
            "enabled_tool_count": None,
            "usage_fields": None,
            "note": "static repository measurement cannot determine runtime tools or token usage",
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, required=True)
    args = parser.parse_args()
    try:
        result = measure(args.repo_root)
    except (OSError, UnicodeError, ValueError) as exc:
        result = {"schema_version": 1, "status": "FAIL", "error": str(exc)}
        print(json.dumps(result, ensure_ascii=False, sort_keys=True))
        return 1
    print(json.dumps(result, ensure_ascii=False, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
