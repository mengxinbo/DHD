"""
材质参数管理模块
为改进的渲染管线提供材质参数配置
"""

import numpy as np
from typing import Dict, Tuple, Optional
from pathlib import Path


class MaterialProperties:
    """材质属性类"""

    def __init__(self,
                 roughness: float = 0.5,
                 metallic: float = 0.0,
                 specular_boost: float = 1.0):
        """
        初始化材质属性

        Args:
            roughness: 表面粗糙度 (0-1)，0=镜面，1=完全漫反射
            metallic: 金属度 (0-1)，0=非金属，1=金属
            specular_boost: 高反强度倍数
        """
        self.roughness = np.clip(roughness, 0.0, 1.0)
        self.metallic = np.clip(metallic, 0.0, 1.0)
        self.specular_boost = np.clip(specular_boost, 0.5, 2.0)

    def to_dict(self) -> Dict:
        """转换为字典"""
        return {
            'roughness': float(self.roughness),
            'metallic': float(self.metallic),
            'specular_boost': float(self.specular_boost)
        }


class MaterialLibrary:
    """材质库 - 预定义的常见材质"""

    # 预定义材质
    MATERIALS = {
        'plastic': MaterialProperties(roughness=0.3, metallic=0.0, specular_boost=1.0),
        'rubber': MaterialProperties(roughness=0.8, metallic=0.0, specular_boost=0.5),
        'metal_smooth': MaterialProperties(roughness=0.1, metallic=1.0, specular_boost=1.5),
        'metal_rough': MaterialProperties(roughness=0.5, metallic=1.0, specular_boost=1.2),
        'ceramic': MaterialProperties(roughness=0.4, metallic=0.0, specular_boost=1.1),
        'glass': MaterialProperties(roughness=0.0, metallic=0.0, specular_boost=2.0),
        'wood': MaterialProperties(roughness=0.6, metallic=0.0, specular_boost=0.8),
        'fabric': MaterialProperties(roughness=0.9, metallic=0.0, specular_boost=0.3),
    }

    @classmethod
    def get(cls, name: str) -> MaterialProperties:
        """获取预定义材质"""
        if name not in cls.MATERIALS:
            raise ValueError(f"Unknown material: {name}. Available: {list(cls.MATERIALS.keys())}")
        return cls.MATERIALS[name]

    @classmethod
    def list_materials(cls) -> list:
        """列出所有可用材质"""
        return list(cls.MATERIALS.keys())


class MaterialAssigner:
    """材质分配器 - 为网格分配材质参数"""

    def __init__(self, num_faces: int):
        """
        初始化材质分配器

        Args:
            num_faces: 网格面数
        """
        self.num_faces = num_faces
        self.roughness = np.ones(num_faces, dtype=np.float32) * 0.5
        self.metallic = np.zeros(num_faces, dtype=np.float32)
        self.specular_boost = np.ones(num_faces, dtype=np.float32)

    def assign_uniform(self, material: MaterialProperties):
        """为所有面分配相同材质"""
        self.roughness[:] = material.roughness
        self.metallic[:] = material.metallic
        self.specular_boost[:] = material.specular_boost

    def assign_by_material_id(self, material_ids: np.ndarray, material_map: Dict[int, MaterialProperties]):
        """
        根据材质ID分配材质

        Args:
            material_ids: 每个面的材质ID (shape: num_faces)
            material_map: 材质ID到MaterialProperties的映射
        """
        for mat_id, material in material_map.items():
            mask = material_ids == mat_id
            self.roughness[mask] = material.roughness
            self.metallic[mask] = material.metallic
            self.specular_boost[mask] = material.specular_boost

    def assign_by_region(self, region_mask: np.ndarray, material: MaterialProperties):
        """
        根据区域掩码分配材质

        Args:
            region_mask: 布尔掩码 (shape: num_faces)
            material: 要分配的材质
        """
        self.roughness[region_mask] = material.roughness
        self.metallic[region_mask] = material.metallic
        self.specular_boost[region_mask] = material.specular_boost

    def add_variation(self, variation_type: str = 'roughness', amount: float = 0.1):
        """
        添加材质变化（模拟自然变化）

        Args:
            variation_type: 'roughness', 'metallic', 'specular'
            amount: 变化量
        """
        noise = np.random.normal(0, amount, self.num_faces)

        if variation_type == 'roughness':
            self.roughness = np.clip(self.roughness + noise, 0.0, 1.0)
        elif variation_type == 'metallic':
            self.metallic = np.clip(self.metallic + noise, 0.0, 1.0)
        elif variation_type == 'specular':
            self.specular_boost = np.clip(self.specular_boost + noise, 0.5, 2.0)

    def get_arrays(self) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
        """获取材质参数数组"""
        return self.roughness.copy(), self.metallic.copy(), self.specular_boost.copy()

    def save(self, output_dir: Path):
        """保存材质参数"""
        output_dir = Path(output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)

        np.save(output_dir / 'roughness.npy', self.roughness)
        np.save(output_dir / 'metallic.npy', self.metallic)
        np.save(output_dir / 'specular_boost.npy', self.specular_boost)

    @classmethod
    def load(cls, input_dir: Path) -> 'MaterialAssigner':
        """加载材质参数"""
        input_dir = Path(input_dir)

        roughness = np.load(input_dir / 'roughness.npy')
        metallic = np.load(input_dir / 'metallic.npy')
        specular_boost = np.load(input_dir / 'specular_boost.npy')

        assigner = cls(len(roughness))
        assigner.roughness = roughness
        assigner.metallic = metallic
        assigner.specular_boost = specular_boost

        return assigner


class DegradationMaterialAssigner(MaterialAssigner):
    """
    降级材质分配器
    为高反和模糊效果分配特殊的材质参数
    """

    def assign_specular_regions(self, specular_mask: np.ndarray, intensity: float = 0.8):
        """
        为高反区域分配材质

        Args:
            specular_mask: 高反掩码 (shape: num_faces)
            intensity: 高反强度 (0-1)
        """
        # 高反区域：低粗糙度 + 高金属度 + 高反强度
        self.roughness[specular_mask > 0.5] = 0.1 * (1 - intensity) + 0.3 * intensity
        self.metallic[specular_mask > 0.5] = 0.5 + intensity * 0.5
        self.specular_boost[specular_mask > 0.5] = 1.0 + intensity * 1.0

    def assign_blur_regions(self, blur_mask: np.ndarray, intensity: float = 0.5):
        """
        为模糊区域分配材质

        Args:
            blur_mask: 模糊掩码 (shape: num_faces)
            intensity: 模糊强度 (0-1)
        """
        # 模糊区域：高粗糙度（模拟焦点外）
        self.roughness[blur_mask > 0.5] = 0.5 + intensity * 0.4
        self.metallic[blur_mask > 0.5] = 0.2
        self.specular_boost[blur_mask > 0.5] = 0.8

    def create_degraded_materials(self,
                                 base_material: MaterialProperties,
                                 specular_intensity: float = 0.5,
                                 blur_intensity: float = 0.3) -> 'DegradationMaterialAssigner':
        """
        创建包含高反和模糊的降级材质

        Args:
            base_material: 基础材质
            specular_intensity: 高反强度
            blur_intensity: 模糊强度

        Returns:
            新的材质分配器
        """
        # 初始化为基础材质
        self.assign_uniform(base_material)

        # 随机分配高反和模糊区域
        num_specular = int(self.num_faces * specular_intensity * 0.3)
        num_blur = int(self.num_faces * blur_intensity * 0.2)

        specular_indices = np.random.choice(self.num_faces, num_specular, replace=False)
        blur_indices = np.random.choice(self.num_faces, num_blur, replace=False)

        specular_mask = np.zeros(self.num_faces, dtype=bool)
        blur_mask = np.zeros(self.num_faces, dtype=bool)

        specular_mask[specular_indices] = True
        blur_mask[blur_indices] = True

        self.assign_specular_regions(specular_mask.astype(np.float32), specular_intensity)
        self.assign_blur_regions(blur_mask.astype(np.float32), blur_intensity)

        return self


def create_material_config(degradation_level: str = 'medium') -> Dict:
    """
    创建预定义的材质配置

    Args:
        degradation_level: 'light', 'medium', 'heavy'

    Returns:
        config: 材质配置字典
    """
    configs = {
        'light': {
            'base_roughness': 0.4,
            'base_metallic': 0.1,
            'specular_intensity': 0.3,
            'blur_intensity': 0.2,
            'variation_amount': 0.05
        },
        'medium': {
            'base_roughness': 0.5,
            'base_metallic': 0.2,
            'specular_intensity': 0.5,
            'blur_intensity': 0.4,
            'variation_amount': 0.1
        },
        'heavy': {
            'base_roughness': 0.6,
            'base_metallic': 0.3,
            'specular_intensity': 0.8,
            'blur_intensity': 0.6,
            'variation_amount': 0.15
        }
    }

    return configs.get(degradation_level, configs['medium'])
