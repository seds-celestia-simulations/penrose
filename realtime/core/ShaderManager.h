#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <set>
#include "core/Shader.h"

enum class MetricType {
    SCHWARZSCHILD_REDUCED
};

class ShaderManager {
public:
    ShaderManager() = default;
    ~ShaderManager() = default;

    void setBasePath(const std::string& path) { m_basePath = path; }
    void loadMetricCompute(MetricType type, const std::string& computePath);
    void setMetric(MetricType type);
    Shader* getActive() const;

private:
    std::string m_basePath;
    std::unordered_map<MetricType, std::string> m_computePaths;
    std::unordered_map<MetricType, std::unique_ptr<Shader>> m_cache;
    MetricType m_activeMetric = MetricType::SCHWARZSCHILD_REDUCED;

    std::string resolveIncludes(const std::string& filePath, std::set<std::string>& visited);
    std::string readFile(const std::string& filePath);
};
