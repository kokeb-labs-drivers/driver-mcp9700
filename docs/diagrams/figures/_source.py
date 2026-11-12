"""Shared helpers for the MCP970x figures: read constants and function bodies from the repository.

Every helper raises when the code no longer looks the way a figure expects, so a figure fails to build instead of
drawing something the code does not do.
"""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
TYPES = "inc/mcp9700_types.hpp"
UMBRELLA = "inc/mcp9700.hpp"
IMPL = "src/mcp9700.ipp"
SNIPPETS = "tests/docs/configuration.cpp"


def read(rel: str) -> str:
    return (ROOT / rel).read_text()


def need(pattern: str, text: str, what: str, flags: int = re.S) -> re.Match:
    m = re.search(pattern, text, flags)
    if not m:
        raise SystemExit(f"figure source drift: {what} not found (pattern {pattern!r})")
    return m


def body(text: str, signature: str) -> str:
    """The body of the function whose definition contains ``signature``, braces matched."""
    start = need(re.escape(signature) + r"[^{;]*\{", text, signature).end()
    depth, i = 1, start
    while depth:
        depth += {"{": 1, "}": -1}.get(text[i], 0)
        i += 1
    return text[start : i - 1]


def num(s: str) -> int:
    return int(s.replace("'", ""))


def variant_rows() -> list[dict]:
    """The first five fields of every row of ``kVariantTable``: V0, TC, minimum, maximum, High maximum."""
    table = need(r"kVariantTable\{(.*?)\}\}\}\}\};", read(TYPES), "kVariantTable").group(1)
    def field(name: str, value: str) -> list[str]:
        return re.findall(r"\." + name + r" = (" + value + ")", table)

    cols = {
        "v0_mv": field("v0_mv", r"\d+"),
        "tc_uv": field("tc_uv_per_c", r"[\d']+"),
        "min_mc": field("min_mc", r"-?[\d']+"),
        "max_mc": field("max_ext_mc", r"-?[\d']+"),
        "high_mc": field("max_high_mc", r"-?[\d']+"),
        "has_high": field("has_high", "true|false"),
    }
    if {len(v) for v in cols.values()} != {5}:
        raise SystemExit("figure source drift: kVariantTable fields are not five rows of every field")
    rows = [
        {"v0_mv": num(cols["v0_mv"][i]), "tc_uv": num(cols["tc_uv"][i]), "min_mc": num(cols["min_mc"][i]),
         "max_mc": num(cols["max_mc"][i]), "high_mc": num(cols["high_mc"][i]),
         "has_high": cols["has_high"][i] == "true"}
        for i in range(5)
    ]
    if len(rows) != 5:
        raise SystemExit(f"figure source drift: kVariantTable has {len(rows)} rows, expected 5")
    return rows
