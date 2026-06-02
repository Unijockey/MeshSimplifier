// pybind_module.cpp - pybind11 bindings for QEM simplifier
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include "math_utils.h"
#include "quadric.h"
#include "qem_decimate.h"
#include "obj_io.h"

namespace py = pybind11;
using namespace blender;

py::tuple decimate(
    py::array_t<float, py::array::c_style | py::array::forcecast> vertices,
    py::array_t<int,   py::array::c_style | py::array::forcecast> faces,
    float target_ratio,
    int symmetry_axis,
    float symmetry_eps,
    py::object py_uvs
) {
    auto v_buf = vertices.request();
    auto f_buf = faces.request();
    if (v_buf.ndim != 2 || v_buf.shape[1] != 3) throw std::runtime_error("vertices must be (N, 3)");
    if (f_buf.ndim != 2 || f_buf.shape[1] != 3) throw std::runtime_error("faces must be (M, 3)");
    int nv = (int)v_buf.shape[0], nf = (int)f_buf.shape[0];

    // Per-face-vertex UVs: each face has 3 UV pairs
    const float *face_uv_ptr = nullptr;
    std::vector<float> face_uv_storage;
    if (!py_uvs.is_none()) {
        auto uv_buf = py_uvs.cast<py::array_t<float>>().request();
        if (uv_buf.ndim == 2 && uv_buf.shape[0] == nf*3 && uv_buf.shape[1] >= 2) {
            // Per-face-vertex UVs: (nf*3, 2)
            face_uv_storage.assign((float*)uv_buf.ptr, (float*)uv_buf.ptr + nf*3*2);
            face_uv_ptr = face_uv_storage.data();
        } else if (uv_buf.ndim == 2 && uv_buf.shape[0] == nv && uv_buf.shape[1] >= 2) {
            // Per-vertex UVs: expand to per-face-vertex (same UV for all faces of a vertex)
            face_uv_storage.resize(nf*3*2);
            auto uv_arr = py_uvs.cast<py::array_t<float>>();
            auto uv2 = uv_arr.unchecked<2>();
            auto f_arr = faces.cast<py::array_t<int>>();
            auto f2 = f_arr.unchecked<2>();
            for (int i=0;i<nf;i++) {
                for (int k=0;k<3;k++) {
                    int vi = f2(i,k);
                    face_uv_storage[(i*3+k)*2+0] = uv2(vi,0);
                    face_uv_storage[(i*3+k)*2+1] = uv2(vi,1);
                }
            }
            face_uv_ptr = face_uv_storage.data();
        }
    }

    QEMMesh mesh;
    mesh.init((float*)v_buf.ptr, nv, (int*)f_buf.ptr, nf, face_uv_ptr);

    SimplifyParams par;
    par.target_ratio = target_ratio;
    par.symmetry_axis = symmetry_axis;
    par.symmetry_eps = symmetry_eps;

    std::vector<float> ov, out_face_uvs;
    std::vector<int> of;
    simplify(mesh, par, ov, of, out_face_uvs);

    int onv = (int)ov.size()/3, onf = (int)of.size()/3;
    auto rv = py::array_t<float>({onv, 3});
    auto rf = py::array_t<int>({onf, 3});
    auto rv2 = rv.mutable_unchecked<2>();
    auto rf2 = rf.mutable_unchecked<2>();
    for (int i=0;i<onv;i++) { rv2(i,0)=ov[i*3]; rv2(i,1)=ov[i*3+1]; rv2(i,2)=ov[i*3+2]; }
    for (int i=0;i<onf;i++) { rf2(i,0)=of[i*3]; rf2(i,1)=of[i*3+1]; rf2(i,2)=of[i*3+2]; }

    int nuv = (int)out_face_uvs.size() / 2;
    auto ru = py::array_t<float>({nuv, 2});
    auto ru2 = ru.mutable_unchecked<2>();
    for (int i=0;i<nuv;i++) { ru2(i,0)=out_face_uvs[i*2]; ru2(i,1)=out_face_uvs[i*2+1]; }
    return py::make_tuple(rv, rf, ru);
}

py::tuple decimate_obj(const std::string& inp, const std::string& outp, float ratio, int sym) {
    ObjData obj = load_obj(inp.c_str());
    if (obj.verts.empty() || obj.faces.empty()) throw std::runtime_error("Failed to load OBJ: " + inp);

    QEMMesh mesh;
    mesh.init(obj.verts.data(), (int)obj.verts.size()/3, obj.faces.data(), (int)obj.faces.size()/3,
              obj.has_uvs ? obj.face_uvs.data() : nullptr,
              obj.face_materials.empty() ? nullptr : obj.face_materials.data());

    SimplifyParams par;
    par.target_ratio = ratio;
    par.symmetry_axis = sym;

    std::vector<float> ov, out_face_uvs;
    std::vector<int> of;
    std::vector<int> out_face_materials;
    simplify(mesh, par, ov, of, out_face_uvs);
    mesh.collect_result(ov, of, out_face_uvs, &out_face_materials);

    save_obj(outp.c_str(), ov.data(), (int)ov.size()/3, of.data(), (int)of.size()/3,
             obj.has_uvs ? out_face_uvs.data() : nullptr,
             out_face_materials.empty() ? nullptr : out_face_materials.data(),
             obj.material_names.empty() ? nullptr : &obj.material_names,
             obj.mtllib.empty() ? nullptr : &obj.mtllib);
    copy_obj_mtl(inp, outp, obj.mtllib);

    return py::make_tuple((int)ov.size()/3, (int)of.size()/3);
}

PYBIND11_MODULE(qem_simplifier, m) {
    m.doc() = "QEM mesh simplifier (Blender-compatible)";
    m.def("decimate", &decimate, "Simplify mesh",
          py::arg("vertices"), py::arg("faces"), py::arg("target_ratio")=0.5f,
          py::arg("symmetry_axis")=-1, py::arg("symmetry_eps")=2e-5f, py::arg("uvs")=py::none());
    m.def("decimate_obj", &decimate_obj, "Load OBJ, simplify, save",
          py::arg("input_path"), py::arg("output_path"), py::arg("target_ratio")=0.5f, py::arg("symmetry_axis")=-1);
}
