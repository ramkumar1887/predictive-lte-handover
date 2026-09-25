#!/bin/bash
set -e

echo "================================================================="
echo " Setting up srsRAN_4G + Open5GS with ZMQ in Ubuntu / WSL2"
echo "================================================================="

# 1. Update and install dependencies
sudo apt update && sudo apt upgrade -y
sudo apt install -y cmake libfftw3-dev libmbedtls-dev libboost-program-options-dev \
                    libconfig++-dev libsctp-dev libzmq3-dev git build-essential \
                    software-properties-common curl gnupg

# 2. Install MongoDB & Open5GS
echo "Installing Open5GS Core Network..."
sudo add-apt-repository -y ppa:open5gs/latest
sudo apt update
sudo apt install -y open5gs mongodb
sudo systemctl enable --now open5gs-mmed open5gs-sgwud open5gs-upfd mongodb || true

# 3. Clone and Build srsRAN_4G
SRSRAN_DIR="$HOME/srsRAN_4G"
if [ ! -d "$SRSRAN_DIR" ]; then
    echo "Cloning srsRAN_4G..."
    git clone https://github.com/srsran/srsRAN_4G.git "$SRSRAN_DIR"
fi

cd "$SRSRAN_DIR"
mkdir -p build && cd build
cmake ../ -DENABLE_ZEROMQ=ON
make -j$(nproc)
sudo make install
sudo srsran_install_configs.sh user

echo "================================================================="
echo " Environment setup complete! srsRAN and Open5GS are ready."
echo "================================================================="
