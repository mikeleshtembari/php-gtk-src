/*
 * PHP-GTK - The PHP language bindings for GTK+
 *
 * Copyright (C) 2001-2004 Andrei Zmievski <andrei@php.net>
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

#include "php_gtk.h"

#if HAVE_PHP_GTK

/*
 * GType PHP class definition
 */

typedef struct {
	PHPG_OBJ_HEADER
	GType type;
} php_gtype_t;

static PHP_METHOD(GType, __construct);
static int gtype_type_read(void *object, zval *return_value);

static zend_function_entry gtype_methods[] = {
	ZEND_ME(GType, __construct, NULL, ZEND_ACC_PRIVATE)
	{NULL, NULL, NULL}
};

static prop_info_t gtype_props_info[] = {
	{ "type", gtype_type_read, NULL },
	{ NULL, NULL, NULL },
};

PHP_GTK_EXPORT_CE(gtype_ce) = NULL;

static void gtype_destroy_object(php_gtype_t *object, zend_object_handle handle TSRMLS_DC)
{
	zend_hash_destroy(object->zobj.properties);
	FREE_HASHTABLE(object->zobj.properties);
	efree(object);
}

static zend_object_value gtype_create_object(zend_class_entry *ce TSRMLS_DC)
{
	zend_object_value zov;
	php_gtype_t *object;

	object = emalloc(sizeof(php_gtype_t));
	phpg_init_object(object, ce);

	object->type = 0;

	zov.handlers = &php_gtk_handlers;
	zov.handle = zend_objects_store_put(object, (zend_objects_store_dtor_t) gtype_destroy_object, NULL TSRMLS_CC);

	return zov;
}

static PHP_METHOD(GType, __construct) {}

static int gtype_type_read(void *object, zval *return_value)
{
	ZVAL_LONG(return_value, ((php_gtype_t *)object)->type);
	return SUCCESS;
}

/*
 * External API functions
 */ 
PHP_GTK_API zval* php_gtype_new(GType type)
{
	zval *wrapper;
	php_gtype_t *object;
	TSRMLS_FETCH();

	MAKE_STD_ZVAL(wrapper);
	object_init_ex(wrapper, gtype_ce);
	object = zend_object_store_get_object(wrapper TSRMLS_CC);
	object->type = type;

	return wrapper;
}

PHP_GTK_API GType php_gtype_from_zval(zval *value)
{
	GType type;

	if (!value) {
		PHPG_THROW_EXCEPTION(phpg_type_exception, "cannot get type from NULL value");
		return 0;
	}

	switch (Z_TYPE_P(value)) {
		case IS_NULL:
			return G_TYPE_NONE;

		case IS_LONG:
			return G_TYPE_INT;

		case IS_DOUBLE:
			return G_TYPE_DOUBLE;

		case IS_STRING:
			type = g_type_from_name(Z_STRVAL_P(value));
			if (type == 0)
				return G_TYPE_STRING;
			else
				return type;

		case IS_BOOL:
			return G_TYPE_BOOLEAN;

		case IS_OBJECT:
			if (Z_OBJCE_P(value) == gtype_ce) {
				php_gtype_t *object = zend_object_store_get_object(value TSRMLS_CC);
				if (object) {
					return object->type;
				}
			} else {
				zval **gtype;
				if (zend_hash_find(Z_OBJPROP_P(value), "__gtype", sizeof("__gtype"), (void**)&gtype) == SUCCESS
					&& Z_TYPE_PP(gtype) == IS_OBJECT
					&& Z_OBJCE_PP(gtype) == gtype_ce) {

					php_gtype_t *object = zend_object_store_get_object(*gtype TSRMLS_CC);
					if (object) {
						return object->type;
					}
				}

				return G_TYPE_OBJECT;
			}
			break;

		default:
			break;
	}

	PHPG_THROW_EXCEPTION(phpg_type_exception, "could not get typecode from value");
	return 0;
}

void php_gtype_register_self()
{
	if (gtype_ce) return;

	gtype_ce = phpg_register_class("GType", gtype_methods, NULL, gtype_props_info, gtype_create_object TSRMLS_CC);
}

#endif /* HAVE_PHP_GTK */