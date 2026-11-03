#!/usr/bin/env python3
"""Generates the stress-test config.yaml of task 117 (no secrets).

  tools/testdata/stress/gen.py [--tabs 16] [--per-tab 24] [--nx 25] > config.yaml

Real ids of the dev installation are reused (they answer), the rest are made-up ids that Home
Assistant does not know (they show n/a). --nx is the percentage of widgets that use made-up ids.
Upload: tools/provision.py config.yaml (this overwrites the SD card config; back it up first).
"""
import argparse

TEMP = "sensor.altherma_climatecontrol_outdoor_temperature"
SWITCHES = ["switch.livingroom_main_switch_0", "switch.shellywalldisplay_0008226c4b4b_switch_0",
            "switch.shelly_kitchen_table_switch_0"]
LIGHT = "light.shellydimmer2_3c6105fe7a6c"
COVERS = ["cover.livingroom_shutter", "cover.terrace_shutter", "cover.kitchen_shutter"]
CLIMATES = ["climate.nappali_6", "climate.dolgozo_8", "climate.furdo_fsz_7", "climate.folyoso_em_4",
            "climate.szoba_eny_1", "climate.szoba_dk_2", "climate.szoba_dny_3", "climate.furdo_em_5"]
AUTOS = ["automation.automated_shutter_control_in_livingroom_ai_version",
         "automation.automated_shutter_control_in_office_ai_version"]
LONG = "A very long widget name that goes on and on and on %d"

KINDS = ["switch", "cover", "light", "climate", "sensor", "sensor2x1", "graph_line", "graph_bar", "graph_step",
         "graph_bin", "automation", "weather", "forecast", "media", "calendar", "notifications", "energy",
         "group", "binary", "vacuum", "graph_long", "sensor_attr", "cover_nx", "switch_nx"]


def widget(kind, t, i, nx):
    n = t * 100 + i
    nxid = lambda d: "%s.stress_nx_%d" % (d, n)
    real = (n * 37) % 100 >= nx  # about nx% of the widgets use made-up ids
    name = (LONG % n) if i % 6 == 5 else "W%d.%d" % (t, i)
    if kind == "switch":
        e = SWITCHES[n % 3] if real else nxid("switch")
        return ["- entity: %s" % e, "  name: %s" % name]
    if kind == "switch_nx":
        return ["- entity: %s" % nxid("switch"), "  name: %s" % name]
    if kind == "cover":
        return ["- entity: %s" % (COVERS[n % 3] if real else nxid("cover")), "  name: %s" % name]
    if kind == "cover_nx":
        return ["- entity: %s" % nxid("cover")]
    if kind == "light":
        return ["- entity: %s" % (LIGHT if real else nxid("light")), "  name: %s" % name]
    if kind == "climate":
        return ["- entity: %s" % (CLIMATES[n % 8] if real else nxid("climate")), "  name: %s" % name]
    if kind == "automation":
        return ["- entity: %s" % (AUTOS[n % 2] if real else nxid("automation")), "  name: %s" % name]
    if kind == "sensor":
        return ["- entity: %s" % (TEMP if real else nxid("sensor")), "  name: %s" % name, "  size: 1x1"]
    if kind == "sensor2x1":
        return ["- entity: %s" % (TEMP if real else nxid("sensor")), "  name: %s" % name, "  size: 2x1",
                "  decimals: 1", "  unit: C", "  details: [humidity, dew_point]"]
    if kind == "sensor_attr":
        return ["- entity: sun.sun", "  type: sensor", "  attribute: elevation", "  name: %s" % name,
                "  decimals: 2", "  history: false"]
    if kind == "binary":
        return ["- entity: %s" % nxid("binary_sensor"), "  name: %s" % name]
    if kind == "vacuum":
        return ["- entity: %s" % nxid("vacuum"), "  name: %s" % name]
    if kind == "weather":
        return ["- entity: weather.home"]
    if kind == "forecast":
        return ["- entity: weather.home", "  forecast: true"]
    if kind == "media":
        return ["- entity: %s" % nxid("media_player"), "  name: %s" % name]
    if kind == "calendar":
        return ["- entity: %s" % nxid("calendar")]
    if kind == "notifications":
        return ["- type: notifications", "  name: Alerts %d" % t]
    if kind == "energy":
        return ["- type: energy", "  usage: %s" % TEMP, "  solar: %s" % nxid("sensor")]
    if kind == "group":
        return ["- group: shutters", "  name: Shutters %d" % t]
    e = TEMP if real else nxid("sensor")
    if kind == "graph_line":
        return ["- entity: %s" % e, "  name: %s" % name, "  display: graph", "  history: { hours: %d }" % (6, 24, 168)[n % 3]]
    if kind == "graph_long":
        return ["- entity: %s" % TEMP, "  display: graph", "  history: { hours: 720 }"]
    if kind == "graph_bar":
        return ["- entity: %s" % TEMP, "  display: graph",
                "  history: { hours: 168, type: bar, statistic: max, period: day, source: statistics }"]
    if kind == "graph_step":
        return ["- entity: %s" % e, "  display: graph", "  history: { hours: 24, type: step, min: -20, max: 40 }"]
    if kind == "graph_bin":
        return ["- entity: %s" % nxid("binary_sensor"), "  display: graph", "  history: { hours: 48 }"]
    raise SystemExit("kind " + kind)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tabs", type=int, default=16)
    ap.add_argument("--per-tab", type=int, default=24)
    ap.add_argument("--nx", type=int, default=25)
    a = ap.parse_args()
    out = ["version: 1", "groups:", "  shutters: [%s]" % ", ".join(COVERS), "tabs:"]
    for t in range(a.tabs):
        out.append("  - title: Tab %d %s" % (t, "with a long title" if t % 4 == 3 else ""))
        out.append("    landscape_columns: %d" % (4 if t % 2 else 5))
        out.append("    portrait_columns: %d" % (2, 3, 4)[t % 3])
        out.append("    widgets:")
        for i in range(a.per_tab):
            kind = KINDS[(t * 7 + i * 5 + i // 3) % len(KINDS)]
            for line in widget(kind, t, i, a.nx):
                out.append("      " + line)
    print("\n".join(out))


main()
