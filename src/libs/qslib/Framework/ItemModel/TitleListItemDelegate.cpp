#include "TitleListItemDelegate.h"
#include "TitleListItemDelegate_p.h"

#include "QMarginsImpl.h"

#include <QMouseEvent>
#include <QPainter>

#include "QsFrameworkNamespace.h"

namespace QsApi {

    struct TextBlock {
        QString text;
        bool isHighlight;
        bool isQuote;
        double ratio;
    };

    QList<TextBlock> processString1(const QString &inputString, double ratio) {
        QList<TextBlock> result;

        QRegularExpression tagRegex("<(highlight|quote)>(.*?)</\\1>");

        int currentIndex = 0;
        QRegularExpressionMatch match;
        while ((match = tagRegex.match(inputString, currentIndex)).hasMatch()) {
            int tagStart = match.capturedStart();
            int tagEnd = match.capturedEnd();
            QString blockText = inputString.mid(currentIndex, tagStart - currentIndex);

            if (!blockText.isEmpty())
                result.append({blockText, false, false, ratio});

            QString tagName = match.captured(1);
            if (tagName == "highlight") {
                result.append({match.captured(2), true, false, ratio});
            } else if (tagName == "quote") {
                result.append({match.captured(2), false, true, ratio});
            }

            currentIndex = tagEnd;
        }

        if (currentIndex < inputString.length()) {
            QString remainingText = inputString.mid(currentIndex);
            result.append({remainingText, false, false, ratio});
        }

        return result;
    }

    QList<TextBlock> processString(const QString &inputString) {
        QList<TextBlock> result;

        QRegularExpression tagRegex("<(x[\\d.]+)>(.*?)</\\1>");

        int currentIndex = 0;
        QRegularExpressionMatch match;
        while ((match = tagRegex.match(inputString, currentIndex)).hasMatch()) {
            int tagStart = match.capturedStart();
            int tagEnd = match.capturedEnd();
            QString blockText = inputString.mid(currentIndex, tagStart - currentIndex);

            if (!blockText.isEmpty()) {
                result.append(processString1(blockText, 1));
            }

            QString tagName = match.captured(1);
            double ratio = tagName.midRef(1).toDouble();
            if (ratio == 0)
                ratio = 1;
            result.append(processString1(match.captured(2), ratio));

            currentIndex = tagEnd;
        }

        if (currentIndex < inputString.length()) {
            QString remainingText = inputString.mid(currentIndex);
            result.append(processString1(remainingText, 1));
        }

        return result;
    }

    static QSize richTextSize(const QFont &font, const QFont &fontH, const QList<TextBlock> &blocks,
                              const QRectF &rect) {

        double width = 0;
        double height = 0;
        for (const auto &block : blocks) {
            auto f = block.isHighlight ? fontH : font;
            f.setPixelSize(f.pixelSize() * block.ratio);
            auto fm = QFontMetrics(f);

            if (block.isHighlight) {
                width += fm.horizontalAdvance(block.text);
            } else {
                if (block.isQuote) {
                    double spaceWidth = fm.horizontalAdvance(' ');
                    width += spaceWidth + 2 * rect.width();
                }
                width += fm.horizontalAdvance(block.text);
            }
            height = qMax<double>(fm.height(), height);
        }

        return {int(width), int(height + 2 * rect.height())};
    }

    static void drawRichTexts(QPainter *painter, const QFont &font, const QFont &fontH, const QPoint &start,
                              const QList<TextBlock> &blocks, const QColor &foreground, const QColor &background,
                              const QColor &highlightColor, const QRectF &rect, double radius, int height) {

        double currentWidth = 0;
        for (const auto &block : blocks) {
            double width = 0;
            double spaceWidth = 0;

            // Get current font metrics
            auto f = block.isHighlight ? fontH : font;
            f.setPixelSize(f.pixelSize() * block.ratio);
            auto fm = QFontMetrics(f);
            painter->setFont(f);

            if (block.isHighlight) {
                width += fm.horizontalAdvance(block.text);
            } else {
                if (block.isQuote) {
                    spaceWidth = fm.horizontalAdvance(' ');
                    currentWidth += spaceWidth / 2;
                    width += 2 * rect.width();
                }
                width += fm.horizontalAdvance(block.text);
            }

            QRectF dst(start + QPointF(currentWidth, 0), QSizeF(width, height));
            if (block.isHighlight) {
                painter->setPen(highlightColor);
            } else {
                if (block.isQuote) {
                    currentWidth += spaceWidth / 2;
                    painter->setPen(Qt::transparent);
                    painter->setBrush(background);
                    painter->drawRoundedRect(dst, radius, radius);
                }
                painter->setPen(foreground);
            }
            painter->drawText(dst, Qt::AlignHCenter | Qt::AlignVCenter, block.text);

            currentWidth += width;
        }
    }

    TitleListItemDelegatePrivate::TitleListItemDelegatePrivate() {
        m_backgroundType = QRectStyle();
        m_underline = QLineStyle(Qt::lightGray, 1.0);

        m_dateBackType = QRectStyle();

        m_fileMargins = QMargins(5, 5, 5, 5);
        m_locMargins = QMargins(5, 5, 5, 5);
        m_dateMargins = QMargins(5, 5, 5, 5);

        m_iconMargins = QMargins(5, 5, 5, 5);
    }

    TitleListItemDelegatePrivate::~TitleListItemDelegatePrivate() {
    }

    void TitleListItemDelegatePrivate::init() {
    }

    QTypeList TitleListItemDelegatePrivate::styleData_helper() const {
        return {
            QVariant::fromValue(m_backgroundType), QVariant::fromValue(m_underline),
            QVariant::fromValue(m_fileType),       QVariant::fromValue(m_locType),
            QVariant::fromValue(m_dateType),       QVariant::fromValue(m_dateHighlightType),
            QVariant::fromValue(m_dateBackType),

            QVariant::fromValue(m_fileMargins),    QVariant::fromValue(m_locMargins),
            QVariant::fromValue(m_dateMargins),    QVariant::fromValue(m_iconMargins),
            QVariant::fromValue(m_padding),        QVariant::fromValue(m_margins),
        };
    }

    void TitleListItemDelegatePrivate::setStyleData_helper(const QTypeList &list) {
        auto decodeStyle = [](const QVariant &var, auto &val) {
            using Type = decltype(typename std::remove_reference<decltype(val)>::type());
            if (var.canConvert<Type>()) {
                val = var.value<Type>();
            }
        };

        if (list.size() >= 13) {
            int i = 0;

            decodeStyle(list.at(i++), m_backgroundType);
            decodeStyle(list.at(i++), m_underline);
            decodeStyle(list.at(i++), m_fileType);
            decodeStyle(list.at(i++), m_locType);
            decodeStyle(list.at(i++), m_dateType);
            decodeStyle(list.at(i++), m_dateHighlightType);
            decodeStyle(list.at(i++), m_dateBackType);

            decodeStyle(list.at(i++), m_fileMargins);
            decodeStyle(list.at(i++), m_locMargins);
            decodeStyle(list.at(i++), m_dateMargins);
            decodeStyle(list.at(i++), m_iconMargins);
            decodeStyle(list.at(i++), m_padding);
            decodeStyle(list.at(i++), m_margins);
        }
    }

    TitleListItemDelegate::TitleListItemDelegate(QObject *parent)
        : TitleListItemDelegate(*new TitleListItemDelegatePrivate(), parent) {
    }

    TitleListItemDelegate::~TitleListItemDelegate() {
    }

    QSize TitleListItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
        Q_D(const TitleListItemDelegate);

        auto icon = index.data(QsApi::DecorationRole).value<QIcon>();
        auto location = index.data(QsApi::SubtitleRole).toString();
        auto date = index.data(QsApi::DescriptionRole).toString();

        QSize size = QStyledItemDelegate::sizeHint(option, index);
        int iconHeight = icon.isNull() ? 0
                                       : (index.data(QsApi::IconSizeRole).toSize().height() + d->m_iconMargins.top() +
                                          d->m_iconMargins.bottom());
        int midHeight =
            QFontMetrics(d->m_fileType.font()).height() + d->m_fileMargins.top() + d->m_fileMargins.bottom() +
            (location.isEmpty()
                 ? 0
                 : (QFontMetrics(d->m_locType.font()).height() + d->m_locMargins.top() + d->m_locMargins.bottom()));
        int dateHeight =
            date.isEmpty()
                ? 0
                : (QFontMetrics(d->m_dateType.font()).height() + d->m_dateMargins.top() + d->m_dateMargins.bottom());

        int h = qMax(iconHeight, qMax(midHeight, dateHeight));
        h += d->m_margins.top() + d->m_margins.bottom() + d->m_padding.top() + d->m_padding.bottom();
        if (size.height() < h) {
            size.setHeight(h);
        }
        return size;
    }

    void TitleListItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                      const QModelIndex &index) const {
        Q_D(const TitleListItemDelegate);

        QRect rect = option.rect;
        rect.adjust(d->m_margins.left(), d->m_margins.top(), -d->m_margins.right(), -d->m_margins.bottom());

        QRect backgroundRect = rect;
        rect.adjust(d->m_padding.left(), d->m_padding.top(), -d->m_padding.right(), -d->m_padding.bottom());

        // Color
        QColor backgroundColor;
        QColor underlineColor;
        QColor fileColor;
        QColor locColor;
        QColor dateColor;
        QColor dateHighlightColor;
        QColor dateBackColor;

        underlineColor = d->m_underline.color();
        if (!(option.state & QStyle::State_Enabled)) {
            backgroundColor = d->m_backgroundType.color4();
            fileColor = d->m_fileType.color4();
            locColor = d->m_locType.color4();
            dateColor = d->m_dateType.color4();
            dateHighlightColor = d->m_dateHighlightType.color4();
            dateBackColor = d->m_dateBackType.color4();
        } else if (option.state & QStyle::State_Selected) {
            backgroundColor = d->m_backgroundType.color3();
            fileColor = d->m_fileType.color3();
            locColor = d->m_locType.color3();
            dateColor = d->m_dateType.color3();
            dateHighlightColor = d->m_dateHighlightType.color3();
            dateBackColor = d->m_dateBackType.color3();
        } else if (option.state & QStyle::State_MouseOver) {
            backgroundColor = d->m_backgroundType.color2();
            fileColor = d->m_fileType.color2();
            locColor = d->m_locType.color2();
            dateColor = d->m_dateType.color2();
            dateHighlightColor = d->m_dateHighlightType.color2();
            dateBackColor = d->m_dateBackType.color2();
        } else {
            backgroundColor = d->m_backgroundType.color();
            underlineColor = d->m_underline.color();
            fileColor = d->m_fileType.color();
            locColor = d->m_locType.color();
            dateColor = d->m_dateType.color();
            dateHighlightColor = d->m_dateHighlightType.color();
            dateBackColor = d->m_dateBackType.color();
        }

        // Fetch data
        QString filename = index.data(QsApi::DisplayRole).toString();
        QString location = index.data(QsApi::SubtitleRole).toString();
        QString date = index.data(QsApi::DescriptionRole).toString();

        auto icon = index.data(QsApi::DecorationRole).value<QIcon>();
        QSize iconSize = index.data(QsApi::IconSizeRole).toSize();
        if (icon.isNull()) {
            iconSize = QSize(0, 0);
        }

        QMargins iconMargins = icon.isNull() ? QMargins() : d->m_iconMargins;
        QMargins locMargins = location.isEmpty() ? QMargins() : d->m_locMargins;

        Qt::AlignmentFlag dateVAlign = static_cast<Qt::AlignmentFlag>(index.data(QsApi::AlignmentRole).toInt());

        // Calculate size
        QFont fileFont = d->m_fileType.font();
        QFont locFont = d->m_locType.font();
        QFont dateFont = d->m_dateType.font();
        QFont dateHighlightFont = d->m_dateHighlightType.font();

        QFontMetrics fileFontM(fileFont);
        QFontMetrics locFontM(locFont);

        int fileFontHeight = fileFontM.height();
        int locFontHeight = location.isEmpty() ? 0 : locFontM.height();

        double radius = d->m_dateBackType.radius();
        auto dateTextBlocks = processString(date);

        auto dateSize = dateTextBlocks.isEmpty()
                            ? QSize()
                            : richTextSize(dateFont, dateHighlightFont, dateTextBlocks, d->m_dateBackType.rect());
        int dateFontHeight = dateSize.height();
        int dateWidth = dateSize.width();

        int iconHeight = index.data(QsApi::IconSizeRole).toSize().height() + iconMargins.top() + iconMargins.bottom();
        int midHeight = fileFontHeight + d->m_fileMargins.top() + d->m_fileMargins.bottom() + locFontHeight +
                        locMargins.top() + locMargins.bottom();
        int dateHeight = dateFontHeight + d->m_dateMargins.top() + d->m_dateMargins.bottom();

        int realHeight = rect.height();

        QRect iconRect;
        iconRect.setTop(rect.top() + (realHeight - iconHeight) / 2 + iconMargins.top());
        iconRect.setLeft(rect.left() + iconMargins.left());
        iconRect.setSize(iconSize);

        int iconRight = iconRect.right() + iconMargins.right();
        int wordsWidth = rect.width() - iconRight;

        QRect dateRect;
        dateRect.setTop(rect.top() + (realHeight - dateHeight) / 2 + d->m_dateMargins.top());
        dateRect.setRight(rect.right() - d->m_dateMargins.right());
        dateRect.setLeft(
            qMax(iconRect.right() + wordsWidth / 2 + d->m_dateMargins.left(), dateRect.right() - dateWidth));
        dateRect.setHeight(dateFontHeight);

        int dateLeft = dateRect.left() - d->m_dateMargins.left();

        QRect fileRect;
        fileRect.setTop(rect.top() + (realHeight - midHeight) / 2 + d->m_fileMargins.top());
        fileRect.setLeft(iconRight + d->m_fileMargins.left());
        fileRect.setRight(dateLeft - d->m_fileMargins.right());
        fileRect.setHeight(fileFontHeight);

        int fileBottom = fileRect.bottom() + d->m_fileMargins.bottom();

        QRect locRect;
        locRect.setTop(fileBottom + locMargins.top());
        locRect.setLeft(iconRight + locMargins.left());
        locRect.setRight(dateLeft - locMargins.right());
        locRect.setHeight(locFontHeight);

        painter->setRenderHint(QPainter::Antialiasing);

        // Status
        painter->setPen(Qt::NoPen);
        painter->setBrush(backgroundColor);
        painter->drawRoundedRect(backgroundRect, d->m_backgroundType.radius(), d->m_backgroundType.radius());

        // Icon
        if (!icon.isNull()) {
            painter->drawPixmap(iconRect, icon.pixmap(iconSize));
        }

        // Last open time
        if (!dateSize.isEmpty()) {
            QPoint start = dateRect.topLeft();
            switch (dateVAlign) {
                case Qt::AlignTop:
                    start.setY(fileRect.center().y() - dateRect.height() / 2);
                    break;
                case Qt::AlignBottom:
                    start.setY(locRect.center().y() - dateRect.height() / 2);
                    break;
                default:
                    break;
            }

            painter->setFont(dateFont);
            drawRichTexts(painter, dateFont, dateHighlightFont, start, dateTextBlocks, dateColor, dateBackColor,
                          dateHighlightColor, d->m_dateBackType.rect(), radius, dateFontHeight);
        }

        painter->setFont(fileFont);
        painter->setPen(fileColor);

        // Filename
        {
            QString text = filename;
            const QRect &dst = fileRect;
            const QFontMetrics &fm = fileFontM;

            int width = fm.horizontalAdvance(text);
            if (width > dst.width()) {
                text = fm.elidedText(text, Qt::ElideRight, dst.width());
            }
            painter->drawText(dst, Qt::AlignLeft | Qt::AlignVCenter, text);
        }

        painter->setFont(locFont);
        painter->setPen(locColor);

        // Address
        if (!location.isEmpty()) {
            QString text = location;
            const QRect &dst = locRect;
            const QFontMetrics &fm = locFontM;

            int width = fm.horizontalAdvance(text);
            if (width > dst.width()) {
                text = fm.elidedText(text, Qt::ElideRight, dst.width());
            }
            painter->drawText(dst, Qt::AlignLeft | Qt::AlignVCenter, text);
        }

        if (index.data(QsApi::SeparatorRole).toBool()) {
            painter->setPen(QPen(underlineColor, d->m_underline.widthF()));
            painter->drawLine(backgroundRect.bottomLeft(), backgroundRect.bottomRight());
        }
    }

    QTypeList TitleListItemDelegate::styleData() const {
        Q_D(const TitleListItemDelegate);
        return d->styleData_helper();
    }
    void TitleListItemDelegate::setStyleData(const QTypeList &list) {
        Q_D(TitleListItemDelegate);
        d->setStyleData_helper(list);
    }

    bool TitleListItemDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                            const QStyleOptionViewItem &option, const QModelIndex &index) {
        if (event->type() == QEvent::MouseButtonRelease) {
            auto e = static_cast<QMouseEvent *>(event);

            if (e->button() == Qt::RightButton) {
                emit clicked(index, Qt::RightButton);
            } else if (e->button() == Qt::LeftButton) {
                emit clicked(index, Qt::LeftButton);
            }
        }

        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }

    TitleListItemDelegate::TitleListItemDelegate(TitleListItemDelegatePrivate &d, QObject *parent)
        : QStyledItemDelegate(parent), d_ptr(&d) {
        d.q_ptr = this;

        d.init();
    }

}
