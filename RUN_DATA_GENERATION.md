# 生成改进数据集 - 快速启动

## ✅ 已完成
- [x] 编译改进的渲染器
- [x] 修改create_syn_data.py集成材质参数

## 🚀 现在可以生成数据

### 命令
```bash
cd /home/xinbo/Desktop/DHD
python create_syn_data.py
```

### 会发生什么
1. 加载ShapeNet模型
2. 为每个样本创建材质分配器
3. 生成包含高反和模糊的改进数据集
4. 保存到 `./data/syn/` 目录

### 配置选项

如果想改变降级程度，修改这一行：
```python
material_config = create_material_config('medium')  # 改为 'light' 或 'heavy'
```

- **light**：轻度降级，接近原始
- **medium**：中度降级，现实场景（推荐）
- **heavy**：重度降级，极端情况

### 样本数量

修改这一行改变生成的样本数：
```python
n_samples = 2**8  # 改为其他值，如 2**10 = 1024
```

## 📊 输出结构

生成的数据会保存在：
```
./data/syn/
├── 00000000/
│   ├── im0_0.npy          # RGB图像
│   ├── refimg0_0.npy      # 反射率图
│   ├── disp0_0.npy        # 视差图
│   ├── im1_0.npy
│   ├── refimg1_0.npy
│   ├── disp1_0.npy
│   └── ...
├── 00000001/
│   └── ...
└── settings.pkl           # 相机参数
```

## 🎯 关键改进

### 材质参数
每个面都有：
- **roughness**（粗糙度）：0-1，控制高反强度
- **metallic**（金属度）：0-1，控制金属效果
- **specular_boost**（高反强度）：0.5-2.0，倍数控制

### 着色模型
使用改进的Phong着色：
- 菲涅尔效应：高反强度随视线角度增大
- 粗糙度控制：光滑表面高反强，粗糙表面高光分散
- 自适应曝光：防止高反区域过曝

## ⏱️ 预计时间

- 256个样本（2^8）：约30-60分钟
- 1024个样本（2^10）：约2-4小时
- 8192个样本（2^13）：约16-32小时

## 🔍 监控进度

脚本会打印进度：
```
-----------------
进度: 1/256
loading mesh for sample 1/256 took 0.123[s]
xinbo2
Disp Min: 0.5; Disp Max: 2.5. Whole Min: 0.5; Whole Max: 2.5
...
```

## ❌ 如果出错

### 错误：ModuleNotFoundError: No module named 'material_properties'
```bash
# 确保在DHD目录下运行
cd /home/xinbo/Desktop/DHD
python create_syn_data.py
```

### 错误：CUDA out of memory
- 减少样本数量
- 或使用CPU渲染（修改engine='cpu'）

### 错误：找不到ShapeNet模型
- 检查config.json中的SHAPENET_ROOT是否正确

## 📝 下一步

1. **生成数据**
   ```bash
   python create_syn_data.py
   ```

2. **验证数据**
   ```python
   import numpy as np
   data = np.load('./data/syn/00000000/im0_0.npy')
   print(data.shape)  # 应该是 (480, 640, 3)
   ```

3. **训练模型**
   - 使用原方法在原始数据上训练
   - 使用改进方法在改进数据上训练
   - 对比评估

## 💡 提示

- 第一次运行会比较慢（编译CUDA kernel）
- 后续运行会快很多（kernel已缓存）
- 可以在后台运行：`nohup python create_syn_data.py > output.log 2>&1 &`

## 📚 参考文档

- 详细说明：`ADVANCED_RENDERING_GUIDE.md`
- 快速参考：`QUICK_REFERENCE.md`
- 技术总结：`ADVANCED_RENDERING_SUMMARY.md`
