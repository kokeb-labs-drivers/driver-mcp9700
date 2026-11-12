"""The reading pipeline: what ReadMeasurement() does with the ADC, step by step, and where each error comes from.

Reads: Top to bottom, each card is one step of one reading. The driver first discards dummy conversions (an ADC
multiplexer's sample capacitor still holds the previous channel), then converts N times with a wait between
conversions and none after the last. The next steps look at those conversions together: a conversion within one
ADC step of the declared full scale means the ADC clipped, a wide spread means a floating or disturbed line, and only
then are the conversions averaged, scaled by the divider and turned into a temperature with the datasheet's transfer
function in one integer expression. The window check compares that temperature with the specified range plus the
margin; the calibration offset is added last. The red chips on the right are the one error each step can return;
the first one that applies is returned, so a conversion that failed is Bus whatever else is wrong.
"""

from _source import IMPL, body, need, read

from figkit import Fig


def data():
    impl = read(IMPL)
    collect = body(impl, "MCP9700<Port>::collect()")
    need(r"i >= discard", collect, "the dummy conversions are not summed")
    need(r"i > 0U\) && \(config_\.sample_interval_us > 0U", collect, "no wait before the first conversion")
    need(r"Error::Bus", collect, "a failed conversion is Bus")
    ev = body(impl, "Evaluate(const Config& config, const VariantSpec& spec,")
    need(r"adc_full_scale_mv\) \{\s*return Result<Measurement>::Failure\(Error::Saturated\)", ev, "Saturated check")
    need(r"max_spread_mv\)\) \{\s*return Result<Measurement>::Failure\(Error::Unstable\)", ev, "Unstable check")
    need(r"Error::OutputLow", ev, "OutputLow")
    need(r"Error::OutputHigh", ev, "OutputHigh")
    need(r"kPerMega", ev, "the transfer function in one expression")
    need(r"offset_cal_mc", body(impl, "MCP9700<Port>::ReadMeasurement()"), "the calibration after the checks")
    steps = [
        ("Discard", "discard_conversions dummy conversions, default 1", "Bus", "slate"),
        ("Convert", "samples conversions, default 8, sample_interval_us apart, no wait after the last", "Bus", "blue"),
        ("Clip check", "largest conversion + one ADC step >= adc_full_scale_mv", "Saturated", "amber"),
        ("Noise check", "largest - smallest (at VOUT) > max_spread_mv", "Unstable", "amber"),
        ("Scale", "mean x divider_num / divider_den = output_mv, then V0 and TC of the variant row", "", "teal"),
        ("Window check", "below min - range_margin_mc, or above max + range_margin_mc", "OutputLow / OutputHigh",
         "amber"),
        ("Calibrate", "add offset_cal_mc after the window check", "", "brand"),
    ]
    return {"steps": steps,
            "sources": [f"{IMPL}:collect()", f"{IMPL}:Evaluate()", f"{IMPL}:ReadMeasurement()"]}


def build(d):
    f = Fig(
        "read-pipeline",
        "One reading, step by step",
        alt=("Flow of one reading: discard dummy conversions, convert and average N samples, check for a clipped ADC "
             "(Saturated), check the spread (Unstable), scale to VOUT and convert to a temperature, check the window "
             "(OutputLow, OutputHigh) and add the calibration offset. A failed conversion is Bus."),
        width=1200,
        kicker="Architecture",
        subtitle="Never a retry and never a clamp: the first check that fails is the error.",
        profile="overview",
        legend=[("slate", "dummy"), ("blue", "ADC"), ("amber", "check"), ("teal", "maths"), ("brand", "result")],
    )
    k = f.k
    W = f.body_w
    side = 250
    gap = 40
    main = W - side - gap
    y = 0.0
    prev = None
    for i, (title, line, err, tone) in enumerate(d["steps"], start=1):
        card = f.card(0, y, main, f"{i}. {title}", [line], tone=tone, accent=True)
        if prev is not None:
            f.edge([(main / 2, prev.b + 4), (main / 2, card.y - 4)])
        if err:
            pill_w = f.pill_width(err)
            f.pill(main + gap + 8, card.y + (card.b - card.y) / 2 - (k["pill"] + 10) / 2, err, tone="rose")
            f.edge([(main + 4, card.y + (card.b - card.y) / 2), (main + gap, card.y + (card.b - card.y) / 2)],
                   tone="rose", dashed=True)
            del pill_w
        prev = card
        y = card.b + 34
    return f
