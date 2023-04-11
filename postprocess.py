#!/bin/python

import jsbeautifier

SCRIPT="web/static/game/plurabus.js"

with open(SCRIPT, "r") as f:
    body = f.read()
    result = jsbeautifier.beautify(body)
with open(SCRIPT, "w") as f:
    f.write(result)
    f.flush()
