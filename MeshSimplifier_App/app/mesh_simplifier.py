import sys
import os
import re
import math
import shutil
from pathlib import Path

import pyvista as pv
import numpy as np
import importlib.util as _ilu
if getattr(sys, 'frozen', False):
    _base = Path(sys._MEIPASS)
    _candidates = [
        _base / "qem_simplifier",
        _base,
    ]
else:
    _candidates = [Path(__file__).parent / "qem_simplifier"]

_qem_pyd = None
for _d in _candidates:
    hits = list(_d.glob("qem_simplifier*.pyd"))
    if hits:
        _qem_pyd = hits[0]
        break

if _qem_pyd is None:
    raise FileNotFoundError(
        f"Cannot find qem_simplifier*.pyd, searched: {[str(d) for d in _candidates]}"
    )

_spec = _ilu.spec_from_file_location("qem_simplifier", _qem_pyd)
qem_simplifier = _ilu.module_from_spec(_spec)
_spec.loader.exec_module(qem_simplifier)
from PyQt5 import QtWidgets, QtCore, QtGui
from pyvistaqt import QtInteractor


# ================= 配置区 =================
OBJ_PATH = None
CACHE_DIR = Path("_lod_cache")
CACHE_DIR.mkdir(exist_ok=True)

SMALL_MESH_FACES = 8_000
MEDIUM_MESH_FACES = 80_000

DEFAULT_AUTO_PREVIEW = True
# ==========================================


APP_STYLESHEET = """
* {
    font-family: 'Segoe UI', 'Microsoft YaHei UI', 'Microsoft YaHei', sans-serif;
    font-size: 14px;
}

QMainWindow {
    background: #F4F7FB;
}

QWidget {
    background: transparent;
    color: #172033;
}

QFrame#RootPanel {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 #F8FBFF, stop:0.54 #EDF4FA, stop:1 #F9F3EC);
}

QFrame#ViewportCard {
    background: #FFFFFF;
    border: 1px solid #D7E1EC;
    border-radius: 18px;
}

QFrame#SidePanel {
    background: #F9FBFD;
    border: 1px solid #D7E1EC;
    border-radius: 18px;
}

QFrame#HeroCard {
    background-color: #FFFFFF;
    border: 1px solid #D7E1EC;
    border-radius: 16px;
}

QFrame#GlassCard {
    background-color: #FFFFFF;
    border: 1px solid #D7E1EC;
    border-radius: 14px;
}

QFrame#MiniCard {
    background-color: #F6F9FC;
    border: 1px solid #DDE6F0;
    border-radius: 12px;
}

QFrame#ToolbarCard {
    background-color: #F8FBFE;
    border: 1px solid #DCE6F0;
    border-radius: 12px;
}

QLabel#FilePath {
    color: #33475F;
    font-size: 12px;
    font-weight: 650;
}

QLabel#Chip {
    padding: 6px 9px;
    background-color: #EEF6FF;
    border: 1px solid #CFE3F7;
    border-radius: 9px;
    color: #1F5E9E;
    font-size: 11px;
    font-weight: 800;
}

QLabel#AppTitle {
    color: #102033;
    font-size: 25px;
    font-weight: 800;
    letter-spacing: 0px;
}

QLabel#AppSubtitle {
    color: #62748A;
    font-size: 12px;
    font-weight: 500;
}

QLabel#SectionTitle {
    color: #172033;
    font-size: 15px;
    font-weight: 800;
}

QLabel#SectionHint {
    color: #68798E;
    font-size: 12px;
    line-height: 18px;
}

QLabel#MetricLabel {
    color: #738397;
    font-size: 11px;
    font-weight: 700;
    letter-spacing: 0px;
}

QLabel#MetricValue {
    color: #152235;
    font-size: 20px;
    font-weight: 850;
}

QLabel#StatusPill {
    padding: 10px 12px;
    background-color: #EEF6FF;
    border: 1px solid #CFE3F7;
    border-radius: 10px;
    color: #1F5E9E;
    font-size: 13px;
    font-weight: 700;
}

QLabel#RatioBig {
    color: #102033;
    font-size: 34px;
    font-weight: 900;
}

QLabel#RatioCaption {
    color: #65778C;
    font-size: 12px;
    font-weight: 600;
}

QPushButton {
    background-color: #2563EB;
    border: 1px solid #1D57D8;
    border-radius: 10px;
    padding: 11px 14px;
    color: white;
    font-weight: 800;
}

QPushButton:hover {
    background-color: #1D4ED8;
    border: 1px solid #1748C8;
}

QPushButton:pressed {
    background-color: #153EAA;
}

QPushButton:disabled {
    background-color: #E7EDF4;
    border: 1px solid #D4DEEA;
    color: #94A3B8;
}

QPushButton#Ghost {
    background-color: #F7FAFD;
    border: 1px solid #D3DEEA;
    color: #26384E;
}

QPushButton#Ghost:hover {
    background-color: #EEF5FC;
    border: 1px solid #BBD0E5;
}

QPushButton#Ghost:checked {
    background-color: #E6F1FF;
    border: 1px solid #6AA7E8;
    color: #164E8B;
}

QPushButton#Preset {
    background-color: #FFFFFF;
    border: 1px solid #CBD8E6;
    border-radius: 9px;
    padding: 8px 9px;
    color: #25364B;
    font-size: 12px;
    font-weight: 800;
}

QPushButton#Preset:hover {
    background-color: #EEF6FF;
    border: 1px solid #8DBBEA;
}

QPushButton#Accent {
    background-color: #0F766E;
    border: 1px solid #0B625C;
    color: #FFFFFF;
}

QPushButton#Danger {
    background-color: #FFF1F2;
    border: 1px solid #FDA4AF;
    color: #A21D2D;
}

QPushButton#Danger:hover {
    background-color: #FFE4E6;
    border: 1px solid #FB7185;
}

QPushButton#Warm {
    background-color: #FFF7ED;
    border: 1px solid #FDBA74;
    color: #9A3412;
}

QPushButton#Warm:hover {
    background-color: #FFEDD5;
    border: 1px solid #FB923C;
}

QSlider::groove:horizontal {
    height: 10px;
    background: #D8E3EF;
    border-radius: 5px;
}

QSlider::sub-page:horizontal {
    background: qlineargradient(
        x1:0, y1:0, x2:1, y2:0,
        stop:0 #14B8A6,
        stop:0.55 #2563EB,
        stop:1 #7C3AED
    );
    border-radius: 5px;
}

QSlider::handle:horizontal {
    width: 24px;
    height: 24px;
    margin: -8px 0;
    background: #FFFFFF;
    border: 4px solid #2563EB;
    border-radius: 12px;
}

QSlider::handle:horizontal:hover {
    border: 4px solid #14B8A6;
}

QCheckBox {
    spacing: 10px;
    color: #2C3E53;
    font-weight: 600;
}

QCheckBox::indicator {
    width: 19px;
    height: 19px;
    border-radius: 6px;
    border: 1px solid #B7C6D8;
    background: #FFFFFF;
}

QCheckBox::indicator:checked {
    background: #2563EB;
    border: 1px solid #1D4ED8;
}

QComboBox {
    background-color: #FFFFFF;
    border: 1px solid #CBD8E6;
    border-radius: 9px;
    padding: 8px 10px;
    color: #172033;
    font-weight: 650;
}

QSpinBox {
    background-color: #FFFFFF;
    border: 1px solid #CBD8E6;
    border-radius: 9px;
    padding: 8px 10px;
    color: #172033;
    font-weight: 800;
}

QProgressBar {
    border: none;
    border-radius: 7px;
    background: #D8E3EF;
    height: 10px;
    text-align: center;
    color: transparent;
}

QProgressBar::chunk {
    border-radius: 7px;
    background: qlineargradient(
        x1:0, y1:0, x2:1, y2:0,
        stop:0 #14B8A6,
        stop:1 #2563EB
    );
}

QScrollArea {
    border: none;
    background: transparent;
}
"""


def safe_face_count(mesh: pv.DataSet) -> int:
    return int(mesh.n_cells)


def mesh_center_and_radius(mesh: pv.DataSet):
    bounds = mesh.bounds
    cx = (bounds[0] + bounds[1]) * 0.5
    cy = (bounds[2] + bounds[3]) * 0.5
    cz = (bounds[4] + bounds[5]) * 0.5

    dx = bounds[1] - bounds[0]
    dy = bounds[3] - bounds[2]
    dz = bounds[5] - bounds[4]

    radius = max(math.sqrt(dx * dx + dy * dy + dz * dz) * 0.5, 1e-6)
    return (cx, cy, cz), radius


def parse_obj_mtl_path(obj_path: Path) -> Path | None:
    if not obj_path.exists():
        return None

    try:
        with obj_path.open("r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                line = line.strip()
                if line.lower().startswith("mtllib "):
                    mtl_name = line.split(maxsplit=1)[1].strip().strip('"')
                    return obj_path.parent / mtl_name
    except Exception:
        return None

    return None


def parse_mtl_texture_path(mtl_path: Path | None) -> Path | None:
    if not mtl_path or not mtl_path.exists():
        return None

    try:
        with mtl_path.open("r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                line = line.strip()

                if not line or line.startswith("#"):
                    continue

                if line.lower().startswith("map_kd"):
                    parts = line.split()

                    if len(parts) >= 2:
                        tex_name = parts[-1].strip().strip('"')
                        tex_path = (mtl_path.parent / tex_name).resolve()
                        if tex_path.exists():
                            return tex_path

                    raw = re.sub(r"^map_kd\s+", "", line, flags=re.I).strip().strip('"')
                    tex_path = (mtl_path.parent / raw).resolve()
                    if tex_path.exists():
                        return tex_path

    except Exception:
        return None

    return None


def parse_mtl_texture_paths(mtl_path: Path | None) -> dict[str, Path]:
    textures = {}
    if not mtl_path or not mtl_path.exists():
        return textures

    current_material = None
    try:
        with mtl_path.open("r", encoding="utf-8", errors="ignore") as f:
            for raw_line in f:
                line = raw_line.strip()
                if not line or line.startswith("#"):
                    continue

                lower = line.lower()
                if lower.startswith("newmtl "):
                    current_material = line.split(maxsplit=1)[1].strip().strip('"')
                    continue

                if current_material and lower.startswith("map_kd"):
                    parts = line.split()
                    tex_name = parts[-1].strip().strip('"') if len(parts) >= 2 else ""
                    tex_path = (mtl_path.parent / tex_name).resolve()
                    if tex_path.exists():
                        textures[current_material] = tex_path
    except Exception:
        return textures

    return textures


def is_multi_texture_payload(texture) -> bool:
    return isinstance(texture, dict) and texture.get("mode") == "multi"


def build_multi_texture_payload(mesh, texture_paths: dict[str, Path]):
    if not texture_paths or "MaterialIds" not in mesh.cell_data:
        return None

    textures = {}
    for material, path in texture_paths.items():
        try:
            textures[material] = pv.Texture(str(path))
        except Exception:
            pass

    if not textures:
        return None

    material_ids = np.unique(np.asarray(mesh.cell_data["MaterialIds"]))
    parts = []
    used_materials = set()

    for material_id in material_ids:
        mask = np.asarray(mesh.cell_data["MaterialIds"]) == material_id
        if not np.any(mask):
            continue

        try:
            try:
                part = mesh.extract_cells(mask).extract_surface(algorithm="dataset_surface")
            except TypeError:
                part = mesh.extract_cells(mask).extract_surface()
        except Exception:
            continue

        best_material = None
        best_valid = 0
        for material in textures:
            if material not in part.point_data:
                continue
            uv = np.asarray(part.point_data[material])
            if uv.ndim != 2 or uv.shape[1] < 2:
                continue
            valid = int(np.count_nonzero((uv[:, 0] >= 0.0) & (uv[:, 1] >= 0.0)))
            if valid > best_valid:
                best_valid = valid
                best_material = material

        if not best_material or best_valid == 0:
            continue

        try:
            part.active_texture_coordinates = part.point_data[best_material]
        except Exception:
            continue

        parts.append({
            "mesh": part,
            "texture": textures[best_material],
            "material": best_material,
        })
        used_materials.add(best_material)

    if len(parts) <= 1:
        return None

    return {
        "mode": "multi",
        "parts": parts,
        "textures": textures,
        "materials": sorted(used_materials),
    }


def load_mesh_and_texture(obj_path: Path):
    mesh = pv.read(str(obj_path))

    texture = None

    mtl_path = parse_obj_mtl_path(obj_path)
    texture_paths = parse_mtl_texture_paths(mtl_path)
    texture = build_multi_texture_payload(mesh, texture_paths)

    if texture is not None:
        return mesh, texture

    texture_path = parse_mtl_texture_path(mtl_path)

    if texture_path is None and obj_path != OBJ_PATH:
        original_mtl = parse_obj_mtl_path(OBJ_PATH)
        texture_path = parse_mtl_texture_path(original_mtl)

    if texture_path and texture_path.exists():
        try:
            texture = pv.Texture(str(texture_path))
        except Exception:
            texture = None

    return mesh, texture


class QEMWorker(QtCore.QThread):
    finished = QtCore.pyqtSignal(bool, str, int, str)

    def __init__(self, ratio_percent: int, target_faces: int, output_path: Path):
        super().__init__()
        self.ratio_percent = int(ratio_percent)
        self.target_faces = int(target_faces)
        self.output_path = Path(output_path)

    def run(self):
        try:
            # Count faces in OBJ
            total_faces = 0
            with open(OBJ_PATH, 'r', encoding='utf-8', errors='ignore') as f:
                for line in f:
                    if line.startswith('f '):
                        total_faces += 1
            total_faces = max(total_faces, 4)

            ratio = min(self.target_faces / total_faces, 1.0)
            qem_simplifier.decimate_obj(str(OBJ_PATH), str(self.output_path), ratio)

            self.finished.emit(True, "Done", self.ratio_percent, str(self.output_path))

        except Exception as e:
            self.finished.emit(False, str(e), self.ratio_percent, str(self.output_path))


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()

        self.setWindowTitle("Pro Mesh Simplifier")
        self.resize(1480, 900)
        self.setStyleSheet(APP_STYLESHEET)

        self.original_mesh = None
        self.original_texture = None
        self.original_faces = 0

        self.current_mesh_obj = None
        self.current_texture_obj = None
        self.current_obj_path = None
        self.current_ratio = 100

        self.cache = {}

        self.show_texture = True
        self.show_wireframe = False
        self.texture_safe_floor = True
        self.force_rebuild = False

        self.worker = None
        self.pending_ratio = None

        self.first_camera_fit_done = False
        self.trackball_style = None

        self.preview_timer = QtCore.QTimer(self)
        self.preview_timer.setSingleShot(True)
        self.preview_timer.timeout.connect(self.request_preview_for_current_slider)

        self.init_ui()
        self.load_initial_model()

    # ---------- UI ----------

    def init_ui(self):
        root_panel = QtWidgets.QFrame()
        root_panel.setObjectName("RootPanel")
        self.setCentralWidget(root_panel)

        root = QtWidgets.QHBoxLayout(root_panel)
        root.setContentsMargins(18, 18, 18, 18)
        root.setSpacing(16)

        viewport_card = QtWidgets.QFrame()
        viewport_card.setObjectName("ViewportCard")
        viewport_layout = QtWidgets.QVBoxLayout(viewport_card)
        viewport_layout.setContentsMargins(12, 12, 12, 12)
        viewport_layout.setSpacing(10)

        topbar = QtWidgets.QHBoxLayout()
        topbar.setSpacing(10)

        self.lbl_view_title = QtWidgets.QLabel("3D Viewport")
        self.lbl_view_title.setObjectName("SectionTitle")

        self.lbl_view_hint = QtWidgets.QLabel("左键旋转 / 中键平移 / 滚轮缩放")
        self.lbl_view_hint.setObjectName("SectionHint")

        topbar.addWidget(self.lbl_view_title)
        topbar.addStretch()
        topbar.addWidget(self.lbl_view_hint)

        viewport_layout.addLayout(topbar)

        toolbar_card = QtWidgets.QFrame()
        toolbar_card.setObjectName("ToolbarCard")
        toolbar_layout = QtWidgets.QHBoxLayout(toolbar_card)
        toolbar_layout.setContentsMargins(12, 9, 12, 9)
        toolbar_layout.setSpacing(10)

        self.lbl_file_path = QtWidgets.QLabel("未加载 OBJ")
        self.lbl_file_path.setObjectName("FilePath")
        self.lbl_file_path.setWordWrap(False)
        self.lbl_file_path.setTextInteractionFlags(QtCore.Qt.TextSelectableByMouse)

        self.lbl_texture_chip = QtWidgets.QLabel("TEXTURE --")
        self.lbl_texture_chip.setObjectName("Chip")

        self.lbl_quality_chip = QtWidgets.QLabel("MODE 纹理优先")
        self.lbl_quality_chip.setObjectName("Chip")

        toolbar_layout.addWidget(self.lbl_file_path, stretch=1)
        toolbar_layout.addWidget(self.lbl_texture_chip)
        toolbar_layout.addWidget(self.lbl_quality_chip)
        viewport_layout.addWidget(toolbar_card)

        self.plotter = QtInteractor(self)
        self.plotter.set_background("#E8EDF3")

        self.setup_camera_interaction()

        viewport_layout.addWidget(self.plotter.interactor)

        root.addWidget(viewport_card, stretch=1)

        side = QtWidgets.QFrame()
        side.setObjectName("SidePanel")
        side.setMinimumWidth(360)
        side.setMaximumWidth(460)

        side_layout = QtWidgets.QVBoxLayout(side)
        side_layout.setContentsMargins(20, 20, 20, 20)
        side_layout.setSpacing(14)

        hero = QtWidgets.QFrame()
        hero.setObjectName("HeroCard")
        hero_layout = QtWidgets.QVBoxLayout(hero)
        hero_layout.setContentsMargins(18, 18, 18, 18)
        hero_layout.setSpacing(8)

        app_title = QtWidgets.QLabel("Mesh Simplifier")
        app_title.setObjectName("AppTitle")

        app_subtitle = QtWidgets.QLabel("Textured QEM LOD Preview Studio")
        app_subtitle.setObjectName("AppSubtitle")

        self.lbl_status = QtWidgets.QLabel("状态：准备就绪")
        self.lbl_status.setObjectName("StatusPill")
        self.lbl_status.setWordWrap(True)

        hero_layout.addWidget(app_title)
        hero_layout.addWidget(app_subtitle)
        hero_layout.addSpacing(8)
        hero_layout.addWidget(self.lbl_status)

        side_layout.addWidget(hero)

        ratio_card = QtWidgets.QFrame()
        ratio_card.setObjectName("GlassCard")
        ratio_layout = QtWidgets.QVBoxLayout(ratio_card)
        ratio_layout.setContentsMargins(18, 18, 18, 18)
        ratio_layout.setSpacing(12)

        row = QtWidgets.QHBoxLayout()

        ratio_text_box = QtWidgets.QVBoxLayout()
        self.lbl_ratio_value = QtWidgets.QLabel("100%")
        self.lbl_ratio_value.setObjectName("RatioBig")

        ratio_caption = QtWidgets.QLabel("当前保留比例")
        ratio_caption.setObjectName("RatioCaption")

        ratio_text_box.addWidget(self.lbl_ratio_value)
        ratio_text_box.addWidget(ratio_caption)

        row.addLayout(ratio_text_box)
        row.addStretch()

        ratio_input_box = QtWidgets.QVBoxLayout()
        ratio_input_label = QtWidgets.QLabel("手动输入")
        ratio_input_label.setObjectName("RatioCaption")
        self.spin_ratio = QtWidgets.QSpinBox()
        self.spin_ratio.setRange(1, 100)
        self.spin_ratio.setSuffix("%")
        self.spin_ratio.setValue(100)
        self.spin_ratio.setKeyboardTracking(False)
        self.spin_ratio.valueChanged.connect(self.on_ratio_spin_changed)
        ratio_input_box.addWidget(ratio_input_label)
        ratio_input_box.addWidget(self.spin_ratio)
        row.addLayout(ratio_input_box)

        self.progress = QtWidgets.QProgressBar()
        self.progress.setRange(0, 0)
        self.progress.hide()

        self.slider = QtWidgets.QSlider(QtCore.Qt.Horizontal)
        self.slider.setRange(1, 100)
        self.slider.setValue(100)
        self.slider.valueChanged.connect(self.on_slider_changed)

        ratio_layout.addLayout(row)
        ratio_layout.addWidget(self.slider)

        preset_layout = QtWidgets.QGridLayout()
        preset_layout.setSpacing(8)
        for col, ratio in enumerate([75, 50, 30, 20, 10]):
            btn = self.make_preset_button(f"{ratio}%", ratio)
            preset_layout.addWidget(btn, 0, col)
        ratio_layout.addLayout(preset_layout)
        ratio_layout.addWidget(self.progress)

        side_layout.addWidget(ratio_card)

        metrics_card = QtWidgets.QFrame()
        metrics_card.setObjectName("GlassCard")
        metrics_layout = QtWidgets.QVBoxLayout(metrics_card)
        metrics_layout.setContentsMargins(16, 16, 16, 16)
        metrics_layout.setSpacing(12)

        section = QtWidgets.QLabel("模型信息")
        section.setObjectName("SectionTitle")
        metrics_layout.addWidget(section)

        metric_grid = QtWidgets.QGridLayout()
        metric_grid.setHorizontalSpacing(10)
        metric_grid.setVerticalSpacing(10)

        self.lbl_original_value = self.make_metric_value("--")
        self.lbl_current_value = self.make_metric_value("--")
        self.lbl_cache_value = self.make_metric_value("0")

        metric_grid.addWidget(self.make_metric_card("原始面数", self.lbl_original_value), 0, 0)
        metric_grid.addWidget(self.make_metric_card("当前面数", self.lbl_current_value), 0, 1)
        metric_grid.addWidget(self.make_metric_card("缓存 LOD", self.lbl_cache_value), 1, 0, 1, 2)

        metrics_layout.addLayout(metric_grid)

        side_layout.addWidget(metrics_card)

        display_card = QtWidgets.QFrame()
        display_card.setObjectName("GlassCard")
        display_layout = QtWidgets.QVBoxLayout(display_card)
        display_layout.setContentsMargins(16, 16, 16, 16)
        display_layout.setSpacing(12)

        display_title = QtWidgets.QLabel("显示模式")
        display_title.setObjectName("SectionTitle")
        display_layout.addWidget(display_title)

        display_buttons = QtWidgets.QGridLayout()
        display_buttons.setSpacing(10)

        self.btn_texture = QtWidgets.QPushButton("纹理 ON")
        self.btn_texture.setCheckable(True)
        self.btn_texture.setChecked(True)
        self.btn_texture.setObjectName("Ghost")
        self.btn_texture.clicked.connect(self.toggle_texture)

        self.btn_wireframe = QtWidgets.QPushButton("网格 OFF")
        self.btn_wireframe.setCheckable(True)
        self.btn_wireframe.setChecked(False)
        self.btn_wireframe.setObjectName("Ghost")
        self.btn_wireframe.clicked.connect(self.toggle_wireframe)

        self.btn_focus = QtWidgets.QPushButton("聚焦模型")
        self.btn_focus.setObjectName("Ghost")
        self.btn_focus.clicked.connect(self.focus_current_model)

        self.btn_reset_camera = QtWidgets.QPushButton("重置视角")
        self.btn_reset_camera.setObjectName("Ghost")
        self.btn_reset_camera.clicked.connect(self.reset_camera_view)

        display_buttons.addWidget(self.btn_texture, 0, 0)
        display_buttons.addWidget(self.btn_wireframe, 0, 1)
        display_buttons.addWidget(self.btn_focus, 1, 0)
        display_buttons.addWidget(self.btn_reset_camera, 1, 1)

        display_layout.addLayout(display_buttons)

        camera_buttons = QtWidgets.QGridLayout()
        camera_buttons.setSpacing(8)
        self.btn_view_iso = self.make_preset_button("ISO", "iso")
        self.btn_view_top = self.make_preset_button("TOP", "top")
        self.btn_view_front = self.make_preset_button("FRONT", "front")
        self.btn_view_side = self.make_preset_button("SIDE", "side")
        camera_buttons.addWidget(self.btn_view_iso, 0, 0)
        camera_buttons.addWidget(self.btn_view_top, 0, 1)
        camera_buttons.addWidget(self.btn_view_front, 0, 2)
        camera_buttons.addWidget(self.btn_view_side, 0, 3)
        display_layout.addLayout(camera_buttons)

        hint = QtWidgets.QLabel("纹理与网格可以独立开关；旋转异常时点“聚焦模型”会重新设置旋转中心。")
        hint.setObjectName("SectionHint")
        hint.setWordWrap(True)
        display_layout.addWidget(hint)

        side_layout.addWidget(display_card)

        advanced_card = QtWidgets.QFrame()
        advanced_card.setObjectName("GlassCard")
        advanced_layout = QtWidgets.QVBoxLayout(advanced_card)
        advanced_layout.setContentsMargins(16, 16, 16, 16)
        advanced_layout.setSpacing(12)

        advanced_title = QtWidgets.QLabel("高级设置")
        advanced_title.setObjectName("SectionTitle")
        advanced_layout.addWidget(advanced_title)

        self.cmb_quality = QtWidgets.QComboBox()
        self.cmb_quality.addItems(["纹理优先（推荐）", "均衡预览", "速度优先"])
        self.cmb_quality.currentIndexChanged.connect(self.on_quality_mode_changed)
        advanced_layout.addWidget(self.cmb_quality)

        self.chk_texture_floor = QtWidgets.QCheckBox("纹理模型最低建议 20%")
        self.chk_texture_floor.setChecked(True)
        self.chk_texture_floor.toggled.connect(self.on_texture_floor_toggled)
        advanced_layout.addWidget(self.chk_texture_floor)

        self.chk_force_rebuild = QtWidgets.QCheckBox("强制重新生成，不读缓存")
        self.chk_force_rebuild.setChecked(False)
        self.chk_force_rebuild.toggled.connect(self.on_force_rebuild_toggled)
        advanced_layout.addWidget(self.chk_force_rebuild)

        self.chk_smooth = QtWidgets.QCheckBox("平滑着色")
        self.chk_smooth.setChecked(True)
        self.chk_smooth.toggled.connect(lambda _checked: self.refresh_current_render())
        advanced_layout.addWidget(self.chk_smooth)

        self.chk_axes = QtWidgets.QCheckBox("显示坐标轴")
        self.chk_axes.setChecked(True)
        self.chk_axes.toggled.connect(lambda _checked: self.refresh_current_render())
        advanced_layout.addWidget(self.chk_axes)

        kernel_hint = QtWidgets.QLabel("内核保护项")
        kernel_hint.setObjectName("MetricLabel")
        advanced_layout.addWidget(kernel_hint)

        self.chk_material_guard = QtWidgets.QCheckBox("保持材质边界")
        self.chk_material_guard.setChecked(True)
        self.chk_material_guard.setEnabled(False)
        advanced_layout.addWidget(self.chk_material_guard)

        self.chk_uv_guard = QtWidgets.QCheckBox("UV 接缝高权重保护")
        self.chk_uv_guard.setChecked(True)
        self.chk_uv_guard.setEnabled(False)
        advanced_layout.addWidget(self.chk_uv_guard)

        self.chk_flip_guard = QtWidgets.QCheckBox("法线翻转检查")
        self.chk_flip_guard.setChecked(True)
        self.chk_flip_guard.setEnabled(False)
        advanced_layout.addWidget(self.chk_flip_guard)

        advanced_hint = QtWidgets.QLabel("核心算法使用纹理保护版本；这里控制预览策略、缓存策略和显示效果。")
        advanced_hint.setObjectName("SectionHint")
        advanced_hint.setWordWrap(True)
        advanced_layout.addWidget(advanced_hint)

        side_layout.addWidget(advanced_card)

        actions_card = QtWidgets.QFrame()
        actions_card.setObjectName("GlassCard")
        actions_layout = QtWidgets.QVBoxLayout(actions_card)
        actions_layout.setContentsMargins(16, 16, 16, 16)
        actions_layout.setSpacing(12)

        actions_title = QtWidgets.QLabel("LOD 操作")
        actions_title.setObjectName("SectionTitle")
        actions_layout.addWidget(actions_title)

        self.btn_open = QtWidgets.QPushButton("打开 OBJ 文件")
        self.btn_open.clicked.connect(self.open_file_dialog)
        actions_layout.addWidget(self.btn_open)

        self.chk_auto = QtWidgets.QCheckBox("拖动滑条时自动生成预览")
        self.chk_auto.setChecked(DEFAULT_AUTO_PREVIEW)
        actions_layout.addWidget(self.chk_auto)

        self.btn_apply = QtWidgets.QPushButton("生成当前比例")
        self.btn_apply.clicked.connect(self.request_preview_for_current_slider)
        actions_layout.addWidget(self.btn_apply)

        self.btn_original = QtWidgets.QPushButton("显示原始模型")
        self.btn_original.setObjectName("Warm")
        self.btn_original.clicked.connect(self.show_original)
        actions_layout.addWidget(self.btn_original)

        self.btn_export = QtWidgets.QPushButton("导出当前 OBJ")
        self.btn_export.setObjectName("Accent")
        self.btn_export.clicked.connect(self.export_current_obj)
        actions_layout.addWidget(self.btn_export)

        self.btn_clear_cache = QtWidgets.QPushButton("清空缓存")
        self.btn_clear_cache.setObjectName("Danger")
        self.btn_clear_cache.clicked.connect(self.clear_cache)
        actions_layout.addWidget(self.btn_clear_cache)

        side_layout.addWidget(actions_card)

        side_layout.addStretch()

        scroll = QtWidgets.QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOff)
        scroll.setWidget(side)
        scroll.setMinimumWidth(380)
        scroll.setMaximumWidth(480)

        root.addWidget(scroll, stretch=0)

        for btn in [
            self.btn_texture,
            self.btn_wireframe,
            self.btn_focus,
            self.btn_reset_camera,
            self.btn_apply,
            self.btn_original,
            self.btn_open,
            self.btn_export,
            self.btn_clear_cache,
            self.btn_view_iso,
            self.btn_view_top,
            self.btn_view_front,
            self.btn_view_side,
        ]:
            btn.setCursor(QtGui.QCursor(QtCore.Qt.PointingHandCursor))

    def make_metric_value(self, text):
        label = QtWidgets.QLabel(text)
        label.setObjectName("MetricValue")
        return label

    def make_metric_card(self, title, value_label):
        card = QtWidgets.QFrame()
        card.setObjectName("MiniCard")

        layout = QtWidgets.QVBoxLayout(card)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(4)

        label = QtWidgets.QLabel(title)
        label.setObjectName("MetricLabel")

        layout.addWidget(label)
        layout.addWidget(value_label)

        return card

    def make_preset_button(self, text, value):
        button = QtWidgets.QPushButton(text)
        button.setObjectName("Preset")
        button.clicked.connect(lambda _checked=False, v=value: self.apply_preset(v))
        return button

    def apply_preset(self, value):
        if isinstance(value, int):
            if self.texture_safe_floor and value < 20:
                self.set_status("纹理优先模式下建议不低于 20%；可在高级设置中关闭限制。", "#9A3412")
                value = 20
            self.slider.setValue(value)
            self.request_preview_for_current_slider()
            return

        self.set_camera_preset(value)

    def sync_ratio_controls(self, value, source=None):
        if source != "slider":
            self.slider.blockSignals(True)
            self.slider.setValue(value)
            self.slider.blockSignals(False)
        if hasattr(self, "spin_ratio") and source != "spin":
            self.spin_ratio.blockSignals(True)
            self.spin_ratio.setValue(value)
            self.spin_ratio.blockSignals(False)
        self.lbl_ratio_value.setText(f"{value}%")

    def update_file_badges(self, obj_path=None, texture=None):
        if obj_path:
            path = Path(obj_path)
            self.lbl_file_path.setText(str(path))
        else:
            self.lbl_file_path.setText("未加载 OBJ")

        if is_multi_texture_payload(texture):
            count = len(texture.get("parts", []))
            self.lbl_texture_chip.setText(f"TEXTURE 多纹理 x{count}")
        elif texture is not None:
            self.lbl_texture_chip.setText("TEXTURE 单纹理")
        else:
            self.lbl_texture_chip.setText("TEXTURE 未找到")

    # ---------- Camera / Interaction ----------

    def setup_camera_interaction(self):
        try:
            from vtkmodules.vtkInteractionStyle import vtkInteractorStyleTrackballCamera

            style = vtkInteractorStyleTrackballCamera()
            style.SetMotionFactor(1.8)

            try:
                style.SetCurrentRenderer(self.plotter.renderer)
            except Exception:
                pass

            self.plotter.interactor.SetInteractorStyle(style)
            self.trackball_style = style

        except Exception:
            try:
                self.plotter.enable_trackball_style()
            except Exception:
                pass

        try:
            self.plotter.enable_anti_aliasing()
        except Exception:
            pass

        try:
            self.plotter.enable_depth_peeling()
        except Exception:
            pass

    def focus_camera_on_mesh(self, mesh, reset_position=False):
        if mesh is None:
            return

        center, radius = mesh_center_and_radius(mesh)

        try:
            camera = self.plotter.camera
            camera.focal_point = center

            if reset_position:
                distance = radius * 2.8
                camera.position = (
                    center[0] + distance,
                    center[1] - distance,
                    center[2] + distance * 0.7,
                )
                camera.up = (0, 0, 1)

            self.plotter.reset_camera_clipping_range()
            self.plotter.render()

        except Exception:
            try:
                self.plotter.reset_camera()
                self.plotter.render()
            except Exception:
                pass

    def focus_current_model(self):
        if self.current_mesh_obj is not None:
            self.focus_camera_on_mesh(self.current_mesh_obj, reset_position=False)
            self.set_status("已重新设置旋转中心", "#1F5E9E")

    def reset_camera_view(self):
        if self.current_mesh_obj is not None:
            self.focus_camera_on_mesh(self.current_mesh_obj, reset_position=True)
            self.set_status("视角已重置", "#1F5E9E")

    def set_camera_preset(self, preset):
        if self.current_mesh_obj is None:
            return

        center, radius = mesh_center_and_radius(self.current_mesh_obj)
        distance = radius * 3.0
        positions = {
            "iso": (center[0] + distance, center[1] - distance, center[2] + distance * 0.75),
            "top": (center[0], center[1], center[2] + distance),
            "front": (center[0], center[1] - distance, center[2]),
            "side": (center[0] + distance, center[1], center[2]),
        }
        ups = {
            "iso": (0, 0, 1),
            "top": (0, 1, 0),
            "front": (0, 0, 1),
            "side": (0, 0, 1),
        }
        try:
            self.plotter.camera.position = positions.get(preset, positions["iso"])
            self.plotter.camera.focal_point = center
            self.plotter.camera.up = ups.get(preset, (0, 0, 1))
            self.plotter.reset_camera_clipping_range()
            self.plotter.render()
            self.set_status(f"已切换视角：{str(preset).upper()}", "#1F5E9E")
        except Exception:
            self.reset_camera_view()

    # ---------- Status ----------

    def set_status(self, text, color="#1F5E9E"):
        self.lbl_status.setText(f"状态：{text}")
        self.lbl_status.setStyleSheet(
            f"""
            QLabel#StatusPill {{
                padding: 10px 12px;
                background-color: #EEF6FF;
                border: 1px solid #CFE3F7;
                border-radius: 10px;
                color: {color};
                font-size: 13px;
                font-weight: 700;
            }}
            """
        )

    def set_busy(self, busy: bool):
        self.progress.setVisible(busy)
        self.btn_open.setEnabled(not busy)
        self.btn_apply.setEnabled(not busy)
        self.btn_original.setEnabled(not busy)
        self.btn_export.setEnabled(not busy and self.current_obj_path is not None)
        self.btn_clear_cache.setEnabled(not busy)

        self.btn_texture.setEnabled(True)
        self.btn_wireframe.setEnabled(True)
        self.btn_focus.setEnabled(True)
        self.btn_reset_camera.setEnabled(True)
        self.slider.setEnabled(True)
        self.spin_ratio.setEnabled(True)

    def on_quality_mode_changed(self, index):
        if index == 0:
            self.texture_safe_floor = True
            self.chk_texture_floor.setChecked(True)
            self.chk_auto.setChecked(True)
            self.lbl_quality_chip.setText("MODE 纹理优先")
            self.set_status("已切换到纹理优先：建议 20% 以上预览", "#1F5E9E")
        elif index == 1:
            self.lbl_quality_chip.setText("MODE 均衡预览")
            self.set_status("已切换到均衡预览", "#1F5E9E")
        else:
            self.texture_safe_floor = False
            self.chk_texture_floor.setChecked(False)
            self.lbl_quality_chip.setText("MODE 速度优先")
            self.set_status("已切换到速度优先：允许生成更低比例", "#9A3412")

    def on_texture_floor_toggled(self, checked):
        self.texture_safe_floor = bool(checked)
        if checked and int(self.slider.value()) < 20:
            self.slider.setValue(20)
        self.set_status("已开启 20% 纹理建议下限" if checked else "已关闭 20% 纹理建议下限", "#1F5E9E")

    def on_force_rebuild_toggled(self, checked):
        self.force_rebuild = bool(checked)
        self.set_status("将强制重新生成当前比例" if checked else "将优先读取已有缓存", "#1F5E9E")

    # ---------- Load / Render ----------

    def load_initial_model(self):
        if OBJ_PATH is None or not OBJ_PATH.exists():
            self.set_status("Click [Open OBJ File] to load a model", "#BBD2FF")
            self.btn_apply.setEnabled(False)
            self.btn_export.setEnabled(False)
            return

        try:
            self.set_status("正在加载 OBJ / MTL / 贴图...", "#FFE4A8")
            QtWidgets.QApplication.processEvents()

            mesh, texture = load_mesh_and_texture(OBJ_PATH)

            self.original_mesh = mesh
            self.original_texture = texture
            self.original_faces = safe_face_count(mesh)

            self.current_mesh_obj = mesh
            self.current_texture_obj = texture
            self.current_obj_path = OBJ_PATH
            self.btn_export.setEnabled(True)
            self.update_file_badges(OBJ_PATH, texture)

            self.lbl_original_value.setText(f"{self.original_faces:,}")
            self.lbl_current_value.setText(f"{self.original_faces:,}")
            self.sync_ratio_controls(100)

            self.render_mesh(mesh, texture, reset_camera=True)

            if texture is None:
                self.set_status(
                    "模型已加载，但没有找到贴图。请确认 OBJ 同目录存在 MTL，且 MTL 中有 map_Kd。",
                    "#FFE4A8"
                )
            else:
                self.set_status("原始模型和贴图加载完成", "#A7F3D0")

        except Exception as e:
            self.set_status(f"读取失败：{e}", "#FF7B8A")

    def render_mesh(self, mesh, texture=None, reset_camera=False):
        self.current_mesh_obj = mesh
        self.current_texture_obj = texture
        smooth_shading = self.chk_smooth.isChecked() if hasattr(self, "chk_smooth") else True

        old_position = None
        old_focal = None
        old_up = None

        if not reset_camera and self.first_camera_fit_done:
            try:
                old_position = self.plotter.camera.position
                old_focal = self.plotter.camera.focal_point
                old_up = self.plotter.camera.up
            except Exception:
                pass

        self.plotter.clear()

        if self.show_texture:
            if is_multi_texture_payload(texture):
                for index, part in enumerate(texture.get("parts", [])):
                    self.plotter.add_mesh(
                        part["mesh"],
                        texture=part["texture"],
                        show_edges=False,
                        smooth_shading=smooth_shading,
                        name=f"textured_surface_{index}",
                        specular=0.12,
                        diffuse=0.88,
                        ambient=0.25,
                    )
            elif texture is not None:
                self.plotter.add_mesh(
                    mesh,
                    texture=texture,
                    show_edges=False,
                    smooth_shading=smooth_shading,
                    name="textured_surface",
                    specular=0.12,
                    diffuse=0.88,
                    ambient=0.25,
                )
            else:
                self.plotter.add_mesh(
                    mesh,
                    color="#9AA7B8",
                    show_edges=False,
                    smooth_shading=smooth_shading,
                    name="solid_surface",
                    specular=0.18,
                    diffuse=0.82,
                    ambient=0.28,
                )

        elif not self.show_wireframe:
            self.plotter.add_mesh(
                mesh,
                color="#9AA7B8",
                show_edges=False,
                smooth_shading=smooth_shading,
                name="solid_surface",
                specular=0.18,
                diffuse=0.82,
                ambient=0.28,
            )

        if self.show_wireframe:
            self.plotter.add_mesh(
                mesh,
                style="wireframe",
                color="#111111",
                line_width=1.25,
                opacity=0.78 if self.show_texture else 1.0,
                name="wireframe_overlay"
            )

        if not hasattr(self, "chk_axes") or self.chk_axes.isChecked():
            try:
                self.plotter.add_axes(interactive=False)
            except Exception:
                pass

        if reset_camera or not self.first_camera_fit_done:
            self.focus_camera_on_mesh(mesh, reset_position=True)
            self.first_camera_fit_done = True
        else:
            try:
                if old_position is not None:
                    self.plotter.camera.position = old_position
                    self.plotter.camera.focal_point = old_focal
                    self.plotter.camera.up = old_up
                else:
                    self.focus_camera_on_mesh(mesh, reset_position=False)

                self.plotter.reset_camera_clipping_range()
                self.plotter.render()
            except Exception:
                self.plotter.render()

    # ---------- Display toggles ----------

    def toggle_texture(self):
        self.show_texture = self.btn_texture.isChecked()
        self.btn_texture.setText("纹理 ON" if self.show_texture else "纹理 OFF")
        self.refresh_current_render()

    def toggle_wireframe(self):
        self.show_wireframe = self.btn_wireframe.isChecked()
        self.btn_wireframe.setText("网格 ON" if self.show_wireframe else "网格 OFF")
        self.refresh_current_render()

    def refresh_current_render(self):
        if self.current_mesh_obj is not None:
            self.render_mesh(self.current_mesh_obj, self.current_texture_obj, reset_camera=False)

    def open_file_dialog(self):
        global OBJ_PATH
        file_path, _ = QtWidgets.QFileDialog.getOpenFileName(
            self, "Select OBJ File", "",
            "OBJ Files (*.obj);;All Files (*)"
        )
        if not file_path:
            return

        OBJ_PATH = Path(file_path)
        self.cache.clear()
        self.lbl_cache_value.setText("0")
        self.first_camera_fit_done = False

        self.set_status("Loading OBJ / MTL / Texture...", "#FFE4A8")
        QtWidgets.QApplication.processEvents()

        try:
            mesh, texture = load_mesh_and_texture(OBJ_PATH)
            self.original_mesh = mesh
            self.original_texture = texture
            self.original_faces = safe_face_count(mesh)
            self.current_mesh_obj = mesh
            self.current_texture_obj = texture
            self.current_obj_path = OBJ_PATH
            self.btn_export.setEnabled(True)
            self.update_file_badges(OBJ_PATH, texture)

            self.lbl_original_value.setText(f"{self.original_faces:,}")
            self.lbl_current_value.setText(f"{self.original_faces:,}")
            self.sync_ratio_controls(100)

            self.render_mesh(mesh, texture, reset_camera=True)

            if texture is None:
                self.set_status("Loaded (no texture found)", "#FFE4A8")
            else:
                self.set_status("Model and texture loaded", "#A7F3D0")
        except Exception as e:
            self.set_status(f"Load failed: {e}", "#FF7B8A")

    def export_current_obj(self):
        if self.current_obj_path is None or not Path(self.current_obj_path).exists():
            self.set_status("没有可导出的 OBJ", "#FF7B8A")
            return

        source = Path(self.current_obj_path)
        default_name = source.with_name(f"{source.stem}_export.obj")
        target, _ = QtWidgets.QFileDialog.getSaveFileName(
            self,
            "导出当前 OBJ",
            str(default_name),
            "OBJ Files (*.obj);;All Files (*)",
        )
        if not target:
            return

        try:
            target_path = Path(target)
            shutil.copy2(source, target_path)

            mtl_path = parse_obj_mtl_path(source)
            if mtl_path and mtl_path.exists():
                shutil.copy2(mtl_path, target_path.parent / mtl_path.name)
                for tex_path in parse_mtl_texture_paths(mtl_path).values():
                    if tex_path.exists():
                        shutil.copy2(tex_path, target_path.parent / tex_path.name)

            self.set_status(f"已导出：{target_path.name}", "#0F766E")
        except Exception as e:
            self.set_status(f"导出失败：{e}", "#FF7B8A")

    # ---------- LOD logic ----------

    def show_original(self):
        if self.original_mesh is None:
            return

        self.current_ratio = 100

        self.sync_ratio_controls(100)
        self.lbl_current_value.setText(f"{self.original_faces:,}")
        self.current_obj_path = OBJ_PATH
        self.btn_export.setEnabled(self.current_obj_path is not None)
        self.update_file_badges(OBJ_PATH, self.original_texture)

        self.render_mesh(self.original_mesh, self.original_texture, reset_camera=False)
        self.set_status("已切换到原始模型", "#A7F3D0")

    def preview_delay_ms(self):
        if self.original_faces <= SMALL_MESH_FACES:
            return 60

        if self.original_faces <= MEDIUM_MESH_FACES:
            return 240

        return 580

    def on_slider_changed(self, value):
        if self.texture_safe_floor and value < 20:
            self.slider.blockSignals(True)
            self.slider.setValue(20)
            self.slider.blockSignals(False)
            value = 20
            self.set_status("纹理优先模式下已限制到 20%，避免过度压缩产生明显条纹", "#9A3412")

        self.current_ratio = int(value)
        self.sync_ratio_controls(value, source="slider")

        if value == 100:
            self.show_original()
            return

        if not self.force_rebuild and value in self.cache and Path(self.cache[value]).exists():
            self.load_cached_ratio(value)
            return

        if self.chk_auto.isChecked():
            self.set_status(f"等待生成 {value}% 预览...", "#BBD2FF")
            self.preview_timer.start(self.preview_delay_ms())

    def on_ratio_spin_changed(self, value):
        if self.texture_safe_floor and value < 20:
            self.spin_ratio.blockSignals(True)
            self.spin_ratio.setValue(20)
            self.spin_ratio.blockSignals(False)
            value = 20
            self.set_status("纹理优先模式下已限制到 20%，避免过度压缩产生明显条纹", "#9A3412")

        self.current_ratio = int(value)
        self.sync_ratio_controls(value, source="spin")

        if value == 100:
            self.show_original()
            return

        if not self.force_rebuild and value in self.cache and Path(self.cache[value]).exists():
            self.load_cached_ratio(value)
            return

        if self.chk_auto.isChecked():
            self.set_status(f"等待生成 {value}% 预览...", "#BBD2FF")
            self.preview_timer.start(self.preview_delay_ms())

    def request_preview_for_current_slider(self):
        ratio = int(self.slider.value())
        if self.texture_safe_floor and ratio < 20:
            ratio = 20
            self.sync_ratio_controls(20)

        if ratio == 100:
            self.show_original()
            return

        if not self.force_rebuild and ratio in self.cache and Path(self.cache[ratio]).exists():
            self.load_cached_ratio(ratio)
            return

        self.start_decimation(ratio)

    def cache_path_for_ratio(self, ratio):
        stem = OBJ_PATH.stem
        return CACHE_DIR / f"{stem}_lod_{ratio:03d}.obj"

    def start_decimation(self, ratio):
        if self.original_faces <= 0:
            return

        target_faces = max(4, int(self.original_faces * ratio / 100.0))
        output_path = self.cache_path_for_ratio(ratio)

        if self.worker is not None and self.worker.isRunning():
            self.pending_ratio = ratio
            self.set_status(f"当前正在计算，之后会自动切换到 {ratio}%...", "#FFE4A8")
            return

        self.pending_ratio = None

        self.set_busy(True)
        self.set_status(
            f"正在后台生成 {ratio}% LOD，目标面数 {target_faces:,}...",
            "#BBD2FF"
        )

        self.worker = QEMWorker(ratio, target_faces, output_path)
        self.worker.finished.connect(self.on_decimation_finished)
        self.worker.start()

    def on_decimation_finished(self, success, msg, ratio, output_path):
        self.set_busy(False)

        if not success:
            self.set_status(f"简化失败：{msg}", "#FF7B8A")
            return

        self.cache[ratio] = output_path
        self.lbl_cache_value.setText(str(len(self.cache)))

        latest_slider_ratio = int(self.slider.value())

        if self.pending_ratio is not None and self.pending_ratio != ratio:
            next_ratio = self.pending_ratio
            self.pending_ratio = None
            self.start_decimation(next_ratio)
            return

        if latest_slider_ratio != ratio:
            self.set_status(f"{ratio}% 已生成并缓存", "#A7F3D0")
            return

        self.load_cached_ratio(ratio)

    def load_cached_ratio(self, ratio):
        try:
            path = Path(self.cache[ratio])
            mesh, texture = load_mesh_and_texture(path)
            faces = safe_face_count(mesh)
            self.current_obj_path = path
            self.btn_export.setEnabled(True)
            self.update_file_badges(path, texture)

            self.lbl_current_value.setText(f"{faces:,}")
            self.sync_ratio_controls(ratio)

            self.render_mesh(mesh, texture, reset_camera=False)

            if texture is None:
                self.set_status(
                    f"{ratio}% LOD 已显示，但贴图没有找到；已尝试回退原始 MTL 贴图。",
                    "#FFE4A8"
                )
            else:
                self.set_status(f"已显示 {ratio}% LOD，面数 {faces:,}", "#A7F3D0")

        except Exception as e:
            self.set_status(f"加载缓存失败：{e}", "#FF7B8A")

    def clear_cache(self):
        try:
            for file in CACHE_DIR.glob("*"):
                if file.is_file():
                    file.unlink()

            self.cache.clear()
            self.lbl_cache_value.setText("0")
            self.set_status("缓存已清空", "#A7F3D0")

        except Exception as e:
            self.set_status(f"清空缓存失败：{e}", "#FF7B8A")


if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)
    app.setApplicationName("Pro Mesh Simplifier")

    window = MainWindow()
    window.show()

    sys.exit(app.exec_())
