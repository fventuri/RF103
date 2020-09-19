# RF103 - A low level library for wideband SDR receivers

A library and a few simple applications for wideband SDR receivers like BBRF103, RX-666, RX888, HF103, etc

These SDR receivers and this library wouldn't be possible without all the excellent work by Oscar Steila, IK1XPV - a great deal of useful information about them is at <http://www.steila.com/blog/> and at <https://sdr-prototypes.blogspot.com/>.

This library is similar in concept to librtlsdr (see <https://osmocom.org/projects/rtl-sdr/wiki> and <https://github.com/librtlsdr/librtlsdr>).

I wrote this library and the example applications from scratch (i.e. any bug in this code is exclusively my fault). Many parts of the code use Oscar's ExtIO dll driver for Windows (<https://github.com/ik1xpv/ExtIO_sddc-Ver0.96>) as a reference, and I want to really thank him for this wonderful project!


## Credits

- Oscar Steila, IK1XPV for the BBRF103 and HR103 projects


## How to build

```
cd rf103
mkdir build
cd build
cmake ..
make
sudo make install
```

## udev rules

On Linux usually only root has full access to the USB devices. In order to be able to run these programs and other programs that use this library as a regular user, you may want to add some exception rules for these USB devices. A simple and effective way to create persistent rules (which will last even after a reboot) is to add the file <misc/99-rf103.rules> to your udev rule directory '/etc/udev/rules.d' and tell 'udev' to reload its rules.

These are the commands that need to be run only once using sudo to grant access to these SDRs to a regular user:
```
sudo cp misc/99-rf103.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
```

## Copyright

(C) 2020 Franco Venturi - Licensed under the GNU GPL V3 (see <LICENSE>)
