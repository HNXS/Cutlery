#pragma once
#include <QObject>
#include <QVariantList>
namespace cutlery {
class KeyboardShortcuts final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList bindings READ bindings NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
  public:
    explicit KeyboardShortcuts(QString path, QObject *parent = nullptr);
    QVariantList bindings() const;
    QString error() const {
        return m_error;
    }
    Q_INVOKABLE bool assign(const QString &command, const QString &sequence);
    Q_INVOKABLE bool reset();
  signals:
    void changed();

  private:
    struct Binding {
        QString id, label, category, standard, sequence;
    };
    QVector<Binding> m_bindings;
    QString m_path, m_error;
    bool persist(const QVector<Binding> &bindings);
    static QString normalize(const QString &sequence);
};
} // namespace cutlery
