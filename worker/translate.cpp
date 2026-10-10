// Task "translate": text lines from one language into another with a Marian (Opus-MT) model,
// exported to ONNX as an encoder and a decoder.
//
// The model folder holds encoder_model.onnx, decoder_model.onnx, source.spm (SentencePiece for
// the source language), vocab.json (pieces to ids, shared by both languages) and config.json
// (start, end and padding ids). Each line is split into pieces, encoded once, and decoded word by
// word, always taking the most likely next piece, until the end piece. Empty lines stay empty.
// Runs on the CPU: lines are short and the models are small.
//
//   cutlery-ai translate --model DIR --input IN.txt --output OUT.txt
//   cutlery-ai tokenize --model FILE.spm --input IN.txt --output OUT.txt   (pieces, for tests)
#include "common.h"
#include "sentencepiece.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <array>

namespace {
QStringList readLines(const QString &path, bool &ok) {
    QFile file(path);
    ok = file.open(QIODevice::ReadOnly);
    if (!ok)
        return {};
    auto text = QString::fromUtf8(file.readAll());
    text.replace("\r\n", "\n");
    if (text.endsWith('\n'))
        text.chop(1);
    return text.split('\n');
}
bool writeLines(const QString &path, const QStringList &lines) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write((lines.join('\n') + '\n').toUtf8()) >= 0;
}
// The index of a model input whose name contains `part`.
size_t inputIndex(Ort::Session &session, const char *part, bool &found) {
    Ort::AllocatorWithDefaultOptions allocator;
    for (size_t i = 0; i < session.GetInputCount(); ++i)
        if (std::string(session.GetInputNameAllocated(i, allocator).get()).find(part) !=
            std::string::npos) {
            found = true;
            return i;
        }
    found = false;
    return 0;
}
} // namespace

int worker::tokenize(const QHash<QString, QString> &o) {
    SentencePiece spm;
    if (!spm.load(o.value("model")))
        return fail("cannot read the SentencePiece model");
    bool ok = false;
    const auto lines = readLines(o.value("input"), ok);
    if (!ok)
        return fail("cannot read the input");
    QStringList out;
    for (const auto &line : lines)
        out << spm.encode(line).join(' ');
    return writeLines(o.value("output"), out) ? 0 : fail("cannot write the output");
}

int worker::translate(const QHash<QString, QString> &o) {
    const QString dir = o.value("model"), input = o.value("input"), output = o.value("output");
    if (dir.isEmpty() || input.isEmpty() || output.isEmpty())
        return fail("usage: translate --model DIR --input IN.txt --output OUT.txt");
    SentencePiece spm;
    if (!spm.load(dir + "/source.spm"))
        return fail("cannot read source.spm");
    QFile vocabFile(dir + "/vocab.json"), configFile(dir + "/config.json");
    if (!vocabFile.open(QIODevice::ReadOnly) || !configFile.open(QIODevice::ReadOnly))
        return fail("cannot read vocab.json or config.json");
    const auto vocabJson = QJsonDocument::fromJson(vocabFile.readAll()).object();
    const auto config = QJsonDocument::fromJson(configFile.readAll()).object();
    QHash<QString, qint64> ids;
    QHash<qint64, QString> pieces;
    for (auto it = vocabJson.begin(); it != vocabJson.end(); ++it) {
        ids.insert(it.key(), it.value().toInteger());
        pieces.insert(it.value().toInteger(), it.key());
    }
    const qint64 eos = config.value("eos_token_id").toInteger(0),
                 pad = config.value("pad_token_id").toInteger(qint64(ids.size()) - 1),
                 start = config.value("decoder_start_token_id").toInteger(pad),
                 unknown = ids.value("<unk>", 1);
    bool ok = false;
    const auto lines = readLines(input, ok);
    if (!ok)
        return fail("cannot read the input");

    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "cutlery-ai");
    auto encoder = openModel(env, dir + "/encoder_model.onnx", true);
    auto decoder = openModel(env, dir + "/decoder_model.onnx", true);
    Ort::AllocatorWithDefaultOptions allocator;
    bool found = false;
    const size_t encIds = inputIndex(*encoder, "input_ids", found);
    if (!found)
        return fail("the encoder has no input_ids");
    const size_t encMask = inputIndex(*encoder, "attention_mask", found);
    const bool encHasMask = found;
    const size_t decIds = inputIndex(*decoder, "input_ids", found);
    if (!found)
        return fail("the decoder has no input_ids");
    const size_t decStates = inputIndex(*decoder, "encoder_hidden_states", found);
    if (!found)
        return fail("the decoder has no encoder_hidden_states");
    const size_t decMask = inputIndex(*decoder, "attention_mask", found);
    const bool decHasMask = found;
    auto names = [&](Ort::Session &s, bool inputs) {
        std::vector<Ort::AllocatedStringPtr> owned;
        const size_t count = inputs ? s.GetInputCount() : s.GetOutputCount();
        for (size_t i = 0; i < count; ++i)
            owned.push_back(inputs ? s.GetInputNameAllocated(i, allocator)
                                   : s.GetOutputNameAllocated(i, allocator));
        return owned;
    };
    const auto encIn = names(*encoder, true), encOut = names(*encoder, false),
               decIn = names(*decoder, true), decOut = names(*decoder, false);
    auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    QStringList out;
    progress(0, lines.size());
    for (int index = 0; index < lines.size(); ++index) {
        const auto source = spm.encode(lines[index]);
        if (source.isEmpty()) {
            out << QString();
            progress(index + 1, lines.size());
            continue;
        }
        std::vector<int64_t> tokens;
        for (const auto &piece : source)
            tokens.push_back(ids.value(piece, unknown));
        tokens.push_back(eos);
        const int64_t n = int64_t(tokens.size());
        std::vector<int64_t> mask(size_t(n), 1);
        const std::array<int64_t, 2> shape{1, n};
        // Encoder: the source pieces once.
        std::vector<Ort::Value> encInputs;
        std::vector<const char *> encNames;
        for (size_t i = 0; i < encIn.size(); ++i) {
            encNames.push_back(encIn[i].get());
            if (i == encIds)
                encInputs.push_back(Ort::Value::CreateTensor<int64_t>(memory, tokens.data(), tokens.size(),
                                                                      shape.data(), 2));
            else if (encHasMask && i == encMask)
                encInputs.push_back(Ort::Value::CreateTensor<int64_t>(memory, mask.data(), mask.size(),
                                                                      shape.data(), 2));
            else
                return fail("unexpected encoder input");
        }
        const char *encOutName = encOut[0].get();
        auto states = encoder->Run(Ort::RunOptions{nullptr}, encNames.data(), encInputs.data(),
                                   encInputs.size(), &encOutName, 1);
        // Decoder: one piece at a time, from the start piece, the most likely next one.
        std::vector<int64_t> target{start};
        const int64_t limit = std::min<int64_t>(512, 3 * n + 10);
        QStringList words;
        while (int64_t(target.size()) < limit) {
            const std::array<int64_t, 2> tshape{1, int64_t(target.size())};
            std::vector<Ort::Value> decInputs;
            std::vector<const char *> decNames;
            for (size_t i = 0; i < decIn.size(); ++i) {
                decNames.push_back(decIn[i].get());
                if (i == decIds)
                    decInputs.push_back(Ort::Value::CreateTensor<int64_t>(
                        memory, target.data(), target.size(), tshape.data(), 2));
                else if (i == decStates)
                    decInputs.push_back(std::move(states[0]));
                else if (decHasMask && i == decMask)
                    decInputs.push_back(Ort::Value::CreateTensor<int64_t>(memory, mask.data(),
                                                                          mask.size(), shape.data(), 2));
                else
                    return fail("unexpected decoder input");
            }
            const char *decOutName = decOut[0].get();
            auto logits = decoder->Run(Ort::RunOptions{nullptr}, decNames.data(), decInputs.data(),
                                       decInputs.size(), &decOutName, 1);
            // The encoder states go back for the next step.
            states[0] = std::move(decInputs[decStates]);
            const auto info = logits[0].GetTensorTypeAndShapeInfo().GetShape();
            const int64_t vocab = info.back();
            const float *last = logits[0].GetTensorData<float>() +
                                (int64_t(target.size()) - 1) * vocab;
            int64_t best = -1;
            for (int64_t v = 0; v < vocab; ++v)
                if (v != pad && (best < 0 || last[v] > last[best]))
                    best = v;
            if (best == eos || best < 0)
                break;
            target.push_back(best);
        }
        QString text;
        for (size_t i = 1; i < target.size(); ++i) {
            const auto piece = pieces.value(target[i]);
            if (piece == "<unk>" || piece.startsWith("</") || piece == "<pad>")
                continue;
            text += piece;
        }
        out << text.replace(QChar(0x2581), ' ').simplified();
        progress(index + 1, lines.size());
    }
    return writeLines(output, out) ? 0 : fail("cannot write the translation");
}
