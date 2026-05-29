/*
 * scriptprocess.cpp
 * Copyright 2020, David Konsumer <konsumer@jetboystudio.com>
 * Copyright 2020, Thorbjørn Lindeijer <bjorn@lindeijer.nl>
 */

#include "scriptmanager.h"
#include <QJSEngine>
#include <QCoreApplication>

#ifndef Q_OS_WASM
#include <QProcess>
#include <QProcessEnvironment>
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#include <QTextCodec>
#else
#include <QStringConverter>
#endif
#include <memory>
#endif

namespace Tiled {

    class ScriptProcess : public QObject
    {
    Q_OBJECT
        Q_PROPERTY(QString workingDirectory READ workingDirectory WRITE setWorkingDirectory);
        Q_PROPERTY(bool atEnd READ atEnd)
        Q_PROPERTY(int exitCode READ exitCode)
        Q_PROPERTY(QString codec READ codec WRITE setCodec)

    public:
        Q_INVOKABLE ScriptProcess();
        ~ScriptProcess() override;

        Q_INVOKABLE QString getEnv(const QString &name);
        Q_INVOKABLE void setEnv(const QString &name, const QString &value);
        QString codec() const;
        void setCodec(const QString &codec);
        Q_INVOKABLE QString workingDirectory();
        Q_INVOKABLE void setWorkingDirectory(const QString &dir);
        Q_INVOKABLE bool start(const QString &program, const QStringList &arguments = {});
        Q_INVOKABLE int exec(const QString &program, const QStringList &arguments = {}, bool throwOnError = true);
        Q_INVOKABLE void close();
        Q_INVOKABLE bool waitForFinished(int msecs = 30000);
        Q_INVOKABLE void terminate();
        Q_INVOKABLE void kill();
        Q_INVOKABLE QString readLine();
        bool atEnd() const;
        Q_INVOKABLE QString readStdOut();
        Q_INVOKABLE QString readStdErr();
        Q_INVOKABLE void closeWriteChannel();
        Q_INVOKABLE void write(const QString &string);
        Q_INVOKABLE void writeLine(const QString &string);
        int exitCode() const;

    private:
        bool checkForClosed() const;
        QByteArray encode(const QString &string) const;
        QString decode(const QByteArray &bytes) const;

#ifndef Q_OS_WASM
        std::unique_ptr<QProcess> m_process;
        QProcessEnvironment m_environment;
        QString m_workingDirectory;
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        QTextCodec *m_codec;
#else
        QStringConverter::Encoding m_encoding = QStringConverter::System;
#endif
#endif
    };

#ifndef Q_OS_WASM
    ScriptProcess::ScriptProcess()
            : m_process(new QProcess)
            , m_environment(QProcessEnvironment::systemEnvironment())
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    , m_codec(QTextCodec::codecForName("UTF-8"))
#endif
    {}

    ScriptProcess::~ScriptProcess() { close(); }
    QString ScriptProcess::getEnv(const QString &name) { return m_environment.value(name); }
    void ScriptProcess::setEnv(const QString &name, const QString &value) { m_environment.insert(name, value); }
    QString ScriptProcess::codec() const { return QStringLiteral("UTF-8"); }
    void ScriptProcess::setCodec(const QString &) {}
    QString ScriptProcess::workingDirectory() { return m_workingDirectory; }
    void ScriptProcess::setWorkingDirectory(const QString &dir) { m_workingDirectory = dir; }
    bool ScriptProcess::start(const QString &program, const QStringList &arguments) {
        if (checkForClosed()) return false;
        m_process->start(program, arguments); return m_process->waitForStarted();
    }
    int ScriptProcess::exec(const QString &p, const QStringList &a, bool t) { return -1; }
    void ScriptProcess::close() { if(m_process) m_process.reset(); }
    bool ScriptProcess::waitForFinished(int m) { return m_process ? m_process->waitForFinished(m) : false; }
    void ScriptProcess::terminate() { if(m_process) m_process->terminate(); }
    void ScriptProcess::kill() { if(m_process) m_process->kill(); }
    QString ScriptProcess::readLine() { return {}; }
    bool ScriptProcess::atEnd() const { return m_process ? m_process->atEnd() : true; }
    QString ScriptProcess::readStdOut() { return {}; }
    QString ScriptProcess::readStdErr() { return {}; }
    void ScriptProcess::closeWriteChannel() { if(m_process) m_process->closeWriteChannel(); }
    void ScriptProcess::write(const QString &s) { if(m_process) m_process->write(s.toUtf8()); }
    void ScriptProcess::writeLine(const QString &s) { if(m_process) m_process->write(s.toUtf8()); m_process->putChar('\n'); }
    int ScriptProcess::exitCode() const { return m_process ? m_process->exitCode() : -1; }
    bool ScriptProcess::checkForClosed() const { return !m_process; }
    QByteArray ScriptProcess::encode(const QString &s) const { return s.toUtf8(); }
    QString ScriptProcess::decode(const QByteArray &b) const { return QString::fromUtf8(b); }
#else
    ScriptProcess::ScriptProcess() {}
ScriptProcess::~ScriptProcess() {}
QString ScriptProcess::getEnv(const QString &) { return {}; }
void ScriptProcess::setEnv(const QString &, const QString &) {}
QString ScriptProcess::codec() const { return {}; }
void ScriptProcess::setCodec(const QString &) {}
QString ScriptProcess::workingDirectory() { return {}; }
void ScriptProcess::setWorkingDirectory(const QString &) {}
bool ScriptProcess::start(const QString &, const QStringList &) { return false; }
int ScriptProcess::exec(const QString &, const QStringList &, bool) { return -1; }
void ScriptProcess::close() {}
bool ScriptProcess::waitForFinished(int) { return false; }
void ScriptProcess::terminate() {}
void ScriptProcess::kill() {}
QString ScriptProcess::readLine() { return {}; }
bool ScriptProcess::atEnd() const { return true; }
QString ScriptProcess::readStdOut() { return {}; }
QString ScriptProcess::readStdErr() { return {}; }
void ScriptProcess::closeWriteChannel() {}
void ScriptProcess::write(const QString &) {}
void ScriptProcess::writeLine(const QString &) {}
int ScriptProcess::exitCode() const { return -1; }
bool ScriptProcess::checkForClosed() const { return true; }
QByteArray ScriptProcess::encode(const QString &) const { return {}; }
QString ScriptProcess::decode(const QByteArray &) const { return {}; }
#endif

    void registerProcess(QJSEngine *jsEngine)
    {
        jsEngine->globalObject().setProperty(QStringLiteral("Process"),
                                             jsEngine->newQMetaObject<ScriptProcess>());
    }

} // namespace Tiled

#include "scriptprocess.moc"