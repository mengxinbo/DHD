"""
改进的合成数据生成脚本 - 使用新的渲染管线
在渲染阶段添加高反和模糊的物理模型
"""

import numpy as np
import json
from pathlib import Path
import sys
from typing import Tuple, Optional

sys.path.append('../')
from material_properties import MaterialAssigner, MaterialLibrary, DegradationMaterialAssigner, create_material_config


class AdvancedDatasetGenerator:
    """使用改进渲染管线的数据集生成器"""

    def __init__(self,
                 output_root: Path,
                 degradation_level: str = 'medium',
                 use_pbr: bool = True,
                 enable_material_variation: bool = True):
        """
        初始化数据集生成器

        Args:
            output_root: 输出根目录
            degradation_level: 降级程度 ('light', 'medium', 'heavy')
            use_pbr: 是否使用PBR着色
            enable_material_variation: 是否启用材质变化
        """
        self.output_root = Path(output_root)
        self.degradation_level = degradation_level
        self.use_pbr = use_pbr
        self.enable_material_variation = enable_material_variation

        # 加载材质配置
        self.material_config = create_material_config(degradation_level)

        # 创建输出目录
        self.output_root.mkdir(parents=True, exist_ok=True)

    def create_material_parameters(self, num_faces: int) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
        """
        创建材质参数

        Args:
            num_faces: 网格面数

        Returns:
            roughness, metallic, specular_boost 数组
        """
        assigner = DegradationMaterialAssigner(num_faces)

        # 创建基础材质
        base_material = MaterialLibrary.get('plastic')

        # 创建降级材质
        assigner.create_degraded_materials(
            base_material,
            specular_intensity=self.material_config['specular_intensity'],
            blur_intensity=self.material_config['blur_intensity']
        )

        # 添加变化
        if self.enable_material_variation:
            assigner.add_variation('roughness', self.material_config['variation_amount'])
            assigner.add_variation('metallic', self.material_config['variation_amount'] * 0.5)

        return assigner.get_arrays()

    def generate_shader_config(self) -> dict:
        """
        生成着色器配置

        Returns:
            着色器配置字典
        """
        return {
            'use_pbr': self.use_pbr,
            'default_roughness': self.material_config['base_roughness'],
            'default_metallic': self.material_config['base_metallic'],
            'specular_intensity': 1.0,
            'ka': 0.0,  # 环境光
            'kd': 1.5,  # 漫反射
            'ks': 0.35, # 镜面反射
            'alpha': 10.0  # Phong指数
        }

    def process_sample(self,
                      sample_dir: Path,
                      track_length: int = 4,
                      scales: list = None) -> dict:
        """
        处理单个样本

        Args:
            sample_dir: 样本目录
            track_length: 轨迹长度
            scales: 多尺度列表

        Returns:
            metadata: 处理元数据
        """
        if scales is None:
            scales = [0, 1, 2, 3]

        metadata = {
            'sample_id': sample_dir.name,
            'degradation_level': self.degradation_level,
            'use_pbr': self.use_pbr,
            'scales': {},
            'material_config': self.material_config
        }

        # 处理每个尺度
        for s in scales:
            scale_metadata = {'frames': {}}

            # 处理每个时间帧
            for tidx in range(track_length):
                try:
                    # 加载原始数据
                    im_path = sample_dir / f'im{s}_{tidx}.npy'
                    refimg_path = sample_dir / f'refimg{s}_{tidx}.npy'
                    disp_path = sample_dir / f'disp{s}_{tidx}.npy'

                    if not all([im_path.exists(), refimg_path.exists(), disp_path.exists()]):
                        continue

                    im = np.load(im_path)
                    refimg = np.load(refimg_path)
                    disp = np.load(disp_path)

                    # 注意：实际的高反和模糊效果应该在CUDA渲染阶段生成
                    # 这里我们只是保存原始数据和材质参数
                    # 真正的改进需要重新编译renderer并运行create_syn_data.py

                    # 保存处理后的数据（暂时与原始数据相同）
                    output_dir = self.output_root / sample_dir.name
                    output_dir.mkdir(parents=True, exist_ok=True)

                    np.save(output_dir / f'im{s}_{tidx}_advanced.npy', im)
                    np.save(output_dir / f'refimg{s}_{tidx}_advanced.npy', refimg)
                    np.save(output_dir / f'disp{s}_{tidx}_advanced.npy', disp)

                    scale_metadata['frames'][tidx] = {
                        'processed': True,
                        'shader_config': self.generate_shader_config()
                    }

                except Exception as e:
                    print(f"Error processing {sample_dir.name} scale {s} frame {tidx}: {e}")
                    continue

            metadata['scales'][s] = scale_metadata

        return metadata

    def generate_dataset(self,
                        input_root: Path,
                        num_samples: Optional[int] = None) -> dict:
        """
        生成完整数据集

        Args:
            input_root: 输入数据集根目录
            num_samples: 处理的样本数量，None表示全部

        Returns:
            dataset_metadata: 数据集元数据
        """
        input_root = Path(input_root)
        sample_dirs = sorted([d for d in input_root.iterdir() if d.is_dir() and d.name.isdigit()])

        if num_samples is not None:
            sample_dirs = sample_dirs[:num_samples]

        dataset_metadata = {
            'degradation_level': self.degradation_level,
            'use_pbr': self.use_pbr,
            'enable_material_variation': self.enable_material_variation,
            'num_samples': len(sample_dirs),
            'material_config': self.material_config,
            'samples': {}
        }

        for idx, sample_dir in enumerate(sample_dirs):
            print(f"Processing {idx + 1}/{len(sample_dirs)}: {sample_dir.name}")
            sample_metadata = self.process_sample(sample_dir)
            dataset_metadata['samples'][sample_dir.name] = sample_metadata

        return dataset_metadata

    def save_config(self, config_path: Path):
        """保存配置文件"""
        config = {
            'degradation_level': self.degradation_level,
            'use_pbr': self.use_pbr,
            'enable_material_variation': self.enable_material_variation,
            'material_config': self.material_config,
            'shader_config': self.generate_shader_config()
        }

        with open(config_path, 'w') as f:
            json.dump(config, f, indent=2)


def main():
    """主函数"""
    # 配置参数
    input_root = Path('./data/syn')  # 原始合成数据目录
    output_root = Path('./data/syn_advanced')  # 输出目录
    degradation_level = 'medium'  # 'light', 'medium', 'heavy'
    num_samples = 10  # 处理的样本数量

    # 创建生成器
    generator = AdvancedDatasetGenerator(
        output_root=output_root,
        degradation_level=degradation_level,
        use_pbr=True,
        enable_material_variation=True
    )

    # 生成数据集
    print(f"Generating advanced dataset with {degradation_level} degradation...")
    dataset_metadata = generator.generate_dataset(input_root, num_samples=num_samples)

    # 保存配置
    generator.save_config(output_root / 'advanced_config.json')

    # 保存元数据
    with open(output_root / 'dataset_metadata.json', 'w') as f:
        json.dump(dataset_metadata, f, indent=2)

    print(f"Dataset generation complete. Output saved to {output_root}")
    print("\n重要提示：")
    print("1. 上述脚本生成了材质参数配置")
    print("2. 要真正使用改进的渲染管线，需要：")
    print("   a) 重新编译renderer (make clean && make)")
    print("   b) 修改create_syn_data.py使用新的材质参数")
    print("   c) 运行create_syn_data.py生成新的数据集")


if __name__ == '__main__':
    main()
