#pragma once

#include <QObject>
#include <QString>

#include <functional>

namespace bitdeck {

// Reporting handle passed into a background task's work function; safe to
// call from the worker thread.
class TaskProgress : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    void reportProgress(int processed, int total) { emit progress(processed, total); }

signals:
    void progress(int processed, int total);
};

// Runs work(TaskProgress&) on a new background thread. If work throws
// std::exception, onFailed runs instead of onFinished. All callbacks run on
// the calling (GUI) thread. The thread is torn down once the task completes.
void runInBackground(std::function<void(TaskProgress&)> work, std::function<void(int, int)> onProgress,
                      std::function<void()> onFinished, std::function<void(QString)> onFailed);

} // namespace bitdeck
