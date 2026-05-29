#include "command.h"

#include "actionmanager.h"
#include "commandmanager.h"
#include "documentmanager.h"
#include "logginginterface.h"
#include "mapdocument.h"
#include "mapobject.h"
#include "projectmanager.h"
#include "worldmanager.h"

#include <QAction>
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>
#include <QUndoStack>

#ifndef Q_OS_WASM
#include <QProcess>
#include <QTemporaryFile>
#endif

using namespace Tiled;

namespace Tiled {

#ifndef Q_OS_WASM

    class CommandProcess : public QProcess
    {
    Q_OBJECT
    public:
        CommandProcess(const Command &command, bool inTerminal = false, bool showOutput = true);
    private:
        void consoleOutput();
        void consoleError();
        void handleProcessError(QProcess::ProcessError);
        void reportErrorAndDelete(const QString &);
        QString mName;
        QString mFinalCommand;
#ifdef Q_OS_MAC
        QTemporaryFile mFile;
#endif
    };

#endif // Q_OS_WASM


// --- Shared Helper (Available to all platforms) ---
    static QString replaceVariables(const QString &string, bool quoteValues = true)
    {
        QString finalString = string;
        QString replaceString = quoteValues ? QStringLiteral("\"%1\"") :
                                QStringLiteral("%1");

        // Perform variable replacement
        if (Document *document = DocumentManager::instance()->currentDocument()) {
            const QString &fileName = document->fileName();
            QFileInfo fileInfo(fileName);
            const QString mapPath = fileInfo.absolutePath();
            const QString projectPath = QFileInfo(ProjectManager::instance()->project().fileName()).absolutePath();

            finalString.replace(QLatin1String("%mapfile"), replaceString.arg(fileName));
            finalString.replace(QLatin1String("%mappath"), replaceString.arg(mapPath));
            finalString.replace(QLatin1String("%projectpath"), replaceString.arg(projectPath));

            if (MapDocument *mapDocument = qobject_cast<MapDocument*>(document)) {
                if (const Layer *layer = mapDocument->currentLayer()) {
                    finalString.replace(QLatin1String("%layername"),
                                        replaceString.arg(layer->name()));
                }
            } else if (TilesetDocument *tilesetDocument = qobject_cast<TilesetDocument*>(document)) {
                QStringList selectedTileIds;
                for (const Tile *tile : tilesetDocument->selectedTiles())
                    selectedTileIds.append(QString::number(tile->id()));

                finalString.replace(QLatin1String("%tileid"),
                                    replaceString.arg(selectedTileIds.join(QLatin1Char(','))));
            }

            if (const MapObject *currentObject = dynamic_cast<MapObject *>(document->currentObject())) {
                finalString.replace(QLatin1String("%objecttype"),
                                    replaceString.arg(currentObject->className()));
                finalString.replace(QLatin1String("%objectclass"),
                                    replaceString.arg(currentObject->className()));
                finalString.replace(QLatin1String("%objectid"),
                                    replaceString.arg(currentObject->id()));
            }

            if (auto worldDocument = WorldManager::instance().worldForMap(fileName)) {
                finalString.replace(QLatin1String("%worldfile"), replaceString.arg(worldDocument->fileName()));
            }
        }

        return finalString;
    }

// --- Shared Method Implementations ---

    QString Command::finalWorkingDirectory() const
    {
        QString finalWorkingDirectory = replaceVariables(workingDirectory, false);
        QString finalExecutable = replaceVariables(executable);
        QFileInfo mFile(finalExecutable);

        if (!mFile.exists())
            mFile = QFileInfo(QStandardPaths::findExecutable(finalExecutable));

        finalWorkingDirectory.replace(QLatin1String("%executablepath"),
                                      mFile.absolutePath());

        return finalWorkingDirectory;
    }

    QString Command::finalCommand() const
    {
        QString exe = executable.trimmed();
        if (!exe.startsWith(QLatin1Char('"')) && !exe.startsWith(QLatin1Char('\'')))
            exe.prepend(QLatin1Char('"')).append(QLatin1Char('"'));

        QString finalCommand = QStringLiteral("%1 %2").arg(exe, arguments);
        return replaceVariables(finalCommand);
    }

    QVariantHash Command::toVariant() const
    {
        return QVariantHash {
                { QStringLiteral("arguments"), arguments },
                { QStringLiteral("command"), executable },
                { QStringLiteral("enabled"), isEnabled },
                { QStringLiteral("name"), name },
                { QStringLiteral("saveBeforeExecute"), saveBeforeExecute },
                { QStringLiteral("shortcut"), shortcut },
                { QStringLiteral("showOutput"), showOutput },
                { QStringLiteral("workingDirectory"), workingDirectory },
        };
    }

    Command Command::fromVariant(const QVariant &variant)
    {
        const auto hash = variant.toHash();
        auto read = [&] (const QString &prop) {
            if (hash.contains(prop)) return hash.value(prop);
            QString oldProp = prop.at(0).toUpper() + prop.mid(1);
            return hash.value(oldProp);
        };

        Command command;
        command.arguments = read(QStringLiteral("arguments")).toString();
        command.isEnabled = read(QStringLiteral("enabled")).toBool();
        command.executable = read(QStringLiteral("command")).toString();
        command.name = read(QStringLiteral("name")).toString();
        command.saveBeforeExecute = read(QStringLiteral("saveBeforeExecute")).toBool();
        command.shortcut = read(QStringLiteral("shortcut")).value<QKeySequence>();
        command.showOutput = read(QStringLiteral("showOutput")).toBool();
        command.workingDirectory = read(QStringLiteral("workingDirectory")).toString();
        return command;
    }

    void Command::execute(bool inTerminal) const
    {
#ifndef Q_OS_WASM
        if (saveBeforeExecute) {
            ActionManager::instance()->action("Save")->trigger();
            if (Document *document = DocumentManager::instance()->currentDocument()) {
                if (document->type() == Document::MapDocumentType) {
                    if (auto worldDocument = WorldManager::instance().worldForMap(document->fileName()))
                        DocumentManager::instance()->saveDocument(worldDocument.data());
                }
            }
        }
        new CommandProcess(*this, inTerminal, showOutput);
#else
        Tiled::ERROR(QObject::tr("Command execution is not supported on WebAssembly."));
#endif
    }


#ifndef Q_OS_WASM

// --- Platform Specific Logic ---

    CommandProcess::CommandProcess(const Command &command, bool inTerminal, bool showOutput)
            : QProcess(DocumentManager::instance())
            , mName(command.name)
            , mFinalCommand(command.finalCommand())
#ifdef Q_OS_MAC
    , mFile(QDir::tempPath() + QLatin1String("/tiledXXXXXX.command"))
#endif
    {
        if (mFinalCommand.trimmed().isEmpty()) {
            handleProcessError(QProcess::FailedToStart);
            return;
        }

        if (inTerminal) {
#ifdef Q_OS_LINUX
            static bool hasGnomeTerminal = QProcess::execute(QLatin1String("which"),
                                                         QStringList(QLatin1String("gnome-terminal"))) == 0;
        if (hasGnomeTerminal)
            mFinalCommand = QLatin1String("gnome-terminal -x ") + mFinalCommand;
        else
            mFinalCommand = QLatin1String("xterm -e ") + mFinalCommand;
#elif defined(Q_OS_MAC)
            if (!mFile.open()) {
            reportErrorAndDelete(tr("Unable to create/open %1").arg(mFile.fileName()));
            return;
        }
        mFile.write(mFinalCommand.toLocal8Bit());
        mFile.close();
        int chmodRet = QProcess::execute(QStringLiteral("chmod"), { QStringLiteral("+x"), mFile.fileName() });
        if (chmodRet != 0) {
            reportErrorAndDelete(tr("Unable to add executable permissions to %1").arg(mFile.fileName()));
            return;
        }
        mFinalCommand = QStringLiteral("open -W -n \"%1\"").arg(mFile.fileName());
#endif
        }

        connect(this, &QProcess::errorOccurred, this, &CommandProcess::handleProcessError);
        connect(this, &QProcess::finished, this, &QObject::deleteLater);

        if (showOutput) {
            Tiled::INFO(tr("Executing: %1").arg(mFinalCommand));
            connect(this, &QProcess::readyReadStandardError, this, &CommandProcess::consoleError);
            connect(this, &QProcess::readyReadStandardOutput, this, &CommandProcess::consoleOutput);
        }

        const QString finalWorkingDirectory = command.finalWorkingDirectory();
        if (!finalWorkingDirectory.trimmed().isEmpty())
            setWorkingDirectory(finalWorkingDirectory);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        QStringList args = QProcess::splitCommand(mFinalCommand);
    const QString executable = args.takeFirst();
    start(executable, args);
#else
        startCommand(mFinalCommand);
#endif
    }

    void CommandProcess::consoleOutput() { Tiled::INFO(QString::fromLocal8Bit(readAllStandardOutput())); }
    void CommandProcess::consoleError() { Tiled::ERROR(QString::fromLocal8Bit(readAllStandardError())); }

    void CommandProcess::handleProcessError(QProcess::ProcessError error)
    {
        QString errorStr;
        switch (error) {
            case QProcess::FailedToStart: errorStr = tr("The command failed to start."); break;
            case QProcess::Crashed: errorStr = tr("The command crashed."); break;
            case QProcess::Timedout: errorStr = tr("The command timed out."); break;
            default: errorStr = tr("An unknown error occurred.");
        }
        reportErrorAndDelete(errorStr);
    }

    void CommandProcess::reportErrorAndDelete(const QString &error)
    {
        const QString title = tr("Error Executing %1").arg(mName);
        const QString message = error + QLatin1String("\n\n") + mFinalCommand;
        QWidget *parent = DocumentManager::instance()->widget();
        QMessageBox::warning(parent, title, message);
        deleteLater();
    }

#endif // Q_OS_WASM

} // namespace Tiled

#include "command.moc"