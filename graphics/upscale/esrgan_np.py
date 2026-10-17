#!/usr/bin/env python3
"""Real-ESRGAN (RRDBNet) 4x upscaling in plain NumPy, no PyTorch: reads the
released .pth weights directly (a zip of pickled tensors) and runs the same
network as basicsr's RRDBNet.  Meant for small art (the title logo), so no
tiling.  Alpha is upscaled through the network too, like Real-ESRGAN does.

  esrgan_np.py MODEL.pth IN.png OUT.png [--blocks 6]
Model: RealESRGAN_x4plus_anime_6B.pth (BSD-3-Clause, xinntao/Real-ESRGAN)."""
import argparse, pickle, zipfile
import numpy as np
from PIL import Image

def load_pth(path):
	z = zipfile.ZipFile(path)
	root = z.namelist()[0].split('/')[0]
	dtypes = {'FloatStorage': np.float32, 'HalfStorage': np.float16, 'DoubleStorage': np.float64, 'LongStorage': np.int64}

	class Storage:
		def __init__(self, dtype, key):
			self.a = np.frombuffer(z.read(f'{root}/data/{key}'), dtype)

	def rebuild(storage, offset, size, stride, *rest):
		if not size:
			return storage.a[offset:offset + 1].reshape(())
		it = storage.a.itemsize
		return np.lib.stride_tricks.as_strided(storage.a[offset:], shape=size, strides=[s * it for s in stride]).copy()

	class U(pickle.Unpickler):
		def find_class(self, mod, name):
			if mod == 'torch._utils' and name == '_rebuild_tensor_v2':
				return rebuild
			if mod == 'torch' and name in dtypes:
				return dtypes[name]
			if mod == 'collections' and name == 'OrderedDict':
				import collections
				return collections.OrderedDict
			raise pickle.UnpicklingError(f'unexpected {mod}.{name}')

		def persistent_load(self, pid):
			_, dtype, key, _loc, _n = pid
			return Storage(dtype, key)

	return U(z.open(f'{root}/data.pkl')).load()

def conv(x, w, b):
	"""3x3 conv, padding 1: x (C,H,W), w (O,C,3,3)."""
	c, h, wd = x.shape
	p = np.pad(x, ((0, 0), (1, 1), (1, 1)))
	out = np.zeros((w.shape[0], h * wd), np.float32)
	for ky in range(3):
		for kx in range(3):
			out += w[:, :, ky, kx] @ np.ascontiguousarray(p[:, ky:ky + h, kx:kx + wd]).reshape(c, -1)
	return (out + b[:, None]).reshape(-1, h, wd)

lrelu = lambda x: np.where(x > 0, x, 0.2 * x)

def rrdbnet(P, x, blocks):
	g = lambda n: (P[n + '.weight'].astype(np.float32), P[n + '.bias'].astype(np.float32))
	feat = conv(x, *g('conv_first'))
	body = feat
	for i in range(blocks):
		inp = body
		for r in (1, 2, 3):
			xs = [body]
			for k in range(1, 5):
				xs.append(lrelu(conv(np.concatenate(xs), *g(f'body.{i}.rdb{r}.conv{k}'))))
			body = conv(np.concatenate(xs), *g(f'body.{i}.rdb{r}.conv5')) * 0.2 + body
		body = body * 0.2 + inp
	feat = feat + conv(body, *g('conv_body'))
	for n in ('conv_up1', 'conv_up2'):
		feat = lrelu(conv(feat.repeat(2, 1).repeat(2, 2), *g(n)))
	return conv(lrelu(conv(feat, *g('conv_hr'))), *g('conv_last'))

def main():
	ap = argparse.ArgumentParser()
	ap.add_argument('model'); ap.add_argument('src'); ap.add_argument('dst')
	ap.add_argument('--blocks', type=int, default=6)
	a = ap.parse_args()
	P = load_pth(a.model)
	P = P.get('params_ema', P.get('params', P))
	im = np.asarray(Image.open(a.src).convert('RGBA'), np.float32) / 255
	rgb = rrdbnet(P, np.ascontiguousarray(im[:, :, :3].transpose(2, 0, 1)), a.blocks)
	alpha = rrdbnet(P, np.repeat(im[None, :, :, 3], 3, 0), a.blocks).mean(0, keepdims=True)
	out = np.clip(np.concatenate([rgb, alpha]).transpose(1, 2, 0), 0, 1)
	Image.fromarray((out * 255 + 0.5).astype(np.uint8), 'RGBA').save(a.dst)

if __name__ == '__main__':
	main()
