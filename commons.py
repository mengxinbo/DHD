import numpy as np
import cv2


def get_patterns(pattern_path, imsizes, pattern_crop=False):
  """Load a structured-light pattern image and resize it to multiple scales.

  Parameters
  ----------
  pattern_path : str or Path
      Path to the pattern PNG image.
  imsizes : list of (int, int)
      List of (height, width) tuples for the desired output sizes.
  pattern_crop : bool
      Unused flag kept for API compatibility. The cropping is assumed to have
      been applied to the source image already.

  Returns
  -------
  list of numpy.ndarray
      One uint8 array of shape (H, W, C) per entry in *imsizes*.
  """
  pattern = cv2.imread(str(pattern_path))
  if pattern is None:
    raise ValueError(f'Could not load pattern image from {pattern_path}')
  pattern = cv2.cvtColor(pattern, cv2.COLOR_BGR2RGB)

  patterns = []
  for height, width in imsizes:
    resized = cv2.resize(pattern, (width, height), interpolation=cv2.INTER_AREA)
    patterns.append(resized.astype(np.uint8))
  return patterns


def get_rotation_matrix(a, b):
  """Return the rotation matrix that rotates unit vector *a* onto unit vector *b*.

  Uses the axis-angle (Rodrigues) formula.

  Parameters
  ----------
  a, b : array_like, shape (3,)
      Source and target direction vectors (need not be normalised).

  Returns
  -------
  numpy.ndarray, shape (3, 3), dtype float32
  """
  a = np.asarray(a, dtype=np.float32)
  b = np.asarray(b, dtype=np.float32)
  a = a / np.linalg.norm(a)
  b = b / np.linalg.norm(b)

  v = np.cross(a, b)
  c = float(np.dot(a, b))
  s = float(np.linalg.norm(v))

  if s < 1e-10:
    if c > 0:
      # vectors are already aligned
      return np.eye(3, dtype=np.float32)
    else:
      # vectors are anti-parallel: rotate 180 degrees around an arbitrary perpendicular axis
      perp = np.array([1, 0, 0], dtype=np.float32)
      if abs(np.dot(a, perp)) > 0.9:
        perp = np.array([0, 1, 0], dtype=np.float32)
      axis = np.cross(a, perp)
      axis = axis / np.linalg.norm(axis)
      vx = np.array([[ 0,       -axis[2],  axis[1]],
                     [ axis[2],  0,       -axis[0]],
                     [-axis[1],  axis[0],  0      ]], dtype=np.float32)
      return (np.eye(3, dtype=np.float32) + 2.0 * (vx @ vx)).astype(np.float32)

  vx = np.array([[ 0,    -v[2],  v[1]],
                 [ v[2],  0,    -v[0]],
                 [-v[1],  v[0],  0   ]], dtype=np.float32)
  R = np.eye(3, dtype=np.float32) + vx + (vx @ vx) * ((1.0 - c) / (s * s))
  return R.astype(np.float32)
