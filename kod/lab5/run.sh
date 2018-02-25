#! /bin/bash

docker build . --rm=true -t ubuntu-srsv

docker rm srsv-lab5

docker run -v $(pwd):/data/code \
           --name='srsv-lab5' \
           --memory='256m' \
           --cpus='1' \
           -it ubuntu-srsv bash