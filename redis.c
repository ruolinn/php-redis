/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2018 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_redis.h"
#include <zend_exceptions.h>

zend_class_entry *redis_ce;
zend_class_entry *redis_exception_ce;

/* If you declare any globals in php_redis.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(redis)
*/

/* True global resources - no need for thread safety here */
static int le_redis;

static zend_function_entry redis_method_entry[] = {
    PHP_ME(Redis, __construct, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Redis, connect, NULL, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

PHP_METHOD(Redis, __construct)
{

}

PHP_METHOD(Redis, connect)
{
    zval *object;
    int host_len;
    char *host = NULL;
    zend_long port = -1;
    double timeout = 0.0;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(),
                                     "Os|ld", &object, redis_ce, &host, &host_len,
                                     &port, &timeout) == FAILURE) {
        RETURN_FALSE;
    }

    if (timeout < 0L || timeout > INT_MAX) {
        zend_throw_exception(redis_exception_ce,
                             "Invalid connect timeout", 0 TSRMLS_CC);
        RETURN_FALSE;
    }

    RETURN_TRUE;
}

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("redis.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_redis_globals, redis_globals)
    STD_PHP_INI_ENTRY("redis.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_redis_globals, redis_globals)
PHP_INI_END()
*/
/* }}} */


/* {{{ php_redis_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_redis_init_globals(zend_redis_globals *redis_globals)
{
	redis_globals->global_value = 0;
	redis_globals->global_string = NULL;
}
*/
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(redis)
{
    zend_class_entry *exception_ce;

    zend_class_entry redis_class_entry;
    INIT_CLASS_ENTRY(redis_class_entry, "Redis", redis_method_entry);
    redis_ce = zend_register_internal_class(&redis_class_entry);

    exception_ce = zend_exception_get_default();

    zend_class_entry redis_exception_class_entry;
    INIT_CLASS_ENTRY(redis_exception_class_entry, "RedisException", NULL);
    redis_exception_ce = zend_register_internal_class_ex(&redis_exception_class_entry, exception_ce);

	/* If you have INI entries, uncomment these lines
	REGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(redis)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(redis)
{
#if defined(COMPILE_DL_REDIS) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(redis)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(redis)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "redis support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */

/* {{{ redis_functions[]
 *
 * Every user visible function must have an entry in redis_functions[].
 */
const zend_function_entry redis_functions[] = {
	PHP_FE_END	/* Must be the last line in redis_functions[] */
};
/* }}} */

/* {{{ redis_module_entry
 */
zend_module_entry redis_module_entry = {
	STANDARD_MODULE_HEADER,
	"redis",
	redis_functions,
	PHP_MINIT(redis),
	PHP_MSHUTDOWN(redis),
	PHP_RINIT(redis),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(redis),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(redis),
	PHP_REDIS_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_REDIS
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(redis)
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

