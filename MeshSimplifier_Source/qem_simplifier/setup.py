from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
import pybind11

ext_modules = [
    Extension(
        "qem_simplifier",
        sources=["src/pybind_module.cpp"],
        include_dirs=[
            "src",
            pybind11.get_include(),
        ],
        language="c++",
        extra_compile_args=["/std:c++17", "/O2"] if __import__('os').name == 'nt' else ["-std=c++17", "-O2"],
    ),
]

setup(
    name="qem_simplifier",
    version="1.0.0",
    description="QEM mesh simplifier (ported from Blender)",
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
)
