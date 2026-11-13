# RayTracing2D

A deterministic horizontal-plane propagation framework for OMNeT++, using
OpenStreetMap building footprints and inspectable per-path calculations.

The framework constructs line-of-sight paths, first- and second-order finite-wall
specular reflections, and a first-order knife-edge diffraction approximation at
unique vertices. It separates geometric visibility, material response, complex
phase, power combination and delay metrics. Closed building interiors are opaque.

## Contents

| Directory | Contents |
| --- | --- |
| `framework/` | C++ kernel, standalone tests, OMNeT++ module/configurations, OSM fixtures and optional INET adapter |
| `experiments/campaign/` | Native configuration manifest, matched MATLAB/Sionna inputs, subareas and repeated timing records |
| `experiments/diagnostic/` | Equivalent-facade meshes, 63 receiver positions, raw API records and independent geometric reconstruction |
| `experiments/data/` | Archived numerical results, native scalar files and analysis tables, preserved without byte changes |
| `tools/` | Material verification and experiment setup |

## Verify the kernel

Requires GNU Make and a C++17 compiler (`g++` in the supplied framework Makefile).
OMNeT++ is not needed for these checks.

```sh
make test
```

This runs 25 baseline groups, 11 additional geometric/electromagnetic groups and
a separate full-map two-reflection regression. The production C++ source files
are identical to the implementation used for the accompanying results.

## Run OMNeT++

The archived native campaign used OMNeT++ 6.0pre11. Start an OMNeT++ environment,
or set `OMNETPP_ROOT`, then build and run a reference configuration:

```sh
make -C framework
make -C framework cmdenv CONFIG=Thesis_T02_LoS_R1R2
```

To replay the supplied research campaigns, prepare their local source folders:

```sh
python3 tools/setup_experiments.py
python3 experiments/campaign/reproduce.py native --omnetpp-root /path/to/omnetpp
python3 experiments/campaign/reproduce.py common
python3 experiments/campaign/reproduce.py subareas
python3 experiments/campaign/reproduce.py scalability
```

New campaign outputs go into `experiments/campaign/reproduced/`. The 32 archived
native scalar files and receiver-level data are under `experiments/data/campaign/`.
See [the experiment guide](experiments/README.txt) for external-tool and diagnostic
entry points. Diagnostic scripts should run in a separate working copy because
they write to their named result folders.

## Model and comparison scope

The results establish numerical and geometric consistency within a static 2D
model. They do not supply field measurements. Diffraction is a blocked-link
horizontal knife-edge approximation, rather than a UTD wedge implementation.

Matched solver runs use MATLAB R2025a and Sionna RT 2.2.0 with Mitsuba 3.9.1 and
Dr.Jit 1.5.0. Geometry, material response, vertical/TE polarization and LoS/R1/R2
physics are configured explicitly. These are not comparisons of unspecified
vendor defaults. The directed mesh study preserves unsuccessful refinements,
recovered paths and all returned Sionna representatives.

## Citation and attribution

`CITATION.cff` provides the authors and title of the accompanying manuscript.
No journal acceptance, DOI or public release identifier is claimed.

Map data are from [OpenStreetMap contributors](https://www.openstreetmap.org/copyright),
under the Open Database License (ODbL). The manuscript, compiled manuscript PDF, LaTeX sources and journal template are
not included in the current branch. Third-party runtimes and proprietary MATLAB
implementation files are not included. No additional software license
has been assigned by this packaging step.
