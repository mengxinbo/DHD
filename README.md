# Deep Hyperspectral-Depth Reconstruction Using Single Color-Dot Projection
This is the official repo for the implementation of the **dataset generateion part** of the CVPR2022 paper: Deep Hyperspectral-Depth Reconstruction Using Single Color-Dot Projection.

## Introduction
<div align="center">
<img src=static/dataset.png width=60% />
</div>

Since it is difficult to simultaneously acquire accurate depth and spectral reflectance as a large-scale ground-truth dataset in real-world situations,
we developed a spectral renderer to generate a synthetic dataset with rendered RGB color-dot images, ground-truth disparity maps,
and ground-truth spectral reflectance images by extending the algorithm of a [structured-light renderer](https://github.com/autonomousvision/connecting_the_dots).

## Usage
### Dependencies
The python packages can be installed with `anaconda`:
```
conda install --file requirements.txt
```

### Building
First make sure the correct `CUDA_LIBRARY_PATH` is set in `config.json`.
Afterwards, the renderer can be build by running `make` within the `renderer` directory.

### Running
First, download [ShapeNet V2](https://www.shapenet.org/) and change `SHAPENET_ROOT` in `config.json`.
Then the data can be generated and saved to `DATA_ROOT` in `config.json` by running
```
python create_syn_data.py
```

### Example
The generated dataset example including 8448 scenes for training (458.6GB) and 256 scenes for testing (13.9GB) can be downloaded [here](https://www.dropbox.com/sh/o8r0sv7vdcitqpd/AAA3TXbXMxdTfBfT1RNbkvBxa?dl=0).


## 参数说明与公式推导（中文）

### 参数含义

| 参数 | 说明 |
|------|------|
| **baseline** | 相机与投影仪光心之间的水平基线距离（单位：米）。在世界坐标系中，投影仪的平移向量为 $\mathbf{t}_\text{proj} = \mathbf{t}_\text{cam} + [-B,\,0,\,0]^\top$，其中 $B$ 即 `baseline`。 |
| **shiftcamera** | 相机主点（光轴与像平面的交点）相对于共享内参主点 $c_x$ 的水平偏移量（单位：像素）。相机实际使用的主点横坐标为 $p_{x,\text{cam}} = c_x - s_\text{cam}$，其中 $s_\text{cam}$ 即 `shiftcamera`。由于相机与投影仪传感器的物理位置不同，两者的主点偏移也不同。 |
| **shiftpattern** | 投影仪主点相对于共享内参主点 $c_x$ 的水平偏移量（单位：像素）。投影仪实际使用的主点横坐标为 $p_{x,\text{proj}} = c_x - s_\text{pat}$，其中 $s_\text{pat}$ 即 `shiftpattern`。 |

---

### 相机-投影仪模型（视差公式）

设共享焦距为 $f$，基线为 $B$，空间点在相机坐标系下的坐标为 $(X, Y, Z)$。

**相机内参矩阵**（共用）：

$$K = \begin{pmatrix} f & 0 & c_x \\ 0 & f & c_y \\ 0 & 0 & 1 \end{pmatrix}$$

**相机实际主点**：

$$p_{x,\text{cam}} = c_x - s_\text{cam}, \quad p_{y} = c_y$$

**投影仪实际主点**：

$$p_{x,\text{proj}} = c_x - s_\text{pat}$$

**相机投影**（空间点投影到相机像素坐标）：

$$u_\text{cam} = f \cdot \frac{X}{Z} + p_{x,\text{cam}}, \quad v_\text{cam} = f \cdot \frac{Y}{Z} + p_y$$

**投影仪投影**（投影仪位于相机左侧 $B$ 处，即 $X_\text{proj} = X - B$）：

$$u_\text{proj} = f \cdot \frac{X - B}{Z} + p_{x,\text{proj}}, \quad v_\text{proj} = f \cdot \frac{Y}{Z} + p_y$$

**水平视差**（disparity）：

$$d = u_\text{cam} - u_\text{proj}
  = f \cdot \frac{X}{Z} + p_{x,\text{cam}} - f \cdot \frac{X-B}{Z} - p_{x,\text{proj}}
  = \frac{f \cdot B}{Z} - s_\text{cam} + s_\text{pat}$$

对应代码中的计算：
```python
disp = baseline * fl / depth - shiftcameras + shiftpatterns
```

**由视差恢复深度**：

$$Z = \frac{f \cdot B}{d + s_\text{cam} - s_\text{pat}}$$

---

### 渲染公式（光谱渲染）

对于相机图像的第 $c$ 个通道（$c \in \{\text{R},\text{G},\text{B}\}$），像素 $(x, y)$ 的渲染值为：

$$I_c(x,y) = \frac{\sigma(\mathbf{n},\,\mathbf{l})}{\left\|\mathbf{p} - \mathbf{o}_p\right\|^2}
  \sum_{k=1}^{3} P_k\!\left(u_p,\,v_p\right)
  \sum_{\lambda} \rho(\lambda)\cdot E_k(\lambda)\cdot S_c(\lambda)$$

各符号说明：

| 符号 | 说明 |
|------|------|
| $\sigma(\mathbf{n}, \mathbf{l})$ | Phong 着色项（漫反射 + 镜面反射），$\mathbf{n}$ 为表面法向量，$\mathbf{l}$ 为入射光方向 |
| $\mathbf{p}$ | 三维曲面点的世界坐标 |
| $\mathbf{o}_p$ | 投影仪光心坐标 |
| $\|\mathbf{p} - \mathbf{o}_p\|^2$ | 投影仪到曲面点的距离平方（光强随距离衰减） |
| $P_k(u_p, v_p)$ | 投影图案在投影仪像素 $(u_p, v_p)$ 处第 $k$ 通道（R/G/B）的强度（双线性插值） |
| $E_k(\lambda)$ | 光源（投影仪）第 $k$ 通道的光谱功率分布（illumination） |
| $\rho(\lambda)$ | 曲面材质在波长 $\lambda$ 处的光谱反射率（reflectance） |
| $S_c(\lambda)$ | 相机第 $c$ 通道在波长 $\lambda$ 处的光谱灵敏度（camerasensitivity） |

对应代码中的核心计算：
```cpp
// 对三个图案通道 k 求和，再对每个相机通道 c 求光谱内积
out_color[c] += P_k(u_p, v_p) * vec_dot_3in(rho, E_k, S_c, wavelength);
// 乘以着色项
out_color[c] *= shading;
// 除以距离平方（衰减）
out_color[c] /= dist_squared;
```

---

## Acknowledgement
The code structure and some code snippets (rasterisation, shading, etc.) are borrowed from [Connecting the Dots](https://github.com/autonomousvision/connecting_the_dots).
Thanks for this great project.
