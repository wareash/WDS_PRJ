#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xec5ba0f9, "module_layout" },
	{ 0xb279da12, "pv_lock_ops" },
	{ 0xd0d8621b, "strlen" },
	{ 0x973873ab, "_spin_lock" },
	{ 0x26fd80b9, "per_cpu__cpu_number" },
	{ 0xb72397d5, "printk" },
	{ 0x16305289, "warn_slowpath_null" },
	{ 0x3ec05838, "i2c_unregister_device" },
	{ 0xa11de823, "module_put" },
	{ 0x7ecb001b, "__per_cpu_offset" },
	{ 0x701d0ebd, "snprintf" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "ED0F9D302DF11CFF55936EF");
