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


## Camera Parameters (`campara`)

The file `para/campara.pkl` stores the calibration parameters for the structured-light system, which consists of one IR camera and one IR pattern projector that share the same lens focal length but are mounted at a known horizontal offset (the **baseline**).

| Parameter | Value | Description |
|-----------|-------|-------------|
| `K` | 3×3 matrix (fx=fy≈797.4, cx≈694.7, cy≈364.7) | Shared intrinsic matrix of camera and projector |
| `baseline` | ≈ 0.1233 m | Physical horizontal distance between camera and projector optical centers |
| `shiftcamera` | 83 px | Principal-point x-offset for the **camera** (see below) |
| `shiftpattern` | −70 px | Principal-point x-offset for the **projector** (see below) |

### Why `shiftcamera` and `shiftpattern`?

The camera and projector share the same focal length `f = K[0,0]` but their sensors are not perfectly centered on the shared optical axis.  
`shiftcamera` and `shiftpattern` encode how much each device's principal point is shifted **relative to the nominal centre `K[0,2]`**, measured in pixels:

```
cx_camera   = K[0,2] − shiftcamera   =  694.7 − 83   =  611.7 px
cx_projector = K[0,2] − shiftpattern  =  694.7 − (−70) = 764.7 px
cy           = K[1,2]                 ≈  364.7 px  (same for both)
```

---

## Model Formulas

### 1 · Camera Model (pinhole)

For a 3-D scene point **P** = (X, Y, Z) expressed in the **camera** coordinate frame,
the projection onto the camera image is:

```
u_cam = f · X/Z + cx_camera   =  f · X/Z + (K[0,2] − shiftcamera)
v_cam = f · Y/Z + cy
```

The renderer builds the `Camera` struct with `(fx, fy, px=cx_camera, py=cy, R, t, width, height)`
and uses these equations to cast rays and to back-project 3-D points (see `renderer/render/render.h`).

### 2 · Projector Model

The projector is modelled as a **second pinhole camera** with:
* the same focal length `f`;
* a different principal point `cx_projector = K[0,2] − shiftpattern`;
* a position offset by `−baseline` along the world x-axis relative to the camera:

```
t_projector = t_camera + [−baseline, 0, 0]
```

The projection of a scene point **P** = (X, Y, Z) in camera coordinates onto the
**projector** image is therefore:

```
u_proj = f · (X + baseline)/Z + cx_projector
       = f · (X + baseline)/Z + (K[0,2] − shiftpattern)
v_proj = f · Y/Z + cy
```

### 3 · Disparity Formula

Combining the camera and projector projection equations above gives the
horizontal disparity `d` (projector pixel minus camera pixel) for a point at
depth Z:

```
d  =  u_proj − u_cam
   =  f · baseline / Z  +  (cx_projector − cx_camera)
   =  f · baseline / Z  −  shiftcamera  +  shiftpattern
```

In code (`create_syn_data.py`, line 264):
```python
disp = baseline * fl / depth - shiftcameras + shiftpatterns
```
where `fl` and `shiftcameras/shiftpatterns` are the scale-adjusted focal length and shifts.  
Inverting gives depth from a measured disparity:

```
Z  =  f · baseline / (d + shiftcamera − shiftpattern)
```

### 4 · Rendering Model

The rendered pixel colour **C** at image location (h, w) is computed by:

1. **Ray casting**: shoot a ray from the camera centre through pixel (h, w) and find
   the intersection point **P** on the mesh.
2. **Back-projection to projector**: project **P** onto the projector image to obtain
   the illuminated pattern value `p(u_proj, v_proj)`.
3. **Spectral shading** (Phong): compute the shading coefficient

   ```
   r̂  = 2(n̂ · l̂) n̂ − l̂          (reflection of l̂ about n̂)
   shading = ka + kd · (n̂ · l̂) + ks · (r̂ · v̂)^α
   ```

   where **n̂** is the surface normal, **l̂** is the unit direction from the
   surface point toward the projector, **v̂** is the unit direction from the
   surface point toward the camera, **r̂** is the mirror-reflection of **l̂**
   about **n̂**, and (ka, kd, ks, α) are the Phong parameters
   (see `renderer/render/geometry.h`, `reflectance_phong`).

4. **Spectral integration**: integrate over wavelengths λ to obtain the RGB colour
   for camera channel c ∈ {R, G, B}:

   ```
   C_c = shading · Σ_λ  reflectance(λ) · illumination_ch(λ) · p_ch(u_proj, v_proj) · sensitivity_c(λ)
   ```

   where:
   * `reflectance(λ)` – spectral reflectance of the surface material at wavelength λ
   * `illumination_ch(λ)` – spectral power of the projector's ch-th colour channel
   * `p_ch(u_proj, v_proj)` – bilinearly-interpolated pattern intensity for channel ch
   * `sensitivity_c(λ)` – spectral sensitivity of camera channel c

5. **Distance decay**: the irradiance falls off with the Euclidean distance
   between the projector and the surface point (linear 1/d approximation,
   consistent with the implementation in `render.h`):

   ```
   C_c  ←  C_c / ‖t_projector − P‖
   ```

   Note: this is a simplified linear decay (1/d) rather than the
   physically exact inverse-square law (1/d²); it is chosen to match the
   sensor behaviour of the real device used for calibration.

The full implementation is in `renderer/render/render.h` (`RenderProjectorFunctor`).

---

## Acknowledgement
The code structure and some code snippets (rasterisation, shading, etc.) are borrowed from [Connecting the Dots](https://github.com/autonomousvision/connecting_the_dots).
Thanks for this great project.
