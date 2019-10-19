#!/usr/bin/env bash

cd $1
shift

$BR2_EXTERNAL_ZYBO_Z7_PATH/board/common/mkfit.py fit.its $@ \
        && mkimage -f fit.its fit.itb
