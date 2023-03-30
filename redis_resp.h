
#ifndef __REDIS_RESP_H__
#define __REDIS_RESP_H__

#include <nginx.h>
#include <ngx_core.h>
#include <ngx_http.h>

#define REDIS_OK         0
#define REDIS_ERROR      -1

#define REDIS_STRING     1
#define REDIS_ARRAY      2
#define REDIS_INTEGER    3
#define REDIS_NIL        4
#define REDIS_STATUS     5
#define REDIS_HASH       100

#define REDIS_HEAD "redis://"
#define REDIS_HEAD_SIZE 8

#define REDIS_KEY1     "$%zd"CRLF"%.*s"CRLF
#define REDIS_KEY2     REDIS_KEY1"$%zd"CRLF"%.*s"CRLF
#define REDIS_KEY3     REDIS_KEY2"$%zd"CRLF"%.*s"CRLF

#define REDIS_VAL1     k1.len,(int)k1.len,k1.data
#define REDIS_VAL2     REDIS_VAL1,k2.len,(int)k2.len,k2.data
#define REDIS_VAL3     REDIS_VAL2,k3.len,(int)k3.len,k3.data

#define REDIS_GET      "*2"CRLF"$3"CRLF"GET"CRLF""REDIS_KEY1
#define REDIS_SET      "*3"CRLF"$3"CRLF"SET"CRLF""REDIS_KEY2
#define REDIS_INCR     "*2"CRLF"$4"CRLF"INCR"CRLF""REDIS_KEY1
#define REDIS_INCRBY   "*3"CRLF"$6"CRLF"INCRBY"CRLF""REDIS_KEY2
#define REDIS_HINCRBY  "*4"CRLF"$7"CRLF"HINCRBY"CRLF""REDIS_KEY3
#define REDIS_EXPIRE   "*3"CRLF"$6"CRLF"EXPIRE"CRLF""REDIS_KEY2
#define REDIS_SADD     "*3"CRLF"$4"CRLF"SADD"CRLF""REDIS_KEY2
#define REDIS_SMEMBERS "*2"CRLF"$8"CRLF"SMEMBERS"CRLF""REDIS_KEY1
#define REDIS_PASS     "*2"CRLF"$4"CRLF"AUTH"CRLF""REDIS_KEY1

typedef struct redis_decode_list redis_decode_list;
struct redis_decode_list {
    redis_decode_list *next;
    ngx_str_t value;
};

typedef struct {
    ngx_pool_t *pool;
    char *pos, *buf;
    int size;
    int type;
    int array_total, array_count;
    redis_decode_list *array;
    ngx_str_t value;
} redis_decode_t;

/*********************************************/
/* redis encode */
/*********************************************/
ngx_str_t redis_encode_get(ngx_pool_t *pool, ngx_str_t key);
ngx_str_t redis_encode_set(ngx_pool_t *pool, ngx_str_t key, ngx_str_t val);
ngx_str_t redis_encode_set_number(ngx_pool_t *pool, ngx_str_t key, int val);
ngx_str_t redis_encode_incr(ngx_pool_t *pool, ngx_str_t key);
ngx_str_t redis_encode_incrby(ngx_pool_t *pool, ngx_str_t key, int val);
ngx_str_t redis_encode_hincrby(ngx_pool_t *pool, ngx_str_t key, ngx_str_t field, int  val);
ngx_str_t redis_encode_expire(ngx_pool_t *pool, ngx_str_t key, int val);
ngx_str_t redis_encode_sadd(ngx_pool_t *pool, ngx_str_t key, ngx_str_t val);
ngx_str_t redis_encode_smembers(ngx_pool_t *pool, ngx_str_t key);
ngx_str_t redis_encode_pass(ngx_pool_t *pool, ngx_str_t key);

ngx_str_t redis_encode_get_r(ngx_pool_t *pool, const char *key);

/*********************************************/
/* redis decode */
/*********************************************/
redis_decode_t *redis_decode(ngx_pool_t *pool, ngx_str_t buf);
redis_decode_t *redis_decode_r(ngx_pool_t *pool, void *buf, int size);
void redis_decode_print(redis_decode_t *c);

#endif

