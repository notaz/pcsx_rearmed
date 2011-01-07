/*
 * (C) Gražvydas "notaz" Ignotas, 2006-2010
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

typedef enum
{
	MB_NONE = 1,		/* no auto processing */
	MB_OPT_ONOFF,		/* ON/OFF setting */
	MB_OPT_RANGE,		/* [min-max] setting */
	MB_OPT_CUSTOM,		/* custom value */
	MB_OPT_CUSTONOFF,
	MB_OPT_CUSTRANGE,
	MB_OPT_ENUM,
} menu_behavior;

typedef struct
{
	const char *name;
	menu_behavior beh;
	int id;
	void *var;		/* for on-off/range settings */
	int mask;		/* bit to toggle for on/off */
	signed short min;	/* for ranged integer settings, to be sign-extended */
	signed short max;
	unsigned int enabled:1;
	unsigned int need_to_save:1;
	unsigned int selectable:1;
	int (*handler)(int id, int keys);
	const char * (*generate_name)(int id, int *offs);
	const void *data;
	const char *help;
} menu_entry;

#define mee_handler_id_h(name, id, handler, help) \
	{ name, MB_NONE, id, NULL, 0, 0, 0, 1, 0, 1, handler, NULL, NULL, help }

#define mee_handler_id(name, id, handler) \
	mee_handler_id_h(name, id, handler, NULL)

#define mee_handler(name, handler) \
	mee_handler_id(name, MA_NONE, handler)

#define mee_handler_h(name, handler, help) \
	mee_handler_id_h(name, MA_NONE, handler, help)

#define mee_label(name) \
	{ name, MB_NONE, MA_NONE, NULL, 0, 0, 0, 1, 0, 0, NULL, NULL, NULL, NULL }

#define mee_label_mk(id, name_func) \
	{ "", MB_NONE, id, NULL, 0, 0, 0, 1, 0, 0, NULL, name_func, NULL, NULL }

#define mee_onoff_h(name, id, var, mask, help) \
	{ name, MB_OPT_ONOFF, id, &(var), mask, 0, 0, 1, 1, 1, NULL, NULL, NULL, help }

#define mee_onoff(name, id, var, mask) \
	mee_onoff_h(name, id, var, mask, NULL)

#define mee_range(name, id, var, min, max) \
	{ name, MB_OPT_RANGE, id, &(var), 0, min, max, 1, 1, 1, NULL, NULL, NULL, NULL }

#define mee_range_hide(name, id, var, min, max) \
	{ name, MB_OPT_RANGE, id, &(var), 0, min, max, 0, 1, 0, NULL, NULL, NULL, NULL }

#define mee_cust_s_h(name, id, need_save, handler, name_func, help) \
	{ name, MB_OPT_CUSTOM, id, NULL, 0, 0, 0, 1, need_save, 1, handler, name_func, NULL, help }

#define mee_cust_h(name, id, handler, name_func, help) \
	mee_cust_s_h(name, id, 1, handler, name_func, help)

#define mee_cust(name, id, handler, name_func) \
	mee_cust_h(name, id, handler, name_func, NULL)

#define mee_cust_nosave(name, id, handler, name_func) \
	mee_cust_s_h(name, id, 0, handler, name_func, NULL)

#define mee_onoff_cust(name, id, var, mask, name_func) \
	{ name, MB_OPT_CUSTONOFF, id, &(var), mask, 0, 0, 1, 1, 1, NULL, name_func, NULL, NULL }

#define mee_range_cust(name, id, var, min, max, name_func) \
	{ name, MB_OPT_CUSTRANGE, id, &(var), 0, min, max, 1, 1, 1, NULL, name_func, NULL, NULL }

#define mee_enum_h(name, id, var, names_list, help) \
	{ name, MB_OPT_ENUM, id, &(var), 0, 0, 0, 1, 1, 1, NULL, NULL, names_list, help }

#define mee_enum(name, id, var, names_list) \
	mee_enum_h(name, id, var, names_list, NULL)

#define mee_end \
	{ NULL, 0, 0, NULL, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL }

typedef struct
{
	char *name;
	int mask;
} me_bind_action;

extern me_bind_action me_ctrl_actions[];
extern me_bind_action emuctrl_actions[];	// platform code

extern void *g_menubg_src_ptr;
extern void *g_menubg_ptr;
extern void *g_menuscreen_ptr;
#if MSCREEN_SIZE_FIXED
#define g_menuscreen_w MSCREEN_WIDTH
#define g_menuscreen_h MSCREEN_HEIGHT
#else
extern int g_menuscreen_w;
extern int g_menuscreen_h;
#endif

void menu_init(void);
void text_out16(int x, int y, const char *texto, ...);
void me_update_msg(const char *msg);

void menu_romload_prepare(const char *rom_name);
void menu_romload_end(void);

void menu_loop(void);
int  menu_loop_tray(void);

menu_entry *me_list_get_first(void);
menu_entry *me_list_get_next(void);

