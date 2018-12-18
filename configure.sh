#!/bin/bash

veloc_HOME=/home/xin/codes/lossyVELOC
/home/xin/utils/cmake-3.11.2-Linux-x86_64/bin/cmake -DWITH_AXL_PREFIX=$veloc_HOME/build -DWITH_ER_PREFIX=$veloc_HOME/build -DBOOST_ROOT=$veloc_HOME/build -DCMAKE_INSTALL_PREFIX=$veloc_HOME/build -DCMAKE_PREFIX_PATH=/home/xin/utils/sz_master/lib #-DCMAKE_CXX_FLAGS=-std=c++11 
