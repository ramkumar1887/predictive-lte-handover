# srsRAN 4G Predictive Handover Integration Guide

This directory contains the drop-in C++ source and unified patch to integrate **Predictive LTE Handover** into the official `srsRAN_4G` eNodeB stack.

## Files
1. `rrc_predictive_ho.h` & `rrc_predictive_ho.cc`: Standalone evaluator module implementing rolling buffer OLS regression slope analysis.
2. `srsran_predictive_ho.patch`: Unified diff patch against `srsRAN_4G/srsenb/src/stack/rrc/`.

## Step-by-Step Installation in Ubuntu / WSL2

```bash
# 1. Clone srsRAN_4G
git clone https://github.com/srsran/srsRAN_4G.git
cd srsRAN_4G

# 2. Copy the evaluator files into RRC stack
cp /path/to/srsran_patch/rrc_predictive_ho.h srsenb/hdr/stack/rrc/
cp /path/to/srsran_patch/rrc_predictive_ho.cc srsenb/src/stack/rrc/

# 3. Add to CMakeLists.txt
# In srsenb/src/stack/rrc/CMakeLists.txt, append:
#   rrc_predictive_ho.cc

# 4. Apply the patch
git apply /path/to/srsran_patch/srsran_predictive_ho.patch

# 5. Recompile
cd build
cmake ../ -DENABLE_ZEROMQ=ON
make -j$(nproc)
sudo make install
```
