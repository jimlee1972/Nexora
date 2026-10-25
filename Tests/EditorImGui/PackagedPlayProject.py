"""Pure saved-project fixture shared by native Editor and relocated package verification."""
import json


def write_fixture(root):
    (root / "Content").mkdir()
    (root / ".nexora").mkdir()
    def write(name, text):
        (root / name).write_bytes(text.encode("utf-8"))
    identity = "c334f934-3121-3a99-7430-dc79b6ab7ee9"
    write("project.nexora", "schema=2\nuuid=" + identity + "\nname=Configured Play\n")
    write(".nexora/workspace", "schema=1\n")
    write(".nexora/scene-session.ini", "schema=1\nproject=" + identity + "\nscene=Title.scene\n")
    write("Content/Triangle.obj", "v 0 0 0\nv 2 0 0\nv 0 2 0\nf 1 2 3\n")
    write("Content/Triangle.obj.meta", "schema=1\nuuid=77b091d1-c220-95a8-9120-d87d46362a98\ntype=.obj\n")
    write("Content/Paint.nmaterial", "NEXORA_MATERIAL 1\nbase_color 1 0 0\nmetallic 0\nroughness 0.5\nocclusion 1\nemission 1 0 0\n")
    write("Content/Paint.nmaterial.meta", "schema=1\nuuid=00000000-0000-0003-0000-000000000003\ntype=.nmaterial\n")
    reference = bytes([1]) + (3).to_bytes(8, "little") + (3).to_bytes(8, "little")
    for name, camera, triangle in (("Title", 0, 1), ("Level01", 10, 8)):
        write(name + ".scene", "NEXORA_EDITOR_SCENE 3\nnode 2 0 Camera\nnode 3 0 Shared triangle\n" +
              f'opaque 3 {0x45444d41544c0001} "editor.material.asset" {reference.hex()}\nworld\n' +
              f'NEXORA_SCENE 3 "{name}" 0 2\n' +
              f"2 0 {camera} 0 5 0 0 0 1 1 1 1 1 0 0 60 0.1 1000 1 0 0\n" +
              f"3 0 {triangle} 0 0 0 0 0 1 1 1 1 0 0 1 60 0.1 1000 1 7983368662727134518 0\n")
    write("Excluded.scene", 'NEXORA_EDITOR_SCENE 2\nworld\nNEXORA_SCENE 3 "Excluded" 0 0\n')
    write("project.build.json", json.dumps({"schema": 1,
          "start_scene": "00000000-0000-0001-0000-000000000002", "scenes": [
          {"id": "00000000-0000-0001-0000-000000000001", "path": "Title.scene", "enabled": True},
          {"id": "00000000-0000-0001-0000-000000000002", "path": "Level01.scene", "enabled": True},
          {"id": "00000000-0000-0001-0000-000000000003", "path": "Excluded.scene", "enabled": False}]}))
    layout = "schema=1\n"
    for title, panel in (("Project", "project"), ("Hierarchy", "hierarchy"),
                         ("Inspector", "inspector"), ("Console", "console"), ("Content", "content"),
                         ("Scene", "scene"), ("Game", "game"), ("Profiler", "profiler"), ("Build", "build")):
        layout += f"[Window][{title}###nexora.{panel}]\n"
        layout += ("Pos=0,40\nSize=620,640\nCollapsed=0\n" if panel == "scene" else
                   "Pos=640,40\nSize=620,640\nCollapsed=0\n" if panel == "game" else
                   "Pos=0,700\nSize=100,20\nCollapsed=1\n") + "\n"
    layout += "[Window][Project Build Settings]\nPos=10,20\nSize=700,680\nCollapsed=0\n\n"
    write(".nexora/editor-layout.ini", layout)
