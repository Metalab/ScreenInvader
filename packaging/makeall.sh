#!/bin/bash

if [ $# -ne 1 ]; then
  echo "Usage: makeall.sh <version>"
fi

./makebinaries.sh $1
./makeconfig.sh $1
./makecore.sh $1
./makemisc.sh $1

