# Architectural Feasibility Review — Penrose

> **Historical / non-normative.** Feasibility review of shared GR abstractions vs dual pipelines.
> Does not describe the current `run/` entry points, dual Stage 3 trajectory backends
> (`GpuPolylineBackend` viewer / `CpuRasterizerBackend` export), CPU Kerr scaffolding, or
> realtime Kerr-as-default compute shaders. Claims about GLAD ownership and `examples/` paths are outdated.
> Current reference: [`../ARCHITECTURE.md`](../ARCHITECTURE.md).

**Review type:** Architecture feasibility (no code changes, no runtime verification)  
**Scope:** Snapshot of implementation vs long-term modular General Relativity framework (as of review date)  
**Evidence basis:** `CMakeLists.txt`, `shared/`, `physics/`, `realtime/`, `visualization/`, examples, shaders

---

## 1. Executive Summary

Penrose has achieved **build-level independence** between the Scientific Framework and the Realtime Rendering Engine, and the Scientific side already expresses a credible geodesic simulation stack. It has **not** achieved shared physical abstractions as a living bridge between those engines.

What exists today:

| Claim | Reality |
|---|---|
| Two independent applications | **True** at link/include level |
| Shared GR vocabulary both consume | **False** — `shared/` is consumed by physics/visualization; realtime includes none of it |
| Shared implementations | **Avoided** (good) |
| Extensibility primarily by adding modules | **Partial** — scientific metric/dynamics yes; realtime metrics/shaders no |

The repository most closely resembles:

```
        shared/  (nascent; physics-consumer only)

               ↙

      Scientific Framework          Realtime Rendering Engine
      (metric-driven CPU geodesics) (hardcoded Schwarzschild GPU/CPU)
```

Not yet the intended:

```
              Shared General Relativity
                 ↙              ↘
        Scientific            Realtime
```

**Primary confusion:** directory modularization and header stubs are being treated as architectural completion. Realtime was reorganized (`core/`, `render/`, `spacetime/`, `scene/`) and gained unused seams (`RenderPass`, `ParticleSystem`), but its physics remains Schwarzschild-specific and formulation-divergent from the scientific stack.

---

## 2. Architectural Overview

### Intended identity
A reusable GR framework: shared mathematical language; two independent execution engines.

### Actual identity
Three pipelines with asymmetric maturity:

1. **`physics/`** — scientific geodesic engine (Metric → Dynamics → Integrator → Solver → Validation). This is the strongest GR framework fragment.
2. **`realtime/`** — interactive OpenGL Schwarzschild visualizer with LUT baking and ray-march shaders. Modular *rendering packaging*, monolithic *physics*.
3. **`visualization/`** — CPU scene/raster presentation of trajectories. Consumes shared `State`; composed with physics only at application level.
4. **`shared/`** — header-only seed of GR abstractions. Present, thin, unevenly adopted.

### Architectural shape (implementation)

```
penrose_shared (INTERFACE)
  Metric, GeodesicState(State), Observer, Constants, Units(stub)
        │
        ├── physics sources ──► benchmark_test
        │                    ──► (duplicated into) visualization_example / viewer_example
        │
        ├── visualization lib ──► export / test / viewer apps
        │         └── (viewer apps also need glfw/glad from realtime/gpu)
        │
        └── Penrose executable (realtime)
                  └── does NOT use shared headers in source
                  └── does NOT link physics
```

---

## 3. Dependency Graph

### Build graph (from `CMakeLists.txt`)

```
Eigen3 ──► penrose_shared
                │
                ├── benchmark_test (+ physics/*.cpp listed inline)
                ├── visualization (+ penrose_shared)
                │       ├── visualization_export
                │       ├── visualization_test
                │       └── visualization_viewer (+ glfw, glad; include realtime/gpu)
                ├── visualization_example (+ physics sources + visualization)
                └── Penrose (+ glfw, glad, glm, Eigen3, penrose_shared)
```

**Evidence of independence**
- `Penrose` target source list contains only `realtime/*` — no `physics/` translation units.
- No `#include` of physics APIs under `realtime/`.
- No `#include` of realtime/OpenGL under `physics/`.

**Evidence of residual soft coupling**
- `visualization_viewer` / `DisplayBlit` take GLAD headers from `realtime/gpu`.
- `StarfieldBackground` hardcodes `realtime/resources/starfield_original.jpg`.
- `Penrose` links `penrose_shared` (and thus Eigen) even though realtime sources do not include shared headers — dependency declared, not used.

### Include graph (actual abstraction use)

| Consumer | Uses `shared/Metric` | Uses `shared/State` | Uses `Observer`/`Constants`/`Units` |
|---|---|---|---|
| physics | Yes | Yes | No |
| visualization | No | Yes (adapter only) | No |
| realtime | **No** | **No** | **No** |

---

## 4. Ownership Graph

| Asset | Declared owner | Actual owner / confusion |
|---|---|---|
| `Spacetime::Metric` interface | `shared/` | Used only by physics |
| `SchwarzschildMetric` | physics | Correct |
| Reduced orbit ODE / LUT bake | realtime `spacetime/` | Namespaced `Physics::` inside realtime — ownership leak in naming |
| Christoffel symbols in GLSL | realtime shaders | Parallel physics world, no shared contract |
| `CoordinateChart` | physics | Scientific-only; visualization reimplements spherical→Cartesian locally |
| `Observer` | shared | Unused everywhere |
| `Particle` / `Light` | shared `GeodesicState.h` | Application concepts embedded in “shared state” |
| GPU `Particle` (GLM/SSBO) | realtime scene | Separate type system; no relation to shared `Particle` |
| Horizon termination | physics simulation | Pattern is extensible; only Schwarzschild policy exists |
| RenderPass / ParticleSystem | realtime headers | Seams exist; not used by `Renderer` / concrete systems |

---

## 5. Subsystem Responsibilities

### `shared/` — intended mathematical language
**Should own:** metric interface, geodesic state, coordinates, observers, units, constants, tensors.  
**Should never own:** integrators, solvers, OpenGL, shaders, scene objects.

**Current state**
- Exists as INTERFACE library.
- `Metric` is a single virtual: `christoffel(mu, alpha, beta, X)`.
- `GeodesicState.h` defines `State` plus `Particle`/`Light`.
- `Observer` is a bare struct (`position`, `velocity`) with no frame/tetrad API.
- `Units.h` is an empty placeholder comment.
- `PhysicalConstants.h` exists but is unused by physics/realtime/visualization.
- `math/`, `utilities/`, `common/` are empty placeholders.

### `physics/` — Scientific Framework
**Owns:** concrete metrics, dynamics models, RK4 stepping, trajectory solving, termination, validation drivers, export helpers.  
**Strength:** geodesic equation is metric-driven:

```10:20:physics/geodesics/GeodesicDynamics.cpp
        // Geodesic equation: a^\mu = -\Gamma^\mu_{\alpha\beta} U^\alpha U^\beta
        for (int mu = 0; mu < 4; ++mu) {
            ...
                    double Gamma = metric_.christoffel(mu, alpha, beta, state.X);
```

**Weaknesses**
- Not packaged as a static library; sources are duplicated into multiple targets.
- Placeholder dirs (`cameras/`, `scenes/`) suggest unfinished scientific product surface.
- Analysis is notebooks/plots, not a first-class analysis module API.
- `TrajectorySolver` hardcodes `Integration::stepRK4` — integrator is not injectable.

### `realtime/` — Realtime Rendering Engine
**Owns:** window/GL, shaders, camera, particles, LUT baking, interactive loop.  
**Physics reality:** Schwarzschild constants and ODEs are embedded in Engine, LutBaker, and GLSL (`rs = 0.25`, `dv = -u + 1.5 * rs * u * u`).  
**Does not consume** shared Metric/State/Observer.

### `visualization/` — offline/CPU presentation
**Owns:** scene graph, CPU rasterizer, presentation pipeline, PPM/CSV I/O.  
**Correct composition pattern:** apps wire physics trajectories into viz (`examples/direct_integration/main.cpp`).  
**Schwarzschild anchoring:** horizon projection and coordinate comments assume spherical Schwarzschild conventions.

---

## 6. Public Interfaces

### Real shared surface
| Interface | Quality | Consumers |
|---|---|---|
| `Spacetime::Metric` | Too narrow for long-term GR (Christoffel-only; Eigen-locked) | physics only |
| `State` | Usable 8-component geodesic state; polluted by Particle/Light in same header | physics, visualization |
| `Dynamics::DynamicsModel` | Good evolution-operator seam | physics |
| `Simulation::TerminationPolicy` | Good policy seam | physics |
| `Observer` | Placeholder | none |
| `Constants` / `Units` | Unused / empty | none |

### Realtime “public” seams (declared, not architectural)
| Interface | Status |
|---|---|
| `RenderPass` | Header only; `Renderer` still monolithic `draw(...)` |
| `ParticleSystem` | Header only; `FallingParticleSystem` does not inherit it; `update` has no `Metric` |
| `LutBaker::bakeSchwarzschildLUT` | Concrete Schwarzschild API, not metric-polymorphic |

### Confused interface: shared abstraction vs shared implementation
The project correctly avoids sharing *implementations* between engines. It incorrectly treats *non-use* of shared abstractions by realtime as equivalent to successful separation. Isolation without a shared language is coexistence, not a framework.

---

## 7. Extension Points

### Already workable (scientific)
- New metric class implementing `Spacetime::Metric` + wire into validation/examples.
- New `DynamicsModel` (e.g. non-geodesic force models).
- New `TerminationPolicy`.
- App-level composition of physics → visualization.

### Apparent but non-functional (realtime)
- `RenderPass` / `ParticleSystem` look like extension points but are not on the execution path.
- Shader metric swap would require editing monolithic `reduced.frag` / `quad.frag` and Engine asset loading.

### Missing extension points for stated future
- Coordinate system abstraction in `shared/` (chart still physics-local).
- Integrator abstraction injectable into `TrajectorySolver`.
- Emitter abstraction.
- Analysis/observable pipeline.
- Backend/compute abstraction (CPU vs GPU geodesic generation).
- Plugin registry for metrics.
- GPU-facing metric contract (Christoffel texture / orbitDerivative provider).

---

## 8. CPU/GPU Separation Analysis

### Question: which dependency shape exists?

**Implemented shape:** peer isolation without shared physics consumption.

```
Scientific Framework  ⟂  Realtime Rendering Engine
         ↑
    shared/ (used here only)
```

**Not implemented shape:**

```
           Shared GR
          ↙         ↘
    Scientific     Realtime
```

### Evidence matrix

| Criterion | Finding | Evidence |
|---|---|---|
| Does either app depend on the other’s implementation? | **No** for core engines | CMake + includes |
| Is dependency direction correct? | **Yes** (no inverted deps) | physics ↛ realtime |
| Are abstractions shared? | **Partially declared, asymmetrically used** | realtime: zero shared includes |
| Are implementations shared? | **No** | separate Schwarzschild math paths |
| Is shared layer math-only? | **Mostly, with leaks** | Particle/Light in GeodesicState; Eigen forced |
| Is realtime dependent on scientific impl details? | **No** | independent; also unaware of scientific abstractions |
| Is scientific framework rendering-aware? | **No** in core; soft asset/GPU coupling in viz apps | `realtime/gpu`, starfield path |

### Critical formulation divergence (not just dual implementation)

Scientific path evolves full 4D state via Christoffel symbols.  
Realtime path evolves a reduced 2D orbit ODE (`u,v`) specialized to Schwarzschild equatorial null geodesics, duplicated in C++ (`LutBaker`) and GLSL (`reduced.frag`), plus Newtonian Cartesian gravity for falling particles.

These are **not two implementations of one shared Metric interface**. They are **two different physical formulations**. That is where “shared abstractions” and “shared implementations” are most confused: the modularization plan describes dual Metric implementations; the code has dual *equations*, with no common interface binding them.

### Verdict for Phase 4
Separation of **execution** is successful. Separation through **shared physical abstractions** is not. Current architecture is closer to two products in one repo than to a framework with two backends.

---

## 9. Architectural Strengths

1. **Clean link isolation** between `physics` and `realtime` — the hardest dependency mistake has been avoided.
2. **Scientific geodesic stack is conceptually correct:** Metric → DynamicsModel → RK4 → TrajectorySolver → TerminationPolicy.
3. **Validation suite exists** as first-class scientific ownership (`freefall`, `orbital`, `null_geodesic`).
4. **Visualization composition is healthy:** physics produces states; viz adapts/renders; coupling happens in examples, not libraries.
5. **Preservation of Schwarzschild mathematics** while extracting modules matches the roadmap principle “architecture evolves, not physics.”
6. **Directory intent for realtime** (`core` / `render` / `spacetime` / `scene`) matches a maintainable renderer packaging, even if physics seams are incomplete.
7. **`penrose_shared` INTERFACE target** is the right build primitive for a future shared language.

---

## 10. Hidden Assumptions

| Assumption | Where embedded | Growth restriction |
|---|---|---|
| Schwarzschild radius `rs` is the primary spacetime parameter | Engine, shaders, LutBaker, HorizonTermination, viz horizon | Kerr/FLRW/SGL need qualitatively different parameters |
| Equatorial reduced ODE is “the” null geodesic model | `LutBaker`, `reduced.frag` | Fails for generic 4D metrics / inclined rays / frame dragging |
| Coordinates are Schwarzschild spherical `(t,r,θ,φ)` | physics charts, viz adapter comments, Christoffel indexing | Boyer-Lindquist / isotropic / Cartesian charts not first-class |
| Horizon ≈ sphere of radius `rs` at origin | viz `HorizonProjection`, particle culls, shaders | Weak-field / cosmological / multi-center geometries invalid |
| Eigen `Vector4d` is the universal state representation | shared Metric/State | GPU/Python/plugin backends inherit Eigen or need adapters |
| Fixed-step RK4 is the integrator | `TrajectorySolver` | Adaptive/symplectic methods require solver edits |
| Keplerian / Newtonian particle heuristics are acceptable “physics” | `AccretionDisk`, `FallingParticleSystem` | Not GR; confuses visualization FX with spacetime modules |
| OpenGL 4.3 + GLSL string shaders are the realtime backend | entire realtime stack | CUDA/Vulkan/compute paths have no seam |
| Geometric units everywhere | Units stub | SI bridging for SGL/mission geometry blocked |
| One metric per process, baked at startup | Engine LUT bake once | Runtime metric switching / multi-metric scenes blocked |

---

## 11. Architectural Weaknesses

1. **Asymmetric shared layer adoption** — the defining framework contract is unused by realtime.
2. **Stub seams mistaken for architecture** — `RenderPass`, `ParticleSystem`, `Observer`, `Units` do not constrain design yet.
3. **Metric interface underpowered** — Christoffel-only cannot express metric tensor queries, null constraints, horizons, Killing fields, or GPU parameter packs without expanding the interface (which will churn all implementers).
4. **Duplicated coordinate transforms** — physics `CoordinateChart` vs viz local spherical→Cartesian; no shared chart abstraction.
5. **Ownership naming collision** — `namespace Physics` inside `realtime/spacetime` for LUT/disk code.
6. **Type-system bifurcation** — shared Eigen `Particle` vs realtime GLM/SSBO `Particle`.
7. **Hardcoded integrator in solver** — reduces composability claimed by DynamicsModel separation.
8. **Physics not a library** — source duplication across targets is a packaging smell that will worsen with more metrics/tests.
9. **No analysis abstraction** — scientific roadmap capabilities (deflection, redshift, magnification, datasets) have no module boundary.
10. **Shader physics is monolithic** — new metrics require editing existing GLSL and Engine loaders, not adding modules.
11. **Documentation drift** — `architecture_refactor.md` still claims nothing is shared between pipelines; `shared/` now exists. Plans ahead of / behind code create false confidence.

### No cyclic dependencies found
Build and include graphs are acyclic. The problem is not cycles; it is **non-shared physics language** plus **Schwarzschild concreteness** inside realtime.

---

## 12. Future Stress Test

| Capability | Natural home | Supported today? | Friction |
|---|---|---|---|
| Kerr | physics metric + realtime spacetime/shaders | Scientific: extend Metric (partial). Realtime: rewrite ODE/LUT/shaders | Shared interface likely insufficient (spin, horizon, Boyer-Lindquist); realtime assumes reduced Schwarzschild ODE |
| Multiple metrics | shared Metric + registries | Scientific yes-ish. Realtime no | Engine/`bakeSchwarzschildLUT`/shader constants must change |
| Minkowski / FLRW | physics metrics | Scientific possible if Christoffel API enough | Realtime horizon/disk assumptions break |
| User-defined / plugin metrics | shared + plugin loader | Absent | No registration, no ABI, Eigen-locked headers |
| New coordinate systems | shared charts | Missing from shared; physics-local | Viz + physics + shaders each change |
| Multiple observers | shared Observer | Stub only | Camera ≠ Observer; no tetrad/frame |
| Alternative integrators | physics integrators | DynamicsModel yes; Solver hardcodes RK4 | Modify `TrajectorySolver` |
| GPU compute geodesics | new realtime/compute or shared backend | Absent | Entirely new subsystem; no backend interface |
| Scientific rendering | visualization (+ optional realtime offline mode) | Partial via viz | Fine if kept as consumer of trajectories |
| SGL | scientific analysis + weak-field metric + detector | Absent | Would force new modules *and* currently missing analysis/units/observer depth; realtime not relevant initially |
| Python bindings | wrap shared + physics | Absent | Shared header pollution (Particle/Light) and Eigen types complicate bindings |
| ML dataset generation | physics batch + export | Partial export helpers | No batch experiment/orchestration abstraction |

### Ideal vs actual change mode

| Change | Ideal | Actual today |
|---|---|---|
| Add scientific Kerr | Add module | Mostly add module + likely widen `Metric` |
| Add realtime Kerr | Add spacetime + shader modules | **Modify** Engine, LutBaker API, monolithic shaders |
| Add adaptive integrator | Add integrator module | **Modify** `TrajectorySolver` |
| Add SGL observables | Add analysis modules | Add modules, but also invent missing abstractions |
| Agree CPU/GPU on same metric meaning | Consume shared Metric | **Architectural redesign of realtime physics path** |

---

## 13. Missing Abstractions

Recommend only what materially improves multi-year extensibility:

1. **CoordinateChart in `shared/`**  
   Charts are currently physics-owned and reimplemented in viz. Metrics and observers need chart language independent of engines.

2. **Richer Metric contract (carefully versioned)**  
   At minimum beyond Christoffel: identity/parameters, domain/horizon queries, and a path toward metric/inverse-metric or connection providers suitable for both CPU and GPU baking. Current single-method interface will churn.

3. **Integrator abstraction used by TrajectorySolver**  
   DynamicsModel already exists; integrator injection is the missing half of “evolution operator” architecture.

4. **Emitter (scientific) / SceneSource (realtime) split**  
   Do not force accretion disks into shared. Do define a scientific Emitter for radiation sources; keep realtime particle FX as engine-local, optionally driven by Metric later.

5. **Analysis/Observable pipeline (scientific)**  
   Required for SGL and validation-against-literature without stuffing logic into validation drivers.

6. **Realtime MetricProvider / OrbitModel seam**  
   Not necessarily the Eigen `Metric` class itself on the GPU path, but an explicit contract that LutBaker and shaders implement for a named spacetime. Without this, dual-engine “agreement” remains aspirational.

**Avoid over-engineering now:** full tensor DSL, plugin ABI, multi-backend compute — after Metric/Chart/Integrator/Analysis stabilize.

---

## 14. Refactoring Risks

### Next major refactor (likely inevitable)
**Realtime physics alignment:** make realtime spacetime evolution depend on an explicit spacetime contract (shared Metric and/or GPU MetricProvider), and break monolithic shader physics into metric modules.

- **Why:** Kerr / multi-metric / validation-against-scientific paths cannot be added by dropping files into current realtime.
- **Root cause:** execution isolation without shared physical language; reduced Schwarzschild ODE treated as the renderer’s physics core.
- **Trigger capability:** first non-Schwarzschild interactive metric (almost certainly Kerr), or any attempt to validate realtime deflection against scientific geodesics under one interface.
- **Disruptiveness:** **High** for realtime (Engine, LutBaker, shaders, particle “physics”); **Low–Medium** for scientific stack; **Medium** for shared interface widening.
- **Does current architecture postpone or eliminate it?** It **postpones** pain via isolation and directory cleanup, but **does not eliminate** the redesign. The modularization plan describes this work; directory moves alone have not completed it.

### Secondary refactor risks
- Widening `Metric` once multiple metrics exist → broad scientific churn if done late.
- Packaging physics as a real library (needed for bindings/tests growth).
- Units/SI introduction when SGL mission geometry arrives.

---

## 15. Overall Assessment

### Framework identity
| Side | Identity |
|---|---|
| Scientific (`physics/` + examples + validation) | Emerging **modular GR trajectory framework** (Schwarzschild-complete, metric-extensible in principle) |
| Realtime | Still fundamentally a **Schwarzschild renderer** with modular OpenGL packaging |
| Shared | **Scaffold**, not yet the framework’s lingua franca |
| Whole repo | **Transitional dual-product architecture**, not yet a unified GR framework |

Anchors to original purpose still present: hardcoded `rs`, reduced orbit ODE in C++/GLSL, spherical horizon projection, Keplerian disk FX, Newtonian falling particles, Christoffel expressions copied into `quad.frag`.

### Answers to the two required questions

#### 1. Can Penrose continue to grow primarily through extension rather than architectural modification?

**Not yet — only on the scientific trajectory stack, and only for closely related metrics.**

- Extending scientific Schwarzschild → another Christoffel-expressible metric can largely be additive.
- Extending realtime to new spacetimes, alternative integrators in the solver, SGL analysis, plugins, or GPU compute requires modifying existing cores or introducing missing abstractions first.
- Therefore growth is **not primarily by extension** at repository scale.

#### 2. Does the current implementation successfully separate Scientific and Realtime through shared physical abstractions rather than shared implementations?

**No.**

- It successfully separates them by **non-dependence / non-sharing of implementations** (good isolation).
- It does **not** separate them through **shared physical abstractions**: realtime does not include or implement `shared` Metric/State/Observer; its physics is a parallel Schwarzschild formulation.
- The ideas become confused wherever file layout (`realtime/spacetime/`), namespaces (`Physics::` in realtime), and unused seams (`RenderPass`, `ParticleSystem`) suggest a shared GR architecture that the dependency and call graphs do not realize.

---

### Architect’s bottom line

Penrose is in a healthy transitional state for a scientific rewrite: the CPU geodesic architecture is worth keeping and deepening. The realtime engine is correctly isolated but still a product-era Schwarzschild visualizer. Until realtime (or an explicit GPU MetricProvider) actually consumes a shared spacetime language—and until `Metric`/charts/integrators/analysis are strong enough for Kerr and SGL—the long-term framework goal remains **documented and partially scaffolded, not yet structurally achieved**.