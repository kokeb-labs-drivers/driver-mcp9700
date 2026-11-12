"""The transfer function: output voltage against temperature for the five devices and a declared Custom part.

Reads: The horizontal axis is the die temperature, the vertical axis the voltage at VOUT, from Equation 4-1,
V = V0 + TC x T. The MCP9700, MCP9700A and MCP9700B share the lower line (500 mV at 0 C, 10.0 mV/C); the MCP9701
and MCP9701A the steeper one (400 mV at 0 C, 19.5 mV/C). The dashed line is a Custom example from the
Configuration page. Each line is solid over its specified temperature range and dotted over the plausibility margin
the driver adds on each side (range_margin_mc, default 8 C): a reading outside the solid part and its dotted margin is
OutputLow or OutputHigh, never a temperature. The dots mark the ends of the specified range and the voltage there.
"""

from _source import SNIPPETS, TYPES, need, num, read, variant_rows

from figkit import Fig

T_MIN, T_MAX = -50, 160
V_MAX = 3200


def data():
    rows = variant_rows()
    margin = num(need(r"range_margin_mc\{(\d+)\}", read(TYPES), "the default margin").group(1))
    snippet = need(r"start:custom-offset\](.*?)end:custom-offset", read(SNIPPETS), "the Custom example").group(1)
    custom = {k: num(need(rf"custom\.{k} = (-?[\d']+);", snippet, k).group(1))
              for k in ("v0_mv", "tc_uv_per_c", "min_mc", "max_mc")}
    lines = [
        {"name": "MCP9700, 9700A, 9700B", "tone": "teal", **rows[0]},
        {"name": "MCP9701, 9701A", "tone": "brand", **rows[3]},
        {"name": "Custom example", "tone": "violet", "v0_mv": custom["v0_mv"], "tc_uv": custom["tc_uv_per_c"],
         "min_mc": custom["min_mc"], "max_mc": custom["max_mc"], "high_mc": custom["max_mc"], "has_high": False,
         "dashed": True},
    ]
    return {"lines": lines, "margin_mc": margin, "sources": [f"{TYPES}:kVariantTable", f"{TYPES}:range_margin_mc",
                                                              f"{SNIPPETS}:custom-offset"]}


def _v(line, t_mc):
    return (line["v0_mv"] * 1000 + line["tc_uv"] * t_mc / 1000) / 1000.0


def build(d):
    f = Fig(
        "transfer-function",
        "Output voltage against temperature",
        alt=("Chart of VOUT in millivolts against temperature from -50 to +160 degrees Celsius: the MCP9700 family at "
             "500 mV plus 10.0 mV per degree, the MCP9701 family at 400 mV plus 19.5 mV per degree, and a Custom "
             "example, each solid over its specified range and dotted over the plausibility margin."),
        width=1200,
        kicker="Transfer function",
        subtitle="V = V0 + TC x T (Equation 4-1). Solid: specified range. Dotted: the margin the driver accepts.",
        profile="detail",
    )
    k = f.k
    left, top, pw, ph = 70.0, 10.0, 760.0, 420.0

    def X(t_c):
        return left + (t_c - T_MIN) / (T_MAX - T_MIN) * pw

    def Y(v_mv):
        return top + ph - v_mv / V_MAX * ph

    for t in range(-50, 161, 25):
        f.line([(X(t), top), (X(t), top + ph)], color="hair", sw=1.0)
        f.text(X(t), top + ph + 24, f"{t}", size=k["small"], color="muted", anchor="middle")
    for v in range(0, V_MAX + 1, 500):
        f.line([(left, Y(v)), (left + pw, Y(v))], color="hair", sw=1.0)
        f.text(left - 10, Y(v) + k["small"] * 0.36, f"{v}", size=k["small"], color="muted", anchor="end")
    f.line([(left, top + ph), (left + pw, top + ph)], color="rail", sw=1.5)
    f.line([(left, top), (left, top + ph)], color="rail", sw=1.5)
    f.text(left + pw / 2, top + ph + 52, "Temperature, degrees Celsius", size=k["small"], color="ink2",
           anchor="middle")
    f.text(0, top + 8, "VOUT, mV", size=k["small"], color="ink2")
    margin = d["margin_mc"] / 1000.0
    for line in d["lines"]:
        tone = line["tone"]
        t0, t1 = line["min_mc"] / 1000.0, line["max_mc"] / 1000.0
        dash = "10 6" if line.get("dashed") else None
        f.line([(X(t0), Y(_v(line, t0 * 1000))), (X(t1), Y(_v(line, t1 * 1000)))], color=f"{tone}.strong", sw=3.2,
               dash=dash, layer="edge")
        for a, b in ((t0 - margin, t0), (t1, t1 + margin)):
            f.line([(X(a), Y(_v(line, a * 1000))), (X(b), Y(_v(line, b * 1000)))], color=f"{tone}.strong", sw=2.0,
                   dash="2 5", layer="edge")
        for t in (t0, t1):
            f.circle(X(t), Y(_v(line, t * 1000)), 5, fill=f"{tone}.strong", layer="label")
        if line["has_high"]:
            th = line["high_mc"] / 1000.0
            f.line([(X(t1), Y(_v(line, t1 * 1000))), (X(th), Y(_v(line, th * 1000)))], color=f"{tone}.strong",
                   sw=3.2, dash="14 5", layer="edge")
            f.circle(X(th), Y(_v(line, th * 1000)), 5, fill=f"{tone}.strong", layer="label")
    nx = left + pw + 40
    ny = 0.0
    for line in d["lines"]:
        tone = line["tone"]
        lo, hi = _v(line, line["min_mc"]), _v(line, line["max_mc"])
        end = f"{line['min_mc'] // 1000} to {line['max_mc'] // 1000} C: {lo:.0f} to {hi:.0f} mV"
        if line["has_high"]:
            end += f"; High {line['high_mc'] // 1000} C: {_v(line, line['high_mc']):.0f} mV"
        text = f"{line['v0_mv']} mV at 0 C, {line['tc_uv'] / 1000:g} mV/C. {end}."
        box = f.note(nx, ny, f.body_w - nx, text, tone=tone, title=line["name"])
        ny = box.b + 16
    return f
