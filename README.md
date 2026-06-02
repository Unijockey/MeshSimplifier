# Mesh Simplifier

这是一个课程实习项目，用于对带纹理的 OBJ 网格模型进行简化。程序由 Python 图形界面和 C++ 简化算法两部分组成：Python 负责界面、模型加载、三维显示和导出；C++ 扩展负责主要的 QEM 网格简化计算。

项目目前整理成两个目录：

```text
GUI/
├─ MeshSimplifier_App/       # 可直接运行的应用包
├─ MeshSimplifier_Source/    # 源码和 C++ 扩展工程
└─ README.md
```

## 主要功能

- 打开 OBJ 模型，并读取对应的 MTL 和贴图文件
- 支持带多材质、多贴图的模型显示
- 使用 C++ 扩展进行 QEM 网格简化
- 支持输入简化比例，也可以使用界面中的预设比例
- 在 3D 窗口中预览原始模型和简化结果
- 导出简化后的 OBJ、MTL 和贴图文件

## 应用包运行方法

如果只想运行程序，使用 `MeshSimplifier_App` 目录即可。

目录内容如下：

```text
MeshSimplifier_App/
├─ app/
├─ python/
└─ Run_MeshSimplifier.bat
```

运行步骤：

```text
1. 保持 app、python 和 Run_MeshSimplifier.bat 在同一个目录中。
2. 双击 Run_MeshSimplifier.bat。
3. 在界面中打开 OBJ 文件，设置简化比例，生成并导出结果。
```

本地整理好的应用包已经包含 Python 环境和依赖库，换到其他 Windows 电脑时一般不需要重新安装 Python。打包或拷贝时，需要复制整个 `MeshSimplifier_App` 文件夹，不能只复制 `.bat` 文件。

如果是从 GitHub 源码仓库下载，`MeshSimplifier_App/python` 运行环境不会随源码提交，需要使用源码方式运行，或另外下载完整应用包压缩文件。

## 源码运行方法

如果需要查看或修改代码，使用 `MeshSimplifier_Source` 目录。

目录内容如下：

```text
MeshSimplifier_Source/
├─ mesh_simplifier.py
└─ qem_simplifier/
   ├─ src/
   ├─ setup.py
   ├─ CMakeLists.txt
   ├─ build.bat
   └─ qem_simplifier.cp312-win_amd64.pyd
```

推荐使用项目已有的虚拟环境运行：

```powershell
K:\WHU\Gaodeng\mesh_simplipy\.venv\Scripts\Activate
cd K:\WHU\Gaodeng\GUI\MeshSimplifier_Source
python mesh_simplifier.py
```

其中：

- `mesh_simplifier.py` 是主界面程序
- `qem_simplifier/src` 存放 C++ 简化算法源码
- `qem_simplifier.cp312-win_amd64.pyd` 是已经编译好的 Python C++ 扩展

## 重新编译 C++ 扩展

如果修改了 `qem_simplifier/src` 中的 C++ 代码，需要重新编译 `.pyd` 文件。

```powershell
K:\WHU\Gaodeng\mesh_simplipy\.venv\Scripts\Activate
cd K:\WHU\Gaodeng\GUI\MeshSimplifier_Source\qem_simplifier
python setup.py build_ext --inplace
```

编译成功后，会在 `qem_simplifier` 目录下生成新的：

```text
qem_simplifier.cp312-win_amd64.pyd
```

如果要同步到应用包，需要把新的 `.pyd` 复制到：

```text
MeshSimplifier_App/app/qem_simplifier/
```

如果修改了 Python 界面代码，需要把：

```text
MeshSimplifier_Source/mesh_simplifier.py
```

复制到：

```text
MeshSimplifier_App/app/mesh_simplifier.py
```

## 工程说明

本项目的核心思路是将网格简化算法放在 C++ 中实现，通过 pybind11 编译成 Python 扩展，再由 Python GUI 调用。这样界面开发比较方便，同时简化计算部分也能保持较好的运行速度。

程序主要依赖：

- Python 3.12
- PyQt5
- PyVista / PyVistaQt
- VTK
- NumPy
- pybind11
- Visual Studio C++ Build Tools

应用包中的依赖已经打包好；只有在源码开发和重新编译 C++ 扩展时，才需要关注这些环境。

## 常见问题

| 问题 | 说明 |
| --- | --- |
| 应用包换电脑后打不开 | 确认复制的是整个 `MeshSimplifier_App` 文件夹 |
| 只复制 `.bat` 后无法运行 | `.bat` 依赖旁边的 `app` 和 `python` 文件夹 |
| 源码运行找不到 `qem_simplifier` | 检查 `.pyd` 是否在 `MeshSimplifier_Source/qem_simplifier/` 中 |
| 编译 C++ 扩展失败 | 检查是否安装 Visual Studio C++ Build Tools，并确认已激活虚拟环境 |
| 3D 视窗显示异常 | 一般和显卡驱动或 OpenGL 支持有关 |
