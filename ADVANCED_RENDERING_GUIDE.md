# 方案A：改进的渲染管线 - 完整集成指南

## 概述

这是一个完整的改进渲染管线实现，在CUDA着色器中添加了：
- 菲涅尔方程（Fresnel Equation）
- 表面粗糙度模型（GGX Distribution）
- 自适应曝光补偿（Adaptive Exposure）
- PBR着色模型（可选）

## 文件结构

```
renderer/render/
├── advanced_shading.h          # 新增：改进的着色函数库
├── render.h                    # 修改：添加材质参数和新着色器
├── render_gpu.cu               # 修改：处理材质参数的GPU内存管理
├── geometry.h                  # 无修改
└── ...

DHD/
├── material_properties.py      # 新增：材质参数管理
├── generate_advanced_dataset.py # 新增：改进的数据生成脚本
├── ADVANCED_RENDERING_GUIDE.md # 本文档
└── ...
```

## 修改说明

### 1. advanced_shading.h（新文件）

包含以下核心函数：

#### 菲涅尔方程 (Fresnel Schlick)
```cpp
T fresnel_schlick(const T* view_dir, const T* normal, const T f0)
```
- 计算表面反射率与视线角度的关系
- F(θ) = F0 + (1 - F0) * (1 - cos(θ))^5
- 高反强度随着视线角度增大而增大

#### GGX分布函数
```cpp
T ggx_distribution(const T* half_dir, const T* normal, const T roughness)
```
- 模拟表面微观几何对高光的影响
- 粗糙度越高，高光越分散
- 粗糙度越低，高光越集中

#### 几何遮挡函数
```cpp
T geometry_schlick(const T cos_theta, const T roughness)
```
- 模拟微观表面的自遮挡
- 粗糙表面自遮挡更多

#### PBR着色模型
```cpp
T pbr_shading(const T* orig, const T* sp, const T* lp, const T* n,
              const T roughness, const T metallic, const T base_color,
              const T specular_intensity)
```
- 完整的物理基础渲染模型
- 结合菲涅尔、GGX分布、几何遮挡
- 支持金属度和基础颜色

#### 改进的Phong着色
```cpp
T phong_with_fresnel(const T* orig, const T* sp, const T* lp, const T* n,
                     const T ka, const T kd, const T ks, const T alpha,
                     const T roughness, const T specular_boost)
```
- 在原Phong基础上添加菲涅尔和粗糙度
- 向后兼容原有代码
- 性能更好

#### 自适应曝光补偿
```cpp
T adaptive_exposure(const T color_value, const T specular_strength, const T exposure_factor)
```
- 根据高反强度自动调整曝光
- 防止高反区域过曝
- 保持暗区细节

### 2. render.h（修改）

#### 扩展RenderInput结构
```cpp
template <typename T>
struct RenderInput {
  // 原有成员...

  // 新增材质参数 (per-face)
  T* roughness;      // 表面粗糙度 (0-1)
  T* metallic;       // 金属度 (0-1)
  T* specular_boost; // 高反强度倍数
};
```

#### 扩展Shader结构
```cpp
template <typename T>
struct Shader {
  // 原有成员...

  // 新增参数
  const bool use_pbr;           // 是否使用PBR着色
  const T default_roughness;    // 默认粗糙度
  const T default_metallic;     // 默认金属度
  const T specular_intensity;   // 高反强度
};
```

#### 改进的着色计算
在RenderProjectorFunctor中替换着色计算：
```cpp
// 获取材质参数
T roughness = this->shader.default_roughness;
T metallic = this->shader.default_metallic;
T specular_boost = this->shader.specular_intensity;

if(this->input.roughness != nullptr) {
  roughness = this->input.roughness[face_idx];
}
// ... 类似处理metallic和specular_boost

// 使用改进的着色模型
T shading;
if(this->shader.use_pbr) {
  shading = pbr_shading(orig, sp, proj_orig, norm, roughness, metallic, base_color, specular_boost);
} else {
  shading = phong_with_fresnel(orig, sp, proj_orig, norm,
                               this->shader.ka, this->shader.kd, this->shader.ks, this->shader.alpha,
                               roughness, specular_boost);
}

// 自适应曝光补偿
T specularity = detect_specularity(norm, dir, roughness, metallic);
shading = adaptive_exposure(shading, specularity, 0.8f);
```

### 3. render_gpu.cu（修改）

#### 扩展input_to_device
处理新的材质参数GPU内存分配：
```cpp
if(input.roughness != nullptr) {
  input_gpu.roughness = host_to_device_malloc(input.roughness, input.n_faces);
}
// ... 类似处理metallic和specular_boost
```

#### 扩展input_free_device
释放新的材质参数GPU内存：
```cpp
if(input.roughness != nullptr) {
  device_free(input.roughness);
}
// ... 类似处理metallic和specular_boost
```

## 编译步骤

### 1. 准备环境
```bash
cd /home/xinbo/Desktop/DHD/renderer
```

### 2. 清理旧编译
```bash
make clean
```

### 3. 重新编译
```bash
make
```

如果编译失败，检查：
- CUDA_LIBRARY_PATH在config.json中是否正确
- advanced_shading.h是否在render/目录中
- render.h中的#include "advanced_shading.h"是否存在

### 4. 验证编译
```bash
python -c "import renderer; print('Renderer compiled successfully')"
```

## 使用步骤

### 1. 生成材质参数
```python
from material_properties import MaterialAssigner, DegradationMaterialAssigner
from pathlib import Path

# 创建材质分配器
assigner = DegradationMaterialAssigner(num_faces=10000)

# 创建降级材质
from material_properties import MaterialLibrary
base_material = MaterialLibrary.get('plastic')
assigner.create_degraded_materials(base_material, specular_intensity=0.5, blur_intensity=0.3)

# 获取材质参数
roughness, metallic, specular_boost = assigner.get_arrays()

# 保存
assigner.save(Path('./material_params'))
```

### 2. 修改create_syn_data.py

在get_mesh函数中添加材质参数：

```python
def get_mesh(rng, rng_clr, ref_len, x, y, min_z=0, material_assigner=None):
    # ... 原有代码 ...

    # 在返回前添加材质参数
    if material_assigner is not None:
        roughness, metallic, specular_boost = material_assigner.get_arrays()
        return verts, faces, colors, normals, roughness, metallic, specular_boost
    else:
        return verts, faces, colors, normals
```

在create_data函数中使用材质参数：

```python
def create_data(..., material_assigner=None):
    # ... 原有代码 ...

    verts, faces, colors, normals, roughness, metallic, specular_boost = get_mesh(...)

    # 创建RenderInput时添加材质参数
    data = renderer.PyRenderInput(
        verts=verts.copy(),
        colors=colors.copy(),
        normals=normals.copy(),
        faces=faces.copy(),
        roughness=roughness.copy(),
        metallic=metallic.copy(),
        specular_boost=specular_boost.copy()
    )

    # ... 后续代码 ...
```

### 3. 创建改进的Shader

```python
# 在create_syn_data.py中
shader = renderer.PyShader(
    ka=0.0,           # 环境光
    kd=1.5,           # 漫反射
    ks=0.35,          # 镜面反射
    alpha=10.0,       # Phong指数
    use_pbr=False,    # 使用改进的Phong而不是PBR
    roughness=0.5,    # 默认粗糙度
    metallic=0.0,     # 默认金属度
    specular_intensity=1.0  # 高反强度
)
```

### 4. 运行数据生成

```bash
python create_syn_data.py
```

## 材质参数说明

### 粗糙度 (Roughness)
- 0.0 = 完全镜面（高反）
- 0.5 = 中等粗糙
- 1.0 = 完全漫反射（无高反）

### 金属度 (Metallic)
- 0.0 = 非金属（绝缘体）
- 0.5 = 半金属
- 1.0 = 完全金属

### 高反强度 (Specular Boost)
- 0.5 = 减弱高反
- 1.0 = 正常高反
- 2.0 = 增强高反

## 预定义材质

```python
from material_properties import MaterialLibrary

# 可用材质
materials = MaterialLibrary.list_materials()
# ['plastic', 'rubber', 'metal_smooth', 'metal_rough', 'ceramic', 'glass', 'wood', 'fabric']

# 获取材质
plastic = MaterialLibrary.get('plastic')
metal = MaterialLibrary.get('metal_smooth')
```

## 性能考虑

### GPU内存
- 每个面额外需要12字节（3个float）
- 100万个面 ≈ 12MB额外内存

### 计算性能
- PBR着色比Phong着色多约30%计算量
- 改进的Phong着色只增加约10%计算量

### 建议
- 对于大规模数据集，使用改进的Phong而不是PBR
- 使用材质变化来增加数据多样性

## 故障排除

### 编译错误：undefined reference to advanced_shading
- 检查advanced_shading.h是否在render/目录中
- 检查render.h中是否有#include "advanced_shading.h"

### 运行时错误：材质参数为nullptr
- 确保在创建RenderInput时传递了材质参数
- 检查材质参数数组大小是否等于面数

### 渲染结果异常
- 检查材质参数范围是否在0-1之间
- 尝试使用默认材质参数（都为nullptr）
- 检查着色器参数是否合理

## 下一步

1. **编译改进的渲染器**
   ```bash
   cd renderer && make clean && make
   ```

2. **生成材质参数**
   ```bash
   python generate_advanced_dataset.py
   ```

3. **修改create_syn_data.py**
   - 集成材质参数
   - 使用改进的Shader

4. **生成改进的数据集**
   ```bash
   python create_syn_data.py
   ```

5. **对比实验**
   - 原方法 vs 改进方法
   - 评估高反和模糊区域的性能

## 参考资源

- PBR着色：https://learnopengl.com/PBR/Theory
- Fresnel方程：https://en.wikipedia.org/wiki/Fresnel_equations
- GGX分布：https://www.unrealengine.com/en-US/blog/physically-based-shading-on-mobile
