/* Shared library add-on to iptables to add devgroup matching support.
 *
 * Copyright (c) 2011 Patrick McHardy <kaber@trash.net>
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <xtables.h>
#include <linux/netfilter/xt_devgroup.h>

static void devgroup_help(void)
{
	printf(
"devgroup match options:\n"
"[!] --src-group value[/mask]	Match device group of incoming device\n"
"[!] --dst-group value[/mask]	Match device group of outgoing device\n"
		);
}

enum {
	O_SRC_GROUP = 0,
	O_DST_GROUP,
};

static const struct xt_option_entry devgroup_opts[] = {
	{.name = "src-group", .id = O_SRC_GROUP, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT},
	{.name = "dst-group", .id = O_DST_GROUP, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT},
	XTOPT_TABLEEND,
};

/* array of devgroups from /etc/iproute2/group */
static struct xtables_lmap *devgroups;

static void devgroup_init(struct xt_entry_match *match)
{
	const char file[] = "/etc/iproute2/group";
	devgroups = xtables_lmap_init(file);
	if (devgroups == NULL && errno != ENOENT)
		fprintf(stderr, "Warning: %s: %s\n", file, strerror(errno));
}

static void devgroup_parse_groupspec(const char *arg, unsigned int *group,
				     unsigned int *mask)
{
	char *end;
	bool ok;

	ok = xtables_strtoui(arg, &end, group, 0, UINT32_MAX);
	if (ok && (*end == '/' || *end == '\0')) {
		if (*end == '/')
			ok = xtables_strtoui(end + 1, NULL, mask,
			                     0, UINT32_MAX);
		else
			*mask = ~0U;
		if (!ok)
			xtables_error(PARAMETER_PROBLEM,
				      "Bad group value \"%s\"", arg);
	} else {
		*group = xtables_lmap_name2id(devgroups, arg);
		if (*group == -1)
			xtables_error(PARAMETER_PROBLEM,
				      "Device group \"%s\" not found", arg);
		*mask = ~0U;
	}
}

static void devgroup_parse(struct xt_option_call *cb)
{
	struct xt_devgroup_info *info = cb->data;
	unsigned int id, mask;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_SRC_GROUP:
		devgroup_parse_groupspec(cb->arg, &id, &mask);
		info->src_group = id;
		info->src_mask  = mask;
		info->flags |= XT_DEVGROUP_MATCH_SRC;
		if (cb->invert)
			info->flags |= XT_DEVGROUP_INVERT_SRC;
		break;
	case O_DST_GROUP:
		devgroup_parse_groupspec(cb->arg, &id, &mask);
		info->dst_group = id;
		info->dst_mask  = mask;
		info->flags |= XT_DEVGROUP_MATCH_DST;
		if (cb->invert)
			info->flags |= XT_DEVGROUP_INVERT_DST;
		break;
	}
}

static void
print_devgroup(unsigned int id, unsigned int mask, int numeric)
{
	const char *name = NULL;

	if (mask != 0xffffffff)
		printf("0x%x/0x%x", id, mask);
	else {
		if (numeric == 0)
			name = xtables_lmap_id2name(devgroups, id);
		if (name)
			printf("%s", name);
		else
			printf("0x%x", id);
	}
}

static void devgroup_show(const char *pfx, const struct xt_devgroup_info *info,
			  int numeric)
{
	if (info->flags & XT_DEVGROUP_MATCH_SRC) {
		if (info->flags & XT_DEVGROUP_INVERT_SRC)
			printf(" !");
		printf(" %ssrc-group ", pfx);
		print_devgroup(info->src_group, info->src_mask, numeric);
	}

	if (info->flags & XT_DEVGROUP_MATCH_DST) {
		if (info->flags & XT_DEVGROUP_INVERT_DST)
			printf(" !");
		printf(" %sdst-group ", pfx);
		print_devgroup(info->dst_group, info->dst_mask, numeric);
	}
}

static void devgroup_print(const void *ip, const struct xt_entry_match *match,
                        int numeric)
{
	const struct xt_devgroup_info *info = (const void *)match->data;

	devgroup_show("", info, numeric);
}

static void devgroup_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_devgroup_info *info = (const void *)match->data;

	devgroup_show("--", info, 0);
}

static void devgroup_check(struct xt_fcheck_call *cb)
{
	if (cb->xflags == 0)
		xtables_error(PARAMETER_PROBLEM,
			      "devgroup match: You must specify either "
			      "'--src-group' or '--dst-group'");
}

static void
print_devgroup_xlate(unsigned int id, uint32_t op,  unsigned int mask,
		     struct xt_xlate *xl, int numeric)
{
	const char *name = NULL;

	if (mask != 0xffffffff)
		xt_xlate_add(xl, "and 0x%x %s 0x%x", mask,
			   op == XT_OP_EQ ? "==" : "!=", id);
	else {
		if (numeric == 0)
			name = xtables_lmap_id2name(devgroups, id);

		xt_xlate_add(xl, "%s", op == XT_OP_EQ ? "" : "!= ");
		if (name)
			xt_xlate_add(xl, "%s", name);
		else
			xt_xlate_add(xl, "0x%x", id);
	}
}

static void devgroup_show_xlate(const struct xt_devgroup_info *info,
				struct xt_xlate *xl, int numeric)
{
	enum xt_op op = XT_OP_EQ;
	char *space = "";

	if (info->flags & XT_DEVGROUP_MATCH_SRC) {
		if (info->flags & XT_DEVGROUP_INVERT_SRC)
			op = XT_OP_NEQ;
		xt_xlate_add(xl, "iifgroup ");
		print_devgroup_xlate(info->src_group, op,
				     info->src_mask, xl, numeric);
		space = " ";
	}

	if (info->flags & XT_DEVGROUP_MATCH_DST) {
		if (info->flags & XT_DEVGROUP_INVERT_DST)
			op = XT_OP_NEQ;
		xt_xlate_add(xl, "%soifgroup ", space);
		print_devgroup_xlate(info->dst_group, op,
				     info->dst_mask, xl, numeric);
	}
}

static int devgroup_xlate(struct xt_xlate *xl,
			  const struct xt_xlate_mt_params *params)
{
	const struct xt_devgroup_info *info = (const void *)params->match->data;

	devgroup_show_xlate(info, xl, 0);

	return 1;
}

static struct xtables_match devgroup_mt_reg = {
	.name		= "devgroup",
	.version	= XTABLES_VERSION,
	.family		= NFPROTO_UNSPEC,
	.size		= XT_ALIGN(sizeof(struct xt_devgroup_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_devgroup_info)),
	.init		= devgroup_init,
	.help		= devgroup_help,
	.print		= devgroup_print,
	.save		= devgroup_save,
	.x6_parse	= devgroup_parse,
	.x6_fcheck	= devgroup_check,
	.x6_options	= devgroup_opts,
	.xlate		= devgroup_xlate,
};

void _init(void)
{
	xtables_register_match(&devgroup_mt_reg);
}