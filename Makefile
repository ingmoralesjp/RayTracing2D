.PHONY: test verify

test:
	$(MAKE) -C framework test-core

verify:
	python3 tools/verify_material.py
