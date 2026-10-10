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

# translate-copy/: a Marian-shaped stand-in "translator" whose decoder repeats the source pieces,
# so a translation gives the input back (normalised). source.spm is a small SentencePiece unigram
# model trained on Cutlery's documentation; vocab.json and config.json follow Marian's layout
# (</s> 0, <unk> 1, <pad> last). Needs the `sentencepiece` Python package as well.
import json
import sentencepiece

spm = sentencepiece.SentencePieceProcessor(model_file='translate-copy/source.spm')
pieces = [spm.id_to_piece(i) for i in range(spm.get_piece_size())
          if not (spm.is_unknown(i) or spm.is_control(i))]
vocab = {'</s>': 0, '<unk>': 1}
for piece in pieces:
    vocab.setdefault(piece, len(vocab))
vocab['<pad>'] = len(vocab)
size = len(vocab)
with open('translate-copy/vocab.json', 'w', encoding='utf-8') as f:
    json.dump(vocab, f, ensure_ascii=False)
with open('translate-copy/config.json', 'w') as f:
    json.dump({'eos_token_id': 0, 'pad_token_id': size - 1, 'decoder_start_token_id': size - 1}, f)

ids = helper.make_tensor_value_info('input_ids', TensorProto.INT64, [1, 'n'])
mask = helper.make_tensor_value_info('attention_mask', TensorProto.INT64, [1, 'n'])
states = helper.make_tensor_value_info('last_hidden_state', TensorProto.FLOAT, [1, 'n', 1])
axes2 = helper.make_tensor('axes2', TensorProto.INT64, [1], [2])
nodes = [helper.make_node('Cast', ['input_ids'], ['f'], to=TensorProto.FLOAT),
         helper.make_node('Unsqueeze', ['f', 'axes2'], ['last_hidden_state'])]
save(helper.make_graph(nodes, 'copy-encoder', [ids, mask], [states], [axes2]),
     'translate-copy/encoder_model.onnx')

target = helper.make_tensor_value_info('input_ids', TensorProto.INT64, [1, 't'])
hidden = helper.make_tensor_value_info('encoder_hidden_states', TensorProto.FLOAT, [1, 'n', 1])
emask = helper.make_tensor_value_info('encoder_attention_mask', TensorProto.INT64, [1, 'n'])
logits = helper.make_tensor_value_info('logits', TensorProto.FLOAT, [1, 't', size])
consts = [helper.make_tensor('axes2', TensorProto.INT64, [1], [2]),
          helper.make_tensor('axes1', TensorProto.INT64, [1], [1]),
          helper.make_tensor('one', TensorProto.INT64, [], [1]),
          helper.make_tensor('depth', TensorProto.INT64, [], [size]),
          helper.make_tensor('onoff', TensorProto.FLOAT, [2], [0, 1]),
          helper.make_tensor('start1', TensorProto.INT64, [1], [1]),
          helper.make_tensor('end2', TensorProto.INT64, [1], [2]),
          helper.make_tensor('lead', TensorProto.INT64, [1], [1]),
          helper.make_tensor('vocab', TensorProto.INT64, [1], [size])]
nodes = [
    # The source piece at the target's length minus one.
    helper.make_node('Shape', ['input_ids'], ['shape']),
    helper.make_node('Slice', ['shape', 'start1', 'end2'], ['tlen']),
    helper.make_node('Squeeze', ['tlen'], ['t']),
    helper.make_node('Sub', ['t', 'one'], ['last']),
    helper.make_node('Squeeze', ['encoder_hidden_states', 'axes2'], ['source']),
    helper.make_node('Gather', ['source', 'last'], ['picked'], axis=1),
    helper.make_node('Cast', ['picked'], ['token'], to=TensorProto.INT64),
    helper.make_node('OneHot', ['token', 'depth', 'onoff'], ['hot'], axis=-1),
    helper.make_node('Unsqueeze', ['hot', 'axes1'], ['hot3']),
    helper.make_node('Concat', ['lead', 'tlen', 'vocab'], ['outshape'], axis=0),
    helper.make_node('Expand', ['hot3', 'outshape'], ['logits']),
]
save(helper.make_graph(nodes, 'copy-decoder', [target, hidden, emask], [logits], consts),
     'translate-copy/decoder_model.onnx')
