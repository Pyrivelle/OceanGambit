#include "BattleView.h"
#include "../render/BattleScene.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QTimer>
#include <osgViewer/Viewer>

#include <iostream>

namespace {
class BattleView : public QOpenGLWidget {
public:
    BattleView(bool moving, bool check) : moving_(moving), check_(check), scene_(viewer_, moving) {
        viewer_.setThreadingModel(osgViewer::Viewer::SingleThreaded);
        setWindowTitle("OceanGambit | 020528 x2 | Triton");
        resize(1200, 740);
        QObject::connect(&timer_, &QTimer::timeout, this, [this] { update(); });
        timer_.start(16);
    }

    ~BattleView() override {
        if (context() && context()->isValid() && window_) {
            makeCurrent();
            scene_.root().releaseGLObjects(window_->getState());
            doneCurrent();
        }
    }

private:
    void initializeGL() override {
        window_ = new osgViewer::GraphicsWindowEmbedded(0, 0, width(), height());
        auto* state = window_->getState();
        state->setUseVertexAttributeAliasing(true);
        state->setUseModelViewAndProjectionUniforms(true);
        auto* camera = viewer_.getCamera();
        camera->setGraphicsContext(window_);
        camera->setDrawBuffer(GL_COLOR_ATTACHMENT0);
        camera->setReadBuffer(GL_COLOR_ATTACHMENT0);
        camera->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
        camera->setViewMatrixAsLookAt({800, -90, 305}, {0, 0, 7}, {0, 0, 1});
        viewer_.realize();
        clock_.start();
    }

    void paintGL() override {
        const int w = qRound(width() * devicePixelRatioF());
        const int h = qRound(height() * devicePixelRatioF());
        window_->setDefaultFboId(defaultFramebufferObject());
        if (w != width_ || h != height_) {
            width_ = w;
            height_ = h;
            window_->resized(0, 0, w, h);
            viewer_.getCamera()->setViewport(0, 0, w, h);
            viewer_.getCamera()->setProjectionMatrixAsPerspective(48, double(w) / h, .15, 14000);
        }
        auto* gl = context()->functions();
        GLint vao = 0;
        gl->glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        window_->getState()->setCurrentVertexArrayObject(static_cast<GLuint>(vao));
        const double now = check_ ? frame_ / 30.0 : clock_.elapsed() / 1000.0;
        viewer_.advance(now);
        scene_.advance(now);
        viewer_.eventTraversal();
        viewer_.updateTraversal();
        viewer_.renderingTraversals();

        if (check_) {
            if (frame_ == 6) firstSea_ = capture("sea-a.png");
            if (frame_ == 72) {
                const auto secondSea = capture("sea-b.png");
                if (!firstSea_.isNull() && firstSea_.size() == secondSea.size()) {
                    unsigned changed = 0;
                    for (int y = 0; y < secondSea.height(); y += 4) {
                        for (int x = 0; x < secondSea.width() / 5; x += 4)
                            changed += firstSea_.pixel(x, y) != secondSea.pixel(x, y);
                    }
                    seaChanged_ = changed > 100;
                }
            }
            if (frame_ == 25) capture("flight.png");
            if (frame_ == 61) capture("impact.png");
            if (frame_ == 83) capture("impact-smoke.png");
            if (frame_ == 220) capture("splash.png");
        }
        if (check_ && frame_++ == 240) {
            const auto state = scene_.status();
            const bool ok = state.oceanReady && state.decalCounts[0] > 0 && state.decalCounts[1] > 0 &&
                            state.flight && state.splash && seaChanged_ &&
                            state.decalAttached && state.decalMoved == moving_ &&
                            state.explosions == state.decalCounts[0] + state.decalCounts[1];
            std::cout << "Triton=" << scene_.oceanReady()
                      << " ship1_decals=" << state.decalCounts[0]
                      << " ship2_decals=" << state.decalCounts[1]
                      << " flight=" << state.flight << " splash=" << state.splash
                      << " ocean_changed=" << seaChanged_ << " decal_attached=" << state.decalAttached
                      << " explosions=" << state.explosions
                      << " result=" << (ok ? "PASS" : "FAIL") << std::endl;
            QTimer::singleShot(0, this, [ok] { QApplication::exit(ok ? 0 : 1); });
        }
        if (!scene_.oceanReady() && now > 2.0) {
            std::cerr << "Triton initialization failed; check local license and resources." << std::endl;
            QTimer::singleShot(0, this, [] { QApplication::exit(1); });
        }
    }

    QImage capture(const char* name) {
        QImage image(width_, height_, QImage::Format_RGBA8888);
        auto* gl = context()->functions();
        gl->glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
        gl->glReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, image.bits());
        image = image.mirrored();
        const auto dir = qEnvironmentVariable("OCEAN_CAPTURE_DIR");
        if (!dir.isEmpty() && QDir().mkpath(dir)) image.save(QDir(dir).filePath(name));
        return image;
    }

    bool moving_;
    bool check_;
    bool seaChanged_ = false;
    QImage firstSea_;
    int width_ = 0;
    int height_ = 0;
    unsigned frame_ = 0;
    QElapsedTimer clock_;
    QTimer timer_;
    osgViewer::Viewer viewer_;
    osg::ref_ptr<osgViewer::GraphicsWindowEmbedded> window_;
    BattleScene scene_;
};
}

int runBattleView(bool moving, bool check) {
    BattleView view(moving, check);
    view.show();
    return QApplication::exec();
}
