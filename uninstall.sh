#!/bin/bash
# Uninstall the driver
#sudo dkms remove -m leetmouse-driver -v 0.9.0 # Newer dkms versions
sudo dkms remove leetmouse-driver/0.9.0 --all # Older (for Ubuntu <= 20.04), but should work now too
sudo make remove_dkms
sudo rmmod leetmouse
