// SPDX-License-Identifier: GPL-2.0-only
/*
 * OF helpers for network devices.
 *
 * Initially copied out of arch/powerpc/kernel/prom_parse.c
 */
#include <common.h>
#include <net.h>
#include <of_net.h>
#include <linux/phy.h>
#include <linux/nvmem-consumer.h>

/**
 * of_get_phy_mode - Get phy mode for given device_node
 * @np:	Pointer to the given device_node
 *
 * The function gets phy interface string from property 'phy-mode',
 * and return its index in phy_modes table, or errno in error case.
 */
int of_get_phy_mode(struct device_node *np)
{
	const char *pm;
	int err, i;

	err = of_property_read_string(np, "phy-mode", &pm);
	if (err < 0)
		err = of_property_read_string(np, "phy-connection-type", &pm);
	if (err < 0)
		return err;

	for (i = 0; i < PHY_INTERFACE_MODE_MAX; i++)
		if (!strcasecmp(pm, phy_modes(i)))
			return i;

	return -ENODEV;
}
EXPORT_SYMBOL_GPL(of_get_phy_mode);

static int of_get_mac_addr(struct device_node *np, const char *name, u8 *addr)
{
	struct property *pp = of_find_property(np, name, NULL);

	if (pp && pp->length == ETH_ALEN && is_valid_ether_addr(pp->value)) {
		memcpy(addr, pp->value, ETH_ALEN);
		return 0;
	}
	return -ENODEV;
}

int of_get_mac_addr_nvmem(struct device_node *np, u8 *addr)
{
	struct nvmem_cell *cell;
	void *mac;
	size_t len;

	if (!IS_ENABLED(CONFIG_NVMEM))
		return -ENODEV;

	cell = of_nvmem_cell_get(np, "mac-address");
	if (IS_ERR(cell))
		return PTR_ERR(cell);

	mac = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(mac))
		return PTR_ERR(mac);

	if (len != ETH_ALEN || !is_valid_ether_addr(mac)) {
		kfree(mac);
		return -EINVAL;
	}

	memcpy(addr, mac, ETH_ALEN);
	kfree(mac);

	return 0;
}

/**
 * Search the device tree for the best MAC address to use.  'mac-address' is
 * checked first, because that is supposed to contain to "most recent" MAC
 * address. If that isn't set, then 'local-mac-address' is checked next,
 * because that is the default address. If that isn't set, then the obsolete
 * 'address' is checked, just in case we're using an old device tree. If any
 * of the above isn't set, then try to get MAC address from nvmem cell named
 * 'mac-address'.
 *
 * Note that the 'address' property is supposed to contain a virtual address of
 * the register set, but some DTS files have redefined that property to be the
 * MAC address.
 *
 * All-zero MAC addresses are rejected, because those could be properties that
 * exist in the device tree, but were not set by U-Boot.  For example, the
 * DTS could define 'mac-address' and 'local-mac-address', with zero MAC
 * addresses.  Some older U-Boots only initialized 'local-mac-address'.  In
 * this case, the real MAC is in 'local-mac-address', and 'mac-address' exists
 * but is all zeros.
*/
int of_get_mac_address(struct device_node *np, u8 *addr)
{
	int ret;

	if (!np)
		return -ENODEV;

	ret = of_get_mac_addr(np, "mac-address", addr);
	if (!ret)
		return 0;

	ret = of_get_mac_addr(np, "local-mac-address", addr);
	if (!ret)
		return 0;

	ret = of_get_mac_addr(np, "address", addr);
	if (!ret)
		return 0;

	return of_get_mac_addr_nvmem(np, addr);
}
