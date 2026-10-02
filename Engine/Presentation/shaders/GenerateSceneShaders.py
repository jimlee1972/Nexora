#!/usr/bin/env python3
"""Compatibility entry point for the unified scene/UI Vulkan shader generator."""
import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).with_name("GenerateShaders.py")), run_name="__main__")
