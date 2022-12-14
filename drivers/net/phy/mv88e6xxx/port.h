/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _MV88E6XXX_PORT_H
#define _MV88E6XXX_PORT_H

#include "chip.h"

/* Offset 0x00: Port Status Register */
#define MV88E6XXX_PORT_STS			0x00
#define MV88E6XXX_PORT_STS_PAUSE_EN		0x8000
#define MV88E6XXX_PORT_STS_MY_PAUSE		0x4000
#define MV88E6XXX_PORT_STS_HD_FLOW		0x2000
#define MV88E6XXX_PORT_STS_PHY_DETECT		0x1000
#define MV88E6XXX_PORT_STS_LINK			0x0800
#define MV88E6XXX_PORT_STS_DUPLEX		0x0400
#define MV88E6XXX_PORT_STS_SPEED_MASK		0x0300
#define MV88E6XXX_PORT_STS_SPEED_10		0x0000
#define MV88E6XXX_PORT_STS_SPEED_100		0x0100
#define MV88E6XXX_PORT_STS_SPEED_1000		0x0200
#define MV88E6XXX_PORT_STS_SPEED_10000		0x0300
#define MV88E6352_PORT_STS_EEE			0x0040
#define MV88E6165_PORT_STS_AM_DIS		0x0040
#define MV88E6185_PORT_STS_MGMII		0x0040
#define MV88E6XXX_PORT_STS_TX_PAUSED		0x0020
#define MV88E6XXX_PORT_STS_FLOW_CTL		0x0010
#define MV88E6XXX_PORT_STS_CMODE_MASK		0x000f
#define MV88E6XXX_PORT_STS_CMODE_100BASE_X	0x0008
#define MV88E6XXX_PORT_STS_CMODE_1000BASE_X	0x0009
#define MV88E6XXX_PORT_STS_CMODE_SGMII		0x000a
#define MV88E6XXX_PORT_STS_CMODE_2500BASEX	0x000b
#define MV88E6XXX_PORT_STS_CMODE_XAUI		0x000c
#define MV88E6XXX_PORT_STS_CMODE_RXAUI		0x000d
#define MV88E6185_PORT_STS_CDUPLEX		0x0008
#define MV88E6185_PORT_STS_CMODE_MASK		0x0007
#define MV88E6185_PORT_STS_CMODE_GMII_FD	0x0000
#define MV88E6185_PORT_STS_CMODE_MII_100_FD_PS	0x0001
#define MV88E6185_PORT_STS_CMODE_MII_100	0x0002
#define MV88E6185_PORT_STS_CMODE_MII_10		0x0003
#define MV88E6185_PORT_STS_CMODE_SERDES		0x0004
#define MV88E6165_PORT_STS_CMODE_PHY		0x0004
#define MV88E6185_PORT_STS_CMODE_1000BASE_X	0x0005
#define MV88E6185_PORT_STS_CMODE_PHY		0x0006
#define MV88E6165_PORT_STS_CMODE_SGMII		0x0006
#define MV88E6185_PORT_STS_CMODE_DISABLED	0x0007



/* Offset 0x01: MAC (or PCS or Physical) Control Register */
#define MV88E6XXX_PORT_MAC_CTL				0x01
#define MV88E6XXX_PORT_MAC_CTL_RGMII_DELAY_RXCLK	0x8000
#define MV88E6XXX_PORT_MAC_CTL_RGMII_DELAY_TXCLK	0x4000
#define MV88E6185_PORT_MAC_CTL_SYNC_OK			0x4000
#define MV88E6390_PORT_MAC_CTL_FORCE_SPEED		0x2000
#define MV88E6390_PORT_MAC_CTL_ALTSPEED			0x1000
#define MV88E6352_PORT_MAC_CTL_200BASE			0x1000
#define MV88E6185_PORT_MAC_CTL_AN_EN			0x0400
#define MV88E6185_PORT_MAC_CTL_AN_RESTART		0x0200
#define MV88E6185_PORT_MAC_CTL_AN_DONE			0x0100
#define MV88E6XXX_PORT_MAC_CTL_FC			0x0080
#define MV88E6XXX_PORT_MAC_CTL_FORCE_FC			0x0040
#define MV88E6XXX_PORT_MAC_CTL_LINK_UP			0x0020
#define MV88E6XXX_PORT_MAC_CTL_FORCE_LINK		0x0010
#define MV88E6XXX_PORT_MAC_CTL_DUPLEX_FULL		0x0008
#define MV88E6XXX_PORT_MAC_CTL_FORCE_DUPLEX		0x0004
#define MV88E6XXX_PORT_MAC_CTL_SPEED_MASK		0x0003
#define MV88E6XXX_PORT_MAC_CTL_SPEED_10			0x0000
#define MV88E6XXX_PORT_MAC_CTL_SPEED_100		0x0001
#define MV88E6065_PORT_MAC_CTL_SPEED_200		0x0002
#define MV88E6XXX_PORT_MAC_CTL_SPEED_1000		0x0002
#define MV88E6390_PORT_MAC_CTL_SPEED_10000		0x0003
#define MV88E6XXX_PORT_MAC_CTL_SPEED_UNFORCED		0x0003

/* Offset 0x03: Switch Identifier Register */
#define MV88E6XXX_PORT_SWITCH_ID		0x03
#define MV88E6XXX_PORT_SWITCH_ID_PROD_MASK	0xfff0
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6085	0x04a0
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6095	0x0950
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6097	0x0990
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6190X	0x0a00
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6390X	0x0a10
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6131	0x1060
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6320	0x1150
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6123	0x1210
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6161	0x1610
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6165	0x1650
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6171	0x1710
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6172	0x1720
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6175	0x1750
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6176	0x1760
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6190	0x1900
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6191	0x1910
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6185	0x1a70
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6240	0x2400
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6250	0x2500
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6290	0x2900
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6321	0x3100
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6141	0x3400
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6341	0x3410
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6352	0x3520
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6350	0x3710
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6351	0x3750
#define MV88E6XXX_PORT_SWITCH_ID_PROD_6390	0x3900
#define MV88E6XXX_PORT_SWITCH_ID_REV_MASK	0x000f


/*
 * SERDES connected to port 0x04 is accessible at address 0xC
 */
#define MV88E6165_PORT_SERDES_OFFSET		(0x0C - 0x04)

int mv88e6xxx_port_read(struct mv88e6xxx_chip *chip, int port, int reg,
			u16 *val);
int mv88e6xxx_port_write(struct mv88e6xxx_chip *chip, int port, int reg,
			 u16 val);

int mv88e6185_port_set_pause(struct mv88e6xxx_chip *chip, int port,
			     int pause);
int mv88e6352_port_set_rgmii_delay(struct mv88e6xxx_chip *chip, int port,
				   phy_interface_t mode);
int mv88e6390_port_set_rgmii_delay(struct mv88e6xxx_chip *chip, int port,
				   phy_interface_t mode);
int mv88e6xxx_port_set_link(struct mv88e6xxx_chip *chip, int port, int link);
int mv88e6xxx_port_set_duplex(struct mv88e6xxx_chip *chip, int port, int dup);
int mv88e6065_port_set_speed(struct mv88e6xxx_chip *chip, int port, int speed);
int mv88e6185_port_set_speed(struct mv88e6xxx_chip *chip, int port, int speed);
int mv88e6250_port_set_speed(struct mv88e6xxx_chip *chip, int port, int speed);
int mv88e6352_port_set_speed(struct mv88e6xxx_chip *chip, int port, int speed);
int mv88e6390_port_set_speed(struct mv88e6xxx_chip *chip, int port, int speed);
int mv88e6390x_port_set_speed(struct mv88e6xxx_chip *chip, int port, int speed);
int mv88e6390x_port_set_cmode(struct mv88e6xxx_chip *chip, int port,
			      phy_interface_t mode);
int mv88e6352_port_link_state(struct mv88e6xxx_chip *chip, int port,
			      struct phylink_link_state *state);
int mv88e6185_port_link_state(struct mv88e6xxx_chip *chip, int port,
			      struct phylink_link_state *state);

/* Barebox specific */
int mv88e6xxx_port_probe(struct mv88e6xxx_chip *chip);


#endif /* _MV88E6XXX_PORT_H */
