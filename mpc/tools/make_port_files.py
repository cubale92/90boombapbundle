#!/usr/bin/env python3
"""Generate the MPC OS port files for the 8cee Golden Era Bundle.

    python3 mpc/tools/make_port_files.py

For each of Era-Q, Era-C, Era-S and Era-L this writes mpc/<P>/vst.json, params.json, presets.json and layout.conf.
Presets come from mpc/presets-src/<P>.json: the desktop plug-ins' factory presets as real values, dumped with
`TestEraX --dump-presets mpc/presets-src/EraX.json` (tests/TestMain.cpp), so both formats share one source.

Knobs whose desktop range is logarithmic (frequencies, times, Q) are "position" params on MPC: 0..100 on the knob,
mapped with a log taper in the engine (era_mpc.h posToLog) and shown in Hz/ms through "<key>_display".
The parameter order is the VST index: append only, never reorder (params.lock guards it).
"""
import json
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
MPC = os.path.dirname(HERE)

OFF_ON = ["Off", "On"]
THEME = """theme_bg=100f17
theme_panel=191823
theme_line=2e2c3d
theme_ink=e9e6f2
theme_ink_dim=8c88a3
theme_ink_faint=5a5670
theme_accent={accent}
theme_accent_hi={accent_hi}
theme_knob_face=22212f
theme_knob_ring=2a2838
theme_knob_dot=e9e6f2
theme_lcd=0b0a10
theme_btn_text=0b0a10
theme_tile_on=2a2838
theme_seg_active={accent}
theme_seg_inactive=22212f
theme_seg_active_tx=0b0a10
"""


def num(key, name, lo, hi, default, unit="", display=True, **extra):
    p = {"key": key, "name": name, "min": lo, "max": hi, "default": default}
    if unit:
        p["unit"] = unit
    if display:
        p["dynamic_display"] = True
    p.update(extra)
    return p


def opts(key, name, options, default=0, **extra):
    p = {"key": key, "name": name, "options": options, "default": default}
    p.update(extra)
    return p


def pos_of(value, lo, hi):
    """Inverse of era_mpc.h posToLog: real value -> 0..100 knob position."""
    value = min(max(value, lo), hi)
    return round(100.0 * math.log(value / lo) / math.log(hi / lo), 3)


# ---- per-plugin parameter tables: (params list, {key: converter real->mpc}) ---------------------------------------------
def eraq():
    params = [opts("band", "Band", [str(i) for i in range(1, 9)], 0, refresh_display=True)]
    conv = {}
    for b in range(1, 9):
        k = "b%d_" % b
        params += [
            opts(k + "on", "%d On" % b, OFF_ON, 0),
            opts(k + "type", "%d Shape" % b, ["Bell", "Lo Shelf", "Hi Shelf", "Lo Cut", "Hi Cut", "Notch"],
                 [3, 1, 0, 0, 0, 0, 2, 4][b - 1]),
            num(k + "freq", "%d Freq" % b, 0, 100, pos_of([30, 90, 250, 700, 2000, 5000, 10000, 18000][b - 1], 20, 20000)),
            num(k + "gain", "%d Gain" % b, -24, 24, 0, "dB"),
            num(k + "q", "%d Q" % b, 0, 100, pos_of(0.707, 0.1, 30)),
            opts(k + "slope", "%d Slope" % b, ["12 dB", "24 dB", "48 dB"], 1),
            opts(k + "place", "%d Place" % b, ["Stereo", "Mid", "Side"], 0),
        ]
        conv[k + "freq"] = lambda v: pos_of(v, 20, 20000)
        conv[k + "q"] = lambda v: pos_of(v, 0.1, 30)
        conv[k + "gain"] = lambda v: min(max(v, -24), 24)
    params += [num("color", "Color", 0, 100, 0, "%"), num("output", "Output", -24, 24, 0, "dB")]
    return params, conv


def erac():
    params = [
        opts("style", "Style", ["Clean", "Punch", "Opto", "Slam"], 0),
        num("threshold", "Threshold", -60, 0, -18, "dB"),
        num("ratio", "Ratio", 1, 20, 4),
        num("attack", "Attack", 0, 100, pos_of(10, 0.05, 250)),
        num("release", "Release", 0, 100, pos_of(150, 5, 2500)),
        opts("autorel", "Auto Rel", OFF_ON, 0),
        num("knee", "Knee", 0, 24, 6, "dB"),
        num("makeup", "Makeup", -12, 24, 0, "dB"),
        opts("automakeup", "Auto Gain", OFF_ON, 0),
        num("mix", "Mix", 0, 100, 100, "%"),
        num("schpf", "SC Hi-Pass", 0, 100, 0),
    ]
    conv = {"attack": lambda v: pos_of(v, 0.05, 250), "release": lambda v: pos_of(v, 5, 2500),
            "schpf": lambda v: pos_of(v, 20, 500)}
    return params, conv


def eras():
    params = [
        opts("mode", "Mode", ["Tape", "Tube", "Console", "SP 26k", "Sixty 40k", "Vari-Rate"], 0),
        num("drive", "Drive", 0, 36, 6, "dB"),
        num("bias", "Bias", -100, 100, 0, "%"),
        num("tone", "Tone", -12, 12, 0, "dB"),
        num("lowcut", "Low Cut", 0, 100, 0),
        num("highcut", "High Cut", 0, 100, 100),
        {"key": "bits", "name": "Bits", "min": 4, "max": 16, "default": 12, "unit": "bit", "display": "int"},
        num("rate", "Rate", 0, 100, pos_of(26040, 2000, 48000)),
        num("dust", "Dust", 0, 100, 0, "%"),
        num("mix", "Mix", 0, 100, 100, "%"),
        num("output", "Output", -24, 24, 0, "dB"),
        opts("autogain", "Auto Gain", OFF_ON, 1),
    ]
    conv = {"lowcut": lambda v: pos_of(v, 10, 500), "highcut": lambda v: pos_of(v, 1000, 22000),
            "rate": lambda v: pos_of(v, 2000, 48000)}
    return params, conv


def eral():
    params = [
        num("gain", "Gain", 0, 24, 0, "dB"),
        num("ceiling", "Ceiling", -12, 0, -1, "dB"),
        num("release", "Release", 0, 100, pos_of(100, 1, 1000)),
        opts("style", "Style", ["Clean", "Punchy", "Warm", "Loud '95"], 0),
        num("link", "Link", 0, 100, 100, "%"),
        opts("unity", "Unity", OFF_ON, 0),
    ]
    conv = {"release": lambda v: pos_of(v, 1, 1000)}
    return params, conv


# ---- skins ----------------------------------------------------------------------------------------------------------
def knob(cx, cy, key, label, r=52):
    return 'knob cx=%d cy=%d r=%d label="%s" key=%s' % (cx, cy, r, label, key)


def layout_eraq(params):
    L = [THEME.format(accent="2fd3c5", accent_hi="a6f0e9"), "[tab EQ]",
         'frame x=16 y=92 w=1248 h=92 title="BAND"',
         'enum_h cx=640 cy=144 w=1200 h=50 key=band options="1,2,3,4,5,6,7,8"']
    for b in range(1, 9):
        k = "b%d_" % b
        w = " when=band:%d" % b
        L += ['frame x=16 y=196 w=930 h=508 title="BAND %d"%s' % (b, w),
              'toggle cx=110 cy=280 label="ON" key=%son%s' % (k, w),
              'enum_v cx=110 cy=520 w=150 h=250 label="SHAPE" key=%stype options="Bell,Lo Shelf,Hi Shelf,Lo Cut,Hi Cut,Notch"%s' % (k, w),
              knob(330, 380, k + "freq", "FREQ", 58) + w,
              knob(540, 380, k + "gain", "GAIN", 58) + w,
              knob(750, 380, k + "q", "Q", 58) + w,
              'enum_h cx=375 cy=620 w=354 h=50 label="SLOPE" key=%sslope options="12 dB,24 dB,48 dB"%s' % (k, w),
              'enum_h cx=740 cy=620 w=354 h=50 label="PLACEMENT" key=%splace options="Stereo,Mid,Side"%s' % (k, w)]
    L += ['frame x=958 y=196 w=306 h=508 title="OUTPUT"',
          knob(1111, 330, "color", "COLOR", 56),
          knob(1111, 560, "output", "OUTPUT", 56),
          'qlinks "EQ" = ' + ",".join(["b%d_freq" % b for b in range(1, 9)] + ["b%d_gain" % b for b in range(1, 9)]),
          "", "[tab ALL BANDS]"]
    for b in range(1, 9):
        x = 16 + (b - 1) * 156
        cx = x + 75
        k = "b%d_" % b
        L += ['frame x=%d y=92 w=150 h=612 title="BAND %d"' % (x, b),
              'toggle cx=%d cy=150 label="ON" key=%son' % (cx, k),
              knob(cx, 262, k + "freq", "FREQ", 42),
              knob(cx, 430, k + "gain", "GAIN", 42),
              knob(cx, 598, k + "q", "Q", 42)]
    L += ['qlinks "ALL BANDS" = ' + ",".join(["b%d_freq" % b for b in range(1, 9)] + ["b%d_gain" % b for b in range(1, 9)]),
          "qlinks_track = " + ",".join(["b%d_freq" % b for b in range(1, 9)] + ["b%d_gain" % b for b in range(1, 9)])]
    return L


def layout_erac(params):
    L = [THEME.format(accent="f2b84b", accent_hi="f9dca3"), "[tab COMP]",
         'frame x=16 y=92 w=1248 h=96 title="STYLE"',
         'enum_h cx=640 cy=146 w=1200 h=52 key=style options="Clean,Punch,Opto,Slam"',
         'frame x=16 y=200 w=1248 h=504 title="COMPRESSOR"']
    row1 = [("threshold", "THRESHOLD"), ("ratio", "RATIO"), ("attack", "ATTACK"), ("release", "RELEASE"), ("knee", "KNEE")]
    row2 = [("makeup", "MAKEUP"), ("mix", "MIX"), ("schpf", "SC HI-PASS")]
    for i, (k, lab) in enumerate(row1):
        L.append(knob(140 + i * 250, 340, k, lab, 58))
    for i, (k, lab) in enumerate(row2):
        L.append(knob(140 + i * 250, 580, k, lab, 58))
    L += ['toggle cx=890 cy=580 label="AUTO RELEASE" key=autorel',
          'toggle cx=1140 cy=580 label="AUTO MAKEUP" key=automakeup',
          'qlinks "COMP" = threshold,ratio,attack,release,knee,makeup,mix,schpf,style,autorel,automakeup',
          "qlinks_track = threshold,ratio,attack,release,knee,makeup,mix,schpf,style,autorel,automakeup"]
    return L


def layout_eras(params):
    L = [THEME.format(accent="e8457e", accent_hi="f5a3c0"), "[tab SATURATE]",
         'frame x=16 y=92 w=1248 h=96 title="MODE"',
         'enum_h cx=640 cy=146 w=1200 h=52 key=mode options="Tape,Tube,Console,SP 26k,Sixty 40k,Vari-Rate"',
         'frame x=16 y=200 w=1248 h=504 title="SATURATION"']
    row1 = [("drive", "DRIVE"), ("bias", "BIAS"), ("tone", "TONE"), ("lowcut", "LOW CUT"), ("highcut", "HIGH CUT")]
    row2 = [("bits", "BITS"), ("rate", "RATE"), ("dust", "DUST"), ("mix", "MIX"), ("output", "OUTPUT")]
    for i, (k, lab) in enumerate(row1):
        L.append(knob(120 + i * 215, 340, k, lab, 56))
    for i, (k, lab) in enumerate(row2):
        L.append(knob(120 + i * 215, 580, k, lab, 56))
    L += ['toggle cx=1170 cy=460 label="AUTO GAIN" key=autogain',
          'qlinks "SATURATE" = drive,bias,tone,lowcut,highcut,bits,rate,dust,mix,output,mode,autogain',
          "qlinks_track = drive,bias,tone,lowcut,highcut,bits,rate,dust,mix,output,mode,autogain"]
    return L


def layout_eral(params):
    L = [THEME.format(accent="9b6cff", accent_hi="cdb6ff"), "[tab LIMIT]",
         'frame x=16 y=92 w=1248 h=96 title="STYLE"',
         'enum_h cx=640 cy=146 w=1200 h=52 key=style options="Clean,Punchy,Warm,Loud \'95"',
         'frame x=16 y=200 w=1248 h=504 title="LIMITER"']
    for i, (k, lab) in enumerate([("gain", "GAIN"), ("ceiling", "CEILING"), ("release", "RELEASE"), ("link", "STEREO LINK")]):
        L.append(knob(170 + i * 270, 450, k, lab, 58))
    L += ['toggle cx=1150 cy=450 label="UNITY MONITOR" key=unity',
          'qlinks "LIMIT" = gain,ceiling,release,link,style,unity',
          "qlinks_track = gain,ceiling,release,link,style,unity"]
    return L


PLUGINS = {
    "EraQ": ("Era-Q", "EraQ", eraq, layout_eraq, "8-band EQ with Stereo/Mid/Side bands and an analog Color stage."),
    "EraC": ("Era-C", "EraC", erac, layout_erac, "Compressor with Clean, Punch, Opto and Slam styles and parallel mix."),
    "EraS": ("Era-S", "EraS", eras, layout_eras, "Tape/Tube/Console saturation and SP 26k / Sixty 40k / Vari-Rate sampler grit."),
    "EraL": ("Era-L", "EraL", eral, layout_eral, "Look-ahead limiter with Clean, Punchy, Warm and Loud '95 styles."),
}


def short_name(name):
    if len(name) <= 24:
        return name
    for a, b in (("New Jack ", "NJ "), ("Background", "BG"), (" Machine", "")):
        name = name.replace(a, b)
    return name[:24]


def convert_presets(params, conv, src):
    by_key = {p["key"]: p for p in params}
    out, seen = [], set()
    for pr in src:
        name = "Init" if pr["category"] == "Init" else short_name(pr["name"])
        if name in seen:
            continue
        seen.add(name)
        vals = {}
        for p in params:
            k = p["key"]
            if k in pr["values"]:
                v = float(pr["values"][k])
                v = conv[k](v) if k in conv else v
                if "options" in p:
                    vals[k] = p["options"][int(round(min(max(v, 0), len(p["options"]) - 1)))]
                else:
                    vals[k] = round(min(max(v, p["min"]), p["max"]), 3)
            else:
                d = p["default"]
                vals[k] = p["options"][d] if "options" in p else d
        out.append({"name": name, "values": vals})
    # Era-Q: open each preset on its first active band
    if "band" in by_key:
        for pr in out:
            on = [b for b in range(1, 9) if pr["values"]["b%d_on" % b] == "On"]
            pr["values"]["band"] = str(on[0] if on else 1)
    return out


def main():
    for folder, (name, uid, mk_params, mk_layout, about) in PLUGINS.items():
        d = os.path.join(MPC, folder)
        os.makedirs(d, exist_ok=True)
        params, conv = mk_params()
        src = json.load(open(os.path.join(MPC, "presets-src", folder + ".json")))
        presets = convert_presets(params, conv, src)
        json.dump({"name": name, "params": params}, open(os.path.join(d, "params.json"), "w"), indent=1)
        json.dump({"presets": presets}, open(os.path.join(d, "presets.json"), "w"), indent=1)
        with open(os.path.join(d, "layout.conf"), "w") as f:
            f.write("# generated by mpc/tools/make_port_files.py: edit that, not this\n")
            f.write("\n".join(mk_layout(params)) + "\n")
        vst = {
            "name": name, "vendor": "8cee", "uid": uid, "version": 1000,
            "so": folder.lower() + ".so",
            "params": "params.json", "layout": "layout.conf", "presets": "presets.json",
            "effect": True,
            "build": {"root": "../..", "sources": ["mpc/%s/%s_engine.cpp" % (folder, folder.lower())],
                      "cflags": ["-Impc/common", "-ffast-math"], "libs": ["-lm", "-static-libstdc++", "-static-libgcc"],
                      "cflags_arm": ["-mfpu=neon"]},
            "targets": ["armv7", "aarch64"],
        }
        json.dump(vst, open(os.path.join(d, "vst.json"), "w"), indent=1)
        print("%s: %d params, %d presets" % (name, len(params), len(presets)))


if __name__ == "__main__":
    main()
