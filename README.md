SC2 OFDM Reference Design Compilation Instructions
Instructions assume operation inside of a container. 
Tested on SC2 base4nocuda-v1

ofdm_reference.tar.gz contents:
ofdm_reference/
|-- dependencies/
    |-- libconfig/
    |-- libfec/
    |-- liquid_dsp/
    |-- liquid_usrp/
    |-- uhd/
|-- ofdm_radio/
    |-- src/
    |-- src_reusable/

Dependency Compilation and Installation:

1. uhd
Note: can skip if using base4 or above
Follow instructions on https://files.ettus.com/manual/page_build_guide.html

2. libconfig
cd dependencies/libconfig/
./configure
make
make install
ldconfig

3. libfec
cd dependencies/libfec/
mkdir build
cd build
cmake ../
make
make install
ldconfig

4. liquid_dsp
cd dependencies/liquid_dsp/
./configure
make
make install
ldconfig

5. liquid_usrp
cd dependencies/liquid_usrp/
./configure
make
make install

OFDM Reference Design Compilation:

1. ofdm_reference
cd ofdm_radio/src/
make

OFDM Reference Design Usage Notes:
-Make sure to modify the example config files in ofdm_radio/src/config_files to use the correct USRP IP addresses (key: usrp_address_name)
-Run the radio by executing: ./ofdm_reference --config-file /path/to/config/
-The radio is designed to run with a 10 MHz reference with PPS which can be provided by a benchtop reference or the Ettus GPSDO. It will work with no reference, but performance will be degraded
-Generally, gain values should be low (0-10.0 range)



