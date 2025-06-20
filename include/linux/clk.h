/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: 2004 ARM Limited */

/*
 *  linux/include/linux/clk.h
 *
 *  Written by Deep Blue Solutions Limited.
 */

#ifndef __LINUX_CLK_H
#define __LINUX_CLK_H

#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/stringify.h>
#include <linux/string.h>
#include <linux/container_of.h>
#include <deep-probe.h>
#include <xfuncs.h>

struct device;

/*
 * The base API.
 */

/*
 * struct clk - an machine class defined object / cookie.
 */
struct clk;
struct clk_hw;

/**
 * struct clk_bulk_data - Data used for bulk clk operations.
 *
 * @id: clock consumer ID
 * @clk: struct clk * to store the associated clock
 *
 * The CLK APIs provide a series of clk_bulk_() API calls as
 * a convenience to consumers which require multiple clks.  This
 * structure is used to manage data for these calls.
 */
struct clk_bulk_data {
	const char		*id;
	struct clk		*clk;
};


#ifdef CONFIG_HAVE_CLK

/**
 * clk_get - lookup and obtain a reference to a clock producer.
 * @dev: device for clock "consumer"
 * @id: clock comsumer ID
 *
 * Returns a struct clk corresponding to the clock producer, or
 * valid IS_ERR() condition containing errno.  The implementation
 * uses @dev and @id to determine the clock consumer, and thereby
 * the clock producer.  (IOW, @id may be identical strings, but
 * clk_get may return different clock producers depending on @dev.)
 *
 * Drivers must assume that the clock source is not enabled.
 *
 * clk_get should not be called from within interrupt context.
 */
struct clk *clk_get(struct device *dev, const char *id);

/**
 * clk_enable - inform the system when the clock source should be running.
 * @clk: clock source
 *
 * If the clock can not be enabled/disabled, this should return success.
 *
 * Returns success (0) or negative errno.
 */
int clk_enable(struct clk *clk);

/**
 * clk_disable - inform the system when the clock source is no longer required.
 * @clk: clock source
 *
 * Inform the system that a clock source is no longer required by
 * a driver and may be shut down.
 *
 * Implementation detail: if the clock source is shared between
 * multiple drivers, clk_enable() calls must be balanced by the
 * same number of clk_disable() calls for the clock source to be
 * disabled.
 */
void clk_disable(struct clk *clk);

/**
 * clk_get_rate - obtain the current clock rate (in Hz) for a clock source.
 *		  This is only valid once the clock source has been enabled.
 * @clk: clock source
 */
unsigned long clk_get_rate(struct clk *clk);
unsigned long clk_hw_get_rate(struct clk_hw *hw);

/*
 * The remaining APIs are optional for machine class support.
 */


/**
 * clk_round_rate - adjust a rate to the exact rate a clock can provide
 * @clk: clock source
 * @rate: desired clock rate in Hz
 *
 * Returns rounded clock rate in Hz, or negative errno.
 */
long clk_round_rate(struct clk *clk, unsigned long rate);
long clk_hw_round_rate(struct clk_hw *hw, unsigned long rate);
/**
 * clk_set_rate - set the clock rate for a clock source
 * @clk: clock source
 * @rate: desired clock rate in Hz
 *
 * Returns success (0) or negative errno.
 */
int clk_set_rate(struct clk *clk, unsigned long rate);
int clk_hw_set_rate(struct clk_hw *hw, unsigned long rate);

/**
 * clk_set_parent - set the parent clock source for this clock
 * @clk: clock source
 * @parent: parent clock source
 *
 * Returns success (0) or negative errno.
 */
int clk_set_parent(struct clk *clk, struct clk *parent);
int clk_hw_set_parent(struct clk_hw *hw, struct clk_hw *hwp);
struct clk_hw *clk_hw_get_parent_by_index(const struct clk_hw *hw, unsigned int idx);

/**
 * clk_get_parent - get the parent clock source for this clock
 * @clk: clock source
 *
 * Returns struct clk corresponding to parent clock source, or
 * valid IS_ERR() condition containing errno.
 */
struct clk *clk_get_parent(struct clk *clk);
struct clk_hw *clk_hw_get_parent(struct clk_hw *hw);
int clk_hw_get_parent_index(struct clk_hw *hw);

int clk_set_phase(struct clk *clk, int degrees);
int clk_get_phase(struct clk *clk);

/**
 * clk_get_sys - get a clock based upon the device name
 * @dev_id: device name
 * @con_id: connection ID
 *
 * Returns a struct clk corresponding to the clock producer, or
 * valid IS_ERR() condition containing errno.  The implementation
 * uses @dev_id and @con_id to determine the clock consumer, and
 * thereby the clock producer. In contrast to clk_get() this function
 * takes the device name instead of the device itself for identification.
 *
 * Drivers must assume that the clock source is not enabled.
 *
 * clk_get_sys should not be called from within interrupt context.
 */
struct clk *clk_get_sys(const char *dev_id, const char *con_id);

/**
 * clk_add_alias - add a new clock alias
 * @alias: name for clock alias
 * @alias_dev_name: device name
 * @id: platform specific clock name
 * @dev: device
 *
 * Allows using generic clock names for drivers by adding a new alias.
 * Assumes clkdev, see clkdev.h for more info.
 */
int clk_add_alias(const char *alias, const char *alias_dev_name, char *id,
			struct device *dev);

#else

static inline struct clk *clk_get(struct device *dev, const char *id)
{
	return NULL;
}

static inline struct clk *clk_get_parent(struct clk *clk)
{
	return NULL;
}

static inline struct clk_hw *clk_hw_get_parent(struct clk_hw *hw)

{
	return NULL;
}

static inline int clk_hw_get_parent_index(struct clk_hw *hw)
{
	return -EINVAL;
}

static inline int clk_enable(struct clk *clk)
{
	return 0;
}

static inline void clk_disable(struct clk *clk)
{
}

static inline unsigned long clk_get_rate(struct clk *clk)
{
	return 0;
}

static inline long clk_round_rate(struct clk *clk, unsigned long rate)
{
	return 0;
}

static inline int clk_set_rate(struct clk *clk, unsigned long rate)
{
	return 0;
}
#endif

#define clk_prepare_enable(clk) clk_enable(clk)
#define clk_disable_unprepare(clk) clk_disable(clk)

static inline void clk_put(struct clk *clk)
{
}

#ifdef CONFIG_COMMON_CLK

#include <linux/list.h>

#define CLK_SET_RATE_PARENT     (1 << 0) /* propagate rate change up one level */
#define CLK_IGNORE_UNUSED       (1 << 3) /* do not gate even if unused */
#define CLK_GET_RATE_NOCACHE    (1 << 6) /* do not use the cached clk rate */
#define CLK_SET_RATE_NO_REPARENT (1 << 7) /* don't re-parent on rate change */
#define CLK_SET_RATE_UNGATE     (1 << 10) /* clock needs to run to set rate */
#define CLK_IS_CRITICAL         (1 << 11) /* do not gate, ever */
/* parents need enable during gate/ungate, set rate and re-parent */
#define CLK_OPS_PARENT_ENABLE   (1 << 12)

#define CLK_GATE_SET_TO_DISABLE	(1 << 0)
#define CLK_GATE_HIWORD_MASK	(1 << 1)

/* Ignored sanity checking flags */
#define CLK_SET_RATE_GATE	0 /* must be gated across rate change */
#define CLK_SET_PARENT_GATE	0 /* must be gated across re-parent */


/**
 * struct clk_ops -  Callback operations for hardware clocks; these are to
 * be provided by the clock implementation, and will be called by drivers
 * through the clk_* api.
 *
 * @init:	Perform platform-specific initialization magic.
 *		This is not used by any of the basic clock types.
 *		This callback exist for HW which needs to perform some
 *		initialisation magic for CCF to get an accurate view of the
 *		clock. It may also be used dynamic resource allocation is
 *		required. It shall not used to deal with clock parameters,
 *		such as rate or parents.
 *		Returns 0 on success, -EERROR otherwise.
 *
 * @enable:	Prepare and enable the clock atomically. This must not return
 *		until the clock is generating a valid clock signal, usable by
 *		consumer devices.
 * @prepare:	Alias for @enable. If the Linux driver defines both, they
 *		must be merged when ported to barebox.
 *
 * @disable:	Unprepare and disable the clock atomically.
 * @unprepare:	Alias for @disable. If the Linux driver defines both, they
 *		must be merged when ported to barebox.
 *
 * @is_enabled:	Queries the hardware to determine if the clock is enabled.
 *		Optional, if this op is not set then the enable count will be
 *		used.
 * @is_prepared Alias for @is_enabled. If the Linux driver defines both, they
 *		must be merged when ported to barebox.
 *
 * @recalc_rate	Recalculate the rate of this clock, by querying hardware. The
 *		parent rate is an input parameter. If the driver cannot figure
 *		out a rate for this clock, it must return 0. Returns the
 *		calculated rate. Optional, but recommended - if this op is not
 *		set then clock rate will be initialized to 0.
 *
 * @round_rate:	Given a target rate as input, returns the closest rate actually
 *		supported by the clock. The parent rate is an input/output
 *		parameter.
 *
 * @set_parent:	Change the input source of this clock; for clocks with multiple
 *		possible parents specify a new parent by passing in the index
 *		as a u8 corresponding to the parent in either the .parent_names
 *		or .parents arrays.  This function in affect translates an
 *		array index into the value programmed into the hardware.
 *		Returns 0 on success, -EERROR otherwise.
 *
 * @get_parent:	Queries the hardware to determine the parent of a clock.  The
 *		return value is a u8 which specifies the index corresponding to
 *		the parent clock.  This index can be applied to either the
 *		.parent_names or .parents arrays.  In short, this function
 *		translates the parent value read from hardware into an array
 *		index.
 *
 * @set_rate:	Change the rate of this clock. The requested rate is specified
 *		by the second argument, which should typically be the return
 *		of .round_rate call.  The third argument gives the parent rate
 *		which is likely helpful for most .set_rate implementation.
 *		Returns 0 on success, -EERROR otherwise.
 *
 * @set_phase:	Shift the phase this clock signal in degrees specified
 *		by the second argument. Valid values for degrees are
 *		0-359. Return 0 on success, otherwise -EERROR.
 *
 * @get_phase:	Queries the hardware to get the current phase of a clock.
 *		Returned values are 0-359 degrees on success, negative
 *		error codes on failure.
 *
 * Unlike Linux, there is no differentiation between clk_prepare/clk_enable
 * and clk_unprepare/clk_disable in barebox as all work is atomic.
 */
struct clk_ops {
	int 		(*init)(struct clk_hw *hw);
	union {
		int		(*enable)(struct clk_hw *hw);
		int		(*prepare)(struct clk_hw *hw);
	};
	union {
		void		(*disable)(struct clk_hw *hw);
		void		(*unprepare)(struct clk_hw *hw);
	};
	union {
		int		(*is_enabled)(struct clk_hw *hw);
		int		(*is_prepared)(struct clk_hw *hw);
	};
	unsigned long	(*recalc_rate)(struct clk_hw *hw,
					unsigned long parent_rate);
	long		(*round_rate)(struct clk_hw *hw, unsigned long,
					unsigned long *);
	int		(*set_parent)(struct clk_hw *hw, u8 index);
	int		(*get_parent)(struct clk_hw *hw);
	int		(*set_rate)(struct clk_hw *hw, unsigned long,
				    unsigned long);
	int		(*set_phase)(struct clk_hw *hw, int degrees);
	int		(*get_phase)(struct clk_hw *hw);
};

/**
 * struct clk_parent_data - clk parent information
 * @hw: parent clk_hw pointer (used for clk providers with internal clks)
 * @name: globally unique parent name (used as a fallback)
 * @index: parent index local to provider registering clk (if @fw_name absent)
 */
struct clk_parent_data {
	const struct clk_hw	*hw;
	const char		*name;
	int			index;
};

/**
 * struct clk_init_data - holds init data that's common to all clocks and is
 * shared between the clock provider and the common clock framework.
 *
 * @name: clock name
 * @ops: operations this clock supports
 * @parent_names: array of string names for all possible parents
 * @parent_hws: array of pointers to all possible parents (when all parents
 *              are internal to the clk controller)
 * @num_parents: number of possible parents
 * @flags: framework-level hints and quirks
 */
struct clk_init_data {
	const char		*name;
	const struct clk_ops	*ops;
	const char		* const *parent_names;
	const struct clk_parent_data    *parent_data;
	const struct clk_hw	**parent_hws;
	unsigned int		num_parents;
	unsigned long		flags;
};

struct clk {
	const struct clk_ops *ops;
	int enable_count;
	struct list_head list;
	const char *name;
	const char * const *parent_names;
	int num_parents;

	struct clk **parents;
	unsigned long flags;
};

/**
 * struct clk_hw - handle for traversing from a struct clk to its corresponding
 * hardware-specific structure.  struct clk_hw should be declared within struct
 * clk_foo and then referenced by the struct clk instance that uses struct
 * clk_foo's clk_ops
 *
 * @clk: pointer to the per-user struct clk instance that can be used to call
 * into the clk API
 *
 * @init: pointer to struct clk_init_data that contains the init data shared
 * with the common clock framework.
 */
struct clk_hw {
	struct clk clk;
	const struct clk_init_data *init;
};

static inline struct clk *clk_hw_to_clk(const struct clk_hw *hw)
{
	return IS_ERR(hw) ? ERR_CAST(hw) : (struct clk *)&hw->clk;
}

static inline struct clk_hw *clk_to_clk_hw(const struct clk *clk)
{
	return container_of_safe(clk, struct clk_hw, clk);
}
#define __clk_get_hw(clk) clk_to_clk_hw(clk)

struct clk_div_table {
	unsigned int	val;
	unsigned int	div;
};

struct clk *clk_register_fixed_rate(const char *name,
				    const char *parent_name, unsigned long flags,
				    unsigned long fixed_rate);

struct clk_hw *clk_hw_register_fixed_rate(struct device *dev,
					  const char *name,
					  const char *parent_name,
					  unsigned long flags,
					  unsigned long rate);

static inline struct clk *clk_fixed(const char *name, int rate)
{
	return clk_register_fixed_rate(name, NULL, 0, rate);
}

struct clk_divider {
	struct clk_hw hw;
	u8 shift;
	u8 width;
	void __iomem *reg;
	const char *parent;
#define CLK_DIVIDER_ONE_BASED	(1 << 0)
	unsigned flags;
	const struct clk_div_table *table;
	int max_div_index;
	int table_size;
	spinlock_t *lock;
};

#define to_clk_divider(_hw) container_of(_hw, struct clk_divider, hw)

#define clk_div_mask(width)	((1 << (width)) - 1)

#define CLK_DIVIDER_POWER_OF_TWO	(1 << 1)
#define CLK_DIVIDER_ALLOW_ZERO		(1 << 2)
#define CLK_DIVIDER_HIWORD_MASK		(1 << 3)
#define CLK_DIVIDER_READ_ONLY		(1 << 5)

#define CLK_MUX_HIWORD_MASK		(1 << 2)
#define CLK_MUX_READ_ONLY		(1 << 3) /* mux can't be changed */

extern const struct clk_ops clk_divider_ops;
extern const struct clk_ops clk_divider_ro_ops;

static inline void clk_hw_reparent(struct clk_hw *hw, struct clk_hw *new_parent)
{
	/* clk_get_parent always reads from HW, so nothing to update here */
}

static inline int __clk_get_enable_count(struct clk *clk)
{
	return !clk ? 0 : clk->enable_count;
}

unsigned long divider_recalc_rate(struct clk *clk, unsigned long parent_rate,
		unsigned int val,
		const struct clk_div_table *table,
		unsigned long flags, unsigned long width);

long divider_round_rate(struct clk *clk, unsigned long rate,
			unsigned long *prate, const struct clk_div_table *table,
			u8 width, unsigned long flags);

int divider_get_val(unsigned long rate, unsigned long parent_rate,
		    const struct clk_div_table *table, u8 width,
		    unsigned long flags);

struct clk *clk_divider_alloc(const char *name, const char *parent,
			      unsigned clk_flags, void __iomem *reg,
			      u8 shift, u8 width, unsigned div_flags);
void clk_divider_free(struct clk *clk_divider);
struct clk *clk_divider(const char *name, const char *parent,
			unsigned clk_flags, void __iomem *reg, u8 shift,
			u8 width, unsigned div_flags);
struct clk *clk_register_divider(struct device *dev, const char *name,
				 const char *parent_name, unsigned long flags,
				 void __iomem *reg, u8 shift, u8 width,
				 u8 clk_divider_flags, spinlock_t *lock);
struct clk *clk_divider_one_based(const char *name, const char *parent,
				  unsigned clk_flags, void __iomem *reg,
				  u8 shift, u8 width, unsigned div_flags);
struct clk *clk_divider_table(const char *name, const char *parent,
			      unsigned clk_flags, void __iomem *reg, u8 shift,
			      u8 width, const struct clk_div_table *table,
			      unsigned div_flags);
struct clk *clk_register_divider_table(struct device *dev, const char *name,
				       const char *parent_name,
				       unsigned long flags,
				       void __iomem *reg, u8 shift, u8 width,
				       u8 clk_divider_flags,
				       const struct clk_div_table *table,
				       spinlock_t *lock);

struct clk_hw *clk_hw_register_divider_table(struct device *dev,
					     const char *name,
					     const char *parent_name,
					     unsigned long flags,
					     void __iomem *reg, u8 shift,
					     u8 width,
					     u8 clk_divider_flags,
					     const struct clk_div_table *table,
					     spinlock_t *lock);

struct clk_hw *clk_hw_register_divider(struct device *dev, const char *name,
				       const char *parent_name,
				       unsigned long flags,
				       void __iomem *reg, u8 shift, u8 width,
				       u8 clk_divider_flags, spinlock_t *lock);

struct clk_fixed_factor {
	struct clk_hw hw;
	int mult;
	int div;
	const char *parent;
};

static inline struct clk_fixed_factor *to_clk_fixed_factor(struct clk_hw *hw)
{
	return container_of(hw, struct clk_fixed_factor, hw);
}

extern struct clk_ops clk_fixed_factor_ops;

struct clk *clk_fixed_factor(const char *name,
		const char *parent, unsigned int mult, unsigned int div,
		unsigned flags);
struct clk *clk_register_fixed_factor(struct device *dev, const char *name,
				      const char *parent_name,
				      unsigned long flags,
				      unsigned int mult, unsigned int div);

struct clk_hw *clk_hw_register_fixed_factor(struct device *dev,
					    const char *name,
					    const char *parent_name,
					    unsigned long flags,
					    unsigned int mult,
					    unsigned int div);

/**
 * struct clk_fractional_divider - adjustable fractional divider clock
 *
 * @hw:		handle between common and hardware-specific interfaces
 * @reg:	register containing the divider
 * @mshift:	shift to the numerator bit field
 * @mwidth:	width of the numerator bit field
 * @nshift:	shift to the denominator bit field
 * @nwidth:	width of the denominator bit field
 * @approximation: clk driver's callback for calculating the divider clock
 * @lock:	register lock
 *
 * Clock with adjustable fractional divider affecting its output frequency.
 *
 * @flags:
 * CLK_FRAC_DIVIDER_ZERO_BASED - by default the numerator and denominator
 *	is the value read from the register. If CLK_FRAC_DIVIDER_ZERO_BASED
 *	is set then the numerator and denominator are both the value read
 *	plus one.
 * CLK_FRAC_DIVIDER_BIG_ENDIAN - By default little endian register accesses are
 *	used for the divider register.  Setting this flag makes the register
 *	accesses big endian.
 * CLK_FRAC_DIVIDER_POWER_OF_TWO_PS - By default the resulting fraction might
 *	be saturated and the caller will get quite far from the good enough
 *	approximation. Instead the caller may require, by setting this flag,
 *	to shift left by a few bits in case, when the asked one is quite small
 *	to satisfy the desired range of denominator. It assumes that on the
 *	caller's side the power-of-two capable prescaler exists.
 */
struct clk_fractional_divider {
	struct clk_hw	hw;
	void __iomem	*reg;
	u8		mshift;
	u8		mwidth;
	u8		nshift;
	u8		nwidth;
	u8		flags;
	void		(*approximation)(struct clk_hw *hw,
				unsigned long rate, unsigned long *parent_rate,
				unsigned long *m, unsigned long *n);
	spinlock_t	*lock;
};

#define to_clk_fd(_hw) container_of(_hw, struct clk_fractional_divider, hw)

#define CLK_FRAC_DIVIDER_ZERO_BASED		BIT(0)
#define CLK_FRAC_DIVIDER_BIG_ENDIAN		BIT(1)
#define CLK_FRAC_DIVIDER_POWER_OF_TWO_PS	BIT(2)

/**
 * struct clk_mux - multiplexer clock
 *
 * @hw:		handle between common and hardware-specific interfaces
 * @reg:	register controlling multiplexer
 * @mask:	mask of mutliplexer bit field
 * @shift:	shift to multiplexer bit field
 * @flags:	hardware-specific flags
 * @table:	array of register values corresponding to the parent index
 * @lock:	register lock
 *
 * Clock with multiple selectable parents.  Implements .get_parent, .set_parent
 * and .recalc_rate
 *
 * Flags:
 * CLK_MUX_HIWORD_MASK - The mux settings are only in lower 16-bit of this
 *	register, and mask of mux bits are in higher 16-bit of this register.
 *	While setting the mux bits, higher 16-bit should also be updated to
 *	indicate changing mux bits.
 * CLK_MUX_READ_ONLY - The mux registers can't be written, only read in the
 * 	.get_parent clk_op.
 */
struct clk_mux {
	struct clk_hw hw;
	void __iomem *reg;
	u32 mask;
	u8 shift;
	unsigned flags;
	u32 *table;
	spinlock_t *lock;
};

#define to_clk_mux(_hw) container_of(_hw, struct clk_mux, hw)

extern const struct clk_ops clk_mux_ops;
extern const struct clk_ops clk_mux_ro_ops;

struct clk *clk_mux_alloc(const char *name, unsigned clk_flags,
			  void __iomem *reg, u8 shift, u8 width,
			  const char * const *parents, u8 num_parents,
			  unsigned mux_flags);
void clk_mux_free(struct clk *clk_mux);
struct clk *clk_mux(const char *name, unsigned clk_flags, void __iomem *reg,
		    u8 shift, u8 width, const char * const *parents,
		    u8 num_parents, unsigned mux_flags);
struct clk *clk_register_mux(struct device *dev, const char *name,
			     const char * const *parent_names, u8 num_parents,
			     unsigned long flags,
			     void __iomem *reg, u8 shift, u8 width,
			     u8 clk_mux_flags, spinlock_t *lock);

struct clk_hw *__clk_hw_register_mux(struct device *dev,
				     const char *name, u8 num_parents,
				     const char * const *parent_names,
				     unsigned long flags, void __iomem *reg,
				     u8 shift, u32 mask,
				     u8 clk_mux_flags, u32 *table,
				     spinlock_t *lock);

#define clk_hw_register_mux(dev, name, parent_names,                  \
		num_parents, flags, reg, shift, width,                \
		clk_mux_flags, lock)                                  \
	__clk_hw_register_mux((dev), (name), (num_parents),           \
				     (parent_names),                  \
				     (flags), (reg),                  \
				     (shift), BIT((width)) - 1,       \
				     (clk_mux_flags), NULL, (lock))

#define clk_hw_register_mux_table(dev, name, parent_names, num_parents,	  \
				  flags, reg, shift, mask, clk_mux_flags, \
				  table, lock)				  \
	__clk_hw_register_mux((dev), (name), (num_parents),	          \
			      (parent_names), (flags), (reg),             \
			      (shift), (mask), (clk_mux_flags), (table),  \
			      (lock))

int clk_mux_val_to_index(struct clk_hw *hw, u32 *table, unsigned int flags,
			 unsigned int val);
unsigned int clk_mux_index_to_val(u32 *table, unsigned int flags, u8 index);

long clk_mux_round_rate(struct clk_hw *hw, unsigned long rate,
			unsigned long *prate);

/**
 * struct clk_gate - gating clock
 *
 * @hw:		handle between common and hardware-specific interfaces
 * @reg:	register controlling gate
 * @bit_idx:	single bit controlling gate
 * @shift:	Alias for @shift
 * @flags:	hardware-specific flags
 * @lock:	register lock
 * @_parent:	for barebox-internal use
 *
 * Clock which can gate its output.  Implements .enable & .disable
 *
 * Flags:
 * CLK_GATE_SET_TO_DISABLE - by default this clock sets the bit at bit_idx to
 *	enable the clock.  Setting this flag does the opposite: setting the bit
 *	disable the clock and clearing it enables the clock
 * CLK_GATE_HIWORD_MASK - The gate settings are only in lower 16-bit
 *	of this register, and mask of gate bits are in higher 16-bit of this
 *	register.  While setting the gate bits, higher 16-bit should also be
 *	updated to indicate changing gate bits.
 */
struct clk_gate {
	struct clk_hw hw;
	void __iomem *reg;
	union {
		u8 bit_idx;
		u8 shift;
	};
	unsigned flags;
	spinlock_t *lock;
	const char *_parent;
};

int clk_gate_is_enabled(struct clk_hw *hw);

#define to_clk_gate(_hw) container_of(_hw, struct clk_gate, hw)

extern struct clk_ops clk_gate_ops;

struct clk *clk_gate_alloc(const char *name, const char *parent,
		void __iomem *reg, u8 shift, unsigned flags,
		u8 clk_gate_flags);
void clk_gate_free(struct clk *clk_gate);
struct clk *clk_gate(const char *name, const char *parent, void __iomem *reg,
		u8 shift, unsigned flags, u8 clk_gate_flags);
struct clk *clk_gate_inverted(const char *name, const char *parent, void __iomem *reg,
		u8 shift, unsigned flags);
struct clk *clk_gate_shared(const char *name, const char *parent, const char *shared,
			    unsigned flags);
struct clk *clk_register_gate(struct device *dev, const char *name,
			      const char *parent_name, unsigned long flags,
			      void __iomem *reg, u8 bit_idx,
			      u8 clk_gate_flags, spinlock_t *lock);

static inline struct clk_hw *clk_hw_register_gate(struct device *dev,
						  const char *name,
						  const char *parent_name,
						  unsigned long flags,
						  void __iomem *reg,
						  u8 bit_idx,
						  u8 clk_gate_flags,
						  spinlock_t *lock)
{
	return clk_to_clk_hw(clk_register_gate(dev, xstrdup_const(name),
					       xstrdup_const(parent_name),
					       flags, reg, bit_idx,
					       clk_gate_flags, lock));
}

int clk_is_enabled(struct clk *clk);
int clk_hw_is_enabled(struct clk_hw *hw);

int clk_is_enabled_always(struct clk_hw *hw);
long clk_parent_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *prate);
int clk_parent_set_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate);

int bclk_register(struct clk *clk);
struct clk *clk_register(struct device *dev, struct clk_hw *hw);

static inline int clk_hw_register(struct device *dev, struct clk_hw *hw)
{
	return PTR_ERR_OR_ZERO(clk_register(dev, hw));
}

struct clk *clk_lookup(const char *name);

#define CLK_DUMP_VERBOSE	(1 << 0)
#define CLK_DUMP_JSON		(1 << 1)
void clk_dump(int flags);
void clk_dump_one(struct clk *clk, int flags);

struct clk *clk_register_composite(const char *name,
			const char * const *parent_names, int num_parents,
			struct clk *mux_clk,
			struct clk *rate_clk,
			struct clk *gate_clk,
			unsigned long flags);

struct clk_hw *clk_hw_register_composite(struct device *dev,
					 const char *name,
					 const char * const *parent_names,
					 int num_parents,
					 struct clk_hw *mux_hw,
					 const struct clk_ops *mux_ops,
					 struct clk_hw *rate_hw,
					 const struct clk_ops *rate_ops,
					 struct clk_hw *gate_hw,
					 const struct clk_ops *gate_ops,
					 unsigned long flags);

static inline const char *clk_hw_get_name(struct clk_hw *hw)
{
	return hw->clk.name;
}

static inline unsigned int clk_hw_get_num_parents(const struct clk_hw *hw)
{
	return hw->clk.num_parents;
}

static inline unsigned long clk_hw_get_flags(const struct clk_hw *hw)
{
	return hw->clk.flags;
}

int clk_name_set_parent(const char *clkname, const char *clkparentname);
int clk_name_set_rate(const char *clkname, unsigned long rate);

#endif

struct device_node;
struct of_phandle_args;
struct of_device_id;

struct clk_onecell_data {
	struct clk **clks;
	unsigned int clk_num;
};

struct clk_hw_onecell_data {
	unsigned int num;
	struct clk_hw *hws[];
};

#if defined(CONFIG_COMMON_CLK_OF_PROVIDER)

#define CLK_OF_DECLARE(name, compat, fn)				\
const struct of_device_id __clk_of_table_##name				\
	__ll_elem(.__clk_of_table) \
	= { .compatible = compat, .data = fn }

void of_clk_del_provider(struct device_node *np);

typedef int (*of_clk_init_cb_t)(struct device_node *);

struct clk *of_clk_src_onecell_get(struct of_phandle_args *clkspec, void *data);
struct clk *of_clk_src_simple_get(struct of_phandle_args *clkspec, void *data);
struct clk_hw *of_clk_hw_onecell_get(struct of_phandle_args *clkspec, void *data);
struct clk_hw *of_clk_hw_simple_get(struct of_phandle_args *clkspec, void *data);

struct clk *of_clk_get(struct device_node *np, int index);
struct clk *of_clk_get_by_name(struct device_node *np, const char *name);
struct clk *of_clk_get_from_provider(struct of_phandle_args *clkspec);
unsigned int of_clk_get_parent_count(struct device_node *np);
int of_clk_parent_fill(struct device_node *np, const char **parents,
		       unsigned int size);
int of_clk_init(void);
int of_clk_add_provider(struct device_node *np,
			struct clk *(*clk_src_get)(struct of_phandle_args *args,
						   void *data),
			void *data);

int of_clk_add_hw_provider(struct device_node *np,
			struct clk_hw *(*clk_hw_src_get)(struct of_phandle_args *clkspec,
							 void *data),
			void *data);

#else


/*
 * Create a dummy variable to avoid 'unused function'
 * warnings. Compiler should be smart enough to throw it out.
 */
#define CLK_OF_DECLARE(name, compat, fn)				\
static const struct of_device_id __clk_of_table_##name			\
__attribute__ ((unused)) = { .data = fn }


static inline struct clk *of_clk_src_onecell_get(struct of_phandle_args *clkspec,
						 void *data)
{
	return ERR_PTR(-ENOENT);
}
static inline struct clk_hw *of_clk_hw_onecell_get(struct of_phandle_args *clkspec,
						 void *data)
{
	return ERR_PTR(-ENOENT);
}
static inline struct clk *
of_clk_src_simple_get(struct of_phandle_args *clkspec, void *data)
{
	return ERR_PTR(-ENOENT);
}
static inline struct clk_hw *
of_clk_hw_simple_get(struct of_phandle_args *clkspec, void *data)
{
	return ERR_PTR(-ENOENT);
}
static inline struct clk *of_clk_get(struct device_node *np, int index)
{
	return ERR_PTR(-ENOENT);
}
static inline struct clk *of_clk_get_by_name(struct device_node *np,
					     const char *name)
{
	return ERR_PTR(-ENOENT);
}

static inline struct clk *of_clk_get_from_provider(struct of_phandle_args *clkspec)
{
	return ERR_PTR(-ENOENT);
}

static inline unsigned int of_clk_get_parent_count(struct device_node *np)
{
	return 0;
}

static inline int of_clk_init(void)
{
	return 0;
}
static inline int of_clk_add_provider(struct device_node *np,
			struct clk *(*clk_src_get)(struct of_phandle_args *args,
						   void *data),
			void *data)
{
	return 0;
}

static inline int of_clk_add_hw_provider(struct device_node *np,
			struct clk_hw *(*clk_hw_src_get)(struct of_phandle_args *clkspec,
							 void *data),
			void *data)
{
	return 0;
}
#endif

#define CLK_OF_DECLARE_DRIVER(name, compat, fn) CLK_OF_DECLARE(name, compat, fn)

struct string_list;

int clk_name_complete(struct string_list *sl, char *instr);

char *of_clk_get_parent_name(const struct device_node *np, int index);

static inline void clk_unregister(struct clk *clk)
{
}

static inline void clk_hw_unregister(struct clk_hw *hw)
{
}

#ifdef CONFIG_COMMON_CLK

/**
 * clk_bulk_get - lookup and obtain a number of references to clock producer.
 * @dev: device for clock "consumer"
 * @num_clks: the number of clk_bulk_data
 * @clks: the clk_bulk_data table of consumer
 *
 * This helper function allows drivers to get several clk consumers in one
 * operation. If any of the clk cannot be acquired then any clks
 * that were obtained will be freed before returning to the caller.
 *
 * Returns 0 if all clocks specified in clk_bulk_data table are obtained
 * successfully, or valid IS_ERR() condition containing errno.
 * The implementation uses @dev and @clk_bulk_data.id to determine the
 * clock consumer, and thereby the clock producer.
 * The clock returned is stored in each @clk_bulk_data.clk field.
 *
 * Drivers must assume that the clock source is not enabled.
 *
 * clk_bulk_get should not be called from within interrupt context.
 */
int __must_check clk_bulk_get(struct device *dev, int num_clks,
			      struct clk_bulk_data *clks);

/**
 * clk_bulk_get_optional - lookup and obtain a number of references to clock producer
 * @dev: device for clock "consumer"
 * @num_clks: the number of clk_bulk_data
 * @clks: the clk_bulk_data table of consumer
 *
 * Behaves the same as clk_bulk_get() except where there is no clock producer.
 * In this case, instead of returning -ENOENT, the function returns 0 and
 * NULL for a clk for which a clock producer could not be determined.
 */
int __must_check clk_bulk_get_optional(struct device *dev, int num_clks,
				       struct clk_bulk_data *clks);

/**
 * clk_bulk_get_all - lookup and obtain all available references to clock
 *                    producer.
 * @dev: device for clock "consumer"
 * @clks: pointer to the clk_bulk_data table of consumer
 *
 * This helper function allows drivers to get all clk consumers in one
 * operation. If any of the clk cannot be acquired then any clks
 * that were obtained will be freed before returning to the caller.
 *
 * Returns a positive value for the number of clocks obtained while the
 * clock references are stored in the clk_bulk_data table in @clks field.
 * Returns 0 if there're none and a negative value if something failed.
 *
 * Drivers must assume that the clock source is not enabled.
 *
 * clk_bulk_get should not be called from within interrupt context.
 */
int __must_check clk_bulk_get_all(struct device *dev,
				  struct clk_bulk_data **clks);

/**
 * clk_bulk_put	- "free" the clock source
 * @num_clks: the number of clk_bulk_data
 * @clks: the clk_bulk_data table of consumer
 *
 * Note: drivers must ensure that all clk_bulk_enable calls made on this
 * clock source are balanced by clk_bulk_disable calls prior to calling
 * this function.
 *
 * clk_bulk_put should not be called from within interrupt context.
 */
void clk_bulk_put(int num_clks, struct clk_bulk_data *clks);

/**
 * clk_bulk_put_all - "free" all the clock source
 * @num_clks: the number of clk_bulk_data
 * @clks: the clk_bulk_data table of consumer
 *
 * Note: drivers must ensure that all clk_bulk_enable calls made on this
 * clock source are balanced by clk_bulk_disable calls prior to calling
 * this function.
 *
 * clk_bulk_put_all should not be called from within interrupt context.
 */
void clk_bulk_put_all(int num_clks, struct clk_bulk_data *clks);

/**
 * clk_bulk_enable - inform the system when the set of clks should be running.
 * @num_clks: the number of clk_bulk_data
 * @clks: the clk_bulk_data table of consumer
 *
 * May be called from atomic contexts.
 *
 * Returns success (0) or negative errno.
 */
int __must_check clk_bulk_enable(int num_clks,
				 const struct clk_bulk_data *clks);

/**
 * clk_bulk_disable - inform the system when the set of clks is no
 *		      longer required.
 * @num_clks: the number of clk_bulk_data
 * @clks: the clk_bulk_data table of consumer
 *
 * Inform the system that a set of clks is no longer required by
 * a driver and may be shut down.
 *
 * May be called from atomic contexts.
 *
 * Implementation detail: if the set of clks is shared between
 * multiple drivers, clk_bulk_enable() calls must be balanced by the
 * same number of clk_bulk_disable() calls for the clock source to be
 * disabled.
 */
void clk_bulk_disable(int num_clks, const struct clk_bulk_data *clks);

#else
static inline int __must_check clk_bulk_get(struct device *dev, int num_clks,
					    struct clk_bulk_data *clks)
{
	return 0;
}

static inline int __must_check clk_bulk_get_optional(struct device *dev,
						     int num_clks,
						     struct clk_bulk_data *clks)
{
	return 0;
}

static inline int __must_check clk_bulk_get_all(struct device *dev,
						struct clk_bulk_data **clks)
{
	return 0;
}

static inline void clk_bulk_put(int num_clks, struct clk_bulk_data *clks) {}

static inline void clk_bulk_put_all(int num_clks, struct clk_bulk_data *clks) {}

static inline int __must_check clk_bulk_enable(int num_clks, struct clk_bulk_data *clks)
{
	return 0;
}

static inline void clk_bulk_disable(int num_clks,
				    struct clk_bulk_data *clks) {}

#endif

/**
 * clk_hw_register_fixed_rate_with_accuracy - register fixed-rate clock with
 * the clock framework
 * @dev: device that is registering this clock
 * @name: name of this clock
 * @parent_name: name of clock's parent
 * @flags: framework-specific flags
 * @fixed_rate: non-adjustable clock rate
 * @fixed_accuracy: non-adjustable clock accuracy (ignored)
 */
#define clk_hw_register_fixed_rate_with_accuracy(dev, name, parent_name,      \
						 flags, fixed_rate,	      \
						 fixed_accuracy)	      \
	clk_hw_register_fixed_rate((dev), (name), (parent_name), (flags), (fixed_rate))

#define clk_bulk_prepare_enable clk_bulk_enable
#define clk_bulk_disable_unprepare clk_bulk_disable

/**
 * clk_get_optional - lookup and obtain a reference to an optional clock
 *		      producer.
 * @dev: device for clock "consumer"
 * @id: clock consumer ID
 *
 * Behaves the same as clk_get() except where there is no clock producer. In
 * this case, instead of returning -ENOENT, the function returns NULL.
 */
static inline struct clk *clk_get_optional(struct device *dev, const char *id)
{
	struct clk *clk = clk_get(dev, id);

	if (clk == ERR_PTR(-ENOENT))
		return NULL;

	return clk;
}

/**
 * clk_get_enabled - clk_get() + clk_prepare_enable()
 * @dev: device for clock "consumer"
 * @id: clock consumer ID
 *
 * Return: a struct clk corresponding to the clock producer, or
 * valid IS_ERR() condition containing errno.  The implementation
 * uses @dev and @id to determine the clock consumer, and thereby
 * the clock producer.  (IOW, @id may be identical strings, but
 * clk_get may return different clock producers depending on @dev.)
 *
 * The returned clk (if valid) is enabled.
 */
static inline struct clk *clk_get_enabled(struct device *dev, const char *id)
{
	struct clk *clk;
	int ret;

	clk = clk_get(dev, id);
	if (IS_ERR(clk))
		return clk;

	ret = clk_enable(clk);
	if (ret) {
		clk_put(clk);
		return ERR_PTR(ret);
	}

	return clk;
}

/**
 * clk_get_optional_enabled - clk_get_optional() +
 *                            clk_prepare_enable()
 * @dev: device for clock "consumer"
 * @id: clock consumer ID
 *
 * Return: a struct clk corresponding to the clock producer, or
 * valid IS_ERR() condition containing errno.  The implementation
 * uses @dev and @id to determine the clock consumer, and thereby
 * the clock producer.  If no such clk is found, it returns NULL
 * which serves as a dummy clk.  That's the only difference compared
 * to clk_get_enabled().
 *
 * The returned clk (if valid) is enabled.
 */
static inline struct clk *clk_get_optional_enabled(struct device *dev, const char *id)
{
	struct clk *clk;
	int ret;

	clk = clk_get_optional(dev, id);
	if (IS_ERR_OR_NULL(clk))
		return clk;

	ret = clk_enable(clk);
	if (ret) {
		clk_put(clk);
		return ERR_PTR(ret);
	}

	return clk;
}

/**
 * clk_get_if_available - get clock, ignoring known unavailable clock controller
 * @dev: device for clock "consumer"
 * @id: clock consumer ID
 *
 * Return: a struct clk corresponding to the clock producer, a
 * valid IS_ERR() condition containing errno or NULL if it could
 * be determined that the clock producer will never be probed in
 * absence of modules.
 */
static inline struct clk *clk_get_if_available(struct device *dev, const char *id)
{
	struct clk *clk = clk_get(dev, id);

	if (clk == ERR_PTR(-EPROBE_DEFER) && deep_probe_is_supported())
		return NULL;

	return clk;
}

#endif
