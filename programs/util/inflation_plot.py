#!/usr/bin/env python3

import datetime
import json
import sys

import matplotlib
matplotlib.use("Agg")

import matplotlib.pyplot as plt
from matplotlib.ticker import AutoMinorLocator

x = []
y = []

names = ["producer"]
inflections = {}
markers = []

colors = iter("mgbr")
shapes = iter("ovx+")

ax = plt.gca()

plt.axis([0,20,10e6,140e6])
ax.set_xticks(range(21))
ax.xaxis.set_minor_locator(AutoMinorLocator(12))
ax.set_ylim([10e6,140e6])
ax.tick_params(axis="y", which="minor", left=False, right=False)
ax.set_yticks([10e6, 20e6, 30e6, 40e6, 50e6, 60e6, 70e6, 80e6, 90e6, 100e6, 110e6, 120e6, 130e6, 140e6])
ax.set_yticklabels(["10M", "20M", "30M", "40M", "50M", "60MB", "70M", "80M", "90M", "100M", "110M", "120M", "130M", "140M"])
plt.grid(True, which="major", linestyle="-")
ax.xaxis.grid(True, which="minor", linestyle="-", color="g")

BLOCKS_PER_YEAR = 20*60*24*365

with open(sys.argv[1], "r") as f:
    n = 0
    for line in f:
        n += 1
        d = json.loads(line)
        b = int(d["b"])
        s = int(d["s"])

        if (b%10000) != 0:
            continue

        px = b / BLOCKS_PER_YEAR
        py = s / 1000

        x.append(px)
        y.append(py)
        for i in range(len(names)):
            if i == 1:
                continue
            if names[i] in inflections:
                continue
            if (int(d["rvec"][i*2])%1000) == 0:
                continue
            inflections[names[i]] = d["b"]
            markers.append([[[px],[py],next(colors)+next(shapes)], {"label" : names[i]}])

plt.plot(x, y)
for m in markers:
    print(m)
    plt.plot(*m[0], **m[1])
plt.legend(loc=4)
plt.title("20-year MORPH supply projection")
plt.savefig("myfig.png")
