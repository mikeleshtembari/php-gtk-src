/*
 * PHP-GTK - The PHP language bindings for GTK+
 *
 * Copyright (C) 2001-2008 Andrei Zmievski <andrei@php.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
 
/* $Id$: */

#include "php_gtk.h"
#include "SAPI.h"

#if HAVE_PHP_GTK

#ifndef PHP_WIN32
#include "build-defs.h"
#endif
#ifdef PHP_WIN32
#include "win32/param.h"
#include "win32/winutil.h"
#define GET_DL_ERROR()  php_win_err()
#else
#include <sys/param.h>
#define GET_DL_ERROR()  dlerror()
#endif

zend_llist php_gtk_ext_registry;
PHP_GTK_API GHashTable *php_gtk_class_hash = NULL;
PHP_GTK_API zend_object_handlers php_gtk_handlers;

const gchar *phpg_class_id = "phpg_class";
GQuark phpg_class_key = 0;
GType G_TYPE_PHP_VALUE = 0;

ZEND_DECLARE_MODULE_GLOBALS(gtk);

//#ifdef HAVE_CAIRO
//static const zend_module_dep gtk_cairo_deps[] = {
//	ZEND_MOD_REQUIRED("cairo")
//	{NULL, NULL, NULL}
//};
//#endif

zend_module_entry gtk_module_entry = {
	STANDARD_MODULE_HEADER_EX,
	NULL,  /* no ini entries */
//#ifdef HAVE_CAIRO_0
//	gtk_cairo_deps,
//#else
	NULL,
//#endif
 	"php-gtk",
 	NULL,
 	PHP_MINIT(gtk),
	PHP_MSHUTDOWN(gtk),
	PHP_RINIT(gtk),
	PHP_RSHUTDOWN(gtk),
	PHP_MINFO(gtk),
	PHP_GTK_VERSION,
#if ZEND_MODULE_API_NO < 20060613
	STANDARD_MODULE_PROPERTIES
#else
	PHP_MODULE_GLOBALS(gtk),
	NULL,
	NULL,
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
#endif
};

#ifdef COMPILE_DL_PHP_GTK2
ZEND_GET_MODULE(gtk)
#endif

static void php_gtk_startup_shared_extensions(int module_number);

static ZEND_INI_MH(OnUpdateCodepage)
{
	char **p;
#ifndef ZTS
	char *base = (char *) mh_arg2;
#else
	char *base;

	base = (char *) ts_resource(*((int *) mh_arg2));
#endif

	p = (char **) (base+(size_t) mh_arg1);

	if (!new_value) {
		new_value = "ISO-8859-1";
	} else {
		if (!strcasecmp(new_value, "UTF-8")) {
			GTK_G(is_utf8) = 1;
		} else {
			GTK_G(is_utf8) = 0;
		}
	}

	*p = new_value;
	return SUCCESS;
}

PHP_INI_BEGIN()
	STD_PHP_INI_ENTRY    ("php-gtk.codepage", "iso-8859-1",	PHP_INI_ALL,	OnUpdateCodepage, codepage,	zend_gtk_globals,	gtk_globals)
	PHP_INI_ENTRY        ("php-gtk.extensions", NULL, PHP_INI_SYSTEM, NULL)
PHP_INI_END()


static void php_gtk_ext_destructor(php_gtk_ext_entry *ext)
{
	if (ext->ext_started && ext->ext_shutdown_func)
		ext->ext_shutdown_func();
	ext->ext_started = 0;
	if (ext->handle) {
		DL_UNLOAD(ext->handle);
	}
}

static void php_gtk_prop_info_destroy(void *pi_hash)
{
	HashTable *pih = (HashTable *) pi_hash;
	zend_hash_destroy(pih);
}

static gpointer php_object_copy(gpointer boxed)
{
	zval *object = (zval *) boxed;
	zval_add_ref(&object);
	return object;
}

static void php_object_release(gpointer boxed)
{
	zval *object = (zval *) boxed;
	zval_ptr_dtor(&object);
}

PHP_MINIT_FUNCTION(gtk)
{
#if defined(ZTS) && ZEND_MODULE_API_NO < 20060613
	ZEND_INIT_MODULE_GLOBALS(gtk, NULL, NULL);
#endif

	REGISTER_INI_ENTRIES();

	php_gtk_class_hash = g_hash_table_new(g_str_hash, g_str_equal);
	zend_hash_init_ex(&phpg_prop_info, 50, NULL, (dtor_func_t) php_gtk_prop_info_destroy, 1, 0);
	zend_llist_init(&php_gtk_ext_registry, sizeof(php_gtk_ext_entry), (llist_dtor_func_t)php_gtk_ext_destructor, 1);

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(gtk)
{
	UNREGISTER_INI_ENTRIES();

	zend_llist_destroy(&php_gtk_ext_registry);
	zend_hash_destroy(&phpg_prop_info);

#if defined(ZTS) && ZEND_MODULE_API_NO < 20060613
	ts_free_id(gtk_globals_id);
#endif

	return SUCCESS;
}

PHP_RINIT_FUNCTION(gtk)
{
	zend_module_entry *temp_module = EG(current_module);
	EG(current_module) = phpext_php_gtk_ptr;

	zend_alter_ini_entry("max_execution_time", sizeof("max_execution_time")-1, "0", sizeof("0")-1,PHP_INI_SYSTEM, PHP_INI_STAGE_ACTIVATE);

	php_gtk_handlers = *zend_get_std_object_handlers();
	php_gtk_handlers.read_property  = phpg_read_property;
	php_gtk_handlers.write_property = phpg_write_property;
	php_gtk_handlers.get_properties = phpg_get_properties;
	php_gtk_handlers.get_property_ptr_ptr = phpg_get_property_ptr_ptr;
	php_gtk_handlers.clone_obj = NULL;

	phpg_register_exceptions(TSRMLS_C);

	/*
	 * Initialize the type system and the GType wrapper class.
	 */
	g_type_init();

	G_TYPE_PHP_VALUE = g_boxed_type_register_static("PhpValue", php_object_copy, php_object_release);

	phpg_gtype_register_self(TSRMLS_C);
	phpg_gobject_register_self(TSRMLS_C);
	phpg_gboxed_register_self(TSRMLS_C);
	phpg_gpointer_register_self(TSRMLS_C);
	phpg_gdkatom_register_self(TSRMLS_C);
	phpg_paramspec_register_self(TSRMLS_C);

	if (php_gtk_startup_all_extensions(module_number) == FAILURE) {
		php_error(E_WARNING, "Unable to start internal extensions");
		return FAILURE;
	}

	php_gtk_startup_shared_extensions(module_number);
	EG(current_module) = temp_module;

	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(gtk)
{
	GMainContext *context;

	/*
	 * Looks like we have to free up the default main context ourselves, in case
	 * gtk::main() was never called by a script.
	 */
	context = g_main_context_default();
	g_main_context_unref(context);

	/* more clean-ups after Gtk+ */
	gtk_about_dialog_set_url_hook(NULL, NULL, NULL);
	gtk_about_dialog_set_email_hook(NULL, NULL, NULL);
	gtk_color_selection_set_change_palette_with_screen_hook(NULL);

	return SUCCESS;
}

PHP_MINFO_FUNCTION(gtk)
{
	char buf[32];

	php_info_print_table_start();
	php_info_print_table_row(2, "GTK+ support", "enabled");
	sprintf(buf, "%d.%d.%d", gtk_major_version, gtk_minor_version, gtk_micro_version);
	php_info_print_table_row(2, "GTK+ v", buf);
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}

PHP_GTK_API PHP_FUNCTION(no_constructor)
{
 	php_error(E_ERROR, "%s: An abstract or unimplemented class", get_active_function_name(TSRMLS_C));
	php_gtk_invalidate(this_ptr);
}

PHP_GTK_API PHP_FUNCTION(no_direct_constructor)
{
    php_error(E_ERROR, "Class %s cannot be instantiated directly", Z_OBJCE_P(this_ptr)->name);
}

static int php_gtk_startup_extension(php_gtk_ext_entry *ext, int module_number)
{
	TSRMLS_FETCH();

	if (ext) {
		if (ext->ext_startup_func) {
			if (ext->ext_startup_func(module_number TSRMLS_CC) == FAILURE) {
				php_error(E_WARNING, "Unable to start PHP-GTK extension: %s", ext->name);
				return FAILURE;
			}
		}
		ext->ext_started = 1;
		if (!strcmp(ext->name, "gtk+")) {
			zend_llist_add_element(&php_gtk_ext_registry, (void *)ext);
		} else {
			zend_llist_prepend_element(&php_gtk_ext_registry, (void *)ext);
		}
	}

	return SUCCESS;
}

int php_gtk_startup_extensions(php_gtk_ext_entry **ext, int ext_count, int module_number)
{
	php_gtk_ext_entry **end = ext + ext_count;

	while (ext < end) {
		if (*ext) {
			if (php_gtk_startup_extension(*ext, module_number)==FAILURE) {
				return FAILURE;
			}
		}
		ext++;
	}

	return SUCCESS;
}

static void php_gtk_dl(char *ext_name, int module_number)
{
	char *extension_dir, *libpath;
	int ext_name_len = strlen(ext_name);
	void *handle;
	php_gtk_ext_entry *(*get_extension)(void);
	php_gtk_ext_entry *ext_entry;

	if (cfg_get_string("extension_dir", &extension_dir) == FAILURE) {
		extension_dir = PHP_EXTENSION_DIR;
	}

	if (extension_dir && extension_dir[0]) {
		int extension_dir_len = strlen(extension_dir);

		libpath = emalloc(extension_dir_len + ext_name_len + 2);

		if (IS_SLASH(extension_dir[extension_dir_len-1])) {
			sprintf(libpath, "%s%s", extension_dir, ext_name); /* SAFE */
		} else {
			sprintf(libpath, "%s/%s", extension_dir, ext_name); /* SAFE */
		}
	} else {
		libpath = estrndup(ext_name, ext_name_len);
	}

	handle = DL_LOAD(libpath);
	if (!handle) {
		php_error(E_WARNING, "Unable to load shared PHP-GTK extension: %s - '%s'", libpath, GET_DL_ERROR());
		efree(libpath);
		return;
	}

	efree(libpath);

	get_extension = (php_gtk_ext_entry *(*)(void)) DL_FETCH_SYMBOL(handle, "get_extension");

	if (!get_extension) 
		get_extension = (php_gtk_ext_entry *(*)(void)) DL_FETCH_SYMBOL(handle, "_get_extension");

	if (!get_extension) {
		php_error(E_WARNING, "Invalid shared PHP-GTK extension: %s", ext_name);
		DL_UNLOAD(handle);
		return;
	}

	ext_entry = get_extension();

	if (php_gtk_startup_extension(ext_entry, module_number) == FAILURE) {
		php_error(E_WARNING, "Unable to start shared PHP-GTK extension: %s", ext_name); 
		DL_UNLOAD(handle);
		return;
	}

	((php_gtk_ext_entry *)zend_llist_get_first(&php_gtk_ext_registry))->handle = handle;
}

static void php_gtk_startup_shared_extensions(int module_number)
{
	zval exts_val;
	char *exts;
	char *ext_name;

	if (zend_get_configuration_directive("php-gtk.extensions", sizeof("php-gtk.extensions"), &exts_val) == FAILURE)
		return;

	exts = estrndup(Z_STRVAL(exts_val), Z_STRLEN(exts_val));

	ext_name = strtok(exts, ", ");
	while (ext_name) {
		php_gtk_dl(ext_name, module_number);
		ext_name = strtok(NULL, ", ");
	}
	efree(exts);
}

#endif	/* HAVE_PHP_GTK */
