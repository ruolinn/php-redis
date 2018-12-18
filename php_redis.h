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

#ifndef PHP_REDIS_H
#define PHP_REDIS_H


#define REDIS_SOCK_STATUS_FAILED       0
#define REDIS_SOCK_STATUS_DISCONNECTED 1
#define REDIS_SOCK_STATUS_CONNECTED    2

extern zend_module_entry redis_module_entry;

PHP_METHOD(Redis, __construct);
PHP_METHOD(Redis, connect);
PHP_METHOD(Redis, ping);
#define phpext_redis_ptr &redis_module_entry

#define PHP_REDIS_VERSION "0.1.0" /* Replace with version number for your extension */

#ifdef PHP_WIN32
#	define PHP_REDIS_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_REDIS_API __attribute__ ((visibility("default")))
#else
#	define PHP_REDIS_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

/*
  	Declare any global variables you may need between the BEGIN
	and END macros here:

ZEND_BEGIN_MODULE_GLOBALS(redis)
	zend_long  global_value;
	char *global_string;
ZEND_END_MODULE_GLOBALS(redis)
*/

/* Always refer to the globals in your function as REDIS_G(variable).
   You are encouraged to rename these macros something shorter, see
   examples in any other php module directory.
*/
#define REDIS_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(redis, v)

#if defined(ZTS) && defined(COMPILE_DL_REDIS)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

#define PHPREDIS_GET_OBJECT(class_entry, z) (class_entry *)((char *)Z_OBJ_P(z) - XtOffsetOf(class_entry, std))

typedef struct {
    php_stream *stream;
    zend_string *host;
    short port;
    double timeout;
    int status;
    zend_string *err;
} RedisSock;

typedef struct {
    RedisSock *sock;
    zend_object std;
} redis_object;

#endif	/* PHP_REDIS_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
