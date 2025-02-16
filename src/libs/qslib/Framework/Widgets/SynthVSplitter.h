#ifndef SYNTHVSPLITTER_H
#define SYNTHVSPLITTER_H

#include <QAbstractScrollArea>
#include <QFrame>
#include <QSplitter>

#include <QPixelSize.h>

#include "QsFrameworkGlobal.h"

namespace QsApi {

    class SynthVSplitterPrivate;

    class SynthVSplitterHandle;

    class QSFRAMEWORK_API SynthVSplitter : public QFrame {
        Q_OBJECT
        Q_DECLARE_PRIVATE(SynthVSplitter)
    public:
        explicit SynthVSplitter(QWidget *parent = nullptr);
        ~SynthVSplitter();

        void addWidget(QWidget *w);
        void insertWidget(int index, QWidget *w);
        void removeWidget(QWidget *w);
        void moveWidget(int from, int to);

        QWidget *widget(int index) const;
        int indexOf(QWidget *w) const;
        int count() const;
        bool isEmpty() const;

        QSize sizeHint() const override;

    public:
        QAbstractScrollArea *scrollArea() const;
        QAbstractScrollArea *takeScrollArea();
        void setScrollArea(QAbstractScrollArea *area);

    protected:
        virtual SynthVSplitterHandle *createHandle(QWidget *w);

        void resizeEvent(QResizeEvent *event) override;

    protected:
        SynthVSplitter(SynthVSplitterPrivate &d, QWidget *parent = nullptr);

        QScopedPointer<SynthVSplitterPrivate> d_ptr;

        friend class SynthVSplitterHandle;
        friend class SynthVSplitterHandlePrivate;
    };

    class SynthVSplitterHandlePrivate;

    class SynthVSplitterHandle : public QFrame {
        Q_OBJECT
        Q_PROPERTY(QPixelSize handleHeight READ handleHeight WRITE setHandleHeight)
    public:
        explicit SynthVSplitterHandle(SynthVSplitter *parent);
        ~SynthVSplitterHandle();

        QPixelSize handleHeight();
        void setHandleHeight(const QPixelSize &h);

        QSize sizeHint() const override;

    protected:
        void resizeEvent(QResizeEvent *event) override;
        bool event(QEvent *event) override;
        bool eventFilter(QObject *obj, QEvent *event) override;

    private:
        SynthVSplitterHandlePrivate *d;

        friend class SynthVSplitter;
        friend class SynthVSplitterPrivate;
    };

} // namespace QsApi

#endif // SYNTHVSPLITTER_H
