#ifndef CHORUSKIT_FILELISTWIDGET_P_H
#define CHORUSKIT_FILELISTWIDGET_P_H

#include "../FileListWidget.h"

namespace QsApi {

    class FileListWidgetPrivate : public QObject {
        Q_OBJECT
        Q_DECLARE_PUBLIC(FileListWidget)
    public:
        FileListWidgetPrivate();
        ~FileListWidgetPrivate();

        void init();

        QTypeList styleData_helper() const;
        void setStyleData_helper(const QTypeList &list);

        FileListWidget *q_ptr;

        FileListItemDelegate *m_delegate;
        QSize oldContentsSize;

    private:
        void _q_delegateClicked(const QModelIndex &index, int button);
    };

}

#endif // CHORUSKIT_FILELISTWIDGET_P_H
