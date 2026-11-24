#pragma once

#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <osg/State>
#include <osg/VertexArrayState>

// SDK 直接调用 OpenGL；恢复 Qt 的 FBO，并使后续 OSG 绘制重新应用状态。
class EnvironmentRenderState {
public:
    explicit EnvironmentRenderState(osg::State& state) : state_(state),
        gl_(QOpenGLContext::currentContext()->extraFunctions())
    {
        gl_->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer_);
        gl_->glGetIntegerv(GL_VIEWPORT, viewport_);
        gl_->glGetIntegerv(GL_CURRENT_PROGRAM, &program_);
        gl_->glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer_);
        gl_->glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &elementBuffer_);
        gl_->glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture_);
#ifdef VEYLAXIS_GLCORE
        gl_->glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao_);
#endif
        if (state_.getCurrentVertexArrayState()) state_.disableAllVertexArrays();
#ifdef VEYLAXIS_GLCORE
        // SDK 初始化也会绘制几何，不能继承 Qt/OSG 的顶点布局和索引绑定。
        gl_->glGenVertexArrays(1, &sdkVao_);
        gl_->glBindVertexArray(sdkVao_);
#endif
        gl_->glBindBuffer(GL_ARRAY_BUFFER, 0);
        gl_->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        gl_->glUseProgram(0);
        gl_->glActiveTexture(GL_TEXTURE0);
    }

    ~EnvironmentRenderState()
    {
        gl_->glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        gl_->glViewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);
        gl_->glUseProgram(program_);
#ifdef VEYLAXIS_GLCORE
        gl_->glBindVertexArray(vao_);
        state_.setCurrentVertexArrayObject(vao_);
        gl_->glDeleteVertexArrays(1, &sdkVao_);
#endif
        gl_->glBindBuffer(GL_ARRAY_BUFFER, arrayBuffer_);
        gl_->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer_);
        gl_->glActiveTexture(activeTexture_);
        if (auto* arrays = state_.getCurrentVertexArrayState()) {
            arrays->dirty();
            arrays->resetBufferObjectPointers();
        }
        state_.dirtyAllVertexArrays();
        state_.dirtyAllModes();
        state_.dirtyAllAttributes();
        state_.setLastAppliedProgramObject(nullptr);
    }

private:
    osg::State& state_;
    QOpenGLExtraFunctions* gl_;
    GLint framebuffer_ = 0, program_ = 0, viewport_[4]{};
    GLint arrayBuffer_ = 0, elementBuffer_ = 0, activeTexture_ = GL_TEXTURE0;
#ifdef VEYLAXIS_GLCORE
    GLint vao_ = 0;
    GLuint sdkVao_ = 0;
#endif
};
