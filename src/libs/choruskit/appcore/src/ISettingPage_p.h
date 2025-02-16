#ifndef ISETTINGPAGEPRIVATE_H
#define ISETTINGPAGEPRIVATE_H

#include "ISettingPage.h"

#include "Collections/QMChronMap.h"

namespace Core {

    class ISettingPagePrivate {
        Q_DECLARE_PUBLIC(ISettingPage)
    public:
        ISettingPagePrivate();
        virtual ~ISettingPagePrivate();

        void init();

        ISettingPage *q_ptr;

        QMChronMap<QString, ISettingPage *> pages;

        QString id;
        QString title;
        QString description;
    };

}

#endif // ISETTINGPAGEPRIVATE_H