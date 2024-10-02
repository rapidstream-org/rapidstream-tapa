# This scipt demonstrates how to run the VecAdd application with RapidStream

# This script demonstrates how to csynth TAPA applications to XO files, optimize the
# design with RapidStream, and cosimulate the design with TAPA.
# To use this script, cd into the directory of this script and run
# with "source run_rs.sh". The run_rs.sh script under the rs_templetes directory
# contains the main logic for running TAPA with the RapidStream tools.

APP=Cannon
CSYNTH_FILE=cannon.cpp
ARGS=""

source ../../rs_templetes/run_rs.sh
