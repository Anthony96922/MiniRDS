#!/bin/bash

# Create station logo data to build into the mpxgen executable for RDS2

if [ -z "$1" ]; then
  echo "Usage: $0 <logo file>"
  exit
fi

if [ -f "$1" ]; then
  echo 'unsigned char station_logo[] = {' > rds2_image_data.c
  xxd -i < "$1" >> rds2_image_data.c
  echo '};' >> rds2_image_data.c
  echo 'unsigned int station_logo_len = sizeof(station_logo);' >> rds2_image_data.c
fi
