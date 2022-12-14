Altera SOCFPGA FPGA Manager
===========================

This binding defines the FPGA Manager on Altera SOCFPGAs. This is used to upload
the firmware to the FPGA part of the SoC.

Required properties:

- ``compatible``: shall be ``"altr,socfpga-fpga-mgr"``
- ``reg``: Must contain 2 register ranges:

  1. The control address space of the FPGA manager.
  2. The configuration data address space where the firmware data is written to.

Example:

.. code-block:: none

	fpgamgr@ff706000 {
		compatible = "altr,socfpga-fpga-mgr";
		reg = <0xff706000 0x1000>,
		      <0xffb90000 0x1000>;
	};
