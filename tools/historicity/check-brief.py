"""Validate the traceability of a Pontic historicity brief; not its truth."""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / "docs" / "historicity"
BASES = {"atteste_local", "analogie_regionale", "hypothese_de_conception", "inconnu"}
VERDICTS = {"PROPOSE", "MEC_OBSERVE", "SCN_OBSERVE", "PLY_OBSERVE"}


def ids(path, prefix):
    body = path.read_text(encoding="utf-8-sig")
    return set(re.findall(r"(?m)^\| (" + prefix + r"-[A-Z]+-\d+|" + prefix + r"-\d+) \|", body))


def validate(path):
    data = json.loads(path.read_text(encoding="utf-8-sig"))
    errors = []
    required = ("mission", "target_period", "setting", "model_ids", "claim", "basis",
                "source_ids", "inference", "current", "target", "causal_effect",
                "owner_paths", "evidence_plan", "verdict", "evidence_paths")
    for key in required:
        if key not in data:
            errors.append(f"missing {key}")
    if errors:
        return errors
    if data["basis"] not in BASES:
        errors.append("basis must name its evidence class")
    if data["verdict"] not in VERDICTS:
        errors.append("unknown verdict")
    for key in ("mission", "target_period", "setting", "claim", "inference", "current", "target", "causal_effect"):
        value = data[key]
        if not isinstance(value, str) or not value.strip() or "<" in value:
            errors.append(f"{key} must be concrete")
    known_model = ids(DOCS / "MODELE.md", "PONT")
    known_sources = ids(DOCS / "SOURCES.md", "(?:GEO|ECO|HIS)")
    for key, known in (("model_ids", known_model), ("source_ids", known_sources)):
        values = data[key]
        if not isinstance(values, list) or not values or any(v not in known for v in values):
            errors.append(f"{key} must contain registered IDs")
    if not isinstance(data["owner_paths"], list) or not data["owner_paths"] or any("<" in str(v) for v in data["owner_paths"]):
        errors.append("owner_paths must name owned paths")
    plan = data["evidence_plan"]
    if not isinstance(plan, dict) or any(not isinstance(plan.get(k), str) or not plan[k].strip() or "<" in plan[k] for k in ("mec", "scn", "ply")):
        errors.append("evidence_plan needs mec, scn and ply")
    paths = data["evidence_paths"]
    if not isinstance(paths, list):
        errors.append("evidence_paths must be a list")
    elif data["verdict"] != "PROPOSE" and not paths:
        errors.append("observed verdict needs raw evidence_paths")
    if data["basis"] == "inconnu" and data["verdict"] != "PROPOSE":
        errors.append("inconnu cannot be presented as observed")
    return errors


def main():
    if len(sys.argv) != 2:
        print("usage: python tools/historicity/check-brief.py <brief.json>", file=sys.stderr)
        return 2
    path = Path(sys.argv[1])
    try:
        errors = validate(path)
    except (OSError, ValueError, TypeError) as exc:
        print(f"BRIEF::FAIL {exc}", file=sys.stderr)
        return 1
    if errors:
        for error in errors:
            print(f"BRIEF::FAIL {error}")
        return 1
    print(f"BRIEF::PASS traceability only: {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
