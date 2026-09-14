Import("env")

# The upstream sketch lives as a .txt; copy it into src/ as an .ino before each
# build so Arduino-style prototype generation still applies.
import os
import shutil

project_dir = env.subst("$PROJECT_DIR")
sketch = os.path.join(project_dir, "..", "Firmware", "(.txt) Control Board Code")
dest_dir = os.path.join(project_dir, "src")
dest = os.path.join(dest_dir, "ControlBoard.ino")

os.makedirs(dest_dir, exist_ok=True)
if not os.path.exists(dest) or os.path.getmtime(sketch) > os.path.getmtime(dest):
    shutil.copyfile(sketch, dest)
    print("Copied sketch -> src/ControlBoard.ino")
