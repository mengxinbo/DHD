#ifndef RENDER_H
#define RENDER_H

#include <cmath>
#include <algorithm>
#include <cstdio>

#include "co_types.h"
#include "common.h"
#include "geometry.h"
#include "advanced_shading.h"


template <typename T>
struct Camera {
  const T fx;
  const T fy;
  const T px;
  const T py;
  const T R0, R1, R2, R3, R4, R5, R6, R7, R8;
  const T t0, t1, t2;
  const T C0, C1, C2;
  const int height;
  const int width;

  Camera(const T fx, const T fy, const T px, const T py, const T* R, const T* t, int width, int height) :
    fx(fx), fy(fy), px(px), py(py),
    R0(R[0]), R1(R[1]), R2(R[2]), R3(R[3]), R4(R[4]), R5(R[5]), R6(R[6]), R7(R[7]), R8(R[8]),
    t0(t[0]), t1(t[1]), t2(t[2]),
    C0(-(R[0] * t[0] + R[3] * t[1] + R[6] * t[2])),
    C1(-(R[1] * t[0] + R[4] * t[1] + R[7] * t[2])),
    C2(-(R[2] * t[0] + R[5] * t[1] + R[8] * t[2])),
    height(height), width(width)
  {
  }

  CPU_GPU_FUNCTION
  inline void to_cam(const T* x, T* y) const {
    y[0] = R0 * x[0] + R1 * x[1] + R2 * x[2] + t0;
    y[1] = R3 * x[0] + R4 * x[1] + R5 * x[2] + t1;
    y[2] = R6 * x[0] + R7 * x[1] + R8 * x[2] + t2;
  }

  CPU_GPU_FUNCTION
  inline void to_world(const T* x, T* y) const {
    y[0] = R0 * (x[0] - t0) + R3 * (x[1] - t1) + R6 * (x[2] - t2);
    y[1] = R1 * (x[0] - t0) + R4 * (x[1] - t1) + R7 * (x[2] - t2);
    y[2] = R2 * (x[0] - t0) + R5 * (x[1] - t1) + R8 * (x[2] - t2);
  }

  CPU_GPU_FUNCTION
  inline void to_ray(const int h, const int w, T* dir) const {
    T uhat[2];
    uhat[0] = (w - px) / fx;
    uhat[1] = (h - py) / fy;
    dir[0] = R0 * (uhat[0]) + R3 * (uhat[1]) + R6;
    dir[1] = R1 * (uhat[0]) + R4 * (uhat[1]) + R7;
    dir[2] = R2 * (uhat[0]) + R5 * (uhat[1]) + R8;
  }

  CPU_GPU_FUNCTION
  inline void to_2d(const T* xyz, T* u, T* v, T* d) const {
    T xyz_t[3];
    to_cam(xyz, xyz_t);
    *u = fx * xyz_t[0] + px * xyz_t[2];
    *v = fy * xyz_t[1] + py * xyz_t[2];
    *d = xyz_t[2];
    *u /= *d;
    *v /= *d;
  }

  CPU_GPU_FUNCTION
  inline void get_C(T* C) const {
    C[0] = C0;
    C[1] = C1;
    C[2] = C2;
  }

  CPU_GPU_FUNCTION
  inline int num_pixel() const {
    return height * width;
  }
};


template <typename T>
struct RenderInput {
  T* verts;
  int* colors;
  T* normals;
  int n_verts;
  int* faces;
  int n_faces;

  // 材质参数 (per-face)
  T* roughness;      // 表面粗糙度 (0-1)
  T* metallic;       // 金属度 (0-1)
  T* specular_boost; // 高反强度倍数

  RenderInput() : verts(nullptr), colors(nullptr), normals(nullptr), n_verts(0), faces(nullptr), n_faces(0),
                  roughness(nullptr), metallic(nullptr), specular_boost(nullptr) {}
};

template <typename T>
struct Buffer {
  T* depth;
  T* color;
  T* reflectance;
  T* normal;

  Buffer() : depth(nullptr), color(nullptr), normal(nullptr) {}
};

template <typename T>
struct Shader {
  const T ka;
  const T kd;
  const T ks;
  const T alpha;

  // 改进的着色参数
  const bool use_pbr;           // 是否使用PBR着色
  const T default_roughness;    // 默认粗糙度
  const T default_metallic;     // 默认金属度
  const T specular_intensity;   // 高反强度

  Shader(T ka, T kd, T ks, T alpha, bool use_pbr = false, T roughness = 0.5f, T metallic = 0.0f, T spec_intensity = 1.0f)
    : ka(ka), kd(kd), ks(ks), alpha(alpha), use_pbr(use_pbr),
      default_roughness(roughness), default_metallic(metallic), specular_intensity(spec_intensity) {}

  CPU_GPU_FUNCTION
  T operator()(const T* orig, const T* sp, const T* lp, const T* norm) const {
    return reflectance_phong(orig, sp, lp, norm, ka, kd, ks, alpha);
  }
};



template <typename T>
class BaseRenderer {
public:
  const Camera<T> cam;
  const Shader<T> shader;
  Buffer<T> buffer;
  int wavelength;

  BaseRenderer(const Camera<T> cam, const Shader<T> shader, Buffer<T> buffer, int wavelength) : cam(cam), shader(shader), buffer(buffer), wavelength(wavelength) {
  }

  virtual ~BaseRenderer() {}

  virtual void render_mesh(const RenderInput<T> input) = 0;
  virtual void render_mesh_proj(const RenderInput<T> input, const Camera<T> proj, const unsigned char* pattern, const float* reflectance, const float* camerasensitivity, const float* illumination, int ref_num, int camsen_num, int illu_num, float d_alpha, float d_beta) = 0;
};



template <typename T>
struct RenderFunctor {
  const Camera<T> cam;
  const Shader<T> shader;
  Buffer<T> buffer;

  RenderFunctor(const Camera<T> cam, const Shader<T> shader, Buffer<T> buffer) : cam(cam), shader(shader), buffer(buffer) {}
};


template <typename T>
struct RenderMeshFunctor : public RenderFunctor<T> {
  const RenderInput<T> input;

  RenderMeshFunctor(const RenderInput<T> input, const Shader<T> shader, const Camera<T> cam, Buffer<T> buffer) : RenderFunctor<T>(cam, shader,buffer), input(input) {
  }

  CPU_GPU_FUNCTION void operator()(const int idx) {
    int h = idx / this->cam.width;
    int w = idx % this->cam.width;

    T orig[3];
    this->cam.get_C(orig);
    T dir[3];
    this->cam.to_ray(h, w, dir);

    int face_idx;
    T t, tu, tv;
    bool valid = ray_triangle_mesh_intersect_3d(orig, dir, this->input.faces, this->input.n_faces, this->input.verts, &face_idx, &t, &tu, &tv);

    if(this->buffer.depth != nullptr) {
      this->buffer.depth[idx] = valid ? t : -1;
    }

    if(!valid) {
      if(this->buffer.color != nullptr) {
        this->buffer.color[idx * 3 + 0] = 0;
        this->buffer.color[idx * 3 + 1] = 0;
        this->buffer.color[idx * 3 + 2] = 0;
      }
      if(this->buffer.normal != nullptr) {
        this->buffer.normal[idx * 3 + 0] = 0;
        this->buffer.normal[idx * 3 + 1] = 0;
        this->buffer.normal[idx * 3 + 2] = 0;
      }
    }
    else if(this->buffer.normal != nullptr || this->buffer.color != nullptr) {
      const int* face = input.faces + face_idx * 3;
      T tw = 1 - tu - tv;

      T norm[3];
      vec_fill(norm, 0.f);
      vec_add(1.f, norm, tu, this->input.normals + face[0] * 3, norm);
      vec_add(1.f, norm, tv, this->input.normals + face[1] * 3, norm);
      vec_add(1.f, norm, tw, this->input.normals + face[2] * 3, norm);
      if(vec_dot(norm, dir) > 0) {
        vec_mul_scalar(norm, -1.f, norm);
      }

      if(this->buffer.normal != nullptr) {
        this->buffer.normal[idx * 3 + 0] = norm[0];
        this->buffer.normal[idx * 3 + 1] = norm[1];
        this->buffer.normal[idx * 3 + 2] = norm[2];
      }

      if(this->buffer.color != nullptr) {
        T color[3];
        vec_fill(color, 0.f);
        vec_add(1.f, color, tu, this->input.colors + face[0] * 3, color);
        vec_add(1.f, color, tv, this->input.colors + face[1] * 3, color);
        vec_add(1.f, color, tw, this->input.colors + face[2] * 3, color);

        T sp[3];
        vec_add(1.f, orig, t, dir, sp);
        T shading = this->shader(orig, sp, orig, norm);

        this->buffer.color[idx * 3 + 0] = mmin(1.f, mmax(0.f, shading * color[0]));
        this->buffer.color[idx * 3 + 1] = mmin(1.f, mmax(0.f, shading * color[1]));
        this->buffer.color[idx * 3 + 2] = mmin(1.f, mmax(0.f, shading * color[2]));
      }
    }
  }
};

template <typename T, int n=3>
CPU_GPU_FUNCTION
inline void interpolate_linear(const T* im, T x, T y, int height, int width, T* out_vec) {
  int x1 = int(x);
  int y1 = int(y);
  int x2 = x1 + 1;
  int y2 = y1 + 1;

  T denom = (x2 - x1) * (y2 - y1);
  T t11 = (x2 - x) * (y2 - y);
  T t21 = (x - x1) * (y2 - y);
  T t12 = (x2 - x) * (y - y1);
  T t22 = (x - x1) * (y - y1);

  x1 = mmin(mmax(x1, int(0)), width-1);
  x2 = mmin(mmax(x2, int(0)), width-1);
  y1 = mmin(mmax(y1, int(0)), height-1);
  y2 = mmin(mmax(y2, int(0)), height-1);

  for(int idx = 0; idx < n; ++idx) {
    out_vec[idx] = (im[(y1 * width + x1) * 3 + idx] * t11 +
                    im[(y2 * width + x1) * 3 + idx] * t12 +
                    im[(y1 * width + x2) * 3 + idx] * t21 +
                    im[(y2 * width + x2) * 3 + idx] * t22) / denom;
  }
}

template <typename T>
CPU_GPU_FUNCTION
inline int interpolate_linear2(const int* im, T x, T y, int height, int width) {
  int x1 = std::round(x);
  int y1 = std::round(y);

  x1 = mmin(mmax(x1, int(0)), width-1);
  y1 = mmin(mmax(y1, int(0)), height-1);

  return im[y1 * width + x1];
}

template <typename T, int n=3>
CPU_GPU_FUNCTION
inline void interpolate_linear3(const unsigned char* im, T x, T y, int height, int width, const T * reflectance_local, const float* camerasensitivity, const float* illumination, const int wavelength, const T shading, T* out_color) {
  int x1 = int(x);
  int y1 = int(y);
  int x2 = x1 + 1;
  int y2 = y1 + 1;

  T t11 = (x2 - x) * (y2 - y);
  T t21 = (x - x1) * (y2 - y);
  T t12 = (x2 - x) * (y - y1);
  T t22 = (x - x1) * (y - y1);

  x1 = mmin(mmax(x1, int(0)), width-1);
  x2 = mmin(mmax(x2, int(0)), width-1);
  y1 = mmin(mmax(y1, int(0)), height-1);
  y2 = mmin(mmax(y2, int(0)), height-1);

  for(int illu=0; illu<3; illu++){
      float pattern_num11 = im[(y1 * width + x1)*3+illu]/255.0;
      float pattern_num21 = im[(y1 * width + x2)*3+illu]/255.0;
      float pattern_num12 = im[(y2 * width + x1)*3+illu]/255.0;
      float pattern_num22 = im[(y2 * width + x2)*3+illu]/255.0;
//printf("%d pattern_num11%f\n",index, pattern_num11);
      for(int idx = 0; idx < n; ++idx) {
        out_color[idx] += vec_dot_3in(reflectance_local, illumination + illu * wavelength, camerasensitivity + idx * wavelength, wavelength) * t11 * pattern_num11
            + vec_dot_3in(reflectance_local, illumination + illu * wavelength, camerasensitivity + idx * wavelength, wavelength) * t21 * pattern_num21
            + vec_dot_3in(reflectance_local, illumination + illu * wavelength, camerasensitivity + idx * wavelength, wavelength) * t12 * pattern_num12
            + vec_dot_3in(reflectance_local, illumination + illu * wavelength, camerasensitivity + idx * wavelength, wavelength) * t22 * pattern_num22;
      }
//printf("%d a:%f\n",index, out_color[0]);
  }
//printf("%d shading:%f\n",index, shading);
  for(int idx = 0; idx < n; ++idx) {
    out_color[idx] *= shading;
  }
//printf("%d b:%f\n",index, out_color[0]);
}

// ---------------------------------------------------------------------------
// RenderProjectorFunctor  –  spectral structured-light rendering
// ---------------------------------------------------------------------------
// Rendering formula for one camera pixel (channel c):
//
//   I_c = shading(θ) / r²  ×  Σ_{e=0}^{2}  Σ_λ  p_e(u,v) · ρ(λ) · E_e(λ) · s_c(λ)
//
// where
//   I_c         : rendered intensity for camera channel c  (R=0, G=1, B=2)
//   shading(θ)  : Phong shading term – a function of surface normal, view
//                 direction and light (projector) direction (see Shader struct)
//   r           : Euclidean distance from the projector centre to the surface
//                 point (inverse-square distance attenuation)
//   p_e(u,v)    : bilinearly-interpolated value of pattern channel e at the
//                 projected UV coordinate (u,v) on the projector image plane;
//                 normalised to [0, 1]  (i.e. pattern[...] / 255)
//   ρ(λ)        : spectral reflectance of the surface material at wavelength λ
//                 (loaded from reflectance.txt; per-material, per-wavelength)
//   E_e(λ)      : spectral power distribution of the projector's illuminant for
//                 colour channel e  (loaded from illumination.txt)
//   s_c(λ)      : spectral sensitivity of camera channel c at wavelength λ
//                 (loaded from camerasensitivity.txt)
//
// The inner sum over wavelengths λ is computed by vec_dot_3in(), accumulating
// the triple product  ρ(λ) · E_e(λ) · s_c(λ)  across the full spectrum.
// The outer sum over projector colour channels e ∈ {0,1,2} integrates the
// contribution from each LED/spectral band of the structured-light projector.
//
// Ambient (pattern-free) image for channel c:
//   A_c = shading(θ)  ×  Σ_λ  ρ(λ) · s_c(λ)
// (stored in buffer.normal; computed before the projector visibility check)
// ---------------------------------------------------------------------------
template <typename T>
struct RenderProjectorFunctor : public RenderFunctor<T> {
  const RenderInput<T> input;
  const Camera<T> proj;
  const unsigned char* pattern;
  const float* reflectance;
  const float* camerasensitivity;
  const float* illumination;
  const float d_alpha;
  const float d_beta;
  const int wavelength;

  RenderProjectorFunctor(const RenderInput<T> input, const Shader<T> shader, const Camera<T> cam, const Camera<T> proj, const unsigned char* pattern, const float* reflectance, const float* camerasensitivity, const float* illumination, float d_alpha, float d_beta, Buffer<T> buffer, int wavelength) : RenderFunctor<T>(cam, shader, buffer), input(input), proj(proj), pattern(pattern), reflectance(reflectance), camerasensitivity(camerasensitivity), illumination(illumination), d_alpha(d_alpha), d_beta(d_beta), wavelength(wavelength) {
  }

  CPU_GPU_FUNCTION void operator()(const int idx) {
//    printf("%d start\n",idx);
    int h = idx / this->cam.width;
    int w = idx % this->cam.width;

    T orig[3];
    this->cam.get_C(orig);
    T dir[3];
    this->cam.to_ray(h, w, dir);

    int face_idx;
    T t, tu, tv;
    bool valid = ray_triangle_mesh_intersect_3d(orig, dir, this->input.faces, this->input.n_faces, this->input.verts, &face_idx, &t, &tu, &tv);
    if(this->buffer.depth != nullptr) {
      this->buffer.depth[idx] = valid ? t : -1;
    }

    const int* face = input.faces + face_idx * 3;

    //if(this->buffer.normal != nullptr) {
    //    this->buffer.normal[idx * 3 + 0] = 0;
     //   this->buffer.normal[idx * 3 + 1] = 0;
     //   this->buffer.normal[idx * 3 + 2] = 0;
    //}

    //if(this->buffer.color != nullptr) {
        this->buffer.color[idx * 3 + 0] = 0;
        this->buffer.color[idx * 3 + 1] = 0;
        this->buffer.color[idx * 3 + 2] = 0;
    //}
    for(int i=0;i<this->wavelength;i++) {
        this->buffer.reflectance[idx * wavelength + i]=0;
    }

    if(valid) {
      T tw = 1 - tu - tv;
      //T* reflectance_local = new T[this->wavelength];
      //T reflectance_local[27];
      //if (idx == 0){
        //printf("%d\n", 1);
      //}
      const T * reflectance_local = this->reflectance + this->input.colors[face_idx] * this->wavelength;
//      vec_fill(reflectance_local, 0.f, this->wavelength);
//      vec_add(1.f, reflectance_local, tu, this->reflectance + this->input.colors[face[0]] * this->wavelength, reflectance_local, this->wavelength);
//      vec_add(1.f, reflectance_local, tv, this->reflectance + this->input.colors[face[1]] * this->wavelength, reflectance_local, this->wavelength);
//      vec_add(1.f, reflectance_local, tw, this->reflectance + this->input.colors[face[2]] * this->wavelength, reflectance_local, this->wavelength);
      for(int i=0;i<this->wavelength;i++) {
          this->buffer.reflectance[idx * wavelength + i]=reflectance_local[i];
      }

      T norm[3];
      vertex_normal_3d(
        this->input.verts + face[0] * 3,
        this->input.verts + face[1] * 3,
        this->input.verts + face[2] * 3,
        norm);
      vec_normalize(norm, norm);

      if(vec_dot(norm, dir) > 0) {
        vec_mul_scalar(norm, -1.f, norm);
      }

      // get 3D point
      T pt[3];
      vec_mul_scalar(dir, t, pt);
      vec_add(orig, pt, pt);

      // get dir from proj
      T proj_orig[3];
      proj.get_C(proj_orig);
      T proj_dir[3];
      vec_sub(pt, proj_orig, proj_dir);
      vec_div_scalar(proj_dir, proj_dir[2], proj_dir);

      // calculate shading with advanced model
      T sp[3];
      vec_add(1.f, orig, t, dir, sp);

      // 获取材质参数
      T roughness = this->shader.default_roughness;
      T metallic = this->shader.default_metallic;
      T specular_boost = this->shader.specular_intensity;

      if(this->input.roughness != nullptr) {
        roughness = this->input.roughness[face_idx];
      }
      if(this->input.metallic != nullptr) {
        metallic = this->input.metallic[face_idx];
      }
      if(this->input.specular_boost != nullptr) {
        specular_boost = this->input.specular_boost[face_idx];
      }

      // 使用改进的着色模型
      T shading;
      if(this->shader.use_pbr) {
        // PBR着色
        T base_color = 0.5f;  // 默认基础颜色
        shading = pbr_shading(orig, sp, proj_orig, norm, roughness, metallic, base_color, specular_boost);
      } else {
        // 改进的Phong着色 + 菲涅尔 + 高反
        shading = phong_with_fresnel(orig, sp, proj_orig, norm,
                                     this->shader.ka, this->shader.kd, this->shader.ks, this->shader.alpha,
                                     roughness, specular_boost);
      }

      // 自适应曝光补偿
      T specularity = detect_specularity(norm, dir, roughness, metallic);
      shading = adaptive_exposure(shading, specularity, 0.8f);

      if(this->buffer.normal != nullptr) {
          this->buffer.normal[idx * 3 + 0] = shading * vec_dot(reflectance_local, this->camerasensitivity + 0 * this->wavelength, this->wavelength);
          this->buffer.normal[idx * 3 + 1] = shading * vec_dot(reflectance_local, this->camerasensitivity + 1 * this->wavelength, this->wavelength);
          this->buffer.normal[idx * 3 + 2] = shading * vec_dot(reflectance_local, this->camerasensitivity + 2 * this->wavelength, this->wavelength);
      }
      // check if it hit same tria
      int p_face_idx;
      T p_t, p_tu, p_tv;
      valid = ray_triangle_mesh_intersect_3d(proj_orig, proj_dir, this->input.faces, this->input.n_faces, this->input.verts, &p_face_idx, &p_t, &p_tu, &p_tv);
      // if(!valid || p_face_idx != face_idx) {
      //   return;
      // }

      T p_pt[3];
      vec_mul_scalar(proj_dir, p_t, p_pt);
      vec_add(proj_orig, p_pt, p_pt);
      T diff[3];
      vec_sub(p_pt, pt, diff);
      if(!valid || vec_norm(diff) > 1e-5) {
        return;
      }

        // get uv in proj
        T u,v,d;
        proj.to_2d(p_pt, &u,&v,&d);

        // if valid u,v than use it to inpaint
        if(u >= 0 && v >= 0 && u < this->proj.width && v < this->proj.height) {
            // int pattern_idx = ((int(v) * this->proj.width) + int(u)) * 3;
            // this->buffer.color[idx * 3 + 0] = pattern[pattern_idx + 0];
            // this->buffer.color[idx * 3 + 1] = pattern[pattern_idx + 1];
            // this->buffer.color[idx * 3 + 2] = pattern[pattern_idx + 2];

//            int pattern_num = interpolate_linear2(this->pattern, u, v, this->proj.height, this->proj.width);

//            this->buffer.color[idx * 3 + 0] = shading * vec_dot_3in(reflectance_local, this->illumination + pattern_num * wavelength, this->camerasensitivity + 0 * this->wavelength, this->wavelength);
//            this->buffer.color[idx * 3 + 1] = shading * vec_dot_3in(reflectance_local, this->illumination + pattern_num * wavelength, this->camerasensitivity + 1 * this->wavelength, this->wavelength);
//            this->buffer.color[idx * 3 + 2] = shading * vec_dot_3in(reflectance_local, this->illumination + pattern_num * wavelength, this->camerasensitivity + 2 * this->wavelength, this->wavelength);
            interpolate_linear3(this->pattern, u, v, this->proj.height, this->proj.width, reflectance_local, this->camerasensitivity, this->illumination, this->wavelength, shading, this->buffer.color + idx * 3);
            // decay based on distance
//            T decay = d_alpha + d_beta * d;
//            decay *= decay;
            T decay = distance_euclidean(proj_orig, sp, 3);
//            decay = mmax(decay, T(1));
            vec_div_scalar(this->buffer.color + idx * 3, decay, this->buffer.color + idx * 3);
        }

      //delete[] reflectance_local;

    }
//    printf("%d end\n",idx);

  }
};




#endif
