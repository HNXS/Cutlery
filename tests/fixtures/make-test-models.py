# whisper-for-tests-tiny.bin is whisper.cpp's models/for-tests-ggml-tiny.bin (MIT, v1.9.4): a
# loadable model without trained weights, for running speech recognition end to end.
#
# Regenerates the stand-in models used to test the cutlery-ai worker end to end without the real
# models. Requires the `onnx` Python package. Run in this folder.
#  - red-matte.onnx: [1,3,32,32] -> [1,1,32,32], the matte is the input's red channel.
#  - nearest-x2.onnx: [1,3,H,W] -> [1,3,2H,2W], nearest-neighbour upscale.
#  - half-spectrum.onnx: [B,4,3072,256] -> the same, halved: an MDX-Net stand-in whose "music"
#    is half the mix.
import onnx
from onnx import helper, TensorProto


def save(graph, name):
    model = helper.make_model(graph, opset_imports=[helper.make_opsetid('', 13)],
                              producer_name='cutlery-tests')
    model.ir_version = 8
    onnx.checker.check_model(model)
    onnx.save(model, name)


x = helper.make_tensor_value_info('image', TensorProto.FLOAT, [1, 3, 32, 32])
y = helper.make_tensor_value_info('matte', TensorProto.FLOAT, [1, 1, 32, 32])
consts = [helper.make_tensor('starts', TensorProto.INT64, [1], [0]),
          helper.make_tensor('ends', TensorProto.INT64, [1], [1]),
          helper.make_tensor('axes', TensorProto.INT64, [1], [1])]
node = helper.make_node('Slice', ['image', 'starts', 'ends', 'axes'], ['matte'])
save(helper.make_graph([node], 'red-matte', [x], [y], consts), 'red-matte.onnx')

x = helper.make_tensor_value_info('image', TensorProto.FLOAT, [1, 3, 'h', 'w'])
y = helper.make_tensor_value_info('upscaled', TensorProto.FLOAT, [1, 3, 'h2', 'w2'])
scales = helper.make_tensor('scales', TensorProto.FLOAT, [4], [1, 1, 2, 2])
node = helper.make_node('Resize', ['image', '', 'scales'], ['upscaled'], mode='nearest')
save(helper.make_graph([node], 'nearest-x2', [x], [y], [scales]), 'nearest-x2.onnx')

x = helper.make_tensor_value_info('input', TensorProto.FLOAT, ['batch_size', 4, 3072, 256])
y = helper.make_tensor_value_info('output', TensorProto.FLOAT, ['batch_size', 4, 3072, 256])
half = helper.make_tensor('half', TensorProto.FLOAT, [], [0.5])
node = helper.make_node('Mul', ['input', 'half'], ['output'])
save(helper.make_graph([node], 'half-spectrum', [x], [y], [half]), 'half-spectrum.onnx')
