/*
 * 改进的着色器模块
 * 添加菲涅尔方程、表面粗糙度和自适应曝光补偿
 * 用于模拟高反和模糊效果
 */

#ifndef ADVANCED_SHADING_H
#define ADVANCED_SHADING_H

#include <cmath>
#include "co_types.h"

/*
 * 菲涅尔方程 (Fresnel Equation)
 * 计算表面反射率与视线角度的关系
 *
 * F(θ) = F0 + (1 - F0) * (1 - cos(θ))^5
 * 其中 F0 是法向入射时的反射率
 */
template <typename T>
CPU_GPU_FUNCTION
inline T fresnel_schlick(const T* view_dir, const T* normal, const T f0) {
  // 计算视线与法向的夹角
  T cos_theta = -vec_dot(view_dir, normal);
  cos_theta = fmax(fmin(cos_theta, T(1.0)), T(0.0));

  // Schlick近似
  T one_minus_cos = 1.0f - cos_theta;
  T fresnel = f0 + (1.0f - f0) * one_minus_cos * one_minus_cos * one_minus_cos * one_minus_cos * one_minus_cos;

  return fresnel;
}

/*
 * 粗糙度相关的高光分布 (GGX/Trowbridge-Reitz)
 * 模拟表面微观几何对高光的影响
 *
 * 粗糙度越高，高光越分散；粗糙度越低，高光越集中
 */
template <typename T>
CPU_GPU_FUNCTION
inline T ggx_distribution(const T* half_dir, const T* normal, const T roughness) {
  T cos_nh = vec_dot(half_dir, normal);
  cos_nh = fmax(fmin(cos_nh, T(1.0)), T(0.0));

  T alpha = roughness * roughness;
  alpha = alpha * alpha;

  T cos_nh_sq = cos_nh * cos_nh;
  T denom = cos_nh_sq * (alpha - 1.0f) + 1.0f;
  denom = denom * denom;

  return alpha / (3.14159265f * denom + 1e-6f);
}

/*
 * 几何遮挡函数 (Geometry Occlusion)
 * 模拟微观表面的自遮挡
 */
template <typename T>
CPU_GPU_FUNCTION
inline T geometry_schlick(const T cos_theta, const T roughness) {
  T r = roughness + 1.0f;
  T k = (r * r) / 8.0f;

  T denom = cos_theta * (1.0f - k) + k;
  return cos_theta / (denom + 1e-6f);
}

/*
 * 改进的PBR着色模型
 * 结合菲涅尔、粗糙度和高反效果
 *
 * 参数：
 *   orig: 相机位置
 *   sp: 表面点
 *   lp: 光源位置
 *   n: 表面法向
 *   roughness: 表面粗糙度 (0-1)，0=镜面，1=完全漫反射
 *   metallic: 金属度 (0-1)，0=非金属，1=金属
 *   base_color: 基础颜色反射率
 *   specular_intensity: 高反强度倍数
 */
template <typename T>
CPU_GPU_FUNCTION
T pbr_shading(const T* orig, const T* sp, const T* lp, const T* n,
              const T roughness, const T metallic, const T base_color,
              const T specular_intensity = 1.0f) {

  // 计算方向向量
  T l[3];  // 光线方向
  vec_sub(lp, sp, l);
  vec_normalize(l, l);

  T v[3];  // 视线方向
  vec_sub(orig, sp, v);
  vec_normalize(v, v);

  T h[3];  // 半向量
  vec_add(l, v, h);
  vec_normalize(h, h);

  // 计算各种点积
  T nl = vec_dot(n, l);
  T nv = vec_dot(n, v);
  T nh = vec_dot(n, h);
  T lh = vec_dot(l, h);

  // 确保点积在有效范围内
  nl = fmax(nl, T(0.0));
  nv = fmax(nv, T(0.0));
  nh = fmax(fmin(nh, T(1.0)), T(0.0));
  lh = fmax(fmin(lh, T(1.0)), T(0.0));

  // 菲涅尔项
  T f0 = 0.04f * (1.0f - metallic) + base_color * metallic;
  T fresnel = fresnel_schlick(v, n, f0);

  // 粗糙度项
  T alpha = roughness * roughness;
  T d = ggx_distribution(h, n, roughness);
  T g = geometry_schlick(nl, roughness) * geometry_schlick(nv, roughness);

  // 镜面反射项
  T specular = (d * g * fresnel) / (4.0f * nl * nv + 1e-6f);

  // 漫反射项
  T kd = (1.0f - fresnel) * (1.0f - metallic);
  T diffuse = kd * base_color / 3.14159265f;

  // 合并结果
  T result = (diffuse + specular) * nl;

  // 应用高反强度倍数
  result = result * specular_intensity;

  return result;
}

/*
 * 改进的Phong着色 + 高反效果
 * 在原Phong基础上添加菲涅尔和高反强度控制
 */
template <typename T>
CPU_GPU_FUNCTION
T phong_with_fresnel(const T* orig, const T* sp, const T* lp, const T* n,
                     const T ka, const T kd, const T ks, const T alpha,
                     const T roughness = 0.5f, const T specular_boost = 1.0f) {

  // 计算方向向量
  T l[3];
  vec_sub(lp, sp, l);
  vec_normalize(l, l);

  T r[3];
  vec_add(2 * vec_dot(l, n), n, -1.f, l, r);
  vec_normalize(r, r);

  T v[3];
  vec_sub(orig, sp, v);
  vec_normalize(v, v);

  // 基础Phong项
  T diffuse = vec_dot(l, n);
  T specular_phong = std::pow(vec_dot(r, v), alpha);

  // 菲涅尔项 - 增强高反
  T fresnel = fresnel_schlick(v, n, 0.04f);

  // 粗糙度衰减 - 粗糙表面高光更分散
  T roughness_factor = 1.0f / (1.0f + roughness * roughness * 10.0f);

  // 合并结果
  T result = ka + kd * diffuse + ks * specular_phong * fresnel * roughness_factor * specular_boost;

  return result;
}

/*
 * 自适应曝光补偿
 * 根据高反强度自动调整曝光
 * 防止高反区域过曝
 */
template <typename T>
CPU_GPU_FUNCTION
T adaptive_exposure(const T color_value, const T specular_strength, const T exposure_factor = 0.8f) {
  // 高反强度越高，曝光补偿越强
  T exposure_compensation = 1.0f / (1.0f + specular_strength * exposure_factor);

  // 应用补偿
  T compensated = color_value * exposure_compensation;

  // 防止完全黑化
  compensated = fmax(compensated, color_value * 0.3f);

  return compensated;
}

/*
 * 模糊掩码生成
 * 基于表面粗糙度和深度梯度生成模糊掩码
 * 用于后续的焦点模糊处理
 */
template <typename T>
CPU_GPU_FUNCTION
T compute_blur_mask(const T roughness, const T depth_gradient, const T focus_distance, const T current_depth) {
  // 粗糙度贡献
  T roughness_blur = roughness * 0.5f;

  // 深度梯度贡献（表面倾斜）
  T gradient_blur = fmin(depth_gradient * 0.3f, 1.0f);

  // 焦点模糊贡献
  T depth_diff = fabs(current_depth - focus_distance);
  T focus_blur = fmin(depth_diff / (focus_distance * 0.2f + 1e-6f), 1.0f);

  // 合并
  T blur_mask = fmin(roughness_blur + gradient_blur + focus_blur, 1.0f);

  return blur_mask;
}

/*
 * 高反检测
 * 返回高反强度 (0-1)
 */
template <typename T>
CPU_GPU_FUNCTION
T detect_specularity(const T* normal, const T* view_dir, const T roughness, const T metallic) {
  // 菲涅尔强度
  T fresnel = fresnel_schlick(view_dir, normal, 0.04f * (1.0f - metallic) + metallic);

  // 粗糙度影响 - 光滑表面高反更强
  T roughness_factor = 1.0f - roughness * roughness;

  // 金属度影响
  T metallic_factor = 0.5f + metallic * 0.5f;

  // 综合高反强度
  T specularity = fresnel * roughness_factor * metallic_factor;

  return specularity;
}

#endif // ADVANCED_SHADING_H
