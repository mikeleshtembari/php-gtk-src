/*
 * PHP-GTK - The PHP language bindings for GTK+
 *
 * Copyright (C) 2001 Andrei Zmievski <andrei@php.net>
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
GHashTable *php_gtk_class_hash = NULL;

zend_module_entry gtk_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"php-gtk",
	NULL,
	PHP_MINIT(gtk),
	PHP_MSHUTDOWN(gtk),
	PHP_RINIT(gtk),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(gtk),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(gtk),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_GTK_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};

ZEND_DECLARE_MODULE_GLOBALS(gtk)

#ifdef COMPILE_DL_PHP_GTK
ZEND_GET_MODULE(gtk)
#endif

static void php_gtk_startup_shared_extensions(int module_number);

PHP_INI_BEGIN()
	STD_PHP_INI_ENTRY    ("php-gtk.codepage", "CP1252",	PHP_INI_SYSTEM,	OnUpdateString, codepage,	zend_gtk_globals,	gtk_globals)
	PHP_INI_ENTRY        ("php-gtk.extensions", NULL, PHP_INI_SYSTEM, NULL)
PHP_INI_END()


PHP_MINIT_FUNCTION(gtk)
{
#ifdef ZTS
	ZEND_INIT_MODULE_GLOBALS(gtk, NULL, NULL);
#endif

	REGISTER_INI_ENTRIES();

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(gtk)
{
	UNREGISTER_INI_ENTRIES();

	return SUCCESS;
}

static void php_gtk_ext_destructor(php_gtk_ext_entry *ext)
{
	if (ext->ext_started && ext->ext_shutdown_func)
		ext->ext_shutdown_func();
	ext->ext_started = 0;
	if (ext->handle) {
		DL_UNLOAD(ext->handle);
	}
}

/* Remove if there's nothing to do at request start */
PHP_RINIT_FUNCTION(gtk)
{
	zend_llist_init(&php_gtk_ext_registry, sizeof(php_gtk_ext_entry), (llist_dtor_func_t)php_gtk_ext_destructor, 1);

	php_gtk_class_hash = g_hash_table_new(g_str_hash, g_str_equal);
	zend_hash_init_ex(&php_gtk_prop_getters, 20, NULL, NULL, 1, 0);
	zend_hash_init_ex(&php_gtk_prop_setters, 20, NULL, NULL, 1, 0);
	zend_hash_init_ex(&php_gtk_rsrc_hash, 50, NULL, NULL, 1, 0);
	zend_hash_init_ex(&php_gtk_type_hash, 50, NULL, NULL, 1, 0);
	
	zend_unset_timeout(TSRMLS_C);
	zend_set_timeout(0);

	if (php_gtk_startup_all_extensions(module_number) == FAILURE) {
		php_error(E_WARNING, "Unable to start internal extensions");
		return FAILURE;
	}

	php_gtk_startup_shared_extensions(module_number);

	return SUCCESS;
}

/* Remove if there's nothing to do at request end */
PHP_RSHUTDOWN_FUNCTION(gtk)
{
	zend_hash_destroy(&php_gtk_prop_getters);
	zend_hash_destroy(&php_gtk_prop_setters);
	zend_hash_destroy(&php_gtk_rsrc_hash);
	zend_hash_destroy(&php_gtk_type_hash);

	zend_llist_destroy(&php_gtk_ext_registry);

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

PHP_FUNCTION(no_constructor)
{
 	php_error(E_WARNING, "%s: An abstract or unimplemented class", get_active_function_name(TSRMLS_C));
	php_gtk_invalidate(this_ptr);
}

PHP_FUNCTION(no_direct_constructor)
{
	php_error(E_WARNING, "Class %s cannot be constructed directly", get_active_function_name(TSRMLS_C));
	php_gtk_invalidate(this_ptr);
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