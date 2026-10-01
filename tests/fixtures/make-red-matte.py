# Regenerates red-matte.onnx: a stand-in segmentation model whose matte is the input's red
# channel, so the cutlery-matte worker can be tested end to end without the real model.
# Requires the `onnx` Python package.
import onnx
from onnx import helper, TensorProto

x = helper.make_tensor_value_info('image', TensorProto.FLOAT, [1, 3, 32, 32])
y = helper.make_tensor_value_info('matte', TensorProto.FLOAT, [1, 1, 32, 32])
starts = helper.make_tensor('starts', TensorProto.INT64, [1], [0])
ends = helper.make_tensor('ends', TensorProto.INT64, [1], [1])
axes = helper.make_tensor('axes', TensorProto.INT64, [1], [1])
node = helper.make_node('Slice', ['image', 'starts', 'ends', 'axes'], ['matte'])
graph = helper.make_graph([node], 'red-matte', [x], [y], [starts, ends, axes])
model = helper.make_model(graph, opset_imports=[helper.make_opsetid('', 13)],
                          producer_name='cutlery-tests')
model.ir_version = 8
onnx.checker.check_model(model)
onnx.save(model, 'red-matte.onnx')
