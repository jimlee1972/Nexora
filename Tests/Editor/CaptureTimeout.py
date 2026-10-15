"""Bounded local-host deadline for C++ capture generators; CI defaults stay unchanged."""

import math
import os


def capture_timeout():
    seconds = float(os.environ.get("NEXORA_CAPTURE_TIMEOUT_SECONDS", "30"))
    if not math.isfinite(seconds) or seconds <= 0:
        raise ValueError("NEXORA_CAPTURE_TIMEOUT_SECONDS must be positive and finite")
    return seconds
