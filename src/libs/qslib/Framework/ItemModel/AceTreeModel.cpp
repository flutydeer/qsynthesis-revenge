#include "AceTreeModel.h"
#include "AceTreeModel_p.h"

#include <QMMath.h>

#include <QDataStream>
#include <QDebug>
#include <QStack>
#include <QTextCodec>

namespace QsApi {

    template <typename ForwardIterator>
    static void qCloneAll(ForwardIterator begin, ForwardIterator end, ForwardIterator dest) {
        while (begin != end) {
            *dest = (*begin)->clone();
            ++begin;
            ++dest;
        }
    }

    template <template <class> class Container1, template <class> class Container2, class T>
    static Container2<T> qCloneAll(const Container1<T> &container) {
        Container2<T> res;
        res.reserve(container.size());
        for (const auto &item : container)
            res.push_back(item->clone());
        return res;
    }

    static const char SIGN_TREE_MODEL[] = "ATM"
                                          "TX"
                                          "LOG";
    static const char SIGN_TREE_ITEM[] = "item";
    static const char SIGN_OPERATION[] = "SEG ";

    static QDataStream &operator<<(QDataStream &stream, const QString &s) {
        return stream << s.toUtf8();
    }

    static QDataStream &operator>>(QDataStream &stream, QString &s) {
        QByteArray arr;
        stream >> arr;
        if (stream.status() == QDataStream::Ok) {
            s = QString::fromUtf8(arr);
        }
        return stream;
    }

    static QDataStream &operator<<(QDataStream &stream, const QVariantHash &s) {
        stream << s.size();
        for (auto it = s.begin(); it != s.end(); ++it) {
            QsApi::operator<<(stream, it.key());
            stream << it.value();
        }
        return stream;
    }

    static QDataStream &operator>>(QDataStream &stream, QVariantHash &s) {
        int size;
        stream >> size;
        s.reserve(size);
        for (int i = 0; i < size; ++i) {
            QString key;
            QsApi::operator>>(stream, key);
            if (stream.status() != QDataStream::Ok) {
                break;
            }
            QVariant val;
            stream >> val;
            if (stream.status() != QDataStream::Ok) {
                break;
            }
            s.insert(key, val);
        }
        return stream;
    }

    /**
     * User can only add child items whose index is zero into a model
     *
     * `index` field will be serialized in to binary data, but the index information in binary data deserialized to
     * `indexHint` field, because user may add an deserialized item to a model
     *
     * When a model is recovering from binary data itself, the `indexHint` really plays an important role in
     * rebuilding the original structure precisely
     */

    // Item
    AceTreeItemPrivate::AceTreeItemPrivate() {
        parent = nullptr;
        model = nullptr;
        m_index = 0;
        m_indexHint = 0;
    }

    AceTreeItemPrivate::~AceTreeItemPrivate() {
    }

    void AceTreeItemPrivate::init() {
    }

    AceTreeItem::AceTreeItem(const QString &name) : AceTreeItem(*new AceTreeItemPrivate(), name) {
    }

    AceTreeItem::~AceTreeItem() {
        Q_D(AceTreeItem);

        auto model = d->model;
        if (model && !model->d_func()->is_destruct)
            propagateModel(nullptr);

        qDeleteAll(d->vector);
        qDeleteAll(d->set);
    }

    QString AceTreeItem::name() const {
        Q_D(const AceTreeItem);
        return d->name;
    }

    AceTreeItem *AceTreeItem::parent() const {
        Q_D(const AceTreeItem);
        return d->parent;
    }

    AceTreeModel *AceTreeItem::model() const {
        Q_D(const AceTreeItem);
        return d->model;
    }

    int AceTreeItem::index() const {
        Q_D(const AceTreeItem);
        return d->model ? d->m_index : 0;
    }

    bool AceTreeItem::isFree() const {
        Q_D(const AceTreeItem);
        return !d->model && !d->parent && d->m_index == 0;
    }

    bool AceTreeItem::isObsolete() const {
        Q_D(const AceTreeItem);
        return !d->model && d->m_index > 0;
    }

    bool AceTreeItem::isWritable() const {
        Q_D(const AceTreeItem);
        return d->model ? d->model->isWritable() : (d->m_index == 0);
    }

    QVariant AceTreeItem::property(const QString &key) const {
        Q_D(const AceTreeItem);
        return d->properties.value(key, {});
    }

    void AceTreeItem::setProperty(const QString &key, const QVariant &value) {
        Q_D(AceTreeItem);
        if (!d->allowModify())
            return;

        d->setProperty_helper(key, value);
    }

    QVariantHash AceTreeItem::properties() const {
        Q_D(const AceTreeItem);
        return d->properties;
    }

    void AceTreeItem::setBytes(int start, const QByteArray &bytes) {
        Q_D(AceTreeItem);
        if (!d->allowModify())
            return;

        d->setBytes_helper(start, bytes);
    }

    void AceTreeItem::truncateBytes(int size) {
        Q_D(AceTreeItem);
        if (!d->allowModify())
            return;

        d->truncateBytes_helper(size);
    }

    QByteArray AceTreeItem::bytes() const {
        Q_D(const AceTreeItem);
        return d->byteArray;
    }

    int AceTreeItem::bytesSize() const {
        Q_D(const AceTreeItem);
        return d->byteArray.size();
    }

    void AceTreeItem::insertRows(int index, const QVector<AceTreeItem *> &items) {
        Q_D(AceTreeItem);
        if (!d->allowModify())
            return;

        // Validate
        QVector<AceTreeItem *> validItems;
        validItems.reserve(items.size());
        for (const auto &item : items) {
            if (!item)
                continue;

            if (!item->isFree()) {
                qWarning() << "AceTreeItem::insertRows(): item is not free" << item;
                continue;
            }
            validItems.append(item);
        }

        if (validItems.isEmpty())
            return;

        d->insertRows_helper(index, validItems);
    }

    void AceTreeItem::moveRows(int index, int count, int dest) {
        Q_D(AceTreeItem);

        if (!d->allowModify())
            return;

        d->moveRows_helper(index, count, dest);
    }

    void AceTreeItem::removeRows(int index, int count) {
        Q_D(AceTreeItem);

        if (!d->allowModify())
            return;

        // Validate
        count = qMin(count, d->vector.size() - index);
        if (count <= 0 || count > d->vector.size()) {
            return;
        }

        d->removeRows_helper(index, count);
    }

    AceTreeItem *AceTreeItem::row(int row) const {
        Q_D(const AceTreeItem);
        return (row >= 0 && row < d->vector.size()) ? d->vector.at(row) : nullptr;
    }

    int AceTreeItem::rowIndexOf(QsApi::AceTreeItem *item) const {
        Q_D(const AceTreeItem);
        return (item && item->parent() == this) ? d->vector.indexOf(item) : -1;
    }

    int AceTreeItem::rowCount() const {
        Q_D(const AceTreeItem);
        return d->vector.size();
    }

    void AceTreeItem::addNode(AceTreeItem *item) {
        Q_D(AceTreeItem);

        if (!d->allowModify())
            return;

        if (!item)
            return;

        // Validate
        if (!item->isFree()) {
            qWarning() << "AceTreeItem::addNode(): item is not free" << item;
            return;
        }

        return d->addNode_helper(item);
    }

    bool AceTreeItem::containsNode(QsApi::AceTreeItem *item) {
        Q_D(const AceTreeItem);
        return d->set.contains(item);
    }

    void AceTreeItem::removeNode(AceTreeItem *item) {
        Q_D(AceTreeItem);

        if (!d->allowModify())
            return;

        // Validate
        if (item->parent() != this || !d->set.contains(item)) {
            qWarning() << "AceTreeItem: item" << item << "has nothing to do with parent" << this;
            return;
        }

        d->removeNode_helper(item);
    }

    QList<AceTreeItem *> AceTreeItem::nodes() const {
        Q_D(const AceTreeItem);
        return d->set.values();
    }

    int AceTreeItem::nodeCount() const {
        Q_D(const AceTreeItem);
        return d->set.size();
    }

    AceTreeItem *AceTreeItem::read(QDataStream &in) {
        char sign[sizeof(SIGN_TREE_ITEM) - 1];
        in.readRawData(sign, sizeof(sign));
        if (memcmp(SIGN_TREE_ITEM, sign, sizeof(sign)) != 0) {
            return nullptr;
        }

        QString name;
        QsApi::operator>>(in, name);

        auto item = new AceTreeItem(name);
        auto d = item->d_func();

        in >> d->m_indexHint;
        QsApi::operator>>(in, d->properties);
        in >> d->byteArray;

        if (in.status() != QDataStream::Ok) {
            qWarning() << "AceTreeItem::read(): read item data failed";
            goto abort;
        } else {
            int size;
            in >> size;
            d->vector.reserve(size);
            for (int i = 0; i < size; ++i) {
                auto child = read(in);
                if (!child) {
                    qWarning() << "AceTreeItem::read(): read vector item failed";
                    goto abort;
                }
                child->d_func()->parent = item;
                d->vector.append(child);
            }

            in >> size;
            d->set.reserve(size);
            for (int i = 0; i < size; ++i) {
                auto child = read(in);
                if (!child) {
                    qWarning() << "AceTreeItem::read(): read set item failed";
                    goto abort;
                }
                child->d_func()->parent = item;
                d->set.insert(child);
            }
        }

        return item;

    abort:
        delete item;
        return nullptr;
    }

    void AceTreeItem::write(QDataStream &out) const {
        Q_D(const AceTreeItem);

        out.writeRawData("item", 4);
        QsApi::operator<<(out, d->name);
        out << d->m_index;
        QsApi::operator<<(out, d->properties);
        out << d->byteArray;

        out << d->vector.size();
        for (const auto &item : d->vector) {
            item->write(out);
        }

        out << d->set.size();
        for (const auto &item : d->set) {
            item->write(out);
        }
    }

    AceTreeItem *AceTreeItem::clone() const {
        Q_D(const AceTreeItem);
        auto item = new AceTreeItem(d->name);

        auto d2 = item->d_func();
        d2->properties = d->properties;
        d2->byteArray = d->byteArray;

        d2->vector.reserve(d->vector.size());
        for (auto &child : d->vector) {
            auto newChild = child->clone();
            newChild->d_func()->parent = item;
            d2->vector.append(newChild);
        }

        d2->set.reserve(d->set.size());
        for (auto &child : d->set) {
            auto newChild = child->clone();
            newChild->d_func()->parent = item;
            d2->set.insert(newChild);
        }

        return item;
    }

    void AceTreeItem::propagateModel(AceTreeModel *model) {
        Q_D(AceTreeItem);
        auto &m_model = d->model;
        if ((m_model && model) || m_model == model) {
            return;
        }

        if (m_model) {
            m_model->d_func()->removeIndex(d->m_index);
        } else {
            auto d2 = model->d_func();
            d->m_index = d2->addIndex(this, (d2->internalChange && d->m_indexHint != 0) ? d->m_indexHint : d->m_index);
            d->m_indexHint = 0;
        }

        m_model = model;

        for (auto item : qAsConst(d->vector))
            item->propagateModel(model);

        for (auto item : qAsConst(d->set))
            item->propagateModel(model);
    }

    AceTreeItem::AceTreeItem(AceTreeItemPrivate &d, const QString &name) : d_ptr(&d) {
        d.q_ptr = this;
        d.name = name;

        d.init();
    }

    // Model
    AceTreeModelPrivate::AceTreeModelPrivate() {
        m_dev = nullptr;
        m_fileDev = nullptr;
        is_destruct = false;
        maxIndex = 1;
        rootItem = nullptr;
        stepIndex = 0;
        internalChange = false;
    }

    AceTreeModelPrivate::~AceTreeModelPrivate() {
    }

    void AceTreeModelPrivate::init() {
    }

    void AceTreeModelPrivate::setRootItem_helper(QsApi::AceTreeItem *item) {
        Q_Q(AceTreeModel);

        auto org = rootItem;

        // Pre-Propagate
        emit q->rootAboutToChange(org, item);

        // Do change
        if (org)
            org->propagateModel(nullptr);
        if (item)
            item->propagateModel(q);
        rootItem = item;

        // Propagate signal
        rootChanged(org, item);
    }

    AceTreeItem *AceTreeModelPrivate::reset_helper() {
        Q_Q(AceTreeModel);

        if (stepIndex != 0)
            truncate(0);

        auto func = [&](AceTreeItem *item) {
            auto d = item->d_func();
            d->model = nullptr;
            d->m_index = 0;
        };

        AceTreeItemPrivate::propagateItems(rootItem, func);
        indexes.clear();
        maxIndex = 1;

        auto org = rootItem;
        rootItem = nullptr;

        emit q->stepChanged(0);

        return org;
    }

    void AceTreeModelPrivate::setCurrentStep_helper(int step) {
        Q_Q(AceTreeModel);

        internalChange = true;

        // Undo
        while (stepIndex > step) {
            if (!execute(operations.at(stepIndex - 1), true)) {
                goto out;
            }
            stepIndex--;
        }

        // Redo
        while (stepIndex < step) {
            if (!execute(operations.at(stepIndex), false)) {
                goto out;
            }
            stepIndex++;
        }

    out:
        internalChange = false;

        // Serialize
        if (m_dev) {
            QDataStream stream(m_dev);

            // Change step in log
            writeCurrentStep(stepIndex);

            if (stream.status() != QDataStream::Ok || (m_fileDev && (!m_fileDev->flush()))) {
                q->stopRecord();
                emit q->recordError();
            }
        }

        emit q->stepChanged(stepIndex);
    }

    int AceTreeModelPrivate::addIndex(AceTreeItem *item, int idx) {
        int index = idx > 0 ? (maxIndex = qMax(maxIndex, idx), idx) : maxIndex++;
        indexes.insert(index, item);
        return index;
    }

    void AceTreeModelPrivate::removeIndex(int index) {
        indexes.remove(index);
    }

    bool AceTreeItemPrivate::allowModify() const {
        Q_Q(const AceTreeItem);

        if (model && model->d_func()->internalChange) {
            qWarning() << "AceTreeItem: modifying data during model's internal state switching is prohibited" << q;
            return false;
        }

        if (!model && m_index > 0) {
            qWarning() << "AceTreeItem: the item is now obsolete" << q;
            return false;
        }

        return true;
    }

    void AceTreeItemPrivate::setProperty_helper(const QString &key, const QVariant &value) {
        Q_Q(AceTreeItem);

        QVariant oldValue;
        auto it = properties.find(key);
        if (it == properties.end()) {
            if (value.isNull() || !value.isValid())
                return;
            properties.insert(key, value);
        } else {
            oldValue = it.value();
            if (value.isNull() || !value.isValid())
                properties.erase(it);
            else if (oldValue == value)
                return;
            else
                it.value() = value;
        }

        // Propagate signal
        if (model)
            model->d_func()->propertyChanged(q, key, oldValue, value);
    }

    void AceTreeItemPrivate::setBytes_helper(int start, const QByteArray &bytes) {
        Q_Q(AceTreeItem);

        auto len = bytes.size();
        auto oldBytes = this->byteArray.mid(start, len);

        // Do change
        int newSize = start + len;
        if (newSize > this->byteArray.size())
            this->byteArray.resize(newSize);
        this->byteArray.replace(start, len, bytes);

        // Propagate signal
        if (model)
            model->d_func()->bytesSet(q, start, oldBytes, bytes);
    }

    void AceTreeItemPrivate::truncateBytes_helper(int size) {
        Q_Q(AceTreeItem);

        int len = size - byteArray.size();
        if (len == 0)
            return;

        auto oldBytes = byteArray.mid(size);

        // Do change
        byteArray.resize(size);

        // Propagate signal
        if (model)
            model->d_func()->bytesTruncated(q, size, oldBytes, len);
    }

    void AceTreeItemPrivate::insertRows_helper(int index, const QVector<AceTreeItem *> &items) {
        Q_Q(AceTreeItem);

        // Do change
        vector.insert(vector.begin() + index, items.size(), nullptr);
        for (int i = 0; i < items.size(); ++i) {
            auto item = items[i];
            vector[index + i] = item;

            item->d_func()->parent = q;
            if (model)
                item->propagateModel(model);
        }

        // Propagate signal
        if (model)
            model->d_func()->rowsInserted(q, index, items);
    }

    void AceTreeItemPrivate::moveRows_helper(int index, int count, int dest) {
        Q_Q(AceTreeItem);

        // Do change
        if (!QMMath::arrayMove(vector, index, count, dest)) {
            return;
        }

        // Propagate signal
        if (model)
            model->d_func()->rowsMoved(q, index, count, dest);
    }

    void AceTreeItemPrivate::removeRows_helper(int index, int count) {
        Q_Q(AceTreeItem);

        QVector<AceTreeItem *> tmp;
        tmp.resize(count);
        std::copy(vector.begin() + index, vector.begin() + index + count, tmp.begin());

        // Pre-Propagate signal
        if (model)
            emit model->rowsAboutToRemove(q, index, tmp);

        // Do change
        for (const auto &item : qAsConst(tmp)) {
            item->d_func()->parent = nullptr;
            if (model)
                item->propagateModel(nullptr);
        }
        vector.erase(vector.begin() + index, vector.begin() + index + count);

        // Propagate signal
        if (model)
            model->d_func()->rowsRemoved(q, index, tmp);
    }

    void AceTreeItemPrivate::addNode_helper(AceTreeItem *item) {
        Q_Q(AceTreeItem);

        // Do change
        item->d_func()->parent = q;
        if (model)
            item->propagateModel(model);
        set.insert(item);

        // Propagate signal
        if (model)
            model->d_func()->nodeAdded(q, item);
    }

    void AceTreeItemPrivate::removeNode_helper(AceTreeItem *item) {
        Q_Q(AceTreeItem);

        // Pre-Propagate signal
        if (model)
            emit model->nodeAboutToRemove(q, item);

        // Do change
        item->d_func()->parent = nullptr;
        if (model)
            item->propagateModel(nullptr);
        set.remove(item);

        // Propagate signal
        if (model)
            model->d_func()->nodeRemoved(q, item);
    }

    void AceTreeModelPrivate::serializeOperation(QDataStream &stream, BaseOp *baseOp) {
        // Write begin sign
        stream.writeRawData(SIGN_OPERATION, sizeof(SIGN_OPERATION) - 1);

        stream << (int) baseOp->c;
        switch (baseOp->c) {
            case PropertyChange: {
                auto op = static_cast<PropertyChangeOp *>(baseOp);
                stream << op->id;
                QsApi::operator<<(stream, op->key);
                stream << op->oldValue << op->newValue;
                break;
            }
            case BytesSet: {
                auto op = static_cast<BytesSetOp *>(baseOp);
                stream << op->id << op->start << op->oldBytes << op->newBytes;
                break;
            }
            case BytesTruncate: {
                auto op = static_cast<BytesTruncateOp *>(baseOp);
                stream << op->id << op->size << op->oldBytes << op->delta;
                break;
            }
            case RowsInsert:
            case RowsRemove: {
                auto op = static_cast<RowsInsertRemoveOp *>(baseOp);
                stream << op->id << op->index << op->items.size();
                if (op->c == RowsInsert) {
                    for (const auto &item : qAsConst(op->serialized)) {
                        stream.writeRawData(item.data(), item.size());
                    }
                } else {
                    // Write id only
                    for (const auto &item : qAsConst(op->items)) {
                        stream << item->d_func()->m_index;
                    }
                }
                break;
            }
            case RowsMove: {
                auto op = static_cast<RowsMoveOp *>(baseOp);
                stream << op->id << op->index << op->count << op->dest;
                break;
            }
            case NodeAdd:
            case NodeRemove: {
                auto op = static_cast<NodeAddRemoveOp *>(baseOp);
                stream << op->id;
                if (op->c == NodeAdd) {
                    stream.writeRawData(op->serialized, op->serialized.size());
                } else {
                    // Write id only
                    stream << op->item->d_func()->m_index;
                }
                break;
            }
            case RootChange: {
                auto op = static_cast<RootChangeOp *>(baseOp);

                // Write old root id
                stream << (op->oldRoot ? op->oldRoot->d_func()->m_index : 0);

                // Write new root id
                if (op->newRoot) {
                    stream << op->newRoot->d_func()->m_index;

                    // Write new root
                    op->newRoot->write(stream);
                } else {
                    stream << int(0);
                }
                break;
            }
            default:
                break;
        }
    }

    AceTreeModelPrivate::BaseOp *AceTreeModelPrivate::deserializeOperation(QDataStream &stream, QList<int> *ids) {
        // Read begin sign
        char sign[sizeof(SIGN_OPERATION) - 1];
        stream.readRawData(sign, sizeof(sign));
        if (memcmp(sign, SIGN_OPERATION, sizeof(sign)) != 0) {
            return nullptr;
        }

        int c;
        stream >> c;

        BaseOp *res = nullptr;
        switch (c) {
            case PropertyChange: {
                auto op = new PropertyChangeOp();
                stream >> op->id;
                QsApi::operator>>(stream, op->key);
                stream >> op->oldValue >> op->newValue;
                res = op;
                break;
            }
            case BytesSet: {
                auto op = new BytesSetOp();
                stream >> op->id >> op->start >> op->oldBytes >> op->newBytes;
                res = op;
                break;
            }
            case BytesTruncate: {
                auto op = new BytesTruncateOp();
                stream >> op->id >> op->size >> op->oldBytes >> op->delta;
                res = op;
                break;
            }
            case RowsInsert:
            case RowsRemove: {
                auto op = new RowsInsertRemoveOp(c == RowsInsert);
                int size;
                stream >> op->id >> op->index >> size;
                op->items.reserve(size);
                if (c == RowsInsert) {
                    for (int i = 0; i < size; ++i) {
                        auto item = AceTreeItem::read(stream);
                        if (!item) {
                            stream.setStatus(QDataStream::ReadCorruptData);
                            break;
                        }
                        op->items.append(item);
                    }
                } else {
                    int id;
                    for (int i = 0; i < size; ++i) {
                        stream >> id;
                        if (ids)
                            ids->append(id);
                    }
                }
                res = op;
                break;
            }
            case RowsMove: {
                auto op = new RowsMoveOp();
                stream >> op->id >> op->index >> op->count >> op->dest;
                res = op;
                break;
            }
            case NodeAdd:
            case NodeRemove: {
                auto op = new NodeAddRemoveOp(c == NodeAdd);
                int size;
                stream >> op->id;
                if (c == NodeAdd) {
                    auto item = AceTreeItem::read(stream);
                    if (!item) {
                        stream.setStatus(QDataStream::ReadCorruptData);
                    } else {
                        op->item = item;
                    }
                } else {
                    int id;
                    stream >> id;
                    if (ids)
                        ids->append(id);
                }
                res = op;
                break;
            }
            case RootChange: {
                auto op = new RootChangeOp();

                // Read old root id
                int id;
                stream >> id;
                if (ids)
                    ids->append(id);

                // Read new root id
                stream >> id;
                if (id != 0) {
                    auto item = AceTreeItem::read(stream);
                    if (!item) {
                        stream.setStatus(QDataStream::ReadCorruptData);
                    } else {
                        op->newRoot = item;
                    }
                }
                res = op;
                break;
            }
            default:
                break;
        }

        if (stream.status() != QDataStream::Ok) {
            delete res;
            return nullptr;
        }

        return res;
    }

    void AceTreeModelPrivate::propertyChanged(AceTreeItem *item, const QString &key, const QVariant &oldValue,
                                              const QVariant &newValue) {
        Q_Q(AceTreeModel);
        if (!internalChange) {
            auto op = new PropertyChangeOp();
            op->id = item->d_func()->m_index;
            op->key = key;
            op->oldValue = oldValue;
            op->newValue = newValue;
            push(op);
        }
        emit q->propertyChanged(item, key, oldValue, newValue);
    }

    void AceTreeModelPrivate::bytesSet(AceTreeItem *item, int start, const QByteArray &oldBytes,
                                       const QByteArray &newBytes) {
        Q_Q(AceTreeModel);
        if (!internalChange) {
            auto op = new BytesSetOp();
            op->id = item->d_func()->m_index;
            op->start = start;
            op->oldBytes = oldBytes;
            op->newBytes = newBytes;
            push(op);
        }
        emit q->bytesSet(item, start, oldBytes, newBytes);
    }

    void AceTreeModelPrivate::bytesTruncated(AceTreeItem *item, int size, const QByteArray &oldBytes, int delta) {
        Q_Q(AceTreeModel);
        if (!internalChange) {
            auto op = new BytesTruncateOp();
            op->id = item->d_func()->m_index;
            op->size = size;
            op->oldBytes = oldBytes;
            op->delta = delta;
            push(op);
        }
        emit q->bytesTruncated(item, size, oldBytes, delta);
    }

    void AceTreeModelPrivate::rowsInserted(AceTreeItem *parent, int index, const QVector<AceTreeItem *> &items) {
        Q_Q(AceTreeModel);
        if (!internalChange) {
            auto op = new RowsInsertRemoveOp(true);
            op->id = parent->d_func()->m_index;
            op->index = index;
            op->items = items;

            op->serialized.reserve(items.size());
            for (const auto &item : items) {
                QByteArray data;
                QDataStream stream(&data, QIODevice::WriteOnly);
                item->write(stream);
                op->serialized.append(data);
            }

            push(op);
        }
        emit q->rowsInserted(parent, index, items);
    }

    void AceTreeModelPrivate::rowsMoved(AceTreeItem *parent, int index, int count, int dest) {
        Q_Q(AceTreeModel);
        if (!internalChange) {
            auto op = new RowsMoveOp();
            op->id = parent->d_func()->m_index;
            op->index = index;
            op->count = count;
            op->dest = dest;
            push(op);
        }
        emit q->rowsMoved(parent, index, count, dest);
    }

    void AceTreeModelPrivate::rowsRemoved(AceTreeItem *parent, int index, const QVector<AceTreeItem *> &items) {
        Q_Q(AceTreeModel);
        if (!internalChange) {
            auto op = new RowsInsertRemoveOp(false);
            op->id = parent->d_func()->m_index;
            op->index = index;
            op->items = items;
            push(op);
        }
        emit q->rowsRemoved(parent, index, items.size());
    }

    void AceTreeModelPrivate::nodeAdded(AceTreeItem *parent, AceTreeItem *item) {
        Q_Q(AceTreeModel);
        if (!internalChange) {
            auto op = new NodeAddRemoveOp(true);
            op->id = parent->d_func()->m_index;
            op->item = item;

            {
                QByteArray data;
                QDataStream stream(&data, QIODevice::WriteOnly);
                item->write(stream);
                op->serialized = data;
            }

            push(op);
        }
        emit q->nodeAdded(parent, item);
    }

    void AceTreeModelPrivate::nodeRemoved(AceTreeItem *parent, AceTreeItem *item) {
        Q_Q(AceTreeModel);
        if (!internalChange) {
            auto op = new NodeAddRemoveOp(false);
            op->id = parent->d_func()->m_index;
            op->item = item;
            push(op);
        }
        emit q->nodeRemoved(parent, item);
    }

    void AceTreeModelPrivate::rootChanged(AceTreeItem *oldRoot, AceTreeItem *newRoot) {
        Q_Q(AceTreeModel);
        if (!internalChange) {
            auto op = new RootChangeOp();
            op->oldRoot = oldRoot;
            op->newRoot = newRoot;
            push(op);
        }
        emit q->rootChanged();
    }

    bool AceTreeModelPrivate::execute(BaseOp *baseOp, bool undo) {
        // #define DO_CHANGE(OBJ, FUNC) OBJ->d_func()->FUNC##_helper
        auto model = q_ptr;
        switch (baseOp->c) {
            case PropertyChange: {
                auto op = static_cast<PropertyChangeOp *>(baseOp);
                auto item = model->itemFromIndex(op->id);
                if (!item)
                    goto obsolete;
                item->d_func()->setProperty_helper(op->key, undo ? op->oldValue : op->newValue);
                break;
            }
            case BytesSet: {
                auto op = static_cast<BytesSetOp *>(baseOp);
                auto item = model->itemFromIndex(op->id);
                if (!item)
                    goto obsolete;
                if (undo) {
                    item->d_func()->setBytes_helper(op->start, op->oldBytes);
                    if (op->newBytes.size() > op->oldBytes.size())
                        item->d_func()->truncateBytes_helper(item->bytesSize() -
                                                             (op->newBytes.size() - op->oldBytes.size()));
                } else {
                    item->d_func()->setBytes_helper(op->start, op->newBytes);
                }
                break;
            }
            case BytesTruncate: {
                auto op = static_cast<BytesTruncateOp *>(baseOp);
                auto item = model->itemFromIndex(op->id);
                if (!item)
                    goto obsolete;
                if (undo) {
                    if (op->oldBytes.isEmpty()) {
                        item->d_func()->truncateBytes_helper(op->size - op->delta);
                    } else {
                        item->d_func()->setBytes_helper(op->size, op->oldBytes);
                    }
                } else {
                    item->d_func()->truncateBytes_helper(op->size);
                }
                break;
            }
            case RowsInsert:
            case RowsRemove: {
                auto op = static_cast<RowsInsertRemoveOp *>(baseOp);
                auto item = model->itemFromIndex(op->id);
                if (!item)
                    goto obsolete;
                ((op->c == RowsRemove) ^ undo) ? item->d_func()->removeRows_helper(op->index, op->items.size())
                                               : item->d_func()->insertRows_helper(op->index, op->items);
                break;
            }
            case RowsMove: {
                auto op = static_cast<RowsMoveOp *>(baseOp);
                auto item = model->itemFromIndex(op->id);
                if (!item)
                    goto obsolete;
                if (undo) {
                    int r_index;
                    int r_dest;
                    if (op->dest > op->index) {
                        r_index = op->dest - op->count;
                        r_dest = op->index;
                    } else {
                        r_index = op->dest;
                        r_dest = op->index + op->count;
                    }
                    item->d_func()->moveRows_helper(r_index, op->count, r_dest);
                } else
                    item->d_func()->moveRows_helper(op->index, op->count, op->dest);
                break;
            }
            case NodeAdd:
            case NodeRemove: {
                auto c = static_cast<NodeAddRemoveOp *>(baseOp);
                auto item = model->itemFromIndex(c->id);
                if (!item)
                    goto obsolete;
                ((c->c == NodeRemove) ^ undo) ? item->d_func()->removeNode_helper(c->item)
                                              : item->d_func()->addNode_helper(c->item);
                break;
            }
            case RootChange: {
                auto op = static_cast<RootChangeOp *>(baseOp);
                model->d_func()->setRootItem_helper(undo ? op->oldRoot : op->newRoot);
                break;
            }
            default:
                break;
        }

        return true;

    obsolete:
        return false;
    }

    void AceTreeModelPrivate::push(BaseOp *op) {
        Q_Q(AceTreeModel);
        truncate(stepIndex);

        operations.append(op);
        stepIndex++;

        // Serialize
        if (m_dev) {
            QDataStream stream(m_dev);

            // Change step in log
            writeCurrentStep(stepIndex);

            serializeOperation(stream, op);
            offsets.begs.append(m_dev->pos());

            if (stream.status() != QDataStream::Ok || (m_fileDev && (!m_fileDev->flush()))) {
                q->stopRecord();
                emit q->recordError();
            }
        }

        emit q->stepChanged(stepIndex);
    }

    void AceTreeModelPrivate::truncate(int step) {
        Q_Q(AceTreeModel);

        // qDeleteAll(operations.rbegin(), operations.rend() - stepIndex);
        // operations.resize(stepIndex);
        while (operations.size() > step) {
            delete operations.takeLast();
        }
        offsets.begs.resize(step);
        stepIndex = step;

        // Serialize
        if (m_dev) {
            QDataStream stream(m_dev);

            // Change step in log
            m_dev->seek(offsets.countPos);

            stream << stepIndex;

            // Restore pos
            qint64 pos = offsets.begs.isEmpty() ? offsets.dataPos : offsets.begs.back();
            m_dev->seek(pos);

            if (stream.status() != QDataStream::Ok || (m_fileDev && (!m_fileDev->resize(pos) || !m_fileDev->flush()))) {
                q->stopRecord();
                emit q->recordError();
            }
        }
    }

    void AceTreeModelPrivate::writeCurrentStep(int step) {
        qint64 pos = m_dev->pos();
        m_dev->seek(offsets.countPos);

        QDataStream stream(m_dev);
        stream << step;

        m_dev->seek(pos);
    }

    AceTreeModel::AceTreeModel(QObject *parent) : AceTreeModel(*new AceTreeModelPrivate(), parent) {
    }

    AceTreeModel::~AceTreeModel() {
        Q_D(AceTreeModel);
        d->is_destruct = true;
        delete d->rootItem;
    }

    int AceTreeModel::steps() const {
        Q_D(const AceTreeModel);
        return d->operations.size();
    }

    int AceTreeModel::currentStep() const {
        Q_D(const AceTreeModel);
        return d->stepIndex;
    }

    void AceTreeModel::setCurrentStep(int step) {
        Q_D(AceTreeModel);
        if (step < 0 || step > d->operations.size() || step == d->stepIndex || d->internalChange)
            return;

        d->setCurrentStep_helper(step);
    }

    bool AceTreeModel::isWritable() const {
        Q_D(const AceTreeModel);
        return !d->internalChange;
    }

    void AceTreeModel::startRecord(QIODevice *dev) {
        Q_D(AceTreeModel);
        if (d->m_dev)
            stopRecord();

        d->m_dev = dev;
        d->m_fileDev = qobject_cast<QFileDevice *>(dev); // Try QFileDevice

        QDataStream stream(dev);

        d->offsets.startPos = dev->pos();
        stream.writeRawData(SIGN_TREE_MODEL, sizeof(SIGN_TREE_MODEL) - 1);

        d->offsets.countPos = dev->pos();
        stream << d->stepIndex;

        d->offsets.dataPos = dev->pos();

        // Write all existing operations
        for (int i = 0; i < d->operations.size(); ++i) {
            AceTreeModelPrivate::serializeOperation(stream, d->operations.at(i));

            if (stream.status() != QDataStream::Ok) {
                stopRecord();
                emit recordError();
                return;
            }

            d->offsets.begs.append(dev->pos());
        }

        if (d->m_fileDev)
            d->m_fileDev->flush();
    }

    void AceTreeModel::stopRecord() {
        Q_D(AceTreeModel);
        d->m_dev = nullptr;
        d->m_fileDev = nullptr;

        d->offsets = {};
    }

    AceTreeModel *AceTreeModel::recover(const QByteArray &data) {
        QDataStream stream(data);

        char sign[sizeof(SIGN_TREE_MODEL) - 1];
        stream.readRawData(sign, sizeof(sign));
        if (memcmp(sign, SIGN_TREE_MODEL, sizeof(sign)) != 0) {
            qDebug() << "AceTreeModel::recover(): read header sign failed";
            return nullptr;
        }

        QHash<int, AceTreeItem *> insertedItems;
        auto collectItems = [&](AceTreeItem *item) {
            if (!item)
                return;

            QStack<AceTreeItem *> stack;
            stack.push(item);
            while (!stack.isEmpty()) {
                auto top = stack.pop();
                auto d = top->d_func();
                insertedItems.insert(d->m_indexHint, top);

                for (const auto &child : qAsConst(d->vector))
                    stack.push(child);
                for (const auto &child : qAsConst(d->set))
                    stack.push(child);
            }
        };

        QVector<AceTreeModelPrivate::BaseOp *> operations;
        int size;
        stream >> size;
        operations.reserve(size);
        for (int i = 0; i < size; ++i) {
            QList<int> ids;
            auto baseOp = AceTreeModelPrivate::deserializeOperation(stream, &ids);
            if (!baseOp) {
                qDebug() << "AceTreeModel::recover(): read operation failed";
                goto op_abort;
            }

            switch (baseOp->c) {
                case AceTreeModelPrivate::RowsInsert: {
                    auto op = static_cast<AceTreeModelPrivate::RowsInsertRemoveOp *>(baseOp);
                    for (const auto &item : qAsConst(op->items))
                        collectItems(item);
                    break;
                }
                case AceTreeModelPrivate::RowsRemove: {
                    auto op = static_cast<AceTreeModelPrivate::RowsInsertRemoveOp *>(baseOp);
                    op->items.reserve(ids.size());
                    for (const auto &id : qAsConst(ids)) {
                        auto removedItem = insertedItems.value(id, nullptr);
                        if (!removedItem) {
                            qDebug() << "AceTreeModel::recover(): removed row not found";
                            goto op_abort;
                        }
                        op->items.append(removedItem);
                    }
                    break;
                }
                case AceTreeModelPrivate::NodeAdd: {
                    auto op = static_cast<AceTreeModelPrivate::NodeAddRemoveOp *>(baseOp);
                    collectItems(op->item);
                    break;
                }
                case AceTreeModelPrivate::NodeRemove: {
                    auto op = static_cast<AceTreeModelPrivate::NodeAddRemoveOp *>(baseOp);
                    auto removedItem = insertedItems.value(ids.front(), nullptr);
                    if (!removedItem) {
                        qDebug() << "AceTreeModel::recover(): removed node not found";
                        goto op_abort;
                    }
                    op->item = removedItem;
                    break;
                }
                case AceTreeModelPrivate::RootChange: {
                    auto op = static_cast<AceTreeModelPrivate::RootChangeOp *>(baseOp);

                    auto id = ids.front();
                    if (id != 0) {
                        op->oldRoot = insertedItems.value(id, nullptr);
                        if (!op->oldRoot) {
                            qDebug() << "AceTreeModel::recover(): old root not found";
                            goto op_abort;
                        }
                    }

                    collectItems(op->newRoot);
                    break;
                }
                default:
                    break;
            }

            operations.append(baseOp);
            continue;

        op_abort:
            qDeleteAll(operations);
            return nullptr;
        }

        int step = operations.size();

        // Rebuild
        auto model = new AceTreeModel();
        model->d_func()->operations = std::move(operations);
        model->setCurrentStep(step);
        if (model->currentStep() != step) {
            qDebug() << "AceTreeModel::recover(): failed to reach given step";
            delete model;
            return nullptr;
        }

        return model;
    }

    AceTreeItem *AceTreeModel::itemFromIndex(int index) const {
        Q_D(const AceTreeModel);
        return index > 0 ? d->indexes.value(index, nullptr) : nullptr;
    }

    AceTreeItem *AceTreeModel::rootItem() const {
        Q_D(const AceTreeModel);
        return d->rootItem;
    }

    void AceTreeModel::setRootItem(AceTreeItem *item) {
        Q_D(AceTreeModel);
        if (d->internalChange) {
            qWarning() << "AceTreeModel: modifying root item during model's internal state switching is prohibited"
                       << this;
            return;
        }

        if (item == d->rootItem)
            return;

        if (item && !item->isFree()) {
            qWarning() << "AceTreeModel::setRootItem(): item is not free" << item;
            return;
        }

        d->setRootItem_helper(item);
    }

    AceTreeItem *AceTreeModel::reset() {
        Q_D(AceTreeModel);
        if (d->internalChange) {
            qWarning() << "AceTreeModel: taking root item during model's internal state switching is prohibited"
                       << this;
            return nullptr;
        }

        if (!d->rootItem)
            return nullptr;

        return d->reset_helper();
    }

    AceTreeModel::AceTreeModel(AceTreeModelPrivate &d, QObject *parent) : QObject(parent), d_ptr(&d) {
        d.q_ptr = this;

        d.init();
    }
}
