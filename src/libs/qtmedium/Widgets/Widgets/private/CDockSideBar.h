#ifndef __CDOCKSIDEBAR_H__
#define __CDOCKSIDEBAR_H__

#include <QFrame>

#include "CDockTabBar.h"
#include "QMWidgetsGlobal.h"

class CDockTabDragProxy;

class QMWIDGETS_API CDockSideBar : public QFrame {
    Q_OBJECT
public:
    explicit CDockSideBar(QWidget *parent = nullptr);
    ~CDockSideBar();

    friend class CDockTabBar;

private:
    void init();

public:
    QM::Direction cardDirection() const;
    void setCardDirection(QM::Direction cardDirection);

    Qt::Orientation orientation() const;
    void setOrientation(Qt::Orientation orient);

    bool highlight() const;
    void setHighlight(bool highlight, int widthHint = 0);

    int count() const;

    CDockTabBar *firstBar() const;
    CDockTabBar *secondBar() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    CDockTabBar *m_firstBar;
    CDockTabBar *m_secondBar;

    void resetLayout();

    void mousePressEvent(QMouseEvent *event) override;

private:
    void _q_cardAdded(CDockCard *card);
    void _q_cardRemoved(CDockCard *card);
    void _q_cardToggled(CDockCard *card);

signals:
    void cardAdded(QM::Priority number, CDockCard *card);
    void cardRemoved(QM::Priority number, CDockCard *card);
    void cardToggled(QM::Priority number, CDockCard *card);
};

#endif // __CDOCKSIDEBAR_H__