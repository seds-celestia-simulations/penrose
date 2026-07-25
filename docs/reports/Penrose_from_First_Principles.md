# Penrose from First Principles

> **Note (documentation status):** This is a long-form science and implementation walkthrough.
> Mathematical explanations remain useful. Directory, executable, and shader-layout assumptions may be
> dated relative to the current tree (`run/`, `realtime/core`, compute-shader assembly via
> `reduced.comp` with Kerr default `kerr_full.glsl`, dual Stage 3 trajectory backends:
> GPU viewer + CPU export, `physics/analysis/`, CPU Kerr scaffolding not yet pipeline-wired).
> For current architecture and workflows, see [`../ARCHITECTURE.md`](../ARCHITECTURE.md),
> [`../RUNNING.md`](../RUNNING.md), and [`../VISUALIZATION_GUIDE.md`](../VISUALIZATION_GUIDE.md).

This document explains the Penrose repository from the ground
up. It is not a critique, redesign, or optimization plan. Its purpose is
to make the existing mathematics and code understandable.

Penrose's live GPU application ray-marches null geodesics in a compute shader
assembled from modular GLSL under `realtime/shaders/` (entry `reduced.comp`;
default metric `metrics/kerr_full.glsl`). Older passages below that refer to
`shaders/quad.frag` or Schwarzschild-only realtime rendering describe an earlier
layout. The C++ code creates the window, camera, compute output texture, skybox
texture, particle buffer, and frame capture system. The repository also contains
a CPU physics engine and benchmark programs that implement geodesic machinery in
Eigen; those files are not linked into the main `Penrose` executable by
`CMakeLists.txt`.

Throughout this document:

-   A spacetime coordinate is written as $x^\mu$, where the index $\mu$
    can be $0,1,2,3$.
-   Penrose's spherical spacetime coordinates are ordered as 
    $$x^\mu = (t,r,\theta,\phi).$$
-   Penrose's Cartesian rendering coordinates are ordered as 
    $$(t,x,y,z).$$
-   The shader uses 
    $$r_s = 0.25,$$ 
    while the CPU constants file uses 
    $$r_s = 1.0.$$ 
    These are separate current code paths.
-   The shader uses natural relativistic units where $G=c=1$. In these
    units, time, distance, and mass can be compared without carrying
    factors of $G$ and $c$ through every expression.

------------------------------------------------------------------------

## Part 1 - What Problem Is This Project Solving?

### What Is Being Simulated?

Penrose simulates how light appears to bend near a non-rotating,
uncharged, perfectly spherical black hole. More precisely, it computes
the apparent color of each screen pixel by tracing a light ray backward
from the camera through the curved spacetime outside a Schwarzschild
black hole.

In ordinary computer graphics, a pixel color is often found by shooting
a straight ray from the camera into a Euclidean scene. If the ray hits
an object, the object colors the pixel. If the ray misses everything,
the ray samples a background environment.

Penrose follows the same rendering idea but changes one crucial
assumption:

ordinary rendering assumes light rays are straight lines in flat space;
Penrose assumes light rays follow curved paths determined by the
Schwarzschild geometry.

The project is therefore solving this numerical problem:

1.  For each pixel, construct an initial ray direction from the camera.
2.  Interpret that ray as a photon path in spacetime.
3.  Integrate the photon's curved path near a Schwarzschild black hole.
4.  Stop if the photon falls into the black hole, hits a particle, or
    escapes far away.
5.  Assign the pixel color from the result.

The visible image is the collection of those per-pixel photon outcomes.

### What Does "Rendering a Schwarzschild Black Hole" Mean?

A Schwarzschild black hole is the simplest black hole solution in
general relativity. It represents the gravitational field outside a
spherical mass that:

-   does not rotate,
-   has no electric charge,
-   does not change with time,
-   is surrounded by vacuum.

"Rendering" it does not mean drawing a material surface. A black hole
has no solid surface in the usual sense. The important visual features
come from how spacetime affects light:

-   Light rays passing near the black hole are deflected.
-   Rays that cross the event horizon cannot return to the camera.
-   Background stars appear warped because the rays reaching the camera
    came from distorted directions.
-   Multiple apparent images can occur when light wraps around the black
    hole.

Penrose renders those effects by solving the equations of light motion.

### What Is the Simulation Trying to Compute?

For one pixel, Penrose asks:

"If a photon arrives at the camera from this pixel direction, where did
it come from?"

The shader traces the ray backward from the camera. It does not simulate
photons emitted by the camera physically; backward ray tracing is a
rendering trick. Light paths in this static spacetime can be followed
backward because the same geometric curve describes the path. The
orientation of the parameter changes, but the path itself is valid.

The shader's `raymarch` function is the center of that computation:

``` glsl
vec3 raymarch(vec3 ro, vec3 rd) {
    ray R = ray(vec4(0.0, ro), vec4(0.0, rd));
    ...
}
```

Here:

-   `ro` is the camera position in Cartesian coordinates.
-   `rd` is the initial spatial ray direction in Cartesian coordinates.
-   `R.x` is a four-position.
-   `R.u` is a four-velocity-like tangent vector.

The result is a color:

-   black if the ray crosses the horizon cutoff,
-   a particle color if the ray intersects a particle sphere,
-   a skybox texture sample if the ray escapes.

### Why Ordinary Euclidean Rendering Cannot Produce This Effect

Euclidean rendering assumes space is described by ordinary
three-dimensional geometry. A point is $(x,y,z)$, the distance between
nearby points is

$$
ds^2 = dx^2 + dy^2 + dz^2,
$$

and a free ray is a straight line:

$$
\mathbf{x}(\lambda) = \mathbf{x}_0 + \lambda \mathbf{v}.
$$

That means the direction $\mathbf{v}$ never changes unless the ray hits
a surface, refracts, reflects, or scatters.

Near a black hole, this is not physically correct. Gravity is not
implemented as an ordinary force pulling photons sideways in Euclidean
space. In general relativity, gravity changes the geometry of spacetime
itself. "Straightest possible paths" in that geometry can look curved
when described using familiar coordinates.

So Penrose cannot just use:

``` glsl
position += direction * step;
```

for the whole render. Instead it must update both position and direction
according to the geodesic equation:

$$
\frac{d^2 x^\mu}{d\lambda^2}
+
\Gamma^\mu_{\alpha\beta}
\frac{dx^\alpha}{d\lambda}
\frac{dx^\beta}{d\lambda}
=0.
$$

That equation is implemented by `find_acceleration` and `integrate` in
the fragment shader.

### Why Gravity Changes the Path of Light

In Newtonian mechanics, gravity is a force. Massive bodies accelerate
because a gravitational force changes their velocity. That picture
creates a puzzle for light: photons have no rest mass, so why should
gravity affect them?

General relativity answers differently. Gravity is not fundamentally a
force acting in a fixed background space. Instead, mass and energy
change the relationship between nearby spacetime events. Free objects
follow the straightest possible paths in that changed geometry. Light
follows those paths too.

An analogy helps, but only briefly. On a flat map, the shortest route
between two points is a straight line. On Earth, the shortest route
between two cities is a great circle, which can look curved on a flat
map. The airplane is not being pushed sideways by a mysterious map
force; the map is a distorted coordinate representation of curved
geometry.

Near a black hole, spacetime plays the role of the curved surface. A
light ray follows a straightest-possible path in spacetime. When we
describe that path using coordinates like $r,\theta,\phi$, it can bend.

### Why Geodesics Are the Correct Mathematical Object

A geodesic is the generalization of a straight line to curved geometry.

In flat Euclidean space, a straight line has two equivalent meanings:

1.  It locally extremizes distance.
2.  Its tangent vector does not turn as you move along it.

In curved spacetime, these become:

1.  A free massive particle locally extremizes proper time.
2.  A free particle or photon parallel-transports its tangent vector
    along its own path.

For light, proper time is zero, so the second view is more useful: the
photon's direction is carried along the path without any
non-gravitational force turning it. That condition becomes the geodesic
equation.

Penrose integrates null geodesics. "Null" means the spacetime interval
along the ray is zero. That is the mathematical signature of light.

------------------------------------------------------------------------

## Part 2 - Build General Relativity from Scratch

### Inertial Motion

Start with Newton's first law:

an object with no force on it moves in a straight line at constant
speed.

In coordinates, if $\mathbf{x}(t)$ is position and $t$ is time, inertial
motion satisfies

$$
\frac{d^2 \mathbf{x}}{dt^2}=0.
$$

This means velocity is constant:

$$
\frac{d\mathbf{x}}{dt} = \mathbf{v}.
$$

Special relativity modifies this by joining space and time into
spacetime. Instead of a path through space parameterized by time,

$$
\mathbf{x}(t),
$$

we describe a worldline:

$$
x^\mu(\lambda) = (t(\lambda),x(\lambda),y(\lambda),z(\lambda)).
$$

The parameter $\lambda$ labels points along the path. It might be the
proper time of a massive particle, or some other convenient parameter
for light.

### Spacetime

Spacetime is the set of all events. An event is something that happens
at a place and time. In flat special relativity, an event can be labeled
by four coordinates:

$$
(t,x,y,z).
$$

If $c=1$, time and space share the same units. The flat spacetime
interval is

$$
ds^2 = -dt^2 + dx^2 + dy^2 + dz^2.
$$

The minus sign is essential. Spacetime is not just four-dimensional
Euclidean space. Time contributes with the opposite sign from space.

This interval classifies separations:

-   $ds^2<0$: timelike separation, possible path for a massive particle.
-   $ds^2=0$: null separation, possible path for light.
-   $ds^2>0$: spacelike separation, no causal signal can connect the
    events.

Penrose uses this classification when it sets the initial photon time
component `Rp.u.x` from the null constraint.

### Coordinates

Coordinates are labels. They are not the geometry itself.

In flat three-dimensional space, the same point can be described using
Cartesian coordinates:

$$
(x,y,z),
$$

or spherical coordinates:

$$
(r,\theta,\phi),
$$

where

$$
x = r\sin\theta\cos\phi,
$$

$$
y = r\sin\theta\sin\phi,
$$

$$
z = r\cos\theta.
$$

Penrose uses both:

-   the camera and OpenGL world are Cartesian,
-   the Schwarzschild equations are simpler in spherical coordinates.

That is why `quad.frag` contains:

``` glsl
ray cart_to_sph(ray R) { ... }
ray sph_to_cart(ray R) { ... }
```

The physics is not changed by coordinate conversion. Only the labels
change.

### Manifolds, Intuitively

A manifold is a space that may be curved globally but looks like
ordinary coordinate space locally.

Earth's surface is a two-dimensional example. Small neighborhoods look
like flat maps, but globally the surface is curved. You can place
latitude-longitude coordinates on it, but those coordinates have
singularities at the poles and discontinuities at the date line.

Spacetime in general relativity is a four-dimensional manifold. Near any
event, one can choose local coordinates that look approximately like
special relativity. Globally, however, the geometry can be curved.

Schwarzschild spacetime is one such manifold. Penrose uses one
coordinate chart for it:

$$
(t,r,\theta,\phi).
$$

This chart has coordinate problems:

-   at $r=r_s$, the Schwarzschild metric has a coordinate singularity,
-   at $\theta=0$ and $\theta=\pi$, spherical coordinates have polar
    singularities.

The code contains explicit approximations around both.

### Tangent Vectors

A curve is a function:

$$
x^\mu(\lambda).
$$

Its tangent vector is

$$
u^\mu = \frac{dx^\mu}{d\lambda}.
$$

The components of $u^\mu$ describe how each coordinate changes as the
parameter changes.

In Penrose's shader:

``` glsl
struct ray {
    vec4 x;
    vec4 u;
};
```

`x` stores the position $x^\mu$. `u` stores the tangent $u^\mu$.

In spherical coordinates:

``` text
x = (t, r, theta, phi)
u = (u^t, u^r, u^theta, u^phi)
```

The tangent is not merely a three-dimensional direction. It includes a
time component because light moves through spacetime, not only through
space.

### Why Euclidean Distance No Longer Works

Euclidean distance assumes:

$$
ds^2 = dx^2 + dy^2 + dz^2.
$$

But even flat spacetime uses:

$$
ds^2 = -dt^2 + dx^2 + dy^2 + dz^2.
$$

Near a black hole, the coefficients also vary with position. The
Schwarzschild interval is:

$$
ds^2 =
-\left(1-\frac{r_s}{r}\right)dt^2
+\left(1-\frac{r_s}{r}\right)^{-1}dr^2
+r^2 d\theta^2
+r^2\sin^2\theta\,d\phi^2.
$$

So the "squared length" of a tiny displacement depends on:

-   whether the displacement is in time, radial direction, polar
    direction, or azimuthal direction,
-   the current radius $r$,
-   the current polar angle $\theta$.

Euclidean distance cannot express those facts.

### Metric Tensors

A metric tensor is the mathematical object that converts coordinate
displacements into spacetime intervals.

In coordinates, the metric is written as $g_{\mu\nu}$. The interval is:

$$
ds^2 = g_{\mu\nu} dx^\mu dx^\nu.
$$

Einstein summation convention is used here: if an index appears once up
and once down, it is summed over all coordinate values. So the
expression means:

$$
ds^2 =
\sum_{\mu=0}^{3}\sum_{\nu=0}^{3}
g_{\mu\nu} dx^\mu dx^\nu.
$$

For a diagonal metric, only terms with $\mu=\nu$ remain:

$$
ds^2 =
g_{tt}dt^2+
g_{rr}dr^2+
g_{\theta\theta}d\theta^2+
g_{\phi\phi}d\phi^2.
$$

For Schwarzschild coordinates:

$$
g_{tt}= -\left(1-\frac{r_s}{r}\right),
$$

$$
g_{rr}= \left(1-\frac{r_s}{r}\right)^{-1},
$$

$$
g_{\theta\theta}=r^2,
$$

$$
g_{\phi\phi}=r^2\sin^2\theta.
$$

Every Christoffel symbol in Penrose's `find_acceleration` comes from
derivatives of these metric components.

### Proper Time

For a massive particle, proper time $\tau$ is the time measured by a
clock traveling with the particle. Along a timelike worldline:

$$
d\tau^2 = -ds^2
$$

when $c=1$ and the metric signature is $(- + + +)$.

The four-velocity of a massive particle is:

$$
u^\mu = \frac{dx^\mu}{d\tau}.
$$

Its norm is always:

$$
g_{\mu\nu}u^\mu u^\nu = -1.
$$

The CPU benchmark `orbital.cpp` uses exactly this condition:

``` cpp
// -(1-rs/r)*vt^2 + (1/(1-rs/r))*vr^2 + r^2*vph^2 = -1
```

That benchmark is for massive particles, not photons.

### Line Elements and Intervals

The expression

$$
ds^2 = g_{\mu\nu}dx^\mu dx^\nu
$$

is called the line element. It tells us the infinitesimal interval
between nearby events.

For a curve $x^\mu(\lambda)$, divide by $d\lambda^2$:

$$
\left(\frac{ds}{d\lambda}\right)^2
=
g_{\mu\nu}
\frac{dx^\mu}{d\lambda}
\frac{dx^\nu}{d\lambda}.
$$

Since

$$
u^\mu = \frac{dx^\mu}{d\lambda},
$$

we get:

$$
g_{\mu\nu}u^\mu u^\nu
$$

as the squared norm of the tangent.

For light:

$$
ds^2=0,
$$

so

$$
g_{\mu\nu}u^\mu u^\nu=0.
$$

This is the null constraint used in `quad.frag`.

------------------------------------------------------------------------

## Part 3 - Derive the Schwarzschild Metric

### The Physical Assumptions

The Schwarzschild metric is the unique vacuum, spherically symmetric,
static solution to Einstein's field equations. Each adjective matters.

#### Vacuum

Vacuum means the region being described contains no matter or
non-gravitational energy density:

$$
T_{\mu\nu}=0.
$$

The black hole mass is still present, but outside the central
object/horizon the local stress-energy tensor is zero.

#### Spherical Symmetry

Spherical symmetry means no direction around the center is special.
Rotating the coordinate angles should not change the physical geometry.

This strongly constrains the angular part of the metric. A sphere of
radius $r$ has ordinary angular line element:

$$
r^2 d\theta^2 + r^2\sin^2\theta\,d\phi^2.
$$

That is why the Schwarzschild metric contains those two terms.

#### Static Spacetime

Static means the geometry does not change with time and has no
time-space cross terms like $dt\,dr$. The metric coefficients are
independent of $t$.

So the most general simple form compatible with these assumptions can be
written:

$$
ds^2 = -A(r)dt^2 + B(r)dr^2 + r^2d\theta^2 + r^2\sin^2\theta\,d\phi^2.
$$

Here $A(r)$ and $B(r)$ are unknown functions. Symmetry tells us they
depend only on $r$.

### The Vacuum Field Equation Result

Einstein's field equations are:

$$
G_{\mu\nu} = 8\pi T_{\mu\nu}.
$$

In vacuum:

$$
T_{\mu\nu}=0,
$$

so:

$$
G_{\mu\nu}=0.
$$

For the static, spherically symmetric ansatz above, solving these
equations gives:

$$
A(r)=1-\frac{r_s}{r},
$$

$$
B(r)=\left(1-\frac{r_s}{r}\right)^{-1}.
$$

The constant $r_s$ is determined by matching the weak-field limit to
Newtonian gravity. In ordinary units,

$$
r_s = \frac{2GM}{c^2}.
$$

In Penrose's units $G=c=1$, this becomes:

$$
r_s = 2M.
$$

Therefore:

$$
ds^2 =
-\left(1-\frac{r_s}{r}\right)dt^2
+\left(1-\frac{r_s}{r}\right)^{-1}dr^2
+r^2d\theta^2
+r^2\sin^2\theta\,d\phi^2.
$$

This is the metric Penrose uses.

### Why Each Term Appears

#### The time term

$$
-\left(1-\frac{r_s}{r}\right)dt^2
$$

Far away, $r\to\infty$, so $r_s/r\to 0$, and the term becomes:

$$
-dt^2.
$$

That matches flat spacetime.

Near the horizon, $r\to r_s$, the coefficient approaches zero. In
Schwarzschild coordinates, this means coordinate time $t$ behaves
pathologically at the horizon. A far-away observer sees infalling
objects appear to slow near $r_s$.

#### The radial term

$$
\left(1-\frac{r_s}{r}\right)^{-1}dr^2
$$

Far away, it becomes:

$$
dr^2.
$$

Near the horizon, the coefficient diverges. This is the coordinate
singularity at $r=r_s$. The divergence is not a physical infinite
curvature there; it is a failure of Schwarzschild coordinates.

#### The angular terms

$$
r^2d\theta^2 + r^2\sin^2\theta\,d\phi^2
$$

These are the standard spherical-coordinate angular distances. At fixed
$r$, moving by a small angle $d\theta$ covers distance $r\,d\theta$, so
the squared distance is $r^2d\theta^2$. Moving azimuthally by $d\phi$ at
polar angle $\theta$ covers a circle of radius $r\sin\theta$, so the
squared distance is $r^2\sin^2\theta\,d\phi^2$.

These terms are responsible for several Christoffel symbols even in flat
spherical coordinates. Spherical coordinates themselves have coordinate
effects.

### Event Horizon

The event horizon is at:

$$
r=r_s.
$$

It is the boundary beyond which future-directed light cannot escape to
infinity.

In Penrose, the shader tests:

``` glsl
if(length(Rp.x[1]) < rs*1.1) {
    return vec3(0.0, 0.0, 0.0);
}
```

Since `Rp.x[1]` is the spherical radius $r$, the shader colors rays
black when they get within $1.1r_s$. The factor $1.1$ is a
rendering/numerical safety margin. It stops integration slightly outside
the true horizon.

### Coordinate Singularity vs Physical Singularity

At $r=r_s$, Schwarzschild coordinates become singular because terms like

$$
\frac{1}{r-r_s}
$$

appear in the Christoffel symbols. This is a coordinate singularity.

At $r=0$, curvature itself diverges. That is a physical singularity.

Penrose never tries to integrate through either:

-   the shader stops near $r_s$,
-   rays that fall inward are colored black.

------------------------------------------------------------------------

## Part 4 - Motion in Curved Spacetime

### What Is a Geodesic?

A geodesic is the path whose tangent vector is transported along itself
without turning relative to the local geometry.

In flat Cartesian coordinates, this condition reduces to:

$$
\frac{d^2x^\mu}{d\lambda^2}=0.
$$

That is straight-line motion.

In curved coordinates or curved spacetime, the same idea requires
correction terms because coordinate basis vectors can change from point
to point. The corrected equation is:

$$
\frac{d^2x^\mu}{d\lambda^2}
+
\Gamma^\mu_{\alpha\beta}
\frac{dx^\alpha}{d\lambda}
\frac{dx^\beta}{d\lambda}
=0.
$$

### Why Free Motion Follows Geodesics

In general relativity, a freely falling object feels no proper
acceleration. An accelerometer carried by it reads zero. Its motion is
inertial locally.

Because spacetime may be curved, a path that is locally inertial may not
look globally straight in a chosen coordinate system. The geodesic
equation expresses "no force" in a way that remains meaningful in
arbitrary coordinates.

For Penrose:

-   photons are not pushed by an explicit force,
-   their apparent bending comes from the Christoffel terms,
-   those terms are built from the Schwarzschild metric.

### Affine Parameters

The geodesic equation uses a parameter $\lambda$. For massive particles,
a natural parameter is proper time $\tau$. For light, proper time is
always zero along the path, so it cannot parameterize progress.

Instead, light uses an affine parameter $\lambda$. An affine parameter
is any parameter for which the geodesic equation has the simple form:

$$
\frac{d^2x^\mu}{d\lambda^2}
+
\Gamma^\mu_{\alpha\beta}
\frac{dx^\alpha}{d\lambda}
\frac{dx^\beta}{d\lambda}
=0.
$$

If $\lambda$ is affine, then $a\lambda+b$ is also affine for constants
$a,b$. The scale of $\lambda$ is therefore arbitrary for photons. The
shader's `dL` is a numerical step size in this arbitrary parameter, not
a physical clock tick.

### Four-Position and Four-Velocity

The four-position is:

$$
x^\mu(\lambda)=(t(\lambda),r(\lambda),\theta(\lambda),\phi(\lambda)).
$$

The four-velocity-like tangent is:

$$
u^\mu = \frac{dx^\mu}{d\lambda}
=
\left(
\frac{dt}{d\lambda},
\frac{dr}{d\lambda},
\frac{d\theta}{d\lambda},
\frac{d\phi}{d\lambda}
\right).
$$

In the shader:

``` glsl
struct ray {
    vec4 x;
    vec4 u;
};
```

The code uses array positions:

``` text
x[0] = t        u[0] = u^t
x[1] = r        u[1] = u^r
x[2] = theta    u[2] = u^theta
x[3] = phi      u[3] = u^phi
```

### Why Ordinary Derivatives Fail

Suppose a vector has components $V^\mu$. In flat Cartesian coordinates,
the derivative $\partial_\alpha V^\mu$ measures how the vector changes
as we move in coordinate direction $x^\alpha$.

But in curved coordinates, the coordinate basis vectors themselves
change. Spherical coordinates show this even in flat space: the radial
basis direction $\hat{r}$ points in different Cartesian directions at
different angles.

So a change in vector components can come from two sources:

1.  the physical vector changed,
2.  the coordinate basis changed.

The ordinary derivative does not separate these.

### Covariant Derivative

The covariant derivative fixes this by adding correction terms:

$$
\nabla_\alpha V^\mu =
\partial_\alpha V^\mu
+
\Gamma^\mu_{\alpha\beta}V^\beta.
$$

Here:

-   $\nabla_\alpha$ is the covariant derivative in direction $\alpha$,
-   $\partial_\alpha$ is the ordinary partial derivative with respect to
    $x^\alpha$,
-   $\Gamma^\mu_{\alpha\beta}$ are Christoffel symbols,
-   $V^\beta$ is the vector being differentiated.

The Christoffel symbols encode how the coordinate basis changes and how
the metric varies.

### Parallel Transport

Parallel transport means moving a vector along a curve while keeping it
as "constant" as the geometry allows.

For a curve $x^\mu(\lambda)$, the directional covariant derivative along
the curve is:

$$
\frac{D V^\mu}{D\lambda}
=
\frac{dx^\alpha}{d\lambda}\nabla_\alpha V^\mu.
$$

Substitute the covariant derivative:

$$
\frac{D V^\mu}{D\lambda}
=
\frac{dx^\alpha}{d\lambda}
\left(
\partial_\alpha V^\mu
+
\Gamma^\mu_{\alpha\beta}V^\beta
\right).
$$

Using the chain rule:

$$
\frac{dx^\alpha}{d\lambda}\partial_\alpha V^\mu
=
\frac{dV^\mu}{d\lambda}.
$$

So:

$$
\frac{D V^\mu}{D\lambda}
=
\frac{dV^\mu}{d\lambda}
+
\Gamma^\mu_{\alpha\beta}
\frac{dx^\alpha}{d\lambda}
V^\beta.
$$

For a geodesic, the tangent vector is parallel transported along itself.
Set $V^\mu=u^\mu$:

$$
\frac{D u^\mu}{D\lambda}=0.
$$

Since

$$
u^\mu = \frac{dx^\mu}{d\lambda},
$$

we have

$$
\frac{du^\mu}{d\lambda}
=
\frac{d^2x^\mu}{d\lambda^2}.
$$

Then:

$$
0=
\frac{d^2x^\mu}{d\lambda^2}
+
\Gamma^\mu_{\alpha\beta}
\frac{dx^\alpha}{d\lambda}
\frac{dx^\beta}{d\lambda}.
$$

This is the geodesic equation.

### Every Symbol in the Geodesic Equation

$$
\frac{d^2x^\mu}{d\lambda^2}
+
\Gamma^\mu_{\alpha\beta}
\frac{dx^\alpha}{d\lambda}
\frac{dx^\beta}{d\lambda}
=0
$$

means:

-   $x^\mu$: the four coordinates of the photon or particle.
-   $\mu$: which component of the equation we are computing. There are
    four equations.
-   $\lambda$: the affine parameter along the path.
-   $dx^\alpha/d\lambda$: component $\alpha$ of the tangent vector.
-   $\Gamma^\mu_{\alpha\beta}$: connection coefficient telling how
    coordinates/geometry alter the $\mu$-component when directions
    $\alpha$ and $\beta$ are involved.
-   Repeated $\alpha$ and $\beta$: summed over $0,1,2,3$.

The shader stores

$$
\frac{dx^\mu}{d\lambda}
$$

as `R.u`, and computes

$$
\frac{du^\mu}{d\lambda}
=
-
\Gamma^\mu_{\alpha\beta}u^\alpha u^\beta
$$

inside `find_acceleration`.

------------------------------------------------------------------------

## Part 5 - Christoffel Symbols

### Why Christoffel Symbols Exist

Christoffel symbols appear because coordinates are local labels, and
their basis vectors vary from point to point. They tell the geodesic
equation how a vector that is not turning physically can still have
changing coordinate components.

They are not tensors. Their values can be nonzero even in flat space if
the coordinate system is curvilinear. For example, ordinary spherical
coordinates have Christoffel symbols because $\hat{r}$, $\hat{\theta}$,
and $\hat{\phi}$ change with position.

In Schwarzschild spacetime, the Christoffel symbols include both:

-   coordinate effects from spherical coordinates,
-   real gravitational effects from metric coefficients depending on
    $r$.

### Inverse Metric

The metric $g_{\mu\nu}$ acts like a matrix. Its inverse is $g^{\mu\nu}$,
defined by:

$$
g^{\mu\nu}g_{\nu\sigma}=\delta^\mu_{\ \sigma}.
$$

$\delta^\mu_{\ \sigma}$ is the identity matrix:

$$
\delta^\mu_{\ \sigma} =
\begin{cases}
1 & \mu=\sigma,\\
0 & \mu\ne\sigma.
\end{cases}
$$

For the diagonal Schwarzschild metric, the inverse is easy:

$$
g^{tt}=-\left(1-\frac{r_s}{r}\right)^{-1},
$$

$$
g^{rr}=1-\frac{r_s}{r},
$$

$$
g^{\theta\theta}=\frac{1}{r^2},
$$

$$
g^{\phi\phi}=\frac{1}{r^2\sin^2\theta}.
$$

### Deriving the Christoffel Formula

The Christoffel symbols for the Levi-Civita connection are determined by
two requirements:

1.  The connection is torsion-free: $$
    \Gamma^\mu_{\alpha\beta}=\Gamma^\mu_{\beta\alpha}.
    $$
2.  The metric is compatible with parallel transport: $$
    \nabla_\alpha g_{\beta\gamma}=0.
    $$

Metric compatibility expands to:

$$
\partial_\alpha g_{\beta\gamma}
-
\Gamma^\nu_{\alpha\beta}g_{\nu\gamma}
-
\Gamma^\nu_{\alpha\gamma}g_{\beta\nu}
=0.
$$

Write three versions by cycling indices:

$$
\partial_\alpha g_{\beta\gamma}
=
\Gamma^\nu_{\alpha\beta}g_{\nu\gamma}
+
\Gamma^\nu_{\alpha\gamma}g_{\beta\nu},
$$

$$
\partial_\beta g_{\alpha\gamma}
=
\Gamma^\nu_{\beta\alpha}g_{\nu\gamma}
+
\Gamma^\nu_{\beta\gamma}g_{\alpha\nu},
$$

$$
\partial_\gamma g_{\alpha\beta}
=
\Gamma^\nu_{\gamma\alpha}g_{\nu\beta}
+
\Gamma^\nu_{\gamma\beta}g_{\alpha\nu}.
$$

Add the first two and subtract the third:

$$
\partial_\alpha g_{\beta\gamma}
+
\partial_\beta g_{\alpha\gamma}
-
\partial_\gamma g_{\alpha\beta}
=
2\Gamma^\nu_{\alpha\beta}g_{\nu\gamma}.
$$

This simplification uses symmetry of the metric and torsion-free
symmetry of the Christoffel symbols. Now multiply by the inverse metric
$g^{\mu\gamma}$:

$$
g^{\mu\gamma}
\left(
\partial_\alpha g_{\beta\gamma}
+
\partial_\beta g_{\alpha\gamma}
-
\partial_\gamma g_{\alpha\beta}
\right)
=
2g^{\mu\gamma}g_{\nu\gamma}\Gamma^\nu_{\alpha\beta}.
$$

Since

$$
g^{\mu\gamma}g_{\nu\gamma}=\delta^\mu_{\ \nu},
$$

the right side becomes:

$$
2\Gamma^\mu_{\alpha\beta}.
$$

Therefore:

$$
\Gamma^\mu_{\alpha\beta}
=
\frac12
g^{\mu\nu}
\left(
\partial_\alpha g_{\beta\nu}
+
\partial_\beta g_{\alpha\nu}
-
\partial_\nu g_{\alpha\beta}
\right).
$$

That is the formula used mathematically to get the hard-coded
expressions in Penrose.

### Partial Derivatives

$\partial_\alpha$ means differentiate with respect to coordinate
$x^\alpha$:

$$
\partial_\alpha = \frac{\partial}{\partial x^\alpha}.
$$

Since the Schwarzschild metric coefficients depend only on $r$ and
$\theta$, most derivatives vanish:

-   no coefficient depends on $t$,
-   no coefficient depends on $\phi$,
-   $g_{tt}$ and $g_{rr}$ depend on $r$,
-   $g_{\theta\theta}$ depends on $r$,
-   $g_{\phi\phi}$ depends on $r$ and $\theta$.

This is why only a small number of Christoffel symbols appear in the
code.

### Computing the Symbols Used by Penrose

Let

$$
f(r)=1-\frac{r_s}{r}=\frac{r-r_s}{r}.
$$

The metric components are:

$$
g_{tt}=-f,
\quad
g_{rr}=f^{-1},
\quad
g_{\theta\theta}=r^2,
\quad
g_{\phi\phi}=r^2\sin^2\theta.
$$

The inverse components are:

$$
g^{tt}=-f^{-1},
\quad
g^{rr}=f,
\quad
g^{\theta\theta}=r^{-2},
\quad
g^{\phi\phi}=(r^2\sin^2\theta)^{-1}.
$$

Useful derivatives:

$$
\partial_r f = \frac{r_s}{r^2},
$$

$$
\partial_r g_{tt} = -\frac{r_s}{r^2},
$$

$$
\partial_r g_{rr}
=
\partial_r(f^{-1})
=
-f^{-2}\partial_r f
=
-\frac{r_s}{r^2 f^2},
$$

$$
\partial_r g_{\theta\theta}=2r,
$$

$$
\partial_r g_{\phi\phi}=2r\sin^2\theta,
$$

$$
\partial_\theta g_{\phi\phi}=2r^2\sin\theta\cos\theta.
$$

Now derive each symbol.

#### $\Gamma^t_{\ tr}$

Use:

$$
\Gamma^t_{\ tr}
=
\frac12 g^{tt}\partial_r g_{tt}.
$$

Substitute:

$$
\Gamma^t_{\ tr}
=
\frac12
\left(-\frac{1}{f}\right)
\left(-\frac{r_s}{r^2}\right)
=
\frac{r_s}{2r^2 f}.
$$

Since

$$
r^2f = r^2\left(\frac{r-r_s}{r}\right)=r(r-r_s),
$$

$$
\Gamma^t_{\ tr}
=
\frac{r_s}{2r(r-r_s)}.
$$

Shader:

``` glsl
float Tt_tr = rs/(2*r*(r-rs));
```

#### $\Gamma^r_{\ tt}$

Use:

$$
\Gamma^r_{\ tt}
=
-\frac12 g^{rr}\partial_r g_{tt}.
$$

Substitute:

$$
\Gamma^r_{\ tt}
=
-\frac12 f\left(-\frac{r_s}{r^2}\right)
=
\frac{fr_s}{2r^2}.
$$

Since $f=(r-r_s)/r$:

$$
\Gamma^r_{\ tt}
=
\frac{r_s(r-r_s)}{2r^3}.
$$

Shader:

``` glsl
float Tr_tt = rs * (r-rs)/(2.0*r*r*r);
```

#### $\Gamma^r_{\ rr}$

Use:

$$
\Gamma^r_{\ rr}
=
\frac12 g^{rr}\partial_r g_{rr}.
$$

Substitute:

$$
\Gamma^r_{\ rr}
=
\frac12 f\left(-\frac{r_s}{r^2 f^2}\right)
=
-\frac{r_s}{2r^2f}.
$$

Again $r^2f=r(r-r_s)$, so:

$$
\Gamma^r_{\ rr}
=
-\frac{r_s}{2r(r-r_s)}.
$$

Shader:

``` glsl
float Tr_rr = -rs/(2.0*r*(r-rs));
```

#### $\Gamma^r_{\ \theta\theta}$

Use:

$$
\Gamma^r_{\ \theta\theta}
=
-\frac12 g^{rr}\partial_r g_{\theta\theta}.
$$

Substitute:

$$
\Gamma^r_{\ \theta\theta}
=
-\frac12 f(2r)
=
-fr.
$$

Since $fr=r-r_s$:

$$
\Gamma^r_{\ \theta\theta}=-(r-r_s).
$$

Shader:

``` glsl
float Tr_thth = -(r-rs);
```

#### $\Gamma^r_{\ \phi\phi}$

Use:

$$
\Gamma^r_{\ \phi\phi}
=
-\frac12 g^{rr}\partial_r g_{\phi\phi}.
$$

Substitute:

$$
\Gamma^r_{\ \phi\phi}
=
-\frac12 f(2r\sin^2\theta)
=
-(r-r_s)\sin^2\theta.
$$

Shader:

``` glsl
float Tr_phph = -(r-rs)*sin_theta*sin_theta;
```

#### $\Gamma^\theta_{\ r\theta}$

Use:

$$
\Gamma^\theta_{\ r\theta}
=
\frac12 g^{\theta\theta}\partial_r g_{\theta\theta}.
$$

Substitute:

$$
\Gamma^\theta_{\ r\theta}
=
\frac12 \frac{1}{r^2}(2r)=\frac1r.
$$

Shader:

``` glsl
float Tth_rth = 1.0/r;
```

#### $\Gamma^\theta_{\ \phi\phi}$

Use:

$$
\Gamma^\theta_{\ \phi\phi}
=
-\frac12 g^{\theta\theta}\partial_\theta g_{\phi\phi}.
$$

Substitute:

$$
\Gamma^\theta_{\ \phi\phi}
=
-\frac12\frac{1}{r^2}
(2r^2\sin\theta\cos\theta)
=
-\sin\theta\cos\theta.
$$

Shader:

``` glsl
float Tth_phph = -sin_theta*cos_theta;
```

#### $\Gamma^\phi_{\ r\phi}$

Use:

$$
\Gamma^\phi_{\ r\phi}
=
\frac12 g^{\phi\phi}\partial_r g_{\phi\phi}.
$$

Substitute:

$$
\Gamma^\phi_{\ r\phi}
=
\frac12
\frac{1}{r^2\sin^2\theta}
(2r\sin^2\theta)
=
\frac1r.
$$

Shader:

``` glsl
float Tph_rph = 1.0/r;
```

#### $\Gamma^\phi_{\ \theta\phi}$

Use:

$$
\Gamma^\phi_{\ \theta\phi}
=
\frac12 g^{\phi\phi}\partial_\theta g_{\phi\phi}.
$$

Substitute:

$$
\Gamma^\phi_{\ \theta\phi}
=
\frac12
\frac{1}{r^2\sin^2\theta}
(2r^2\sin\theta\cos\theta)
=
\frac{\cos\theta}{\sin\theta}
=
\cot\theta.
$$

Shader:

``` glsl
float Tph_thph = 1.0/(tan_theta);
```

### From Christoffel Symbols to Acceleration Components

The first-order form of the geodesic equation is:

$$
\frac{dx^\mu}{d\lambda}=u^\mu,
$$

$$
\frac{du^\mu}{d\lambda}
=
-\Gamma^\mu_{\alpha\beta}u^\alpha u^\beta.
$$

Because $\Gamma^\mu_{\alpha\beta}$ is symmetric in $\alpha,\beta$, mixed
terms such as $u^t u^r$ appear with a factor of 2.

The shader computes:

``` glsl
float at = -2*(Tt_tr)*vt*vr;
float ar = -(Tr_tt*vt*vt + Tr_rr*vr*vr + Tr_thth*vtheta*vtheta + Tr_phph*vphi*vphi);
float atheta = -(2*Tth_rth*vr*vtheta + Tth_phph*vphi*vphi);
float aphi = -(2*Tph_rph*vr*vphi + 2* Tph_thph*vtheta*vphi);
```

These are:

$$
a^t =
-2\Gamma^t_{\ tr}u^t u^r,
$$

$$
a^r =
-\left(
\Gamma^r_{\ tt}(u^t)^2
+
\Gamma^r_{\ rr}(u^r)^2
+
\Gamma^r_{\ \theta\theta}(u^\theta)^2
+
\Gamma^r_{\ \phi\phi}(u^\phi)^2
\right),
$$

$$
a^\theta =
-\left(
2\Gamma^\theta_{\ r\theta}u^r u^\theta
+
\Gamma^\theta_{\ \phi\phi}(u^\phi)^2
\right),
$$

$$
a^\phi =
-\left(
2\Gamma^\phi_{\ r\phi}u^r u^\phi
+
2\Gamma^\phi_{\ \theta\phi}u^\theta u^\phi
\right).
$$

That is the exact bridge from metric geometry to the code's acceleration
function.

------------------------------------------------------------------------

## Part 6 - Null Geodesics

### Why Photons Do Not Use Proper Time

For a massive particle, proper time satisfies:

$$
d\tau^2=-ds^2.
$$

For a photon:

$$
ds^2=0.
$$

Therefore:

$$
d\tau=0.
$$

No proper time elapses along a light ray. A photon path cannot be
parameterized by its own clock. This is why the shader uses an affine
parameter step `dL`, not proper time.

### Null Intervals

The Schwarzschild line element is:

$$
ds^2 =
-f(dt)^2
+f^{-1}(dr)^2
+r^2(d\theta)^2
+r^2\sin^2\theta(d\phi)^2,
$$

where:

$$
f=1-\frac{r_s}{r}.
$$

For a curve parameterized by $\lambda$, divide by $d\lambda^2$:

$$
0 =
-f(u^t)^2
+f^{-1}(u^r)^2
+r^2(u^\theta)^2
+r^2\sin^2\theta(u^\phi)^2.
$$

Solve for $u^t$. Move the time term to the other side:

$$
f(u^t)^2
=
f^{-1}(u^r)^2
+r^2(u^\theta)^2
+r^2\sin^2\theta(u^\phi)^2.
$$

Divide by $f$:

$$
(u^t)^2
=
\frac{
f^{-1}(u^r)^2
+r^2(u^\theta)^2
+r^2\sin^2\theta(u^\phi)^2
}{f}.
$$

Since $f^{-1}=1/f$:

$$
u^t
=
\sqrt{
\frac{
(u^r)^2/f
+r^2(u^\theta)^2
+r^2\sin^2\theta(u^\phi)^2
}{f}
}.
$$

This is implemented in `raymarch`:

``` glsl
Rp.u.x = sqrt(
    (
        ((Rp.u.y*Rp.u.y)/(1.0-(rs/Rp.x.y)))
        +(Rp.x.y*Rp.x.y*Rp.u.z*Rp.u.z)
        +(Rp.u.w*Rp.u.w*Rp.x.y*Rp.x.y*sin(Rp.x.z)*sin(Rp.x.z)))
    /(1.0-(rs/Rp.x.y))
    );
```

Mapping code to math:

-   `Rp.u.x` is $u^t$,
-   `Rp.u.y` is $u^r$,
-   `Rp.u.z` is $u^\theta$,
-   `Rp.u.w` is $u^\phi$,
-   `Rp.x.y` is $r$,
-   `Rp.x.z` is $\theta$,
-   `1.0-(rs/Rp.x.y)` is $f=1-r_s/r$.

### Why Penrose Integrates Null Geodesics

The rendered image is formed by photons reaching the camera. Photons
follow null geodesics, not timelike geodesics.

So the shader must:

1.  Generate a spatial camera ray.
2.  Convert its spatial direction into spherical components.
3.  Choose the time component $u^t$ so the full four-vector is null.
4.  Integrate the geodesic equation.

If Penrose integrated massive particle trajectories instead, it would
simulate falling objects, not light paths. The CPU benchmark files do
include massive-particle trajectories, but the renderer does not use
them for pixels.

------------------------------------------------------------------------

## Part 7 - From Mathematics to Numerical Simulation

### Continuous Equations

The geodesic equation is continuous:

$$
\frac{dx^\mu}{d\lambda}=u^\mu,
$$

$$
\frac{du^\mu}{d\lambda}=a^\mu(x,u).
$$

The acceleration function $a^\mu$ is:

$$
a^\mu =
-\Gamma^\mu_{\alpha\beta}(x)u^\alpha u^\beta.
$$

This is an ordinary differential equation system. The unknown is the
state:

$$
S(\lambda)=(x^\mu(\lambda),u^\mu(\lambda)).
$$

Penrose stores this as:

``` glsl
struct ray {
    vec4 x;
    vec4 u;
};
```

### First-Order Reformulation

The original geodesic equation is second order in $x^\mu$. Numerical ODE
solvers usually work with first-order systems. Introduce
$u^\mu=dx^\mu/d\lambda$, then write:

$$
\frac{d}{d\lambda}
\begin{bmatrix}
x^\mu\\
u^\mu
\end{bmatrix}
=
\begin{bmatrix}
u^\mu\\
a^\mu
\end{bmatrix}.
$$

Shader:

``` glsl
ray create_ray_derivative(ray R){
    ray res=ray(vec4(R.u),find_acceleration(R));
    return res;
}
```

This says:

-   derivative of position is velocity/tangent,
-   derivative of tangent is geodesic acceleration.

### Numerical Integration

A numerical integrator approximates the state after a small step $h$:

$$
S(\lambda+h)
$$

from known values at $\lambda$.

The simplest method is Euler:

$$
S_{n+1}=S_n+hF(S_n).
$$

Euler is easy but inaccurate for curved trajectories because it assumes
the derivative stays constant across the whole step.

Penrose uses fourth-order Runge-Kutta, usually called RK4.

### RK4

For:

$$
\frac{dS}{d\lambda}=F(S),
$$

RK4 computes four slope estimates:

$$
k_1=F(S_n),
$$

$$
k_2=F\left(S_n+\frac{h}{2}k_1\right),
$$

$$
k_3=F\left(S_n+\frac{h}{2}k_2\right),
$$

$$
k_4=F(S_n+hk_3).
$$

Then:

$$
S_{n+1}
=
S_n+\frac{h}{6}(k_1+2k_2+2k_3+k_4).
$$

The shader implementation is:

``` glsl
ray integrate(ray R){
    ray k1 = create_ray_derivative(R);
    ray k2 = create_ray_derivative(ray(R.x + (k1.x * (dL/2.0)),R.u + (k1.u * (dL/2.0))));
    ray k3 = create_ray_derivative(ray(R.x + (k2.x * (dL/2.0)),R.u + (k2.u * (dL/2.0))));
    ray k4 = create_ray_derivative(ray(R.x + (k3.x * dL),R.u + (k3.u * dL)));

    ray s_final = ray(
        R.x + (dL/6.0 )*(k1.x + 2.0*k2.x + 2.0*k3.x + k4.x),
        R.u + (dL/6.0 )*(k1.u + 2.0*k2.u + 2.0*k3.u + k4.u)
    );

    return s_final;
}
```

The CPU integrator in `physics.cpp` has the same structure:

``` cpp
State k1 = create_state_derivate(s);
State k2 = create_state_derivate(s + k1 * (dt/2));
State k3 = create_state_derivate(s + k2 * (dt/2));
State k4 = create_state_derivate(s + k3 * dt);

State s_final = s + (dt/6.0 )*(k1 + 2.0*k2 + 2.0*k3 + k4);
```

### Why RK4 Works Here

RK4 samples the derivative at the start, two midpoints, and the end of
the step. The weighted average gives a fourth-order approximation for
smooth ODEs.

Its local truncation error scales like:

$$
O(h^5),
$$

and its accumulated global error over many steps scales like:

$$
O(h^4).
$$

In the shader:

$$
h=dL=0.01.
$$

Each pixel can take up to:

$$
N_{\text{steps}}=1000.
$$

So a ray can be followed for an affine-parameter distance of roughly:

$$
1000\times0.01=10
$$

before the loop ends, unless it hits the horizon, a particle, or escape
radius earlier.

### Limitations

RK4 is not magic. It assumes the derivative behaves smoothly over a
step. Near:

-   $r=r_s$, terms like $1/(r-r_s)$ become large,
-   $\theta=0,\pi$, $\cot\theta$ becomes large,
-   photon orbits can curve rapidly.

Fixed-step RK4 may accumulate visible errors there. Penrose accepts this
because the goal is real-time GPU rendering, and each fragment must run
a bounded amount of work.

------------------------------------------------------------------------

## Part 8 - Explain the Entire Codebase

### Build and Dependency Files

#### `CMakeLists.txt`

This file defines the main executable:

``` cmake
add_executable(Penrose 
    src/main.cpp
    src/render/Texture.cpp
    src/render/Window.cpp
    src/render/Renderer.cpp
    src/render/ParticleBuffer.cpp
)
```

Only these source files are compiled into the live application. Notice
that `src/physics_engine/physics.cpp` and the benchmark files are not
included in this executable.

The project links:

-   GLFW for window and input,
-   GLAD for OpenGL function loading,
-   GLM for camera/vector/matrix math,
-   Eigen3, currently only needed by the CPU physics path if built
    separately.

#### `vcpkg.json`

This lists the package dependencies:

``` json
[
  "glfw3",
  "glad",
  "glm",
  "eigen3"
]
```

### Application Entry Point: `src/main.cpp`

`main.cpp` creates the runtime world.

Important global setup:

``` cpp
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

Camera camera(glm::vec3(0.5f, 0.0f, 2.0f));

float deltaTime = 0.0f; 
float lastFrame = 0.0f;
```

The camera starts at Cartesian position $(0.5,0,2)$. The black hole is
centered at the origin because the shader converts camera/world
coordinates to spherical radius around $(0,0,0)$.

The initialization sequence:

1.  Initialize GLFW.
2.  Request OpenGL 4.3 core profile.
3.  Create a window.
4.  Load OpenGL functions with GLAD.
5.  Compile `quad.vert` and `quad.frag`.
6.  Load the equirectangular starfield texture.
7.  Construct `Renderer`, which creates a full-screen quad.
8.  Create test particles.
9.  Upload particles to the renderer.
10. Enter the render loop.

The particles are initialized as spherical coordinates:

``` cpp
p1.stateX = glm::vec4(0.0f, 2.5f, 0.0f, 0.0f);
p2.stateX = glm::vec4(0.0f, 2.538f, 0.174f, 0.0f);
```

Here:

-   `stateX.x` is time,
-   `stateX.y` is radius,
-   `stateX.z` is $\theta$,
-   `stateX.w` is $\phi$.

In the render loop:

``` cpp
for (Particle& p : particles) {
    p.stateX.z += deltaTime *0.01;
}
renderer.updateParticles(particles);
renderer.draw(shader, window, camera, skyboxTexture, currentFrame);
```

The particle update changes only $\theta$ kinematically. It is not a
relativistic geodesic update. Then the particles are uploaded again to
the GPU, and the renderer draws the full-screen quad.

### Constants: `src/Constants.h`

``` cpp
const double dt = 0.01;
const double rs = 1.0;
```

These constants are used by CPU-side input/benchmark/physics code. The
shader defines its own:

``` glsl
const float dL = 0.01;
const float rs = 0.25;
const int N_STEPS = 1000;
```

So the live black hole radius is the shader's `0.25`.

### Shader Wrapper: `src/render/Shader.h`

`Shader` loads GLSL source files, compiles the vertex and fragment
shaders, links them into a program, and provides uniform setters:

``` cpp
void setFloat(...)
void setVec2(...)
void setVec3(...)
void setInt(...)
```

Mathematically, this file does not implement relativity.
Computationally, it is how C++ passes parameters such as camera
position, time, skybox texture unit, and particle count to the GPU
program where the physics runs.

### Camera: `src/render/Camera.h`

The camera is an ordinary graphics camera. It stores:

-   `Position`,
-   `Front`,
-   `Up`,
-   `Right`,
-   `WorldUp`,
-   yaw and pitch angles.

`GetViewMatrix` returns:

``` cpp
glm::lookAt(Position, Position + Front, Up);
```

This view matrix maps world coordinates into camera/view coordinates.
`Renderer::draw` combines it with a perspective projection matrix, then
sends the inverse projection-view matrix to the shader:

``` cpp
glm::mat4 invProjView = glm::inverse(projection * view);
glUniformMatrix4fv(..., "uInvProjView", ...);
```

The shader uses `uInvProjView` to convert a pixel coordinate into a
world-space ray direction.

### Window/Input: `src/render/Window.cpp`

`processInput` handles:

-   Escape to close,
-   `P` to toggle frame capture,
-   WASD camera translation,
-   arrow-key camera rotation.

It uses:

``` cpp
float deltaTime = dt;
```

where `dt` comes from `Constants.h`. Therefore camera motion uses a
fixed timestep of `0.01`, not the measured frame `deltaTime` in
`main.cpp`.

### Renderer: `src/render/Renderer.h` and `Renderer.cpp`

The `Renderer` owns:

``` cpp
unsigned int quadVAO, quadVBO;
ParticleBuffer particleBuffer;
```

Its constructor creates two triangles covering the whole screen:

``` cpp
float quadVertices[] = {
    -1.0f,  1.0f,  0.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
    ...
};
```

This is important: Penrose does not render a mesh sphere for the black
hole. It renders a screen-sized rectangle, and every fragment of that
rectangle runs the black-hole raymarcher.

`Renderer::draw`:

1.  activates the shader,
2.  sends time and resolution,
3.  sends camera position,
4.  builds view and projection matrices,
5.  sends inverse projection-view matrix,
6.  binds the particle SSBO at binding point 0,
7.  sends particle count,
8.  binds the skybox texture,
9.  draws six vertices.

The line:

``` cpp
glDrawArrays(GL_TRIANGLES, 0, 6);
```

causes the fragment shader to run for every covered pixel.

`Renderer::captureFrame` reads the OpenGL framebuffer and writes a
binary PPM image. It flips rows vertically because OpenGL's framebuffer
origin is bottom-left while typical image files are read top-down.

### Particle Data: `src/render/Particle.h`

The GPU-side particle struct is:

``` cpp
struct Particle {
    glm::vec4 stateX;
    glm::vec4 stateU;
    glm::vec3 color;
    float mass;
    float radius;
    float _pad0, _pad1, _pad2;
};
```

It mirrors the shader struct:

``` glsl
struct particle {
    vec4 x;
    vec4 u;
    vec3 color;
    float m;
    float r;
};
```

The padding helps keep the C++ memory layout compatible with GLSL
`std430` layout expectations. The shader currently uses:

-   `x` for particle position,
-   `color`,
-   `r` for radius.

It does not use particle mass in the geodesic calculation. The black
hole is fixed at the origin and encoded only through `rs`.

### Particle Buffer: `ParticleBuffer.h` and `ParticleBuffer.cpp`

`ParticleBuffer` wraps an OpenGL shader storage buffer object:

``` cpp
glBindBuffer(GL_SHADER_STORAGE_BUFFER, SSBO);
glBufferData(GL_SHADER_STORAGE_BUFFER, particles.size() * sizeof(Particle), particles.data(), GL_DYNAMIC_DRAW);
```

The shader declares:

``` glsl
layout(std430, binding = 0) readonly buffer ParticleBuffer {
    particle particles[];
};
```

Because the renderer calls:

``` cpp
particleBuffer.bind(0);
```

the shader can loop through `particles[j]` for hit testing.

### Texture Loading: `Texture.cpp`

`loadTexture` uses `stb_image` to load the skybox image and uploads it
to OpenGL.

The key line:

``` cpp
glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
```

tells OpenGL that rows of image data are byte-aligned. This matters for
RGB images whose row byte length is not a multiple of four. Without it,
the equirectangular skybox can appear diagonally skewed.

### Frame Capture: `FrameCapture.h`

`FrameCapture` stores:

-   whether capture is active,
-   the capture directory,
-   the frame counter.

When capture starts, it creates:

``` text
imagesequence/YYYY-MM-DD_HH-MM-SS/
```

Each frame path is:

``` text
frame_000000.ppm
frame_000001.ppm
...
```

This is a graphics output utility, not part of the GR simulation.

### Vertex Shader: `shaders/quad.vert`

The vertex shader is minimal:

``` glsl
TexCoords = aTexCoords;
gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
```

It passes texture coordinates to the fragment shader and places the quad
directly in clip space. It does no 3D scene transformation.

### Fragment Shader: `shaders/quad.frag`

This is the core of the live simulation.

Important uniforms:

``` glsl
uniform vec3 uCameraPos;
uniform sampler2D skybox;
uniform mat4 uInvProjView;
uniform int uParticleCount;
```

Important constants:

``` glsl
const float dL = 0.01;
const float rs = 0.25;
const int N_STEPS = 1000;
```

Important functions:

-   `sph_to_cart_Jacobian`: converts spherical tangent components to
    Cartesian tangent components.
-   `cart_to_sph_Jacobian`: inverse Jacobian for Cartesian to spherical
    tangent components.
-   `cart_to_sph`: converts both position and tangent.
-   `sph_to_cart`: converts both position and tangent.
-   `find_acceleration`: computes geodesic acceleration from Christoffel
    symbols.
-   `create_ray_derivative`: builds the ODE derivative state.
-   `integrate`: performs one RK4 step.
-   `DirectionToUV`: maps a 3D direction to equirectangular texture
    coordinates.
-   `rayParticleHit`: tests distance from the ray position to a particle
    sphere.
-   `raymarch`: traces one photon path.
-   `main`: creates the initial camera ray and colors the pixel.

### CPU Physics Engine: `physics.h` and `physics.cpp`

The CPU `State` mirrors the shader's `ray`:

``` cpp
struct State{
    Vector4d X;
    Vector4d U;
};
```

It implements:

-   coordinate Jacobians,
-   Cartesian/spherical conversion,
-   Schwarzschild Christoffel acceleration,
-   RK4 integration.

`physics.cpp` is valuable for understanding the math because it
expresses the same ideas in C++/Eigen. The current build does not
compile it into the main app. Also, the header declaration:

``` cpp
State Integrator(State initState);
```

does not match the implementation:

``` cpp
State Integrator(const State& s, double dt)
```

That is simply a current-code detail to know when reading or separately
building this path.

### Benchmarking Files

`freefall.cpp` simulates radial free fall of a massive particle and
compares the numerical horizon-crossing proper time to:

$$
\tau =
\frac{2}{3}
\frac{1}{\sqrt{r_s}}
\left(r_0^{3/2}-r_s^{3/2}\right).
$$

It uses initial conditions for energy $E=1$:

$$
u^t=\frac{1}{1-r_s/r_0},
$$

$$
u^r=-\sqrt{\frac{r_s}{r_0}}.
$$

`orbital.cpp` simulates a general massive geodesic in the equatorial
plane. It computes $u^t$ from:

$$
-f(u^t)^2+f^{-1}(u^r)^2+r^2(u^\phi)^2=-1.
$$

It also prints conserved quantities:

$$
E=f u^t,
$$

$$
L=r^2u^\phi.
$$

These benchmarks are not used for rendering pixels, but they document
and test the same geodesic machinery for timelike paths.

### Video Conversion: `ppm_to_video.py`

This Python utility reads PPM frame sequences and writes an MP4 using
OpenCV. It is downstream of rendering. It does not affect the
simulation.

------------------------------------------------------------------------

## Part 9 - Explain the Simulation Pipeline

This section follows one photon from pixel to final color.

### 1. Camera Creation

At program start:

``` cpp
Camera camera(glm::vec3(0.5f, 0.0f, 2.0f));
```

The camera has:

-   position $(0.5,0,2)$,
-   yaw $-90^\circ$,
-   pitch $0^\circ$,
-   a front direction computed from yaw and pitch.

The camera is a standard Euclidean graphics camera. It is not itself a
relativistic observer model with tetrads or local orthonormal frames. It
supplies a world-space position and view matrix.

### 2. Ray Generation

For each fragment, the shader receives `TexCoords` in $[0,1]^2$. It maps
them to normalized device coordinates:

``` glsl
vec4 ndcPos = vec4(TexCoords.x * 2.0 - 1.0, TexCoords.y * 2.0 - 1.0, 1.0, 1.0);
```

This creates a point on the far side of the camera frustum in
clip/NDC-style coordinates.

Then:

``` glsl
vec4 worldPos = uInvProjView * ndcPos;
vec3 worldDir = normalize((worldPos.xyz / worldPos.w)-uCameraPos);
```

This unprojects the pixel into world space. The ray direction is the
normalized vector from the camera to that unprojected point.

Data at this stage:

``` text
ro = uCameraPos       Cartesian 3-vector
rd = worldDir         Cartesian normalized 3-vector
```

### 3. Initial Four-State

`raymarch` creates:

``` glsl
ray R = ray(vec4(0.0, ro), vec4(0.0, rd));
```

So initially:

$$
R.x=(0,x,y,z),
$$

$$
R.u=(0,rd_x,rd_y,rd_z).
$$

The time component is temporarily zero. It will be replaced by the null
constraint after conversion to spherical coordinates.

Then:

``` glsl
R.u = normalize(R.u);
```

This normalizes the four-component vector, not just `rd`. Since the time
component is zero at this point, it effectively normalizes the spatial
part, but it is still a GLSL `vec4` normalization.

### 4. Cartesian to Spherical Position

The shader converts position:

``` glsl
vec3 p = R.x.yzw;
float r = length(p);
float theta = acos(clamp(p.z/(r+1e-10), -1.0, 1.0));
float phi = atan(p.y, p.x);
```

Mathematically:

$$
r=\sqrt{x^2+y^2+z^2},
$$

$$
\theta=\arccos(z/r),
$$

$$
\phi=\operatorname{atan2}(y,x).
$$

The small `1e-10` and `clamp` protect against division by zero and
floating-point values slightly outside $[-1,1]$.

### 5. Cartesian to Spherical Velocity via Jacobian

Velocity/tangent components do not transform like positions. If:

$$
x^i=x^i(q^a)
$$

where $q^a=(r,\theta,\phi)$, then:

$$
\frac{dx^i}{d\lambda}
=
\frac{\partial x^i}{\partial q^a}
\frac{dq^a}{d\lambda}.
$$

The matrix:

$$
J^i_{\ a}=\frac{\partial x^i}{\partial q^a}
$$

maps spherical tangent components to Cartesian tangent components:

$$
u_{\text{cart}}=J u_{\text{sph}}.
$$

Therefore:

$$
u_{\text{sph}}=J^{-1}u_{\text{cart}}.
$$

The shader builds $J$ in `sph_to_cart_Jacobian`, then inverts it in
`cart_to_sph_Jacobian`.

Because GLSL matrix constructors are column-major, the columns in:

``` glsl
return mat4(
    1.0, 0.0, 0.0, 0.0,
    0.0, sin(theta)*cos(phi), sin(theta)*sin(phi), cos(theta),
    0.0, r*cos(theta)*cos(phi), r*cos(theta)*sin(phi), -r*sin(theta),
    0.0, -r*sin(theta)*sin(phi), r*sin(theta)*cos(phi), 0.0
);
```

represent derivatives with respect to $t$, $r$, $\theta$, and $\phi$.
Multiplying by `R.u` produces the transformed tangent.

### 6. Null Constraint

After conversion:

``` glsl
ray Rp = cart_to_sph(R);
```

the shader sets:

``` glsl
Rp.u.x = sqrt(...);
```

This computes $u^t$ so:

$$
g_{\mu\nu}u^\mu u^\nu=0.
$$

At this point the photon has a valid null tangent in Schwarzschild
coordinates.

Data:

``` text
Rp.x = (t, r, theta, phi)
Rp.u = (u^t, u^r, u^theta, u^phi)
```

### 7. Integration Loop

The shader runs:

``` glsl
for(int i = 0; i < N_STEPS; i++) {
    ...
}
```

with `N_STEPS = 1000`.

Each loop iteration tests termination conditions and advances the
geodesic.

### 8. Horizon Detection

``` glsl
if(length(Rp.x[1]) < rs*1.1) {
    return vec3(0.0, 0.0, 0.0);
}
```

Since $r\ge 0$, `length(Rp.x[1])` is effectively $|r|$. If the ray
reaches the horizon cutoff, the pixel becomes black.

### 9. Convert Back to Cartesian for Particle Hits

Particle positions are interpreted geometrically in Cartesian space for
the distance test:

``` glsl
R = sph_to_cart(Rp);
```

This gives:

``` text
R.x.yzw = Cartesian photon position
R.u.yzw = Cartesian photon tangent direction-like vector
```

### 10. Particle Intersection

For each particle:

``` glsl
if (rayParticleHit(R, particles[j]) <0.0) {
    return particles[j].color;
}
```

`rayParticleHit` converts the particle's spherical coordinates to
Cartesian:

$$
x_p=r_p\sin\theta_p\cos\phi_p,
$$

$$
y_p=r_p\sin\theta_p\sin\phi_p,
$$

$$
z_p=r_p\cos\theta_p.
$$

Then:

``` glsl
vec3 toParticle = particlePos - R.x.yzw;
float d= length(toParticle);
return d-p.r;
```

If $d-r_p<0$, the current photon sample point lies inside the particle
sphere. This is not an analytic ray-sphere intersection over the whole
RK4 segment; it is a stepwise proximity test.

### 11. Escape Detection

``` glsl
if(Rp.x.y > 20.0) {
    break; 
}
```

If the radius exceeds 20, the ray is considered escaped. The loop stops
and the skybox is sampled using the final direction.

### 12. Metric and Christoffel Evaluation

The metric is not stored as a matrix in the shader. Instead, the
Christoffel expressions already derived from the metric are computed
directly:

``` glsl
float Tr_tt = rs * (r-rs)/(2.0*r*r*r);
float Tr_rr = -rs/(2.0*r*(r-rs));
...
```

So "metric evaluation" in the live shader is implicit. The metric enters
through:

-   the null constraint for $u^t$,
-   the Christoffel symbols in `find_acceleration`.

### 13. Trajectory Evolution

``` glsl
ray der = integrate(Rp);
Rp.u = der.u;
Rp.x = der.x;
```

Despite the variable name `der`, `integrate` returns the next state, not
just the derivative. After this assignment:

``` text
old Rp -> new Rp
```

where the new state approximates the geodesic after one RK4 step of size
`dL`.

### 14. Pole Handling

Spherical coordinates are singular at:

$$
\theta=0,\pi.
$$

The shader handles crossings:

``` glsl
if (Rp.x.z < 0.0) {
    Rp.x.z = -Rp.x.z;
    Rp.x.w += PI;
    Rp.u.z = -Rp.u.z;
} else if (Rp.x.z > PI) {
    Rp.x.z = 2.0 * PI - Rp.x.z;
    Rp.x.w += PI;
    Rp.u.z = -Rp.u.z;
}
```

This reflects $\theta$ back into the valid interval and shifts $\phi$ by
$\pi$, representing the same physical direction on the other side of the
pole.

### 15. Skybox Color

If the ray escapes:

``` glsl
R = sph_to_cart(Rp);
vec2 skyUV = DirectionToUV(normalize(R.u.yzw));
return texture(skybox, skyUV).rgb;
```

`DirectionToUV` maps a 3D direction to equirectangular texture
coordinates:

$$
u = 0.5+\frac{\operatorname{atan2}(d_x,d_z)}{2\pi},
$$

$$
v = 0.5-\frac{\arcsin(d_y)}{\pi}.
$$

The pixel color comes from the skybox direction reached by the curved
photon path.

### 16. Final Pixel Output

The fragment shader writes:

``` glsl
FragColor = vec4(finalColor, 1.0);
```

That is the end of one photon's computational story.

------------------------------------------------------------------------

## Part 10 - Every Mathematical Quantity Appearing in the Code

### `rs`

`rs` is the Schwarzschild radius:

$$
r_s=2M
$$

in units $G=c=1$. It determines:

-   the horizon radius,
-   the strength of metric variation,
-   the magnitude of Christoffel terms.

Shader value:

``` glsl
const float rs = 0.25;
```

CPU value:

``` cpp
const double rs = 1.0;
```

### `dL`

`dL` is the RK4 step size in affine parameter:

``` glsl
const float dL = 0.01;
```

It is not a spatial distance, not coordinate time, and not photon proper
time. It is the numerical increment for the geodesic parameter.

### `dt`

`dt` in `Constants.h` is:

``` cpp
const double dt = 0.01;
```

It is used by the CPU integrator/benchmarks and by camera input. It is
separate from the shader's `dL`.

### `N_STEPS`

``` glsl
const int N_STEPS = 1000;
```

This bounds the work per pixel. Without a bound, rays could orbit or
escape slowly and the shader might not terminate quickly enough for
real-time rendering.

### `x`

In `ray` and `State`, `x` or `X` is four-position:

$$
x^\mu=(t,r,\theta,\phi)
$$

during physics integration.

In the temporary Cartesian form:

$$
x^\mu=(t,x,y,z).
$$

The code relies on knowing which coordinate representation a state
currently uses.

### `u`

`u` is the tangent vector:

$$
u^\mu=\frac{dx^\mu}{d\lambda}.
$$

For photons, it is null:

$$
g_{\mu\nu}u^\mu u^\nu=0.
$$

For massive CPU benchmark particles, it is timelike:

$$
g_{\mu\nu}u^\mu u^\nu=-1.
$$

### `vt`, `vr`, `vtheta`, `vphi`

In `find_acceleration`:

``` glsl
float vt    =R.u[0];
float vr    =R.u[1];
float vtheta=R.u[2];
float vphi  =R.u[3];
```

These are:

$$
u^t,u^r,u^\theta,u^\phi.
$$

The names use `v`, but mathematically they are four-tangent components
with respect to $\lambda$.

### `theta`

$\theta$ is the polar angle measured from the positive $z$-axis:

$$
\theta=0
$$

points along $+z$, and:

$$
\theta=\pi/2
$$

is the equatorial plane.

### `phi`

$\phi$ is the azimuthal angle in the $x$-$y$ plane:

$$
\phi=\operatorname{atan2}(y,x).
$$

### The Jacobian

The Jacobian is:

$$
J^\mu_{\ a}=\frac{\partial x^\mu_{\text{cart}}}{\partial x^a_{\text{sph}}}.
$$

It transforms tangent components:

$$
u^\mu_{\text{cart}}=J^\mu_{\ a}u^a_{\text{sph}}.
$$

Its inverse transforms:

$$
u^a_{\text{sph}}=(J^{-1})^a_{\ \mu}u^\mu_{\text{cart}}.
$$

The code uses it because velocity components must be transformed by
derivatives of the coordinate map, not by simply converting endpoint
positions.

### `f = 1 - rs/r`

The code does not always name this quantity, but it appears repeatedly:

``` glsl
1.0-(rs/Rp.x.y)
```

It is the Schwarzschild lapse factor:

$$
f(r)=1-\frac{r_s}{r}.
$$

It controls both gravitational time dilation and radial stretching in
Schwarzschild coordinates.

### Christoffel Variables

Shader names such as:

``` glsl
Tr_tt
Tth_rth
Tph_thph
```

stand for Christoffel symbols. For example:

-   `Tr_tt` means $\Gamma^r_{\ tt}$,
-   `Tth_rth` means $\Gamma^\theta_{\ r\theta}$,
-   `Tph_thph` means $\Gamma^\phi_{\ \theta\phi}$.

They are not forces. They are connection coefficients.

### `at`, `ar`, `atheta`, `aphi`

These are:

$$
\frac{du^t}{d\lambda},
\quad
\frac{du^r}{d\lambda},
\quad
\frac{du^\theta}{d\lambda},
\quad
\frac{du^\phi}{d\lambda}.
$$

They are acceleration-like because they are derivatives of the tangent.
They arise from:

$$
a^\mu=-\Gamma^\mu_{\alpha\beta}u^\alpha u^\beta.
$$

### Conserved Quantities in Benchmarks

In Schwarzschild spacetime, time-translation symmetry gives conserved
energy:

$$
E=f u^t.
$$

Rotational symmetry gives conserved angular momentum in the equatorial
plane:

$$
L=r^2u^\phi.
$$

`orbital.cpp` prints both:

``` cpp
double E = f * vt;
double L = r0 * r0 * vph;
```

These quantities are not used in the shader integration. The shader
directly integrates the geodesic equations.

------------------------------------------------------------------------

## Part 11 - Every Approximation

### Schwarzschild Approximation

The simulated black hole is Schwarzschild:

-   non-rotating,
-   uncharged,
-   isolated,
-   static,
-   perfectly spherical.

Real astrophysical black holes usually rotate. Rotation would require
the Kerr metric, not the Schwarzschild metric.

### Natural Units

The code uses $G=c=1$. This removes physical unit conversions and makes
$r_s=2M$. The rendered image is dimensionless and controlled by chosen
numeric scales.

### Fixed Background

Particles do not gravitate. The black hole geometry is fixed. The `mass`
field in `Particle` is not used to modify the metric.

### Coordinate Choice

The shader integrates in Schwarzschild spherical coordinates. This makes
the equations compact but introduces coordinate singularities:

-   $r=r_s$,
-   $\theta=0$,
-   $\theta=\pi$.

The horizon cutoff and pole handling are consequences of this coordinate
choice.

### Horizon Cutoff at `1.1 * rs`

The code colors a ray black when:

$$
r<1.1r_s.
$$

This is outside the mathematical horizon. It avoids numerical
instability from integrating too close to $r_s$, where
Schwarzschild-coordinate terms diverge.

Sacrifice: the apparent black region is slightly enlarged relative to
the true event horizon in these coordinates.

### Escape Radius at `r > 20`

The shader treats:

$$
r>20
$$

as escape.

This is not infinity. It is a finite cutoff. It works because the metric
becomes increasingly flat as $r$ grows and $r_s/r$ becomes small.

Sacrifice: rays that would still bend slightly beyond radius 20 are
approximated as already escaped.

### Fixed-Step RK4

The shader always uses:

$$
dL=0.01.
$$

It does not adapt the step size near the horizon or photon sphere.
Fixed-step RK4 is predictable and GPU-friendly, but it can accumulate
errors in high-curvature regions.

### Finite Step Particle Hit Test

The particle test checks whether the current sampled ray position lies
inside a sphere. It does not solve for exact intersection along the
curved segment between samples.

Sacrifice: a ray can skip over a very small particle between steps.

### Single Precision GPU Floats

GLSL `float` is generally 32-bit single precision. Near singular
coordinate regions, subtracting close values such as $r-r_s$ and
inverting small quantities can magnify errors.

Potential consequences:

-   noisy horizon edge,
-   unstable near-pole behavior,
-   drift from exact null normalization,
-   missed or exaggerated bending.

### Null Constraint Applied at Initialization Only

The shader computes $u^t$ from the null constraint at the start. RK4
integration should preserve the null norm mathematically, but finite
precision and finite steps can drift.

The code does not renormalize the null condition after every step.

### Camera Model

The camera is a standard Euclidean graphics camera. It does not model a
local orthonormal tetrad attached to an observer in curved spacetime.
The initial ray is generated from OpenGL projection geometry, then
interpreted as a Schwarzschild-coordinate photon after conversion.

This is adequate for a visualizer, but it is an approximation relative
to a fully relativistic observer model.

### Skybox at Finite Radius

The skybox is sampled when rays escape past $r=20$. The background is an
equirectangular texture, not a physical celestial sphere with
relativistic radiative transfer.

### Particle Motion

Particles are moved in `main.cpp` by directly changing `stateX.z`:

``` cpp
p.stateX.z += deltaTime *0.01;
```

This is a kinematic animation, not a geodesic orbit. The shader only
uses particles as colored hit targets.

### No Accretion Disk Physics

There is no emission model, absorption model, Doppler shift,
gravitational redshift of colors, plasma opacity, or disk temperature
calculation in the current renderer.

### Coordinate Pole Bounce

The pole-crossing fix reflects $\theta$ and shifts $\phi$. This keeps
coordinates valid, but it is a numerical coordinate-management rule, not
a separate physical law.

### CPU/GPU Parameter Mismatch

The CPU code uses $r_s=1.0$. The shader uses $r_s=0.25$. Since the live
renderer uses the shader value, the mismatch does not affect the current
executable's pixel integration. It matters when comparing CPU benchmark
behavior to shader behavior.

### PPM Capture

Frame capture writes raw PPM files from the current framebuffer. This
captures final rendered pixels, not simulation states. It also performs
synchronous disk I/O.

------------------------------------------------------------------------

## Part 12 - Overall Mental Model

When you press Render by running Penrose, the application does not build
a mesh model of a black hole. It builds a mathematical machine that
answers one question per pixel:

"Where does the light ray arriving from this screen direction go when
followed through Schwarzschild spacetime?"

The C++ side creates the conditions for that question:

-   a window,
-   a camera,
-   a projection matrix,
-   an inverse projection-view matrix,
-   a skybox texture,
-   a list of colored particle spheres,
-   a full-screen quad that invokes the fragment shader for every pixel.

The fragment shader does the relativistic work:

1.  Convert the pixel into a Cartesian camera ray.
2.  Convert the ray into spherical Schwarzschild coordinates.
3.  Compute the missing time component so the ray is null.
4.  Use Christoffel symbols from the Schwarzschild metric to compute
    geodesic acceleration.
5.  Use RK4 to advance the photon's four-position and four-tangent.
6.  Repeat until the ray falls inward, hits a particle, escapes, or
    exhausts the step budget.
7.  Convert the result into a color.

Relativity appears in:

-   the Schwarzschild metric,
-   the null interval condition,
-   the geodesic equation,
-   the Christoffel symbols,
-   the distinction between timelike and null paths.

Geometry appears in:

-   the metric tensor,
-   coordinate transformations,
-   Jacobians,
-   spherical-coordinate angular terms,
-   pole singularities,
-   connection coefficients.

Numerical analysis appears in:

-   first-order ODE state representation,
-   RK4 integration,
-   finite step size,
-   error accumulation,
-   termination thresholds.

Computer graphics appears in:

-   OpenGL windowing,
-   camera matrices,
-   full-screen quad rendering,
-   fragment shader parallelism,
-   texture sampling,
-   SSBO particle data,
-   framebuffer capture.

The whole project can be mentally simulated as follows:

For each pixel, OpenGL runs `quad.frag`. The shader uses the camera
matrix to aim a ray into the scene. That ray is initially an ordinary
graphics ray, but Penrose immediately promotes it into a spacetime
tangent vector. The Schwarzschild metric supplies the rule that this
tangent must be null. The Christoffel symbols supply the rule for how
the tangent changes as the ray moves. RK4 repeatedly applies that rule.
The path curves because the coordinate expression of "straightest
possible spacetime path" includes the metric-derived connection terms.
If the path reaches the black hole cutoff, the pixel is black. If it
reaches a particle sphere, the pixel takes that particle's color. If it
escapes, the final direction tells the shader which part of the
starfield the photon came from.

That is the current Penrose simulation from first principles: a
real-time GPU ray tracer where rays are not Euclidean lines, but
numerically integrated null geodesics of the Schwarzschild metric.
