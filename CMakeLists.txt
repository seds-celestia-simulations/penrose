cmake_minimum_required(VERSION 3.22)

# Automatically hook up the local vcpkg toolchain if it exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

project(Penrose)

# Enforce modern C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Fetch and build GLFW automatically (Since it's not using vcpkg)
include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# 2. Find external packages via your local vcpkg
find_package(glm CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)

# 3. Shared cross-module definitions (header-only interface library)
add_library(penrose_shared INTERFACE)
target_include_directories(penrose_shared INTERFACE shared/)
target_link_libraries(penrose_shared INTERFACE Eigen3::Eigen)

# 4. Neutral OpenGL loader for CPU visualization (no realtime/ dependency)
add_library(penrose_glad STATIC vendor/glad/glad.c)
target_include_directories(penrose_glad PUBLIC vendor/glad)

# Realtime GPU loader (unchanged — realtime/gpu layout)
add_library(glad realtime/gpu/glad/glad.c)
target_include_directories(glad PUBLIC realtime/gpu)

# 5. GPU real-time visualization executable
add_executable(Penrose 
    realtime/main.cpp
    realtime/core/Texture.cpp
    realtime/core/Window.cpp
    realtime/core/Framebuffer.cpp
    realtime/core/Engine.cpp
    realtime/core/ShaderManager.cpp
    realtime/render/Renderer.cpp
    realtime/render/GeodesicPass.cpp
    realtime/render/UpscalePass.cpp
    realtime/scene/ParticleBuffer.cpp
    realtime/spacetime/LutBaker.cpp
    realtime/spacetime/AccretionDisk.cpp
)

# Tell the executable where to find realtime headers and vendor (stb)
target_include_directories(Penrose PRIVATE realtime vendor)

# 6. Link everything together
target_link_libraries(Penrose PRIVATE
    glfw
    glad
    glm::glm
    Eigen3::Eigen
    penrose_shared
)

# Copy shaders and resources next to the executable
add_custom_command(TARGET Penrose POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/realtime/shaders"
    "$<TARGET_FILE_DIR:Penrose>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/realtime/resources"
    "$<TARGET_FILE_DIR:Penrose>/resources"
)

# 7. CPU scientific framework library
set(PHYSICS_ENGINE_SOURCES
    physics/metrics/CoordinateChart.cpp
    physics/metrics/SchwarzschildMetric.cpp
    physics/metrics/KerrMetric.cpp
    physics/geodesics/GeodesicDynamics.cpp
    physics/integrators/RK4Integrator.cpp
    physics/simulation/TrajectorySolver.cpp
    physics/simulation/TerminationPolicy.cpp
    physics/simulation/SimulationPipeline.cpp
    physics/simulation/initial_conditions/SchwarzschildInitialStateBuilders.cpp
    physics/simulation/initial_conditions/KerrInitialStateBuilders.cpp
)

add_library(penrose_physics STATIC ${PHYSICS_ENGINE_SOURCES})
target_include_directories(penrose_physics PUBLIC physics)
target_link_libraries(penrose_physics PUBLIC penrose_shared Eigen3::Eigen)

# 8. Physics benchmark — configure in run/benchmark/main.cpp, then run
#    ./build/physics_benchmark
add_executable(physics_benchmark
    run/benchmark/main.cpp
    physics/validation/BenchmarkRunner.cpp
    physics/validation/orbital.cpp
    physics/validation/null_geodesic.cpp
    physics/validation/freefall.cpp
)
target_include_directories(physics_benchmark PRIVATE physics physics/validation)
target_compile_definitions(physics_benchmark PRIVATE PENROSE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(physics_benchmark PRIVATE penrose_physics)

# 9. CPU visualization library (presentation only — no physics headers)
set(VISUALIZATION_SOURCES
    visualization/Geometry/Coordinates.cpp
    visualization/Geometry/Mesh.cpp
    visualization/Trajectory/Trajectory.cpp
    visualization/Trajectory/TrajectoryAdapter.cpp
    visualization/Scene/Scene.cpp
    visualization/Scene/SceneBuilder.cpp
    visualization/Camera/Camera.cpp
    visualization/Renderer/Framebuffer.cpp
    visualization/Renderer/CpuRasterizerBackend.cpp
    visualization/Renderer/CPURasterizer.cpp
    visualization/Presentation/PresentationProfile.cpp
    visualization/Presentation/PresentationDefaults.cpp
    visualization/Presentation/PostProcessor.cpp
    visualization/Presentation/PresentationPipeline.cpp
    visualization/Presentation/VisualizationConfig.cpp
    visualization/Preparation/VisualizationPreparation.cpp
    visualization/IO/CsvTrajectoryLoader.cpp
    visualization/IO/PpmWriter.cpp
    visualization/IO/OutputPaths.cpp
    visualization/IO/StarfieldBackground.cpp
)

add_library(visualization STATIC ${VISUALIZATION_SOURCES})
target_include_directories(visualization
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/visualization
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(visualization PRIVATE PENROSE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(visualization PUBLIC Eigen3::Eigen penrose_shared)

# 9b. GPU trajectory backend (viewer only — no OpenGL in headless export)
set(VISUALIZATION_GPU_SOURCES
    visualization/Renderer/Gpu/GpuShaderProgram.cpp
    visualization/Renderer/Gpu/GpuMeshBuffers.cpp
    visualization/Renderer/Gpu/GpuPolylineBackend.cpp
)

add_library(visualization_gpu STATIC ${VISUALIZATION_GPU_SOURCES})
target_include_directories(visualization_gpu
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/visualization
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(visualization_gpu PRIVATE PENROSE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(visualization_gpu PUBLIC visualization penrose_glad)

# 10. Interactive viewer — configure in run/viewer/main.cpp, then run
#     ./build/visualization_viewer
#     Integrates trajectories in memory (no CSV).
option(PENROSE_BUILD_VIEWER "Build interactive GPU trajectory viewer" ON)
if(PENROSE_BUILD_VIEWER)
    add_executable(visualization_viewer
        run/viewer/main.cpp
        run/adapter/SimulationTrajectoryAdapter.cpp
        visualization/Apps/ViewerApp.cpp
        visualization/Apps/DisplayBlit.cpp
    )
    target_include_directories(visualization_viewer
        PRIVATE physics visualization run ${CMAKE_SOURCE_DIR})
    target_link_libraries(visualization_viewer PRIVATE visualization visualization_gpu penrose_physics glfw penrose_glad)
endif()

# 11. Headless export — configure in run/export/main.cpp, then run
#     ./build/visualization_export
#     Integrates trajectories in memory (no CSV).
add_executable(visualization_export
    run/export/main.cpp
    run/adapter/SimulationTrajectoryAdapter.cpp
)
target_include_directories(visualization_export PRIVATE physics visualization run ${CMAKE_SOURCE_DIR})
target_compile_definitions(visualization_export PRIVATE PENROSE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(visualization_export PRIVATE visualization penrose_physics)

# 12. Internal unit tests (not part of the public three-executable interface)
option(PENROSE_BUILD_TESTS "Build internal visualization unit tests" OFF)
if(PENROSE_BUILD_TESTS)
    add_executable(visualization_test
        visualization/Tests/TestMain.cpp
        visualization/Tests/TestCoordinates.cpp
        visualization/Tests/TestTrajectory.cpp
        visualization/Tests/TestCsv.cpp
        visualization/Tests/TestCamera.cpp
        visualization/Tests/TestRenderer.cpp
    )
    target_link_libraries(visualization_test PRIVATE visualization)
endif()
