Representing flash partitions in devicetree
===========================================

In addition to the upstream binding, another property is added:

Optional properties:
- ``partuuid`` : The global partition UUID for this partition.

Additionally, barebox also supports partitioning the eMMC boot partitions if
the partition table node is named appropriately:
- ``partitions`` : user partition
- ``boot0-partitions`` : boot0 partition
- ``boot1-partitions`` : boot1 partition

Examples:

.. code-block:: none

  / {
  	partitions {
  		compatible = "fixed-partitions";
  		#address-cells = <1>;
  		#size-cells = <1>;

  		state_part: state {
  			partuuid = "16367da7-c518-499f-9aad-e1f366692365";
  		};
  	};
  };

  emmc@1 {
  	boot0-partitions {
  		compatible = "fixed-partitions";
  		#address-cells = <1>;
  		#size-cells = <1>;

  		barebox@0 {
  			label = "barebox";
  			reg = <0x0 0x300000>;
  		};
  	};
  };
