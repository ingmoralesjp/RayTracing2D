REPOSITORY LAYOUT NOTE

The common source is in ../framework/ relative to this folder. From the
repository root, run python3 tools/setup_experiments.py once before using the
archived campaign/source and diagnostic/source entry points below.

RESEARCH MATERIAL FOR THE MANUSCRIPT

The supplied scientific C++ files are unchanged copies of the final delivered
implementation. Archived numerical outputs remain available for inspection
without installing OMNeT++, MATLAB or Sionna. Raw contributor identifiers and
configuration names are preserved for traceability.

campaign/ contains the final kernel, OMNeT++ configurations and archived matched
comparison inputs. diagnostic/ contains the equivalent-mesh/63-position study,
including its independent high-precision reconstruction, all interaction records
and MATLAB candidate-visibility diagnostics. The duplicated source/ folders keep
the original scripts' relative paths functional. They contain the same kernel.
Archived native outputs and analysis tables are in data/ under this experiments
folder. The manuscript, PDF, LaTeX sources and journal template are excluded from
the current branch; result bytes are preserved unchanged.

1. STANDALONE KERNEL CHECKS

    cd campaign/source
    make test-core

Requires a C++17 compiler (the supplied Makefile invokes g++). This runs the
25 baseline groups, 11 additional geometric/electromagnetic groups and the
separate full-map M50_053 regression. These are numerical verification cases.
No changes to the production code are required.

2. OMNET++ AND KERNEL CAMPAIGNS

OMNeT++ 6.0pre11 was used in the archived native executions. Start its environment
or set OMNETPP_ROOT. From campaign/:

    python reproduce.py native --omnetpp-root /path/to/omnetpp
    python reproduce.py common
    python reproduce.py subareas
    python reproduce.py scalability

These commands write new outputs under campaign/reproduced/ and preserve the
archived data. Native replay uses exactly the 32 names in native_configurations.json.
The kernel wrapper uses the archived walls/receiver tables, mechanism masks,
500 m bound and repeat counts. Use --compiler to select the C++17 compiler.
The recorded timing study used Clang 10, -O3 -DNDEBUG=1, on an AMD Ryzen 9 7950X
with CPU affinity 32; repeated times on another machine will differ.

3. MATCHED EXTERNAL TOOLS

MATLAB R2025a: scripts/run_matlab_comparison.m reads 05_E6_platforms/ and writes
the public image-method results there. Requires Antenna Toolbox and Phased Array
System Toolbox. Run in a separate copy to preserve archived results:

    matlab -batch "run('scripts/run_matlab_comparison.m')"

Sionna RT 2.2.0, Mitsuba 3.9.1 and Dr.Jit 1.5.0 were used with the CPU polarized
LLVM variant. Dependency versions are recorded in 09_reproducibility/ and each
run's metadata. With those dependencies installed, in a separate working copy:

    python scripts/run_sionna_comparison.py --samples 1000000 --seed 42
    python scripts/run_sionna_comparison.py --samples 1000000 --seed 73

The scripts explicitly configure the common reflection-only physics. These runs
do not validate the horizontal-plane knife-edge diffraction model.

4. DIRECTED GEOMETRIC DIAGNOSTIC

In diagnostic/, the inputs/scenes/ folders preserve equivalent physical facades,
their triangle mappings and the original/refined representations. The three
receiver CSV files contain 17 initial, 24 directed and 22 transition positions.
results/ holds the raw API records. independent_audit/ holds the Decimal-based
geometric reconstruction. Source fingerprints identify the examined proprietary
MATLAB installation; its proprietary implementation files are not included.

Entry points (use a separate working copy because original scripts write into
their named result folders):

    python scripts/prepare_scenes.py
    python scripts/refine_scene.py
    python scripts/prepare_transitions.py
    python scripts/run_rt2d_diagnostic.py --receivers inputs/receivers_initial.csv
    python scripts/independent_geometry.py --help
    python scripts/run_sionna_diagnostic.py --help

Repeat the RT2D call for receivers_directed.csv and receivers_transition.csv.
The scripts' --help output documents the receiver/case selections. MATLAB entry
points are scripts/run_matlab_diagnostic.m and matlab_visibility_diagnostic.m;
the latter uses release-specific installed internals solely for diagnosis.

5. MAP ATTRIBUTION AND INTERPRETATION

The archived OSM input is source/simulations/data/student/zacatecas_student.osm.
Map data: OpenStreetMap contributors, Open Database License (ODbL).
https://www.openstreetmap.org/copyright

These records establish the reported implementation behavior and computational
agreement within the declared model. They do not supply field measurements or a
general ranking of propagation platforms. Large-array spatial outputs, finite
power masks, excluded interiors and missing paths must be kept distinct.
