#include "GpuShaderProgram.h"

#include <glad/glad.h>

#include <iostream>

namespace viz {

namespace {

unsigned int compile_shader(unsigned int type, const char* source) {
    const unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    int ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "[viz-gpu] shader compile error: " << log << "\n";
    }
    return shader;
}

} // namespace

GpuShaderProgram::~GpuShaderProgram() {
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }
}

bool GpuShaderProgram::link(const char* vertex_src, const char* fragment_src,
                            const char* geometry_src) {
    const unsigned int vs = compile_shader(GL_VERTEX_SHADER, vertex_src);
    const unsigned int fs = compile_shader(GL_FRAGMENT_SHADER, fragment_src);
    unsigned int gs = 0;
    if (geometry_src != nullptr) {
        gs = compile_shader(GL_GEOMETRY_SHADER, geometry_src);
    }

    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    if (gs != 0) {
        glAttachShader(program_, gs);
    }
    glAttachShader(program_, fs);
    glLinkProgram(program_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (gs != 0) {
        glDeleteShader(gs);
    }

    int linked = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[1024];
        glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
        std::cerr << "[viz-gpu] program link error: " << log << "\n";
        return false;
    }
    return true;
}

void GpuShaderProgram::use() const { glUseProgram(program_); }

void GpuShaderProgram::set_float(const char* name, float value) const {
    glUniform1f(glGetUniformLocation(program_, name), value);
}

void GpuShaderProgram::set_int(const char* name, int value) const {
    glUniform1i(glGetUniformLocation(program_, name), value);
}

void GpuShaderProgram::set_vec2(const char* name, float x, float y) const {
    glUniform2f(glGetUniformLocation(program_, name), x, y);
}

void GpuShaderProgram::set_vec3(const char* name, float x, float y, float z) const {
    glUniform3f(glGetUniformLocation(program_, name), x, y, z);
}

void GpuShaderProgram::set_vec4(const char* name, float x, float y, float z, float w) const {
    glUniform4f(glGetUniformLocation(program_, name), x, y, z, w);
}

void GpuShaderProgram::set_mat4(const char* name, const float* column_major_16) const {
    glUniformMatrix4fv(glGetUniformLocation(program_, name), 1, GL_FALSE, column_major_16);
}

} // namespace viz
