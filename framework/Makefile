SHELL := /bin/sh
CONFIG ?= SingleReflection

# Recommended: start from an OMNeT++ terminal/environment so opp_makemake,
# opp_configfilepath and runtime libraries are already available. If preferred,
# pass OMNETPP_ROOT=/path/to/omnetpp; this Makefile only prepends it when set.
ifneq ($(strip $(OMNETPP_ROOT)),)
export PATH := $(OMNETPP_ROOT)/bin:$(PATH)
export LD_LIBRARY_PATH := $(OMNETPP_ROOT)/lib:$(LD_LIBRARY_PATH)
endif

.PHONY: all makefiles build run cmdenv clean test-core regressions urban all-configs summarize validation-fixture inet-adapter validate docs thesis-release thesis-regression thesis-diffraction thesis-ablation thesis-maps thesis-sensitivity thesis-summary help

all: build

makefiles:
	@command -v opp_makemake >/dev/null 2>&1 || { echo "ERROR: opp_makemake not found. Open an OMNeT++ terminal or set OMNETPP_ROOT."; exit 1; }
	cd src && opp_makemake -f --deep -o raytracing2d -KCFLAGS_EXTRA=-std=c++17

build: makefiles
	$(MAKE) -C src

run:
	cd simulations && ../src/raytracing2d -u Qtenv -n ../src:. -f omnetpp.ini -c $(CONFIG)

cmdenv:
	cd simulations && ../src/raytracing2d -u Cmdenv -n ../src:. -f omnetpp.ini -c $(CONFIG)

test-core: test-corrections test-m50
	mkdir -p build-tests
	g++ -std=c++17 -O2 -Wall -Wextra -pedantic \
		tests/geometry_test.cpp src/Geometry2D.cc src/RayTracer2D.cc src/Propagation2D.cc src/Materials2D.cc src/OsmImporter2D.cc src/GridEvaluator2D.cc src/FrequencySweep2D.cc \
		-Isrc -o build-tests/geometry_test
	./build-tests/geometry_test

regressions: build
	./scripts/run_all_cmdenv.sh

urban: build
	./scripts/run_urban_suite.sh

summarize:
	python3 scripts/summarize_urban.py

all-configs: build
	./scripts/run_all_configs.sh

# v1.6 thesis-release campaign groups. Run deliberately; no giant automatic
# campaign is provided because the map groups can be computationally expensive.
thesis-release: test-core docs
	@echo "v1.6 Thesis Release core/docs verified. Run thesis-* campaign groups separately."

thesis-regression: build
	./scripts/run_thesis_suite.sh regression

thesis-diffraction: build
	./scripts/run_thesis_suite.sh diffraction

thesis-ablation: build
	./scripts/run_thesis_suite.sh ablation

thesis-maps: build
	./scripts/run_thesis_suite.sh maps

thesis-sensitivity: build
	./scripts/run_thesis_suite.sh sensitivity

thesis-summary:
	python3 scripts/summarize_thesis_release.py --results simulations/results --out simulations/results/Thesis_v1.6_summary.csv

# Synthetic data verifies the evaluator only; it is never field-validation evidence.
validation-fixture:
	python3 scripts/evaluate_measurements.py \
		simulations/results/Urban05_Full-grid.csv validation/data/synthetic_measurements.csv \
		--summary validation/results/synthetic_summary.csv \
		--pairs validation/results/synthetic_pairs.csv
	python3 scripts/validate_measurement_fixture.py

inet-adapter:
	./scripts/run_inet_adapter.sh
	python3 scripts/validate_inet_adapter.py

# Main v1.5 reproducibility gate. The primary core remains INET-independent;
# the adapter is built and tested as a separate optional library.
validate: test-core all-configs
	python3 scripts/validate_results.py
	$(MAKE) validation-fixture
	$(MAKE) inet-adapter
	$(MAKE) docs

docs:
	cd doc/latex && latexmk -pdf -interaction=nonstopmode -halt-on-error main.tex
	cd doc/latex && latexmk -pdf -interaction=nonstopmode -halt-on-error results_report.tex
	cd doc/latex && latexmk -pdf -interaction=nonstopmode -halt-on-error v1_2_report.tex
	cd doc/latex && latexmk -pdf -interaction=nonstopmode -halt-on-error v1_3_report.tex
	cd doc/latex && latexmk -pdf -interaction=nonstopmode -halt-on-error v1_4_report.tex
	cd doc/latex && latexmk -pdf -interaction=nonstopmode -halt-on-error v1_5_report.tex
	cd doc/latex && latexmk -pdf -interaction=nonstopmode -halt-on-error v1_5_1_report.tex
	cd doc/latex && latexmk -pdf -interaction=nonstopmode -halt-on-error v1_6_thesis_release_report.tex

clean:
	@if [ -f src/Makefile ]; then $(MAKE) -C src clean || true; fi
	rm -rf build-tests out src/out src/raytracing2d
	rm -f simulations/results/* 2>/dev/null || true
	touch simulations/results/.gitkeep

help:
	@echo "RayTracing2D v1.6 Thesis Release"
	@echo "  make test-core                       # 25 C++ groups, no OMNeT++ needed"
	@echo "  make validation-fixture              # synthetic evaluator contract"
	@echo "  make inet-adapter                    # separate INET IPathLoss bridge"
	@echo "  make                                 # build OMNeT++ executable"
	@echo "  make run CONFIG=Urban16_FrequencySweep # Qtenv + frequency sweep"
	@echo "  make cmdenv CONFIG=Urban03_R1        # one configuration on command line"
	@echo "  make regressions                     # inherited analytical cases"
	@echo "  make urban                           # FSPL -> blockage -> R1 -> R2 -> D1"
	@echo "  make all-configs                     # every executable INI configuration"
	@echo "  make validate                        # legacy cumulative reproducibility gate"
	@echo "  python3 scripts/osm_inspect.py FILE  # preflight an OSM map"
	@echo "  make thesis-release                  # core tests + LaTeX docs only"
	@echo "  make thesis-regression               # T00-T02 canonical LoS/R1/R2"
	@echo "  make thesis-diffraction              # D00 discovery + D01-D04 NLoS/D1"
	@echo "  make thesis-ablation                 # A10-A12 at 25 m"
	@echo "  make thesis-maps                     # M50/M25/M10 + 3 final PNG maps"
	@echo "  make thesis-sensitivity              # P/F/G/S one-factor panels"
	@echo "  make thesis-summary                  # collect v1.6 .sca results"

.PHONY: test-corrections
test-corrections:
	mkdir -p build-tests
	g++ -std=c++17 -O2 -Wall -Wextra -pedantic tests/corrections_test.cpp src/Geometry2D.cc src/RayTracer2D.cc src/Propagation2D.cc src/Materials2D.cc src/OsmImporter2D.cc src/GridEvaluator2D.cc -Isrc -o build-tests/corrections_test
	./build-tests/corrections_test

.PHONY: test-m50
test-m50:
	mkdir -p build-tests
	g++ -std=c++17 -O2 -Wall -Wextra -pedantic tests/m50_053_test.cpp src/Geometry2D.cc src/RayTracer2D.cc src/Propagation2D.cc src/Materials2D.cc src/OsmImporter2D.cc -Isrc -o build-tests/m50_053_test
	./build-tests/m50_053_test
