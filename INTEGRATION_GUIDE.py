"""
create_syn_data.py 集成脚本
展示如何在原有脚本中集成改进的渲染管线
"""

# 这是一个示例，展示需要在create_syn_data.py中进行的修改

# ============================================================================
# 修改1：在文件开头添加导入
# ============================================================================

# 在原有导入后添加：
# from material_properties import MaterialAssigner, DegradationMaterialAssigner, create_material_config

# ============================================================================
# 修改2：修改get_mesh函数签名
# ============================================================================

# 原始函数：
# def get_mesh(rng, rng_clr, ref_len, x, y, min_z=0):

# 修改为：
# def get_mesh(rng, rng_clr, ref_len, x, y, min_z=0, material_assigner=None):

# ============================================================================
# 修改3：在get_mesh函数末尾添加材质参数
# ============================================================================

# 在原有返回语句前添加：
"""
  # 添加材质参数
  if material_assigner is not None:
    roughness, metallic, specular_boost = material_assigner.get_arrays()
    return verts, faces, colors, normals, roughness, metallic, specular_boost
  else:
    # 返回None作为占位符
    return verts, faces, colors, normals, None, None, None
"""

# ============================================================================
# 修改4：修改create_data函数
# ============================================================================

# 在函数签名中添加参数：
# def create_data(..., material_assigner=None):

# 在调用get_mesh时：
"""
  if material_assigner is not None:
    verts, faces, colors, normals, roughness, metallic, specular_boost = get_mesh(
        rng, rng_clr, reflectance.shape[0], x_center, y_center, material_assigner=material_assigner)
  else:
    verts, faces, colors, normals, roughness, metallic, specular_boost = get_mesh(
        rng, rng_clr, reflectance.shape[0], x_center, y_center)
    roughness, metallic, specular_boost = None, None, None
"""

# 在创建PyRenderInput时：
"""
  data = renderer.PyRenderInput(
      verts=verts.copy(),
      colors=colors.copy(),
      normals=normals.copy(),
      faces=faces.copy(),
      roughness=roughness.copy() if roughness is not None else None,
      metallic=metallic.copy() if metallic is not None else None,
      specular_boost=specular_boost.copy() if specular_boost is not None else None
  )
"""

# 在创建PyShader时：
"""
  # 使用改进的着色器参数
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
"""

# ============================================================================
# 修改5：在main函数中创建材质分配器
# ============================================================================

"""
if __name__=='__main__':
  # ... 原有代码 ...

  # 创建材质分配器
  material_config = create_material_config('medium')  # 'light', 'medium', 'heavy'

  # 为每个样本创建不同的材质
  material_assigner = None  # 可选：如果不需要材质参数，设为None

  # 如果需要使用材质参数：
  # material_assigner = DegradationMaterialAssigner(num_faces)
  # material_assigner.create_degraded_materials(...)

  # 在循环中使用
  for idx in range(n_samples):
    print(f"-----------------")
    print(f"进度: {idx + 1}/{n_samples}")

    # 为每个样本创建新的材质分配器
    if material_assigner is not None:
      sample_assigner = DegradationMaterialAssigner(num_faces)
      sample_assigner.create_degraded_materials(...)
    else:
      sample_assigner = None

    args = (out_root, idx, n_samples, imsize, patterns, reflectance,
            camerasensitivity, illumination, wavelength, K, shiftcamera,
            shiftpattern, baseline, blend_im, noise, maxdisp, mindisp,
            track_length, sample_assigner)  # 添加材质分配器

    maxdisp, mindisp = create_data(*args)
"""

# ============================================================================
# 完整的修改示例
# ============================================================================

EXAMPLE_MODIFICATIONS = """
# 在create_syn_data.py中的修改示例

# 1. 导入
from material_properties import DegradationMaterialAssigner, create_material_config

# 2. 修改get_mesh函数
def get_mesh(rng, rng_clr, ref_len, x, y, min_z=0, material_assigner=None):
  # ... 原有代码 ...

  verts, faces = co.geometry.stack_mesh(verts, faces)
  normals = np.vstack(normals).astype(np.float32)
  colors = np.hstack(colors).astype(np.int32)

  # 添加材质参数
  if material_assigner is not None:
    roughness, metallic, specular_boost = material_assigner.get_arrays()
    return verts, faces, colors, normals, roughness, metallic, specular_boost
  else:
    return verts, faces, colors, normals, None, None, None


# 3. 修改create_data函数
def create_data(out_root, idx, n_samples, imsize, patterns, reflectance,
                camerasensitivity, illumination, wavelength, K, shiftcamera,
                shiftpattern, baseline, blend_im, noise, maxdisp, mindisp,
                track_length=4, material_assigner=None):

  tic = time.time()
  rng = np.random.RandomState()
  rng_clr = np.random.RandomState()

  rng.seed(idx)
  rng_clr.seed(idx)

  x_center=(imsize[1]/2-K[0,2]+shiftcamera)/K[0,0]
  y_center=(imsize[0]/2-K[1,2])/K[1,1]

  # 调用get_mesh时传递材质分配器
  result = get_mesh(rng, rng_clr, reflectance.shape[0], x_center, y_center,
                    material_assigner=material_assigner)

  if material_assigner is not None:
    verts, faces, colors, normals, roughness, metallic, specular_boost = result
  else:
    verts, faces, colors, normals, _, _, _ = result
    roughness, metallic, specular_boost = None, None, None

  # 创建RenderInput
  data = renderer.PyRenderInput(
      verts=verts.copy(),
      colors=colors.copy(),
      normals=normals.copy(),
      faces=faces.copy(),
      roughness=roughness.copy() if roughness is not None else None,
      metallic=metallic.copy() if metallic is not None else None,
      specular_boost=specular_boost.copy() if specular_boost is not None else None
  )

  # ... 原有代码 ...

  # 创建改进的Shader
  shader = renderer.PyShader(
      ka=0.0,
      kd=1.5,
      ks=0.35,
      alpha=10.0,
      use_pbr=False,
      roughness=0.5,
      metallic=0.0,
      specular_intensity=1.0
  )

  # ... 后续代码保持不变 ...


# 4. 在main函数中
if __name__=='__main__':

  np.random.seed(42)

  # ... 原有代码 ...

  # 创建材质配置
  material_config = create_material_config('medium')

  # 开始生成数据
  n_samples = 2**8
  for idx in range(n_samples):
    print(f"-----------------")
    print(f"进度: {idx + 1}/{n_samples}")

    # 为每个样本创建材质分配器
    material_assigner = DegradationMaterialAssigner(num_faces=10000)  # 根据实际面数调整
    material_assigner.create_degraded_materials(
        base_material=MaterialLibrary.get('plastic'),
        specular_intensity=material_config['specular_intensity'],
        blur_intensity=material_config['blur_intensity']
    )

    args = (out_root, idx, n_samples, imsize, patterns, reflectance,
            camerasensitivity, illumination, wavelength, K, shiftcamera,
            shiftpattern, baseline, blend_im, noise, maxdisp, mindisp,
            track_length, material_assigner)

    maxdisp, mindisp = create_data(*args)
"""

print("=" * 80)
print("create_syn_data.py 集成指南")
print("=" * 80)
print(EXAMPLE_MODIFICATIONS)
print("=" * 80)
