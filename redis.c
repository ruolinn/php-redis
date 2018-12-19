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
#include "php_network.h"

zend_class_entry *redis_ce;
zend_class_entry *redis_exception_ce;

/* If you declare any globals in php_redis.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(redis)
*/

/* True global resources - no need for thread safety here */
static int le_redis;

int redis_check_eof(RedisSock *redis_sock)
{
    if (php_stream_eof(redis_sock->stream) == 0) {
        return 0;
    }

    return -1;
}

RedisSock *redis_sock_get(zval *id)
{
    redis_object *redis;

    if (Z_TYPE_P(id) == IS_OBJECT) {
        redis = PHPREDIS_GET_OBJECT(redis_object, id);

        if (redis->sock) {
            return redis->sock;
        }
    }

    return NULL;
}

int redis_sock_write(RedisSock *redis_sock, char *cmd, size_t sz)
{
    if (redis_check_eof(redis_sock) == 0 && php_stream_write(redis_sock->stream, cmd, sz) == sz) {
        return sz;
    }

    return -1;
}

char *redis_sock_read(RedisSock *redis_sock, int *buf_len)
{
    char buf[4096];
    size_t len;

    if (redis_check_eof(redis_sock) == -1) {
        return NULL;
    }

    if (php_stream_get_line(redis_sock->stream, buf, sizeof(buf)-1, &len) == NULL) {
        // redis_sock_disconnect(redis_sock, 1);
        zend_throw_exception(redis_exception_ce, "read error on connection", 0);
        return NULL;
    }

    switch(buf[0]) {
    case '+':
        if (len > 1) {
            *buf_len = len;
            return estrndup(buf, *buf_len);
        }
    }

    return NULL;
}

void redis_sock_set_err(RedisSock *redis_sock, const char *msg, int msg_len)
{
    if (redis_sock->err != NULL) {
        zend_string_release(redis_sock->err);
        redis_sock->err = NULL;
    }

    if (msg != NULL && msg_len > 0) {
        redis_sock->err = zend_string_init(msg, msg_len, 0);
    }
}

void redis_free_socket(RedisSock *redis_sock)
{
    if (redis_sock->err) {
        zend_string_release(redis_sock->err);
    }

    if (redis_sock->host) {
        zend_string_release(redis_sock->host);
    }

    efree(redis_sock);
}

RedisSock *redis_sock_create(char *host, int host_len, unsigned short port, double timeout)
{
    RedisSock *redis_sock;
    redis_sock = ecalloc(1, sizeof(RedisSock));

    redis_sock->stream = NULL;
    redis_sock->host = zend_string_init(host, host_len, 0);
    redis_sock->port = port;
    redis_sock->timeout = timeout;
    redis_sock->err = NULL;
    redis_sock->status = REDIS_SOCK_STATUS_DISCONNECTED;

    return redis_sock;
}

int redis_sock_connect(RedisSock *redis_sock)
{
    struct timeval tv, *tv_ptr = NULL;
    char host[1024];
    const char *fmtstr = "%s:%d";
    int host_len, err = 0, usocket = 0;
    int tcp_flag = 1;
    php_netstream_data_t *sock;
    zend_string *estr = NULL;

    if (redis_sock->stream != NULL) {
        //redis_sock_disconnect(redis_sock, 0);
    }

    tv.tv_sec = (time_t)redis_sock->timeout;
    tv.tv_usec = (int)((redis_sock->timeout - tv.tv_sec) * 1000000);

    if (tv.tv_sec != 0 || tv.tv_usec != 0) {
        tv_ptr = &tv;
    }

    if (redis_sock->port == 0) redis_sock->port = 6379;

    host_len = snprintf(host, sizeof(host), fmtstr, ZSTR_VAL(redis_sock->host), redis_sock->port);

    redis_sock->stream = php_stream_xport_create(host, host_len, 0,
                                                 STREAM_XPORT_CLIENT|STREAM_XPORT_CONNECT, "",
                                                 tv_ptr, NULL, &estr, &err);

    if (!redis_sock->stream) {
        if (estr) {
            redis_sock_set_err(redis_sock, ZSTR_VAL(estr), ZSTR_LEN(estr));
            zend_string_release(estr);
        }

        return -1;
    }

    sock = (php_netstream_data_t *)redis_sock->stream->abstract;
    if (!usocket) {
        err = setsockopt(sock->socket, IPPROTO_TCP, TCP_NODELAY, (char*) &tcp_flag, sizeof(tcp_flag));
        //PHPREDIS_NOTUSED(err);
        //err = setsockopt(sock->socket, SOL_SOCKET, SO_KEEPALIVE, (char*) &redis_sock->tcp_keepalive, sizeof(redis_sock->tcp_keepalive));
        //PHPREDIS_NOTUSED(err);
    }

    php_stream_auto_cleanup(redis_sock->stream);

    php_stream_set_option(redis_sock->stream,
                          PHP_STREAM_OPTION_WRITE_BUFFER, PHP_STREAM_BUFFER_NONE, NULL);

    redis_sock->status = REDIS_SOCK_STATUS_CONNECTED;

    return 0;
}

int redis_sock_server_open(RedisSock *redis_sock)
{
    int res = -1;

    switch(redis_sock->status) {
    case REDIS_SOCK_STATUS_DISCONNECTED:
        return redis_sock_connect(redis_sock);
    case REDIS_SOCK_STATUS_CONNECTED:
        res = 0;
        break;
    }

    return res;
}

zend_object_handlers redis_object_handlers;

void free_redis_object(zend_object *object)
{
    redis_object *redis = (redis_object *)((char *)(object) - XtOffsetOf(redis_object, std));

    zend_object_std_dtor(&redis->std);
    if (redis->sock) {
        //redis_sock_disconnect(redis->sock, 0 TSRMLS_CC);
        redis_free_socket(redis->sock);
    }
}

zend_object *create_redis_object(zend_class_entry *ce)
{
    redis_object *redis = ecalloc(1, sizeof(redis_object) + zend_object_properties_size(ce));

    redis->sock = NULL;

    zend_object_std_init(&redis->std, ce);
    object_properties_init(&redis->std, ce);

    memcpy(&redis_object_handlers, zend_get_std_object_handlers(), sizeof(redis_object_handlers));
    redis_object_handlers.offset = XtOffsetOf(redis_object, std);
    redis_object_handlers.free_obj = free_redis_object;
    redis->std.handlers = &redis_object_handlers;

    return &redis->std;
}

static zend_function_entry redis_method_entry[] = {
    PHP_ME(Redis, __construct, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Redis, connect, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Redis, ping, NULL, ZEND_ACC_PUBLIC)
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
    redis_object *redis;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|ld", &host, &host_len, &port, &timeout) == FAILURE) {
        RETURN_FALSE;
    }

    if (timeout < 0L || timeout > INT_MAX) {
        zend_throw_exception(redis_exception_ce,
                             "Invalid connect timeout", 0);
        RETURN_FALSE;
    }

    if (port == -1) {
        port = 6379;
    }

    object = getThis();
    //redis = PHPREDIS_GET_OBJECT(redis_object, object);
    redis = PHPREDIS_GET_OBJECT(redis_object, getThis());

    if (redis->sock) {
        //redis_sock_disconnect(redis->sock, 0);
        //redis_free_socket(redis->sock);
    }

    redis->sock = redis_sock_create(host, host_len, port, timeout);

    if (redis_sock_server_open(redis->sock) < 0) {
        if (redis->sock->err) {
            zend_throw_exception(redis_exception_ce, ZSTR_VAL(redis->sock->err), 0);
        }

        //redis_free_socket(redis->sock);
        //redis->sock = NULL;

        RETURN_FALSE;
    }


    RETURN_TRUE;
}

PHP_METHOD(Redis, ping)
{
    RedisSock *redis_sock;
    char *response;
    int response_len;

    redis_sock = redis_sock_get(getThis());

    char cmd[] = "PING\r\n";
    redis_sock_write(redis_sock, cmd, sizeof(cmd)-1);

    if ((response = redis_sock_read(redis_sock, &response_len)) == NULL) {
        RETURN_FALSE;
    }

    RETVAL_STRINGL(response, response_len);

    efree(response);
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
    redis_ce->create_object = create_redis_object;

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

