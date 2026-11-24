#include "BattleView.h"
#include "ShipCameraSettings.h"
#include "../render/BattleScene.h"

#include <QApplication>
#include <QCursor>
#include <QDir>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QElapsedTimer>
#include <QImage>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QTimer>
#include <osgViewer/Viewer>

#include <cmath>
#include <iostream>

namespace {
class BattleView : public QOpenGLWidget {
public:
    BattleView(bool moving, bool check, bool quadratic, bool wows,
               ocean_gambit::ballistics::Integrator integrator, bool overview, bool cameraCheck)
        : moving_(moving), check_(check && !cameraCheck), overview_((overview || check) && !cameraCheck),
          cameraCheck_(cameraCheck),
          scene_(viewer_, moving, quadratic, wows, integrator, !overview_) {
        viewer_.setThreadingModel(osgViewer::Viewer::SingleThreaded);
        setWindowTitle("OceanGambit | 020528 x2 | Triton");
        if (!overview_) {
            cameraSettings_ = loadShipCameraSettings(scene_.shipModelPath(0).value(), scene_.shipBridgeEye(0));
            shipCamera_.follow(0, cameraSettings_);
            setFocusPolicy(Qt::StrongFocus);
            setMouseTracking(true);
            reticle_ = new QLabel("+", this);
            reticle_->setFixedSize(24, 24);
            reticle_->setAlignment(Qt::AlignCenter);
            reticle_->setAttribute(Qt::WA_TransparentForMouseEvents);
            reticle_->setStyleSheet("color: white; background: transparent; font: bold 19px;");
        }
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
    bool looking() const {
        return shipCamera_.binocular() || altLooking_ || temporaryLook_;
    }

    void centerPointer() {
        if (!cameraCheck_) QCursor::setPos(mapToGlobal(rect().center()));
    }

    void updatePointer() {
        if (looking()) {
            setCursor(Qt::BlankCursor);
            centerPointer();
        } else {
            unsetCursor();
        }
    }

    void stopTemporaryLook() {
        temporaryLook_ = false;
        shipCamera_.endTemporaryLook();
    }

    void setBinocular(bool enabled) {
        stopTemporaryLook();
        shipCamera_.setBinocular(enabled);
        scene_.setBridgeHidden(shipCamera_.followedShip(), shipCamera_.binocular());
        updatePointer();
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (!overview_ && event->key() == Qt::Key_Shift) {
            if (!event->isAutoRepeat()) setBinocular(!shipCamera_.binocular());
            event->accept();
            return;
        }
        if (!overview_ && event->key() == Qt::Key_Alt) {
            if (!event->isAutoRepeat()) {
                altLooking_ = true;
                updatePointer();
            }
            event->accept();
            return;
        }
        if (!overview_ && event->key() >= Qt::Key_1 && event->key() <= Qt::Key_2) {
            const ShipID id = static_cast<ShipID>(event->key() - Qt::Key_1);
            if (id != shipCamera_.followedShip()) {
                if (const auto model = scene_.shipModelPath(id)) {
                    const bool binocular = shipCamera_.binocular();
                    setBinocular(false);
                    cameraSettings_ = loadShipCameraSettings(*model, scene_.shipBridgeEye(id));
                    shipCamera_.follow(id, cameraSettings_);
                    scene_.setControlledShip(id);
                    setBinocular(binocular);
                }
            }
            return;
        }
        QOpenGLWidget::keyPressEvent(event);
    }

    void keyReleaseEvent(QKeyEvent* event) override {
        if (!overview_ && event->key() == Qt::Key_Alt) {
            if (!event->isAutoRepeat()) {
                altLooking_ = false;
                updatePointer();
            }
            event->accept();
            return;
        }
        if (!overview_ && event->key() == Qt::Key_Shift) {
            event->accept();
            return;
        }
        QOpenGLWidget::keyReleaseEvent(event);
    }

    void focusInEvent(QFocusEvent* event) override {
        QOpenGLWidget::focusInEvent(event);
        if (!overview_ && looking()) updatePointer();
    }

    void focusOutEvent(QFocusEvent* event) override {
        if (!overview_) {
            altLooking_ = false;
            stopTemporaryLook();
            unsetCursor();
        }
        QOpenGLWidget::focusOutEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (!overview_ && event->button() == Qt::RightButton) {
            temporaryLook_ = true;
            shipCamera_.beginTemporaryLook();
            updatePointer();
            return;
        }
        if (!overview_ && event->button() == Qt::LeftButton) {
            const ShipID shooter = shipCamera_.followedShip();
            if (const auto pose = scene_.shipTransform(shooter)) {
                // During temporary right-button looking, fire along the
                // committed aim, not along the temporary free-look direction.
                const CameraRay ray{shipCamera_.centerRay().origin,
                                    shipCamera_.committedDirection(*pose)};
                if (const auto target = scene_.pickTarget(ray, shooter))
                    scene_.requestPlayerShot(shooter, target->point);
                else
                    scene_.requestPlayerFreeShot(shooter, ray.direction);
            }
            return;
        }
        QOpenGLWidget::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (!overview_ && event->button() == Qt::RightButton) {
            stopTemporaryLook();
            updatePointer();
            return;
        }
        QOpenGLWidget::mouseReleaseEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (!overview_ && looking() && (hasFocus() || cameraCheck_)) {
            const auto delta = event->pos() - rect().center();
            if (!delta.isNull()) {
                shipCamera_.look(delta.x(), delta.y());
                centerPointer();
            }
            return;
        }
        QOpenGLWidget::mouseMoveEvent(event);
    }

    void resizeEvent(QResizeEvent* event) override {
        QOpenGLWidget::resizeEvent(event);
        if (reticle_) reticle_->move(width() / 2 - 12, height() / 2 - 12);
    }

    void updateCamera(double now, double aspect) {
        if (const auto pose = scene_.shipTransform(shipCamera_.followedShip())) {
            shipCamera_.update(*viewer_.getCamera(), *pose, now - cameraTime_, aspect);
            cameraTime_ = now;
            aimTarget_ = scene_.pickTarget(shipCamera_.centerRay(), shipCamera_.followedShip());
            const int state = !aimTarget_ ? 0 : aimTarget_->ship ? 2 : 1;
            if (state != reticleState_) {
                static const char* colors[] = {"#a1a9ad", "#ffffff", "#66e8bd"};
                reticle_->setStyleSheet(QString("color: %1; background: transparent; font: bold 19px;")
                                         .arg(colors[state]));
                reticleState_ = state;
            }
        }
    }

    bool verifyCamera(double aspect) {
        const auto pose = scene_.shipTransform(0).value();
        const auto follow = shipCamera_.centerRay();
        const auto bow = osg::Matrixd::transform3x3(osg::Vec3d(0, 1, 0), pose);
        const auto modelOrigin = pose.getTrans();
        const osg::Vec3d horizontal(follow.direction.x(), follow.direction.y(), 0);
        const bool bowCorrect = horizontal * bow > 0 && (horizontal ^ bow).length() < 1e-6 &&
            (follow.origin - modelOrigin) * bow < 0 && follow.origin.z() > modelOrigin.z();

        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Shift, Qt::ShiftModifier);
        QKeyEvent leave(QEvent::KeyRelease, Qt::Key_Shift, Qt::NoModifier);
        QApplication::sendEvent(this, &enter);
        QApplication::sendEvent(this, &leave);
        updateCamera(0, aspect);
        const auto scope = shipCamera_.centerRay();
        double fov = 0, viewport = 0, nearPlane = 0, farPlane = 0;
        const bool projection = viewer_.getCamera()->getProjectionMatrixAsPerspective(
            fov, viewport, nearPlane, farPlane);
        const bool scopeCorrect = cameraSettings_.bridgeEye && cameraSettings_.binocularFov &&
            shipCamera_.binocular() && projection &&
            std::abs(fov - *cameraSettings_.binocularFov) < 1e-6 &&
            (scope.origin - *cameraSettings_.bridgeEye * pose).length() < 1e-6 &&
            (scope.direction - follow.direction).length() < 1e-6;

        const auto center = rect().center();
        QMouseEvent move(QEvent::MouseMove, QPointF(center + QPoint(30, -20)),
                         Qt::NoButton, Qt::NoButton, Qt::ShiftModifier);
        QApplication::sendEvent(this, &move);
        updateCamera(0, aspect);
        const auto turned = shipCamera_.centerRay();
        const bool mouseCorrect = (turned.direction - scope.direction).length() > .01;

        QApplication::sendEvent(this, &enter);
        QApplication::sendEvent(this, &leave);
        updateCamera(0, aspect);
        const auto returned = shipCamera_.centerRay();
        const bool returnedCorrect = !shipCamera_.binocular() &&
            (returned.direction - turned.direction).length() < 1e-6;

        QKeyEvent altPress(QEvent::KeyPress, Qt::Key_Alt, Qt::AltModifier);
        QKeyEvent altRelease(QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
        QMouseEvent altMove(QEvent::MouseMove, QPointF(center + QPoint(30, -20)),
                            Qt::NoButton, Qt::NoButton, Qt::AltModifier);
        QApplication::sendEvent(this, &altPress);
        QApplication::sendEvent(this, &altMove);
        updateCamera(0, aspect);
        const auto altDirection = shipCamera_.centerRay().direction;
        QApplication::sendEvent(this, &altRelease);
        updateCamera(0, aspect);
        const bool altLocks = (altDirection - returned.direction).length() > .01 &&
            (shipCamera_.centerRay().direction - altDirection).length() < 1e-6;

        QMouseEvent rightPress(QEvent::MouseButtonPress, QPointF(center),
                               Qt::RightButton, Qt::RightButton, Qt::NoModifier);
        QMouseEvent rightMove(QEvent::MouseMove, QPointF(center + QPoint(-40, 25)),
                              Qt::NoButton, Qt::RightButton, Qt::NoModifier);
        QMouseEvent rightRelease(QEvent::MouseButtonRelease, QPointF(center),
                                 Qt::RightButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(this, &rightPress);
        QApplication::sendEvent(this, &rightMove);
        updateCamera(0, aspect);
        const bool temporary = (shipCamera_.centerRay().direction - altDirection).length() > .01 &&
            (shipCamera_.committedDirection(pose) - altDirection).length() < 1e-6;
        QApplication::sendEvent(this, &rightRelease);
        updateCamera(0, aspect);
        const bool rightResets = temporary &&
            (shipCamera_.centerRay().direction - altDirection).length() < 1e-6;

        osg::ref_ptr<osg::Camera> camera = new osg::Camera;
        ShipCameraController test;
        test.follow(1, cameraSettings_);
        test.update(*camera, osg::Matrixd::identity(), 0, aspect);
        const auto initial = test.centerRay();
        const auto rotation = osg::Matrixd::rotate(osg::PI_2, 0, 0, 1);
        const auto moved = rotation * osg::Matrixd::translate(75, 12, 0);
        test.update(*camera, moved, 1, aspect);
        const auto movedRay = test.centerRay();
        const bool followsTransform = cameraSettings_.smoothingSeconds > 0
            ? (movedRay.origin - initial.origin * moved).length() > 0
            : (movedRay.origin - initial.origin * moved).length() < 1e-6;
        const bool transforms = test.followedShip() == 1 && followsTransform &&
            (movedRay.direction - osg::Matrixd::transform3x3(initial.direction, moved)).length() < 1e-6;

        auto rayVector = scene_.shipTransform(1).value();
        const auto point = osg::Vec3d(0, 0, 4) * rayVector;
        const auto side = osg::Vec3d(150, 0, 4) * rayVector;
        auto direction = point - side;
        direction.normalize();
        const auto hull = scene_.pickTarget({side, direction}, 0);
        const auto sea = scene_.pickTarget({{350, 100, 40}, {0, 0, -1}}, 0);
        const auto sky = scene_.pickTarget({{350, 100, 40}, {0, 0, 1}}, 0);
        const bool picking = hull && hull->ship == 1 && sea && !sea->ship &&
            (sea->point - osg::Vec3d(350, 100, 0)).length() < 1e-6 && !sky;
        std::cout << "bow=" << bowCorrect << " scope=" << scopeCorrect << " mouse=" << mouseCorrect
                  << " return=" << returnedCorrect << " transform=" << transforms
                  << " picking=" << picking << " alt_lock=" << altLocks
                  << " right_reset=" << rightResets << std::endl;
        shipCamera_.follow(0, cameraSettings_);
        setBinocular(true);
        return bowCorrect && scopeCorrect && mouseCorrect && returnedCorrect && transforms &&
               picking && altLocks && rightResets;
    }

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
        if (w <= 0 || h <= 0) return;
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
        const double now = (check_ || cameraCheck_) ? frame_ / 30.0 : clock_.elapsed() / 1000.0;
        viewer_.advance(now);
        scene_.advance(now);
        const double aspect = double(w) / h;
        if (!overview_) updateCamera(now, aspect);
        viewer_.eventTraversal();
        viewer_.updateTraversal();
        viewer_.renderingTraversals();

        if (cameraCheck_) {
            if (frame_ == 0) {
                capture("camera-follow.png");
                cameraCheckPassed_ = verifyCamera(aspect);
            } else if (frame_ == 1) {
                capture("camera-binocular.png");
                setBinocular(false);
                QKeyEvent select(QEvent::KeyPress, Qt::Key_2, Qt::NoModifier);
                QApplication::sendEvent(this, &select);
            } else if (frame_ == 2) {
                cameraProbeOrigin_ = shipCamera_.centerRay().origin;
            } else if (frame_ == 22) {
                QMouseEvent fire(QEvent::MouseButtonPress, QPointF(rect().center()),
                                 Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(this, &fire);
            } else if (frame_ == 100) {
                const double travel = (shipCamera_.centerRay().origin - cameraProbeOrigin_).length();
                const bool followed = shipCamera_.followedShip() == 1 &&
                    (moving_ ? travel > .1 : travel < 1e-6);
                const auto state = scene_.status();
                const bool manualFire = state.launches[0] == 1 && state.launches[1] == 1;
                const bool ok = cameraCheckPassed_ && followed && manualFire && state.oceanReady;
                std::cout << "follow_ship_2=" << followed << " manual_fire=" << manualFire
                          << " camera_result=" << (ok ? "PASS" : "FAIL") << std::endl;
                QTimer::singleShot(0, this, [ok] { QApplication::exit(ok ? 0 : 1); });
            }
            ++frame_;
            return;
        }

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
    bool overview_;
    bool cameraCheck_;
    bool cameraCheckPassed_ = false;
    osg::Vec3d cameraProbeOrigin_;
    ShipCameraController shipCamera_;
    ShipCameraSettings cameraSettings_;
    std::optional<CameraTarget> aimTarget_;
    QLabel* reticle_ = nullptr;
    int reticleState_ = -1;
    bool altLooking_ = false;
    bool temporaryLook_ = false;
    double cameraTime_ = 0;
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

int runBattleView(bool moving, bool check, bool quadratic, bool wows,
                  ocean_gambit::ballistics::Integrator integrator, bool overview, bool cameraCheck) {
    BattleView view(moving, check, quadratic, wows, integrator, overview, cameraCheck);
    view.show();
    return QApplication::exec();
}
