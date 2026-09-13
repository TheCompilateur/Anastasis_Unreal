import unreal

def show_doc(owner, name):
    fn = getattr(owner, name, None)
    unreal.log("[doc] ==== %s ====" % name)
    unreal.log("[doc] %s" % (getattr(fn, "__doc__", "NO DOC")))

show_doc(unreal.GeometryScript_Primitives, "append_cone")
show_doc(unreal.GeometryScript_Primitives, "append_cylinder")
show_doc(unreal.GeometryScript_MeshQueries, "get_mesh_bounding_box")
show_doc(unreal.GeometryScript_MeshTransforms, "translate_mesh")
show_doc(unreal.GeometryScript_NewAssetUtils, "create_new_static_mesh_asset_from_mesh")
show_doc(unreal.GeometryScript_AssetUtils, "copy_mesh_to_static_mesh")
