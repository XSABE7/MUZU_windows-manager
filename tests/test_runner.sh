#!/usr/bin/env bash
set -euo pipefail

echo "================================================="
echo "       MUZUwm Automated Test & Benchmark         "
echo "================================================="

echo "1. Running Unit Tests..."
make -s tests muzuwm
./test_struts
./test_titlebar
./test_snap_preview
./test_layout
./test_client_props
./test_config_rules
bash tests/test_session_install.sh
echo "--> All Unit Tests & Verification Passed!"

echo ""
echo "================================================="
echo "           ALL TESTS PASSED CLEANLY              "
echo "================================================="
exit 0
