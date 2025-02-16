#ifndef QMDECORATORV2_H
#define QMDECORATORV2_H

#include <QScreen>
#include <QTimer>

#include "QMCoreDecoratorV2.h"
#include "QMWidgetsGlobal.h"

#ifdef qIDec
#    undef qIDec
#endif
#define qIDec qobject_cast<QMDecoratorV2 *>(QMCoreDecoratorV2::instance())

class QMDecoratorV2Private;

class QMWIDGETS_API QMDecoratorV2 : public QMCoreDecoratorV2 {
    Q_OBJECT
    Q_DECLARE_PRIVATE(QMDecoratorV2)
public:
    explicit QMDecoratorV2(QObject *parent = nullptr);
    ~QMDecoratorV2();

public:
    void addThemePath(const QString &path);
    void removeThemePath(const QString &path);

    void installTheme(QWidget *w, const QString &id);

public:
    QStringList themes() const;
    QString theme() const;
    void setTheme(const QString &theme);
    void refreshTheme();

signals:
    void themeChanged(const QString &theme);

protected:
    QMDecoratorV2(QMDecoratorV2Private &d, QObject *parent = nullptr);
};

#ifndef _LOC
#    define _LOC(T, P) std::bind(&T::reloadStrings, P)
#endif

#endif // QMDECORATORV2_H