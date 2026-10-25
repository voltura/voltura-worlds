"""Offline EXR conversion. Run with Blender --background --python this.py -- input.exr output.png."""
import sys
from pathlib import Path
import bpy

args = sys.argv[sys.argv.index('--') + 1:]
if len(args) != 2:
    raise SystemExit('Expected source EXR and destination PNG')
source, destination = (Path(value).resolve() for value in args)
if destination.exists():
    raise SystemExit('Destination already exists')
image = bpy.data.images.load(str(source))
scene = bpy.context.scene
scene.view_settings.view_transform = 'Standard'
scene.view_settings.look = 'None'
scene.view_settings.exposure = 0
scene.view_settings.gamma = 1
scene.render.image_settings.file_format = 'PNG'
scene.render.image_settings.color_mode = 'RGB'
scene.render.image_settings.color_depth = '8'
image.save_render(str(destination), scene=scene)
print('Converted', image.size[:], destination)
