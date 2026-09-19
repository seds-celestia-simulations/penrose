#include "core/ShaderManager.h"
#include <fstream>
#include <sstream>
#include <iostream>

std::string ShaderManager::readFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "ShaderManager: failed to open " << filePath << "\n";
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::string ShaderManager::resolveIncludes(const std::string& filePath, std::set<std::string>& visited) {
    if (visited.count(filePath)) {
        std::cerr << "ShaderManager: circular include detected: " << filePath << "\n";
        return "";
    }
    visited.insert(filePath);

    std::string content = readFile(filePath);
    if (content.empty()) return "";

    std::string result;
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));

        if (trimmed.rfind("#include", 0) == 0) {
            size_t firstQuote = trimmed.find('"');
            size_t lastQuote = trimmed.rfind('"');
            if (firstQuote == std::string::npos || lastQuote == firstQuote) {
                std::cerr << "ShaderManager: malformed #include: " << line << "\n";
                result += line + "\n";
                continue;
            }

            std::string includePath = trimmed.substr(firstQuote + 1, lastQuote - firstQuote - 1);

            std::string dir;
            size_t lastSlash = filePath.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                dir = filePath.substr(0, lastSlash + 1);
            }

            std::string resolved = dir + includePath;
            result += resolveIncludes(resolved, visited);
        } else {
            result += line + "\n";
        }
    }

    return result;
}

void ShaderManager::loadMetricCompute(MetricType type, const std::string& computePath) {
    m_computePaths[type] = computePath;

    const std::string basePath = m_basePath.empty() ? "shaders" : m_basePath;
    const std::string resolvedComputePath = basePath + "/" + computePath;

    std::set<std::string> visited;
    const std::string computeSource = resolveIncludes(resolvedComputePath, visited);
    m_cache[type] = std::make_unique<Shader>(computeSource);
}

void ShaderManager::setMetric(MetricType type) {
    m_activeMetric = type;
}

Shader* ShaderManager::getActive() const {
    auto it = m_cache.find(m_activeMetric);
    if (it == m_cache.end()) return nullptr;
    return it->second.get();
}
