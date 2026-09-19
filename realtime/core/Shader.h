#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

class Shader {
public:
    unsigned int ID;

    Shader(const char* vertexPath, const char* fragmentPath) {
        std::string vertexCode;
        std::string fragmentCode;
        std::ifstream vShaderFile;
        std::ifstream fShaderFile;

        vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        try {
            vShaderFile.open(vertexPath);
            fShaderFile.open(fragmentPath);
            std::stringstream vShaderStream, fShaderStream;
            vShaderStream << vShaderFile.rdbuf();
            fShaderStream << fShaderFile.rdbuf();
            vShaderFile.close();
            fShaderFile.close();
            vertexCode = vShaderStream.str();
            fragmentCode = fShaderStream.str();
        }
        catch (const std::ifstream::failure& e) {
            std::cerr << "Shader: failed to read shader files from disk: " << vertexPath << " / " << fragmentPath << " (" << e.what() << ")\n";
        }

        compile(vertexCode.c_str(), fragmentCode.c_str());
    }

    Shader(const char* vertexPath, const std::string& fragmentSource) {
        std::string vertexCode;
        std::ifstream vShaderFile(vertexPath);
        if (vShaderFile.is_open()) {
            std::stringstream vShaderStream;
            vShaderStream << vShaderFile.rdbuf();
            vertexCode = vShaderStream.str();
        } else {
            std::cerr << "Shader: failed to open vertex shader: " << vertexPath << "\n";
        }

        compile(vertexCode.c_str(), fragmentSource.c_str());
    }

    // Comp shader
    Shader(const std::string& computeSource) {
        const char* cShaderCode = computeSource.c_str();
        unsigned int compute;

        compute = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(compute, 1, &cShaderCode, NULL);
        glCompileShader(compute);
        checkCompileErrors(compute, "COMPUTE");

        ID = glCreateProgram();
        glAttachShader(ID, compute);
        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");

        glDeleteShader(compute);
    }

    Shader(const char* computePath) {
        std::string computeCode;
        std::ifstream cShaderFile;
        
        cShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        try {
            cShaderFile.open(computePath);
            std::stringstream cShaderStream;
            cShaderStream << cShaderFile.rdbuf();
            cShaderFile.close();
            computeCode = cShaderStream.str();
        }
        catch (const std::ifstream::failure& e) {
            std::cerr << "Shader: failed to read compute shader from disk: " << computePath << " (" << e.what() << ")\n";
        }

        *this = Shader(computeCode);
    }

    void use() { glUseProgram(ID); }

    void setFloat(const std::string &name, float value) const { 
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value); 
    }
    void setVec2(const std::string &name, const glm::vec2 &value) const { 
        glUniform2fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]); 
    }
    void setVec3(const std::string &name, const glm::vec3 &value) const { 
        glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]); 
    }
    void setInt(const std::string &name, int value) const { 
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value); 
    }
    void setBool(const std::string& name, bool value) const{
        glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
    }

private:
    void compile(const char* vertexCode, const char* fragmentCode) {
        unsigned int vertex, fragment;

        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vertexCode, NULL);
        glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");

        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fragmentCode, NULL);
        glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT", fragmentCode);

        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");

        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }

    void checkCompileErrors(unsigned int shader, std::string type, const char* fragmentSource = nullptr) {
        int success;
        char infoLog[4096];
        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 4096, NULL, infoLog);
                std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
                if (fragmentSource) {
                    std::cerr << "Fragment source (first 1000 chars):\n" << std::string(fragmentSource, 1000) << "\n -- --------------------------------------------------- -- " << std::endl;
                }
            }
        } else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(shader, 4096, NULL, infoLog);
                std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        }
    }
};
