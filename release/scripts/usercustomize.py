# SPDX-License-Identifier: GPL-2.0-or-later

#  Filename : usercustomize.py
#  Author   : Brecht van Lommel
#  Date     : 17/08/2022
#  Purpose  : make shared libraries  needed by modules available in standalone Python binary
import sys
import os
if sys.platform == 'win32':
    exe_dir, exe_file = os.path.split(sys.executable)
    if exe_file.startswith('python'):
        blender_dir = os.path.abspath(os.path.join(exe_dir, '..', '..', '..','blender.shared'))
        os.add_dll_directory(blender_dir)