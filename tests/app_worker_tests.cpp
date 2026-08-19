#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#include "app/background_worker.h"

namespace {

int g_failures = 0;

void check(bool condition, const char* description) {
    if (condition) {
        std::printf("[PASS] %s\n", description);
    } else {
        std::printf("[FAIL] %s\n", description);
        ++g_failures;
    }
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    // Success path: a task that reports progress 3 times then finishes.
    bool sawProgress = false;
    bool sawFinished = false;
    int lastProcessed = 0;
    int lastTotal = 0;

    {
        QEventLoop loop;
        bitdeck::runInBackground(
            [](bitdeck::TaskProgress& progress) {
                for (int i = 1; i <= 3; ++i) {
                    progress.reportProgress(i, 3);
                }
            },
            [&](int processed, int total) {
                sawProgress = true;
                lastProcessed = processed;
                lastTotal = total;
            },
            [&] {
                sawFinished = true;
                loop.quit();
            },
            [&](QString) { loop.quit(); });

        QTimer::singleShot(2000, &loop, &QEventLoop::quit); // safety timeout
        loop.exec();
    }

    check(sawProgress, "runInBackground: progress callback fired");
    check(lastProcessed == 3 && lastTotal == 3, "runInBackground: last progress values correct");
    check(sawFinished, "runInBackground: finished callback fired");

    // Failure path: a task that throws should invoke onFailed, not onFinished.
    bool sawFailed = false;
    bool sawFinishedOnFailure = false;
    QString failMessage;

    {
        QEventLoop loop;
        bitdeck::runInBackground(
            [](bitdeck::TaskProgress&) { throw std::runtime_error("boom"); }, nullptr,
            [&] {
                sawFinishedOnFailure = true;
                loop.quit();
            },
            [&](QString error) {
                sawFailed = true;
                failMessage = error;
                loop.quit();
            });

        QTimer::singleShot(2000, &loop, &QEventLoop::quit);
        loop.exec();
    }

    check(sawFailed, "runInBackground: failed callback fired on exception");
    check(!sawFinishedOnFailure, "runInBackground: finished callback does not fire on exception");
    check(failMessage == QStringLiteral("boom"), "runInBackground: error message propagated");

    if (g_failures > 0) {
        std::printf("\n%d check(s) FAILED\n", g_failures);
        return EXIT_FAILURE;
    }
    std::printf("\nAll checks passed\n");
    return EXIT_SUCCESS;
}
