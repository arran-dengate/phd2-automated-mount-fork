#!/bin/bash

cmake .. && make -j4
if [[ $? == 0 ]]; then
  notify-send "Build completed successfully!"
else
  errorcode=$?
  notify-send "Build failed with error code $errorcode."
fi

#$(./phd2 &)
#echo "pulse_output contents:"
#cat pulse_output
