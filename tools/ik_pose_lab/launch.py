"""Entry point for PyInstaller and direct source execution."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.ik_pose_lab.app import main

if __name__ == "__main__":
    main()
