"""Converts Real-ESRGAN's realesr-general-x4v3.pth (SRVGGNetCompact) to ONNX without PyTorch.

    python convert-realesrgan.py realesr-general-x4v3.pth realesr-general-x4v3.onnx

Needs only `numpy` and `onnx`. The checkpoint is a zip of a pickled state dict; tensors are read
straight from its storage files. The network is rebuilt node for node:
conv(3->64), PReLU, 32 x [conv(64->64), PReLU], conv(64->48), pixel shuffle x4, plus the input
upsampled x4 by nearest neighbour. Input and output are RGB in 0..1, NCHW, any size.
"""
import pickle
import sys
import zipfile

import numpy as np
import onnx
from onnx import TensorProto, helper, numpy_helper

DTYPES = {'FloatStorage': np.float32, 'HalfStorage': np.float16, 'LongStorage': np.int64}


def load_state(path):
    archive = zipfile.ZipFile(path)
    prefix = archive.namelist()[0].split('/')[0]

    class Storage:
        def __init__(self, kind, key):
            self.data = np.frombuffer(archive.read(f'{prefix}/data/{key}'), DTYPES[kind])

    def rebuild(storage, offset, size, stride, *rest):
        item = storage.data.itemsize
        return np.lib.stride_tricks.as_strided(
            storage.data[offset:], shape=size, strides=[s * item for s in stride]).copy()

    class Unpickler(pickle.Unpickler):
        def find_class(self, module, name):
            if name == '_rebuild_tensor_v2':
                return rebuild
            if module == 'torch' and name in DTYPES:
                return name
            if module == 'collections' and name == 'OrderedDict':
                import collections
                return collections.OrderedDict
            raise pickle.UnpicklingError(f'unexpected {module}.{name}')

        def persistent_load(self, pid):
            _, kind, key, _, _ = pid
            return Storage(kind, key)

    state = Unpickler(archive.open(f'{prefix}/data.pkl')).load()
    return state.get('params_ema', state.get('params', state))


def convert(source, target):
    state = load_state(source)
    layers = sorted({int(k.split('.')[1]) for k in state if k.startswith('body.')})
    nodes, weights = [], []
    current = 'image'
    for i in layers:
        w = state[f'body.{i}.weight'].astype(np.float32)
        name = f'body{i}'
        if w.ndim == 4:
            weights += [numpy_helper.from_array(w, name + '_w'),
                        numpy_helper.from_array(state[f'body.{i}.bias'].astype(np.float32), name + '_b')]
            nodes.append(helper.make_node('Conv', [current, name + '_w', name + '_b'], [name],
                                          kernel_shape=[3, 3], pads=[1, 1, 1, 1]))
        else:  # PReLU slope per channel
            weights.append(numpy_helper.from_array(w.reshape(-1, 1, 1), name + '_slope'))
            nodes.append(helper.make_node('PRelu', [current, name + '_slope'], [name]))
        current = name
    nodes.append(helper.make_node('DepthToSpace', [current], ['shuffled'], blocksize=4, mode='CRD'))
    weights.append(numpy_helper.from_array(np.array([1, 1, 4, 4], np.float32), 'scales'))
    nodes.append(helper.make_node('Resize', ['image', '', 'scales'], ['base'], mode='nearest'))
    nodes.append(helper.make_node('Add', ['shuffled', 'base'], ['upscaled']))
    graph = helper.make_graph(
        nodes, 'realesr-general-x4v3',
        [helper.make_tensor_value_info('image', TensorProto.FLOAT, [1, 3, 'height', 'width'])],
        [helper.make_tensor_value_info('upscaled', TensorProto.FLOAT,
                                       [1, 3, 'height4', 'width4'])], weights)
    model = helper.make_model(graph, opset_imports=[helper.make_opsetid('', 13)],
                              producer_name='cutlery convert-realesrgan.py')
    model.ir_version = 8
    onnx.checker.check_model(model)
    onnx.save(model, target)


if __name__ == '__main__':
    convert(sys.argv[1], sys.argv[2])
