#include "QsCoreConfig_p.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>

#include <QJsonDocument>
#include <QJsonObject>

#include "QMCoreConsole.h"
#include "QMSystem.h"



static const char SECTION_NAME_DIR[] = "dir";
static const char KEY_NAME_DATA_DIR[] = "data";
static const char KEY_NAME_TEMP_DIR[] = "temp";
static const char KEY_NAME_SHARE_DIR[] = "share";
static const char KEY_NAME_QT_PLUGINS_DIR[] = "qtplugins";
static const char KEY_NAME_QT_TRANSLATIONS_DIR[] = "translations";
static const char KEY_NAME_QS_PLUGINS_DIR[] = "qsplugins";
static const char KEY_NAME_BINTOOL_DIR[] = "tools";

static const char SECTION_NAME_PLUGINS[] = "plugins";
static const char KEY_NAME_AUDIO_DECODER[] = "audioDecoder";
static const char KEY_NAME_AUDIO_ENCODER[] = "audioEncoder";
static const char KEY_NAME_AUDIO_PLAYBACK[] = "audioPlayback";
static const char KEY_NAME_COMPRESS_ENGINE[] = "compressEngine";
static const char KEY_NAME_WINDOW_FACTORY[] = "windowFactory";

static const char DEFAULT_AUDIO_DECODER[] = "FFmpegDecoder";
static const char DEFAULT_AUDIO_ENCODER[] = "FFmpegEncoder";
static const char DEFAULT_AUDIO_PLAYBACK[] = "SDLPlayback";
static const char DEFAULT_COMPRESS_ENGINE[] = "qzlib";
static const char DEFAULT_WINDOW_FACTORY[] = "NativeWindow";

static const char Slash = '/';

QsCoreConfigPrivate::QsCoreConfigPrivate() {
}

QsCoreConfigPrivate::~QsCoreConfigPrivate() {
}

void QsCoreConfigPrivate::init() {
    addInitializer(std::bind(&QsCoreConfigPrivate::initByApp, this));
}

void QsCoreConfigPrivate::initByApp() {
    /* Set basic environments */
    vars.addHash(QMSimpleVarExp::SystemValues());

    /* Set default directories */
    auto wrapDirInfo = [this](const QString &key, const QString &dir, const QString &errorMsg,
                              DirInitArgs::CheckLevel level, bool addLibrary) {
        Q_UNUSED(errorMsg);
        auto funs = addLibrary ? QList<std::function<void(const QString &)>>{QCoreApplication::addLibraryPath}
                               : QList<std::function<void(const QString &)>>{};
        return DirInfo{
            key, vars.parse(dir), dir, DirInitArgs{level, funs, funs}
        };
    };
    dirMap = {
        {QsCoreConfig::AppData,   //
         wrapDirInfo(KEY_NAME_DATA_DIR,         QString("${APPDATA}") + Slash + "ChorusKit" + Slash + qAppName(), "data",
         DirInitArgs::CreateIfNotExist,                                                                                                                                               false)},
        {QsCoreConfig::AppTemp,   //
         wrapDirInfo(KEY_NAME_TEMP_DIR,         QString("${TEMP}") + Slash + "ChorusKit" + Slash + qAppName(),    "temp",
         DirInitArgs::CreateIfNotExist,                                                                                                                                               false)},
        {QsCoreConfig::AppShare,  //
         wrapDirInfo(KEY_NAME_SHARE_DIR,       QString("${BINPATH}") + Slash + "../share",                       "share",     DirInitArgs::OnlyCheck,
         true)                                                                                                                                                                              },
        {QsCoreConfig::QsPlugins, //
         wrapDirInfo(KEY_NAME_QS_PLUGINS_DIR, QString("${BINPATH}") + Slash + "../lib/QsLib/plugins",           "qsplugins",
         DirInitArgs::OnlyCheck,                                                                                                                                                      true) },
    };
    dirTypeMax = QsCoreConfig::UserDir;

    /* Set default plugins */
    auto wrapPluginInfo = [](const QString &key, const QString &catagory, const QString &name) {
        return PluginInfo{key, catagory, name, name};
    };
    pluginMap = {
        {QsCoreConfig::AudioDecoder,   //
         wrapPluginInfo(KEY_NAME_AUDIO_DECODER,     "audiodecoders",   DEFAULT_AUDIO_DECODER)  },
        {QsCoreConfig::AudioEncoder,   //
         wrapPluginInfo(KEY_NAME_AUDIO_ENCODER,     "audioencoders",   DEFAULT_AUDIO_DECODER)  },
        {QsCoreConfig::AudioPlayback,  //
         wrapPluginInfo(KEY_NAME_AUDIO_PLAYBACK,   "audioplaybacks",  DEFAULT_AUDIO_PLAYBACK) },
        {QsCoreConfig::CompressEngine, //
         wrapPluginInfo(KEY_NAME_COMPRESS_ENGINE, "compressengines", DEFAULT_COMPRESS_ENGINE)},
        {QsCoreConfig::WindowFactory, //
         wrapPluginInfo(KEY_NAME_WINDOW_FACTORY,  "windowfactories", DEFAULT_WINDOW_FACTORY) },
    };
    pluginTypeMax = QsCoreConfig::UserPlugin;
}

bool QsCoreConfigPrivate::load_helper(const QString &filename) {
    bool res = true;
    {
        QFile file(filename);
        if (!file.open(QIODevice::ReadOnly)) {
            res = false;
            goto out;
        }

        QByteArray data(file.readAll());
        file.close();

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            res = false;
            goto out;
        }

        QJsonObject objDoc = doc.object();

        QJsonObject::ConstIterator it;

        // Get dirs
        it = objDoc.find(SECTION_NAME_DIR);
        if (it != objDoc.end() && it.value().isObject()) {
            QJsonObject obj = it.value().toObject();
            for (auto it2 = dirMap.begin(); it2 != dirMap.end(); ++it2) {
                DirInfo &info = it2.value();
                auto it3 = obj.find(info.key);
                if (it3 == obj.end() || !it3.value().isString()) {
                    continue;
                }
                info.dir = vars.parse(QDir::fromNativeSeparators(it3.value().toString()));
            }
        }

        // Get plugins
        it = objDoc.find(SECTION_NAME_PLUGINS);
        if (it != objDoc.end() && it.value().isObject()) {
            QJsonObject obj = it.value().toObject();
            for (auto it2 = pluginMap.begin(); it2 != pluginMap.end(); ++it2) {
                PluginInfo &info = it2.value();
                auto it3 = obj.find(info.key);
                if (it3 == obj.end() || !it3.value().isString()) {
                    continue;
                }
                info.name = it3.value().toString();
            }
        }
    }

out:

    // Init when load
    for (auto it = dirMap.begin(); it != dirMap.end(); ++it) {
        const auto &info = it.value();
        const auto &args = info.initArgs;
        QString path = info.dir;
        for (auto it = dirMap.begin(); it != dirMap.end(); ++it) {
            // Traverse dir functions
            for (auto it = args.loadInits.begin(); it != args.loadInits.end(); ++it) {
                (*it)(path);
            }
        }
    }

    return res;
}

bool QsCoreConfigPrivate::apply_helper() {
    for (auto it = dirMap.begin(); it != dirMap.end(); ++it) {
        const auto &info = it.value();
        const auto &args = info.initArgs;
        QString path = info.dir;
        switch (args.level) {
            case DirInitArgs::CreateIfNotExist:
                qDebug() << "coreconfig:" << (QMFs::isDirExist(path) ? "find" : "create") << "directory" << path;
                if (!QMFs::mkDir(path)) {
                    qmCon->MsgBox(nullptr, QMCoreConsole::Critical, qAppName(),
                                  QString("Failed to create %1 directory!").arg(QMFs::PathFindFileName(path)));
                    return false;
                }
                break;
            case DirInitArgs::ErrorIfNotExist:
                qDebug() << "coreconfig: check directory" << path;
                if (!QMFs::isDirExist(path)) {
                    qmCon->MsgBox(nullptr, QMCoreConsole::Critical, qAppName(),
                                  QString("Directory %1 doesn't exist!").arg(QMFs::PathFindFileName(path)));
                    return false;
                }
                break;
            default:
                break;
        }

        // Init when applying
        for (auto it = args.applyInits.begin(); it != args.applyInits.end(); ++it) {
            (*it)(path);
        }
    }
    return true;
}

bool QsCoreConfigPrivate::save_default(const QString &filename) {
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }

    QJsonObject dirsObj;
    for (auto it = dirMap.begin(); it != dirMap.end(); ++it) {
        const auto &info = it.value();
        dirsObj.insert(info.key, info.defaultDir);
    }

    QJsonDocument doc;
    QJsonObject obj{
        {SECTION_NAME_DIR, dirsObj},
        {
         SECTION_NAME_PLUGINS, QJsonObject{//
                        {KEY_NAME_AUDIO_DECODER, DEFAULT_AUDIO_DECODER},
                        {KEY_NAME_AUDIO_ENCODER, DEFAULT_AUDIO_ENCODER},
                        {KEY_NAME_AUDIO_PLAYBACK, DEFAULT_AUDIO_PLAYBACK},
                        {KEY_NAME_COMPRESS_ENGINE, DEFAULT_COMPRESS_ENGINE},
                        {KEY_NAME_WINDOW_FACTORY, DEFAULT_WINDOW_FACTORY}},
         },
    };
    doc.setObject(obj);
    file.write(doc.toJson());
    file.close();

    return true;
}

void QsCoreConfigPrivate::setDefault() {
    for (auto it = dirMap.begin(); it != dirMap.end(); ++it) {
        auto &info = it.value();
        info.dir = vars.parse(info.defaultDir);
    }
    for (auto it = pluginMap.begin(); it != pluginMap.end(); ++it) {
        auto &info = it.value();
        info.name = info.defaultName;
    }
}

void QsCoreConfigPrivate::registerEscapeVar(const QString &key, const QString &val, bool replace) {
    auto it = vars.Variables.find(key);
    if (it != vars.Variables.end()) {
        if (replace) {
            it.value() = val;
        }
    } else {
        vars.Variables.insert(key, val);
    }
}

int QsCoreConfigPrivate::registerUserDir(const QString &key, const QString &dir, const DirInitArgs &args, int hint) {
    dirTypeMax = (hint > dirTypeMax) ? hint : (dirTypeMax + 1);
    dirMap.insert(dirTypeMax, DirInfo{key, dir, dir, args});
    return dirTypeMax;
}

int QsCoreConfigPrivate::registerUserPlugin(const QString &key, const QString &catagory, const QString &name,
                                            int hint) {
    pluginTypeMax = (hint > pluginTypeMax) ? hint : (pluginTypeMax + 1);
    pluginMap.insert(pluginTypeMax, PluginInfo{key, catagory, name, name});
    return pluginTypeMax;
}

void QsCoreConfigPrivate::setDefaultDir(int type, const QString &dir) {
    auto it = dirMap.find(type);
    if (it == dirMap.end()) {
        return;
    }
    DirInfo &info = it.value();
    info.defaultDir = dir;
}

void QsCoreConfigPrivate::setDefaultPluginName(int type, const QString &dir) {
    auto it = pluginMap.find(type);
    if (it == pluginMap.end()) {
        return;
    }
    PluginInfo &info = it.value();
    info.defaultName = dir;
}

void QsCoreConfigPrivate::addInitializer(const std::function<void()> &fun) {
    this->initializers.append(fun);
}

void QsCoreConfigPrivate::initAll() {
    Q_ENTER_ONCE

    for (const auto &fun : qAsConst(initializers)) {
        fun();
    }
}
