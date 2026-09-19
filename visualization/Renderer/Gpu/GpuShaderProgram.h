#pragma once

namespace viz {

class GpuShaderProgram {
public:
    GpuShaderProgram() = default;
    ~GpuShaderProgram();

    GpuShaderProgram(const GpuShaderProgram&) = delete;
    GpuShaderProgram& operator=(const GpuShaderProgram&) = delete;

    bool link(const char* vertex_src, const char* fragment_src,
              const char* geometry_src = nullptr);
    void use() const;
    unsigned int id() const { return program_; }

    void set_float(const char* name, float value) const;
    void set_int(const char* name, int value) const;
    void set_vec2(const char* name, float x, float y) const;
    void set_vec3(const char* name, float x, float y, float z) const;
    void set_vec4(const char* name, float x, float y, float z, float w) const;
    void set_mat4(const char* name, const float* column_major_16) const;

private:
    unsigned int program_ = 0;
};

} // namespace viz
