import bpy
import sys

# Doc can be found here: https://docs.blender.org/api/current/bpy.ops.wm.html
# print("Blender export scene in Collada Format in file "+sys.argv[-1])
# bpy.ops.wm.collada_export(filepath=sys.argv[-1], include_armatures=True)

# Doc can be found here: https://docs.blender.org/api/current/bpy.ops.export_scene.html?highlight=fbx#bpy.ops.export_scene.fbx
print("Blender export scene in FBX Format in file "+sys.argv[-1])
for arg in sys.argv:
    print("arg: " + arg)
bpy.ops.export_scene.fbx(filepath=sys.argv[-1], axis_forward='-Z', axis_up='Y')#, apply_unit_scale=True, global_scale=.01, apply_scale_options='FBX_SCALE_CUSTOM')