#include "background_worker.h"

#include <QThread>

namespace bitdeck {

namespace {

class Worker : public QObject {
    Q_OBJECT

public:
    std::function<void(TaskProgress&)> work;
    TaskProgress taskProgress;

public slots:
    void run() {
        try {
            work(taskProgress);
            emit finished();
        } catch (const std::exception& e) {
            emit failed(QString::fromStdString(e.what()));
        }
    }

signals:
    void finished();
    void failed(QString error);
};

} // namespace

void runInBackground(std::function<void(TaskProgress&)> work, std::function<void(int, int)> onProgress,
                      std::function<void()> onFinished, std::function<void(QString)> onFailed) {
    auto* thread = new QThread();
    auto* worker = new Worker();
    worker->work = std::move(work);
    worker->moveToThread(thread);

    QObject::connect(thread, &QThread::started, worker, &Worker::run);
    if (onProgress) {
        QObject::connect(&worker->taskProgress, &TaskProgress::progress, thread,
                          [onProgress](int processed, int total) { onProgress(processed, total); },
                          Qt::QueuedConnection);
    }
    if (onFinished) {
        QObject::connect(worker, &Worker::finished, thread, [onFinished] { onFinished(); }, Qt::QueuedConnection);
    }
    if (onFailed) {
        QObject::connect(worker, &Worker::failed, thread, [onFailed](const QString& error) { onFailed(error); },
                          Qt::QueuedConnection);
    }
    QObject::connect(worker, &Worker::finished, thread, &QThread::quit);
    QObject::connect(worker, &Worker::failed, thread, &QThread::quit);
    QObject::connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    QObject::connect(thread, &QThread::finished, thread, &QThread::deleteLater);

    thread->start();
}

} // namespace bitdeck

#include "background_worker.moc"
