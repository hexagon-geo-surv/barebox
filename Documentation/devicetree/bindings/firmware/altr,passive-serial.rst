Altera FPGAs in passive serial mode
===================================

This binding defines the control interface to Altera FPGAs in
passive serial mode. This is used to upload the firmware and
to start the FPGA.

Required properties:

- ``compatible``: shall be ``"altr,fpga-passive-serial"`` or
  ``"altr,fpga-arria10-passive-serial"`` for Arria 10
- ``reg``: SPI chip select
- ``nstat-gpios``: Specify GPIO for controlling the nstat pin
- ``confd-gpios``: Specify GPIO for controlling the confd pin
- ``nconfig-gpios``: Specify GPIO for controlling the nconfig pin

Example:

.. code-block:: none

	fpga@0 {
		compatible = "altr,fpga-passive-serial";
		nstat-gpios = <&gpio4 18 0>;
		confd-gpios = <&gpio4 19 0>;
		nconfig-gpios = <&gpio4 20 0>;
		spi-max-frequency = <10000000>;
		reg = <0>;
	};
