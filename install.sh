#!/bin/bash

make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
sudo insmod veikk.ko
