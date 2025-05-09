/*
    Scan Tailor - Interactive post-processing tool for scanned pages.
    Copyright (C)  Joseph Artsimovich <joseph.artsimovich@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ThumbnailPixmapCache.h"
#include "AbstractThumbnailMaker.h"
#include "ImageId.h"
#include "ImageLoader.h"
#include "AtomicFileOverwriter.h"
#include "RelinkablePath.h"
#include "OutOfMemoryHandler.h"
#include "imageproc/Scale.h"
#include "imageproc/GrayImage.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QString>
#include <QChar>
#include <QImage>
#include <QPixmap>
#include <QEvent>
#include <QSize>
#include <QDebug>
#ifndef Q_MOC_RUN
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/member.hpp>
#endif
#include <algorithm>
#include <memory>
#include <vector>
#include <new>

using namespace ::boost;
using namespace ::boost::multi_index;
using namespace imageproc;

struct ThumbnailPixmapCache::ThumbId
{
    ThumbId()
    {
    }

    ThumbId(ImageId const& image_id, QString thumb_version)
        : imageId(image_id), thumbVersion(thumb_version)
    {
    }

    bool operator<(ThumbId const& other) const
    {
        int const comp_image_id = imageId.filePath().compare(other.imageId.filePath());
        if (comp_image_id < 0) {
            return true;
        }
        else if (comp_image_id > 0) {
            return false;
        }

        return thumbVersion < other.thumbVersion;
    }

    ImageId imageId;
    QString thumbVersion;
};

class ThumbnailPixmapCache::Item
{
public:
    enum Status {
        /**
         * The background threaed hasn't touched it yet.
         */
        QUEUED,

        /**
         * The image is currently being loaded by a background
         * thread, or it has been loaded, but the main thread
         * hasn't yet received the loaded image, or it's currently
         * converting it to a pixmap.
         */
        IN_PROGRESS,

        /**
         * The image was loaded and then converted to a pixmap
         * by the main thread.
         */
        LOADED,

        /**
         * The image could not be loaded.
         */
        LOAD_FAILED
    };

    ThumbId thumbId;

    mutable QPixmap pixmap; /**< Guaranteed to be set if status is LOADED */

    mutable std::vector <
    boost::weak_ptr<CompletionHandler>
    > completionHandlers;

    std::unique_ptr<AbstractThumbnailMaker const> thumbMaker;

    /**
     * The total image loading attempts (of any images) by
     * ThumbnailPixmapCache at the time of the creation of this item.
     * This information is used for request expiration.
     * \see ThumbnailLoadResult::REQUEST_EXPIRED
     */
    int precedingLoadAttempts;

    mutable Status status;

    Item(ThumbId const& thumb_id, int preceding_load_attepmts, Status status,
         AbstractThumbnailMaker const& thumbnail_maker);

    Item(Item const& other);
private:
    Item& operator=(Item const& other); // Assignment is forbidden.
};

class ThumbnailPixmapCache::Impl : public QThread
{
public:
    Impl(QString const& thumb_dir, QSize const& max_thumb_size,
         int max_cached_pixmaps, int expiration_threshold);

    ~Impl();

    void setThumbDir(QString const& thumb_dir);

    Status request(
        ThumbId const& thumb_id, QPixmap& pixmap,
        boost::weak_ptr<CompletionHandler> const* completion_handler,
        AbstractThumbnailMaker const& thumbnail_maker);

    void ensureThumbnailExists(
        ThumbId const& thumb_id, QImage const& image,
        AbstractThumbnailMaker const& thumbnail_maker);

    void recreateThumbnail(
        ThumbId const& thumb_id, QImage const& image,
        AbstractThumbnailMaker const& thumbnail_maker);
protected:
    virtual void run();

    virtual void customEvent(QEvent* e);
private:
    class LoadResultEvent;
    class ItemsByKeyTag;
    class LoadQueueTag;
    class RemoveQueueTag;

    typedef multi_index_container <
    Item,
    indexed_by <
    ordered_unique<tag<ItemsByKeyTag>, member<Item, ThumbId, &Item::thumbId> >,
    sequenced<tag<LoadQueueTag> >,
    sequenced<tag<RemoveQueueTag> >
    >
    > Container;

    typedef Container::index<ItemsByKeyTag>::type ItemsByKey;
    typedef Container::index<LoadQueueTag>::type LoadQueue;
    typedef Container::index<RemoveQueueTag>::type RemoveQueue;

    class BackgroundLoader : public QObject
    {
    public:
        BackgroundLoader(Impl& owner);
    protected:
        virtual void customEvent(QEvent* e);
    private:
        Impl& m_rOwner;
    };

    void backgroundProcessing();

    static QImage loadSaveThumbnail(
        ThumbId const& thumb_id, QString const& thumb_dir,
        QSize const& max_thumb_size,
        AbstractThumbnailMaker const& thumbnail_maker);

    static QString getThumbFilePath(
        ThumbId const& thumb_id, QString const& thumb_dir);

    void queuedToInProgress(LoadQueue::iterator const& lq_it);

    void postLoadResult(
        LoadQueue::iterator const& lq_it, QImage const& image,
        ThumbnailLoadResult::Status status);

    void processLoadResult(LoadResultEvent* result);

    void removeExcessLocked();

    void removeItemLocked(RemoveQueue::iterator const& it);

    mutable QMutex m_mutex;
    BackgroundLoader m_backgroundLoader;
    Container m_items;
    ItemsByKey& m_itemsByKey; /**< ImageId => Item mapping */

    /**
     * An "std::list"-like view of QUEUED items in the order they are
     * going to be loaded.  Actually the list contains all kinds of items,
     * but all QUEUED ones precede any others.  New QUEUED items are added
     * to the front of this list for purposes of request expiration.
     * \see ThumbnailLoadResult::REQUEST_EXPIRED
     */
    LoadQueue& m_loadQueue;

    /**
     * An "std::list"-like view of LOADED items in the order they are
     * going to be removed. Actually the list contains all kinds of items,
     * but all LOADED ones precede any others.  Note that we don't bother
     * removing items without a pixmap, which would be all except LOADED
     * items.  New LOADED items are added after the last LOADED item
     * already present in the list.
     */
    RemoveQueue& m_removeQueue;

    /**
     * An iterator of m_removeQueue that marks the end of LOADED items.
     */
    RemoveQueue::iterator m_endOfLoadedItems;

    QString m_thumbDir;
    QSize m_maxThumbSize;
    int m_maxCachedPixmaps;

    /**
     * \see ThumbnailPixmapCache::ThumbnailPixmapCache()
     */
    int m_expirationThreshold;

    int m_numQueuedItems;
    int m_numLoadedItems;

    /**
     * Total image loading attempts so far.  Used for request expiration.
     * \see ThumbnailLoadResult::REQUEST_EXPIRED
     */
    int m_totalLoadAttempts;

    bool m_threadStarted;
    bool m_shuttingDown;
};

class ThumbnailPixmapCache::Impl::LoadResultEvent : public QEvent
{
public:
    LoadResultEvent(Impl::LoadQueue::iterator const& lq_t,
                    QImage const& image, ThumbnailLoadResult::Status status);

    virtual ~LoadResultEvent();

    Impl::LoadQueue::iterator lqIter() const
    {
        return m_lqIter;
    }

    QImage const& image() const
    {
        return m_image;
    }

    void releaseImage()
    {
        m_image = QImage();
    }

    ThumbnailLoadResult::Status status() const
    {
        return m_status;
    }
private:
    Impl::LoadQueue::iterator m_lqIter;
    QImage m_image;
    ThumbnailLoadResult::Status m_status;
};

/*========================== ThumbnailPixmapCache ===========================*/

ThumbnailPixmapCache::ThumbnailPixmapCache(
    QString const& thumb_dir, QSize const& max_thumb_size,
    int const max_cached_pixmaps, int const expiration_threshold)
    :   m_ptrImpl(
            new Impl(
                RelinkablePath::normalize(thumb_dir), max_thumb_size,
                max_cached_pixmaps, expiration_threshold
            )
        )
{
}

ThumbnailPixmapCache::~ThumbnailPixmapCache()
{
}

void
ThumbnailPixmapCache::setThumbDir(QString const& thumb_dir)
{
    m_ptrImpl->setThumbDir(RelinkablePath::normalize(thumb_dir));
}

ThumbnailPixmapCache::Status
ThumbnailPixmapCache::loadRequest(
    ImageId const& image_id, QString const& version, QPixmap& pixmap,
    boost::weak_ptr<CompletionHandler> const& completion_handler,
    AbstractThumbnailMaker const& thumbnail_maker)
{
    return m_ptrImpl->request(
        ThumbId(image_id, version),
        pixmap, &completion_handler,
        thumbnail_maker);
}

void
ThumbnailPixmapCache::ensureThumbnailExists(
    ImageId const& image_id, QString const& version, QImage const& image,
    AbstractThumbnailMaker const& thumbnail_maker)
{
    m_ptrImpl->ensureThumbnailExists(
        ThumbId(image_id, version),
        image, thumbnail_maker);
}

void
ThumbnailPixmapCache::recreateThumbnail(
    ImageId const& image_id, QString const& version, QImage const& image,
    AbstractThumbnailMaker const& thumbnail_maker)
{
    m_ptrImpl->recreateThumbnail(
        ThumbId(image_id, version),
        image, thumbnail_maker);
}

/*======================= ThumbnailPixmapCache::Impl ========================*/

ThumbnailPixmapCache::Impl::Impl(
    QString const& thumb_dir, QSize const& max_thumb_size,
    int const max_cached_pixmaps, int const expiration_threshold)
    :   m_backgroundLoader(*this),
        m_items(),
        m_itemsByKey(m_items.get<ItemsByKeyTag>()),
        m_loadQueue(m_items.get<LoadQueueTag>()),
        m_removeQueue(m_items.get<RemoveQueueTag>()),
        m_endOfLoadedItems(m_removeQueue.end()),
        m_thumbDir(thumb_dir),
        m_maxThumbSize(max_thumb_size),
        m_maxCachedPixmaps(max_cached_pixmaps),
        m_expirationThreshold(expiration_threshold),
        m_numQueuedItems(0),
        m_numLoadedItems(0),
        m_totalLoadAttempts(0),
        m_threadStarted(false),
        m_shuttingDown(false)
{
    // Note that QDir::mkdir() will fail if the parent directory,
    // that is $OUT/cache doesn't exist. We want that behaviour,
    // as otherwise when loading a project from a different machine,
    // a whole bunch of bogus directories would be created.
    QDir().mkdir(m_thumbDir);

    m_backgroundLoader.moveToThread(this);
}

ThumbnailPixmapCache::Impl::~Impl()
{
    {
        QMutexLocker const locker(&m_mutex);

        if (!m_threadStarted) {
            return;
        }

        m_shuttingDown = true;
    }

    quit();
    wait();
}

void
ThumbnailPixmapCache::Impl::setThumbDir(QString const& thumb_dir)
{
    QMutexLocker locker(&m_mutex);

    if (thumb_dir == m_thumbDir) {
        return;
    }

    m_thumbDir = thumb_dir;

    for (Item const& item : m_loadQueue) {
        // This trick will make all queued tasks to expire.
        m_totalLoadAttempts = std::max(
                                  m_totalLoadAttempts,
                                  item.precedingLoadAttempts + m_expirationThreshold + 1
                              );
    }
}

ThumbnailPixmapCache::Status
ThumbnailPixmapCache::Impl::request(
    ThumbId const& thumb_id, QPixmap& pixmap,
    boost::weak_ptr<CompletionHandler> const* completion_handler,
    AbstractThumbnailMaker const& thumbnail_maker)
{
    assert(QCoreApplication::instance()->thread() == QThread::currentThread());

    QMutexLocker locker(&m_mutex);

    if (m_shuttingDown) {
        return LOAD_FAILED;
    }

    ItemsByKey::iterator const k_it(m_itemsByKey.find(thumb_id));
    if (k_it != m_itemsByKey.end()) {
        if (k_it->status == Item::LOADED) {
            pixmap = k_it->pixmap;

            // Move it after all other candidates for removal.
            RemoveQueue::iterator const rq_it(
                m_items.project<RemoveQueueTag>(k_it)
            );
            m_removeQueue.relocate(m_endOfLoadedItems, rq_it);

            return LOADED;
        } else if (k_it->status == Item::LOAD_FAILED) {
            pixmap = k_it->pixmap;
            return LOAD_FAILED;
        }
    }

    if (!completion_handler) {
        return LOAD_FAILED;
    }

    if (k_it != m_itemsByKey.end()) {
        assert(k_it->status == Item::QUEUED || k_it->status == Item::IN_PROGRESS);
        k_it->completionHandlers.push_back(*completion_handler);

        if (k_it->status == Item::QUEUED) {
            // Because we've got a new request for this item,
            // we move it to the beginning of the load queue.
            // Note that we don't do it for IN_PROGRESS items,
            // because all QUEUED items must precede any other
            // items in the load queue.
            LoadQueue::iterator const lq_it(
                m_items.project<LoadQueueTag>(k_it)
            );
            m_loadQueue.relocate(m_loadQueue.begin(), lq_it);
        }

        return QUEUED;
    }

    // Create a new item.
    LoadQueue::iterator const lq_it(
        m_loadQueue.push_front(
            Item(thumb_id, m_totalLoadAttempts, Item::QUEUED, thumbnail_maker)
        ).first
    );
    // Now our new item is at the beginning of the load queue and at the
    // end of the remove queue.

    assert(lq_it->status == Item::QUEUED);
    assert(lq_it->completionHandlers.empty());

    if (m_endOfLoadedItems == m_removeQueue.end()) {
        m_endOfLoadedItems = m_items.project<RemoveQueueTag>(lq_it);
    }
    lq_it->completionHandlers.push_back(*completion_handler);

    if (m_numQueuedItems++ == 0) {
        if (m_threadStarted) {
            // Wake the background thread up.
            QCoreApplication::postEvent(
                &m_backgroundLoader, new QEvent(QEvent::User)
            );
        } else {
            // Start the background thread.
            start();
            m_threadStarted = true;
        }
    }

    return QUEUED;
}

void
ThumbnailPixmapCache::Impl::ensureThumbnailExists(
    ThumbId const& thumb_id, QImage const& image,
    AbstractThumbnailMaker const& thumbnail_maker)
{
    if (m_shuttingDown) {
        return;
    }

    if (image.isNull()) {
        return;
    }

    QMutexLocker locker(&m_mutex);
    QString const thumb_dir(m_thumbDir);
    QSize const max_thumb_size(m_maxThumbSize);
    locker.unlock();

    QString const thumb_file_path(getThumbFilePath(thumb_id, thumb_dir));
    if (QFile::exists(thumb_file_path)) {
        return;
    }

    QImage const thumbnail(thumbnail_maker.makeThumbnail(image, max_thumb_size));

    AtomicFileOverwriter overwriter;
    QIODevice* iodev = overwriter.startWriting(thumb_file_path);
    if (iodev && thumbnail.save(iodev, "PNG")) {
        overwriter.commit();
    }
}

void
ThumbnailPixmapCache::Impl::recreateThumbnail(
    ThumbId const& thumb_id, QImage const& image,
    AbstractThumbnailMaker const& thumbnail_maker)
{
    if (m_shuttingDown) {
        return;
    }

    if (image.isNull()) {
        return;
    }

    QMutexLocker locker(&m_mutex);
    QString const thumb_dir(m_thumbDir);
    QSize const max_thumb_size(m_maxThumbSize);
    locker.unlock();

    QString const thumb_file_path(getThumbFilePath(thumb_id, thumb_dir));
    QImage const thumbnail(thumbnail_maker.makeThumbnail(image, max_thumb_size));
    bool thumb_written = false;

    // Note that we may be called from multiple threads at the same time.
    AtomicFileOverwriter overwriter;
    QIODevice* iodev = overwriter.startWriting(thumb_file_path);
    if (iodev && thumbnail.save(iodev, "PNG")) {
        thumb_written = overwriter.commit();
    } else {
        overwriter.abort();
    }

    if (!thumb_written) {
        return;
    }

    QMutexLocker const locker2(&m_mutex);

    ItemsByKey::iterator const k_it(m_itemsByKey.find(thumb_id));
    if (k_it == m_itemsByKey.end()) {
        return;
    }

    switch (k_it->status) {
    case Item::LOADED:
    case Item::LOAD_FAILED:
        removeItemLocked(m_items.project<RemoveQueueTag>(k_it));
        break;
    case Item::QUEUED:
        break;
    case Item::IN_PROGRESS:
        // We have a small race condition in this case.
        // We don't know if the other thread has already loaded
        // the thumbnail or not.  In case it did, again we
        // don't know if it loaded the old or new version.
        // Well, let's just pretend the thumnail was loaded
        // (or failed to load) before we wrote the new version.
        break;
    }
}

void
ThumbnailPixmapCache::Impl::run()
{
    backgroundProcessing();
    exec(); // Wait for further processing requests (via custom events).
}

void
ThumbnailPixmapCache::Impl::customEvent(QEvent* e)
{
    processLoadResult(dynamic_cast<LoadResultEvent*>(e));
}

void
ThumbnailPixmapCache::Impl::backgroundProcessing()
{
    // This method is called from a background thread.
    assert(QCoreApplication::instance()->thread() != QThread::currentThread());

    for (;;) {
        try {
            // We are going to initialize these while holding the mutex.
            LoadQueue::iterator lq_it;
            ThumbId thumb_id;
            QString thumb_dir;
            QSize max_thumb_size;
            AbstractThumbnailMaker const* thumbnail_maker;

            {
                QMutexLocker const locker(&m_mutex);

                if (m_shuttingDown || m_items.empty()) {
                    break;
                }

                lq_it = m_loadQueue.begin();
                thumb_id = lq_it->thumbId;
                thumbnail_maker = lq_it->thumbMaker.get();

                if (lq_it->status != Item::QUEUED) {
                    // All QUEUED items precede any other items
                    // in the load queue, so it means there are no
                    // QUEUED items at all.
                    assert(m_numQueuedItems == 0);
                    break;
                }

                // By marking the item as IN_PROGRESS, we prevent it
                // from being processed again before the GUI thread
                // receives our LoadResultEvent.
                queuedToInProgress(lq_it);

                if (m_totalLoadAttempts - lq_it->precedingLoadAttempts
                        > m_expirationThreshold) {

                    // Expire this request.  The reasoning behind
                    // request expiration is described in
                    // ThumbnailLoadResult::REQUEST_EXPIRED
                    // documentation.

                    postLoadResult(
                        lq_it, QImage(),
                        ThumbnailLoadResult::REQUEST_EXPIRED
                    );
                    continue;
                }

                // Expired requests don't count as load attempts.
                ++m_totalLoadAttempts;

                // Copy those while holding the mutex.
                thumb_dir = m_thumbDir;
                max_thumb_size = m_maxThumbSize;
            } // mutex scope

            QImage const image(
                loadSaveThumbnail(thumb_id, thumb_dir, max_thumb_size, *thumbnail_maker)
            );

            ThumbnailLoadResult::Status const status = image.isNull()
                    ? ThumbnailLoadResult::LOAD_FAILED
                    : ThumbnailLoadResult::LOADED;
            postLoadResult(lq_it, image, status);
        } catch (std::bad_alloc const&) {
            OutOfMemoryHandler::instance().handleOutOfMemorySituation();
        }
    }
}

QImage
ThumbnailPixmapCache::Impl::loadSaveThumbnail(
    ThumbId const& thumb_id, QString const& thumb_dir,
    QSize const& max_thumb_size,
    AbstractThumbnailMaker const& thumbnail_maker)
{
    QString const thumb_file_path(getThumbFilePath(thumb_id, thumb_dir));

    QImage image(ImageLoader::load(thumb_file_path, 0));
    if (!image.isNull()) {
        return image;
    }

    image = ImageLoader::load(thumb_id.imageId);
    if (image.isNull()) {
        return QImage();
    }

    QImage const thumbnail(thumbnail_maker.makeThumbnail(image, max_thumb_size));
    thumbnail.save(thumb_file_path, "PNG");

    return thumbnail;
}

QString
ThumbnailPixmapCache::Impl::getThumbFilePath(
    ThumbId const& thumb_id, QString const& thumb_dir)
{
    // Because a project may have several files with the same name (from
    // different directories), we add a hash of the original image path
    // to the thumbnail file name.

    QByteArray const orig_path_hash(
        QCryptographicHash::hash(
            thumb_id.imageId.filePath().toUtf8(), QCryptographicHash::Md5
        ).toHex()
    );
    QString const orig_path_hash_str(
        QLatin1String(orig_path_hash.data(), orig_path_hash.size())
    );

    QFileInfo const orig_img_path(thumb_id.imageId.filePath());
    QString thumb_file_path(thumb_dir);
    thumb_file_path += QChar('/');
    thumb_file_path += orig_img_path.baseName();
    thumb_file_path += QChar('_');
    thumb_file_path += QString::number(thumb_id.imageId.zeroBasedPage());
    thumb_file_path += thumb_id.thumbVersion;
    thumb_file_path += QChar('_');
    thumb_file_path += orig_path_hash_str;
    thumb_file_path += QLatin1String(".png");

    return thumb_file_path;
}

void
ThumbnailPixmapCache::Impl::queuedToInProgress(LoadQueue::iterator const& lq_it)
{
    assert(lq_it->status == Item::QUEUED);
    lq_it->status = Item::IN_PROGRESS;

    assert(m_numQueuedItems > 0);
    --m_numQueuedItems;

    // Move it item to the end of load queue.
    // The point is to keep QUEUED items before any others.
    m_loadQueue.relocate(m_loadQueue.end(), lq_it);

    // Going from QUEUED to IN_PROGRESS doesn't require
    // moving it in the remove queue, as we only remove
    // LOADED items.
}

void
ThumbnailPixmapCache::Impl::postLoadResult(
    LoadQueue::iterator const& lq_it, QImage const& image,
    ThumbnailLoadResult::Status const status)
{
    LoadResultEvent* e = new LoadResultEvent(lq_it, image, status);
    QCoreApplication::postEvent(this, e);
}

void
ThumbnailPixmapCache::Impl::processLoadResult(LoadResultEvent* result)
{
    assert(QCoreApplication::instance()->thread() == QThread::currentThread());

    QPixmap pixmap(QPixmap::fromImage(result->image()));
    result->releaseImage();

    std::vector<boost::weak_ptr<CompletionHandler> > completion_handlers;

    {
        QMutexLocker const locker(&m_mutex);

        if (m_shuttingDown) {
            return;
        }

        LoadQueue::iterator const lq_it(result->lqIter());
        RemoveQueue::iterator const rq_it(
            m_items.project<RemoveQueueTag>(lq_it)
        );

        Item const& item = *lq_it;

        if (result->status() == ThumbnailLoadResult::LOADED
                && pixmap.isNull()) {
            // That's a special case caused by cachePixmapLocked().
            assert(!item.pixmap.isNull());
        } else {
            item.pixmap = pixmap;
        }
        item.completionHandlers.swap(completion_handlers);

        if (result->status() == ThumbnailLoadResult::LOADED) {
            // Maybe remove an older item.
            removeExcessLocked();

            item.status = Item::LOADED;
            ++m_numLoadedItems;

            // Move this item after all other LOADED items in
            // the remove queue.
            m_removeQueue.relocate(m_endOfLoadedItems, rq_it);

            // Move to the end of load queue.
            m_loadQueue.relocate(m_loadQueue.end(), lq_it);
        } else if (result->status() == ThumbnailLoadResult::LOAD_FAILED) {
            // We keep items that failed to load, as they are cheap
            // to keep and helps us avoid trying to load them
            // again and again.

            item.status = Item::LOAD_FAILED;

            // Move to the end of load queue.
            m_loadQueue.relocate(m_loadQueue.end(), lq_it);
        } else {
            assert(result->status() == ThumbnailLoadResult::REQUEST_EXPIRED);

            // Just remove it.
            removeItemLocked(rq_it);
        }
    } // mutex scope

    // Notify listeners.
    ThumbnailLoadResult const load_result(result->status(), pixmap);
    typedef boost::weak_ptr<CompletionHandler> WeakHandler;
    for (WeakHandler const& wh : completion_handlers) {
        boost::shared_ptr<CompletionHandler> const sh(wh.lock());
        if (sh.get()) {
            (*sh)(load_result);
        }
    }
}

void
ThumbnailPixmapCache::Impl::removeExcessLocked()
{
    if (m_numLoadedItems >= m_maxCachedPixmaps) {
        assert(m_numLoadedItems > 0);
        assert(!m_removeQueue.empty());
        assert(m_removeQueue.front().status == Item::LOADED);
        removeItemLocked(m_removeQueue.begin());
    }
}

void
ThumbnailPixmapCache::Impl::removeItemLocked(
    RemoveQueue::iterator const& it)
{
    switch (it->status) {
    case Item::QUEUED:
        assert(m_numQueuedItems > 0);
        --m_numQueuedItems;
        break;
    case Item::LOADED:
        assert(m_numLoadedItems > 0);
        --m_numLoadedItems;
        break;
    default:;
    }

    if (m_endOfLoadedItems == it) {
        ++m_endOfLoadedItems;
    }

    m_removeQueue.erase(it);
}

/*====================== ThumbnailPixmapCache::Item =========================*/

ThumbnailPixmapCache::Item::Item(ThumbId const& thumb_id,
                                 int const preceding_load_attempts, Status const st,
                                 AbstractThumbnailMaker const& thumbnail_maker)
    :   thumbId(thumb_id),
        thumbMaker(thumbnail_maker.clone()),
        precedingLoadAttempts(preceding_load_attempts),
        status(st)
{
}

ThumbnailPixmapCache::Item::Item(Item const& other)
    :   thumbId(other.thumbId),
        pixmap(other.pixmap),
        completionHandlers(other.completionHandlers),
        thumbMaker(other.thumbMaker->clone()),
        precedingLoadAttempts(other.precedingLoadAttempts),
        status(other.status)
{
}

/*=============== ThumbnailPixmapCache::Impl::LoadResultEvent ===============*/

ThumbnailPixmapCache::Impl::LoadResultEvent::LoadResultEvent(
    Impl::LoadQueue::iterator const& lq_it, QImage const& image,
    ThumbnailLoadResult::Status const status)
    :   QEvent(QEvent::User),
        m_lqIter(lq_it),
        m_image(image),
        m_status(status)
{
}

ThumbnailPixmapCache::Impl::LoadResultEvent::~LoadResultEvent()
{
}

/*================== ThumbnailPixmapCache::BackgroundLoader =================*/

ThumbnailPixmapCache::Impl::BackgroundLoader::BackgroundLoader(Impl& owner)
    :   m_rOwner(owner)
{
}

void
ThumbnailPixmapCache::Impl::BackgroundLoader::customEvent(QEvent*)
{
    m_rOwner.backgroundProcessing();
}
