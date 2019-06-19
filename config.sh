#!/bin/bash
# beta configuration utility for veikk driver

PARM_DIR=/sys/module/veikk/parameters

function quit {
  echo "======================================="
  echo "Exiting Veikk driver configuration tool"
  echo "======================================="
  echo
  exit
}

# edit setting
function editSetting {
  echo "Enter new value for setting:"
  case $1 in
    0 ) read -p "Orientation: integer; format: 0-3: " RES
        echo $RES | sudo tee ${PARM_DIR}/orientation;;
    1 ) read -p "Bounds map: integer list; format: 0-100,0-100,0-100,0-100; default: 0,0,100,100: " RES
        echo $RES | sudo tee ${PARM_DIR}/bounds_map;;
    2 ) read -p "Pressure map: integer; format: 0-4; default: 0: " RES
        echo $RES | sudo tee ${PARM_DIR}/pressure_map;;
    * ) quit;;
  esac
}

# view setting
function viewSetting {
  case $1 in
    0 ) sudo cat ${PARM_DIR}/orientation;;
    1 ) sudo cat ${PARM_DIR}/bounds_map;;
    2 ) sudo cat ${PARM_DIR}/pressure_map;;
    * ) quit;;
  esac
}

function init {
  echo
  echo "================================================"
  echo "Configure Veikk driver for Linux -- beta version"
  echo "================================================"

  # infinite ask loop
  while [ 1 ]; do

    # choose a setting
    echo; echo "Choose a setting:"
    select opt in "Orientation" "Screen mapping" "Pressure mapping" "(Anything else to quit)"; do
      case $opt in
        "Orientation" ) SETTING=0; break;;
        "Screen mapping" ) SETTING=1; break;;
        "Pressure mapping" ) SETTING=2; break;;
        * ) quit;;
      esac
    done

    # choose an action
    echo "Choose an action:"
    select opt in "Edit setting" "View setting" "(Anything else to quit)"; do
      case $opt in
        "Edit setting" ) editSetting $SETTING; break;;
        "View setting" ) viewSetting $SETTING; break;;
        * ) quit;;
      esac
    done

  done
}
init
