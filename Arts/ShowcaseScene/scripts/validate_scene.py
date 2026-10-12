"""Validate an already-open Blender scene and optionally render its saved camera."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import platform
import sys

import bpy

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--report', type=Path, required=True)
parser.add_argument('--render', type=Path)
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
scene = bpy.context.scene
errors = []
images = []
for image in bpy.data.images:
    if image.source != 'FILE':
        continue
    packed = bool(image.packed_file)
    relative = image.filepath.startswith('//')
    loaded = image.size[0] > 0 and image.size[1] > 0
    images.append({'name': image.name, 'path': image.filepath, 'packed': packed, 'relative': relative, 'loaded': loaded})
    if not packed or not relative or not loaded:
        errors.append('Image is not a loaded, packed, relative resource: ' + image.name)
if bpy.data.libraries:
    errors.append('Scene still depends on linked Blender libraries')
if scene.camera is None or scene.camera.type != 'CAMERA':
    errors.append('No render camera')
reference = scene.objects.get('Concept reference - never rendered')
if reference is None or reference.type != 'EMPTY' or not reference.hide_render:
    errors.append('Reference image must be a non-rendering empty')
elif any(node.type == 'TEX_IMAGE' and node.image == reference.data
         for material in bpy.data.materials if material.node_tree
         for node in material.node_tree.nodes):
    errors.append('Concept reference unexpectedly used as a material')
if scene.render.engine != 'CYCLES':
    errors.append('Snapshot should retain its Cycles renderer')
if errors:
    raise RuntimeError('\n'.join(errors))
report = {
    'schema_version': 1,
    'validated_at_utc': datetime.now(timezone.utc).isoformat(),
    'blender_version': bpy.app.version_string,
    'platform': platform.system(),
    'scene_file': Path(bpy.data.filepath).name,
    'scene_sha256': hashlib.sha256(Path(bpy.data.filepath).read_bytes()).hexdigest(),
    'objects': len(scene.objects),
    'mesh_objects': sum(obj.type == 'MESH' for obj in scene.objects),
    'base_mesh_vertices': sum(len(obj.data.vertices) for obj in scene.objects if obj.type == 'MESH'),
    'packed_images': images,
    'linked_libraries': len(bpy.data.libraries),
    'reference_is_non_rendering': True,
    'render_engine': scene.render.engine,
    'render_device': scene.cycles.device,
    'samples': scene.cycles.samples,
    'render_resolution': [scene.render.resolution_x * scene.render.resolution_percentage // 100,
                          scene.render.resolution_y * scene.render.resolution_percentage // 100],
    'render_completed': False,
    'visual_parity_accepted': False,
}
if args.render:
    args.render.parent.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str(args.render.resolve())
    bpy.ops.render.render(write_still=True)
    report['render_completed'] = True
    report['render_file'] = args.render.name
    report['render_sha256'] = hashlib.sha256(args.render.read_bytes()).hexdigest()
args.report.parent.mkdir(parents=True, exist_ok=True)
args.report.write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
print('SCENE_VALIDATION_PASSED ' + json.dumps({key: report[key] for key in ('objects', 'mesh_objects', 'base_mesh_vertices', 'render_completed', 'render_resolution')}), flush=True)
