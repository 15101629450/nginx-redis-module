
#include <redis_resp.h>

/*********************************************/
/* redis encode */
/*********************************************/
static ngx_str_t redis_encode_key1(ngx_pool_t *pool, char *format,
		ngx_str_t k1)
{
    ngx_str_t str;
	int size = snprintf(NULL, 0, format, REDIS_VAL1);
    str.data = ngx_pcalloc(pool, size+1);
    str.len = size;
    sprintf((char *)str.data, format, REDIS_VAL1);

//	ngx_str_t buffer = ngx_create_temp_buf(pool, size + 1);
//	if (buffer == NULL) {
//		return NULL;
//	}
//
//	sprintf((char *)buffer->pos, format, REDIS_VAL1);
//	buffer->last = buffer->pos + size;
//	buffer->start = buffer->pos;
//	buffer->end = buffer->last;
//	buffer->memory = 1;
//	return buffer;
    return str;
}

static ngx_str_t redis_encode_key2(ngx_pool_t *pool, char *format,
		ngx_str_t k1,
		ngx_str_t k2)
{
    ngx_str_t str;
    int size = snprintf(NULL, 0, format, REDIS_VAL2);
    str.data = ngx_pcalloc(pool, size+1);
    str.len = size;
    sprintf((char *)str.data, format, REDIS_VAL2);
    return str;
}

static ngx_str_t redis_encode_key3(ngx_pool_t *pool, char *format,
		ngx_str_t k1,
		ngx_str_t k2,
		ngx_str_t k3)
{
    ngx_str_t str;
    int size = snprintf(NULL, 0, format, REDIS_VAL3);
    str.data = ngx_pcalloc(pool, size+1);
    str.len = size;
    sprintf((char *)str.data, format, REDIS_VAL3);
    return str;
}

static ngx_str_t redis_encode_itoa(int n)
{
	char str[128];
	int len = sprintf(str, "%d", n);
	ngx_str_t val = {len, (u_char *)str};
	return val;
}

ngx_str_t redis_encode_get(ngx_pool_t *pool, ngx_str_t key)
{
	return redis_encode_key1(pool, REDIS_GET, key);
}

ngx_str_t redis_encode_get_r(ngx_pool_t *pool, const char *key)
{
	ngx_str_t __key = {strlen(key),(u_char *)key};
	return redis_encode_key1(pool, REDIS_GET, __key);
}

ngx_str_t redis_encode_set(ngx_pool_t *pool, ngx_str_t key, ngx_str_t val)
{
	return redis_encode_key2(pool, REDIS_SET, key, val);
}

ngx_str_t redis_encode_set_number(ngx_pool_t *pool, ngx_str_t key, int val)
{
	ngx_str_t k2 = redis_encode_itoa(val);
	return redis_encode_key2(pool, REDIS_SET, key, k2);
}

ngx_str_t redis_encode_incr(ngx_pool_t *pool, ngx_str_t key)
{
	return redis_encode_key1(pool, REDIS_INCR, key);
}

ngx_str_t redis_encode_incrby(ngx_pool_t *pool, ngx_str_t key, int val)
{
	ngx_str_t k2 = redis_encode_itoa(val);
	return redis_encode_key2(pool, REDIS_INCRBY, key, k2);
}

ngx_str_t redis_encode_hincrby(ngx_pool_t *pool, ngx_str_t key, ngx_str_t field, int  val)
{
	ngx_str_t k3 = redis_encode_itoa(val);
	return redis_encode_key3(pool, REDIS_HINCRBY, key, field, k3);
}

ngx_str_t redis_encode_expire(ngx_pool_t *pool, ngx_str_t key, int val)
{
	ngx_str_t k2 = redis_encode_itoa(val);
	return redis_encode_key2(pool, REDIS_EXPIRE, key, k2);
}

ngx_str_t redis_encode_sadd(ngx_pool_t *pool, ngx_str_t key, ngx_str_t val)
{
	return redis_encode_key2(pool, REDIS_SADD, key, val);
}

ngx_str_t redis_encode_smembers(ngx_pool_t *pool, ngx_str_t key)
{
	return redis_encode_key1(pool, REDIS_SMEMBERS, key);
}

ngx_str_t redis_encode_pass(ngx_pool_t *pool, ngx_str_t key)
{
	return redis_encode_key1(pool, REDIS_PASS, key);
}


/*********************************************/
/* redis decode */
/*********************************************/

int redis_decode_string_empty(redis_decode_t *c)
{
	if (strncmp(c->pos, "$-1\r\n", 5) == 0) {
		c->type = REDIS_NIL;
		c->pos = c->pos + 5;
		return REDIS_NIL;
	}

	if (strncmp(c->pos, "$0\r\n\r\n", 6) == 0) {
		c->type = REDIS_NIL;
		c->pos = c->pos + 6;
		return REDIS_NIL;
	}

	return REDIS_OK;
}

int redis_decode_array_empty(redis_decode_t *c)
{
	if (strncmp(c->pos, "*0\r\n", 4) == 0) {
		c->type = REDIS_NIL;
		c->pos = c->pos + 4;
		return REDIS_NIL;
	}

	return REDIS_OK;
}

void redis_decode_array_push(redis_decode_t *c, char *data, int len)
{
	if (!data || !len) {
		return;
	}

	redis_decode_list *node = ngx_pcalloc(c->pool, sizeof (redis_decode_list));
	node->value.data = (u_char *)data;
	node->value.len = len;
	if (c->array) {
		redis_decode_list *temp = c->array;
		c->array = node;
		c->array->next = temp;
	} else {
		c->array = node;
	}
}

int redis_decode_array_node(redis_decode_t *c)
{
	char *s = c->pos;
	if (*s++ != '$') {
		return REDIS_ERROR;
	}

	if (c->pos >= c->buf + c->size) {
		return REDIS_ERROR;
	}

	char *line = strstr(s, "\r\n");
	if (!line) {
		return REDIS_ERROR;
	}

	int len = atoi(s);
	if (len <= 0) {
		return REDIS_ERROR;
	}

	line += 2; //\r\n
	if ((c->size - (line - c->buf)) < len) {
		return REDIS_ERROR;
	}

	redis_decode_array_push(c, line, len);

	c->array_count++;
	if (c->array_count >= c->array_total) {
		c->pos = line + len + 2;
		return REDIS_OK;
	}

	c->pos = line + len + 2;
	return redis_decode_array_node(c);
}

int redis_decode_array(redis_decode_t *c)
{
	char *s = c->pos;
	if (*s++ != '*') {
		return REDIS_ERROR;
	}

	if (c->pos >= c->buf + c->size)
		return REDIS_ERROR;

	if (redis_decode_array_empty(c)) {
		return REDIS_OK;
	}

	char *line = strstr(s, "\r\n$");
	if (!line) {
		return REDIS_ERROR;
	}

	c->array_total = atoi(s);
	if (c->array_total <= 0) {
		return REDIS_ERROR;
	}

	c->type = REDIS_ARRAY;
	c->pos = line + 2;
	return redis_decode_array_node(c);
}

int redis_decode_string(redis_decode_t *c)
{
	char *s = c->pos;
	if (*s++ != '$') {
		return REDIS_ERROR;
	}

	if (c->pos >= c->buf + c->size)
		return REDIS_ERROR;

	if (redis_decode_string_empty(c)) {
		return REDIS_OK;
	}

	char *line = strstr(s, "\r\n");
	if (!line) {
		return REDIS_ERROR;
	}

	int len = atoi(s);
	if (len <= 0) {
		return REDIS_ERROR;
	}

	line += 2; //\r\n
	if ((c->size - (line - c->buf)) < len) {
		return REDIS_ERROR;
	}

	c->type = REDIS_STRING;
	c->value.len = len;
	c->value.data = (u_char *)line;
	c->pos = line + len + 2;
	return REDIS_OK;
}

int redis_decode_easy_string(redis_decode_t *c)
{
	char *s = c->pos;
	if (!(*s == '+' || *s == '-'))
		return REDIS_ERROR;

	if (c->pos >= c->buf + c->size)
		return REDIS_ERROR;

	char *line = strstr(s, "\r\n");
	if (!line) {
		return REDIS_ERROR;
	}

	c->type = REDIS_STRING;
	c->value.data = (u_char *)++s;
	c->value.len = line - s;
	c->pos = line + 2;
	return REDIS_OK;
}

int redis_decode_number(redis_decode_t *c)
{
	char *s = c->pos;
	if (*s++ != ':') {
		return REDIS_ERROR;
	}

	if (c->pos >= c->buf + c->size)
		return REDIS_ERROR;

	char *line = strstr(s, "\r\n");
	if (!line) {
		return REDIS_ERROR;
	}

	c->value.data = (u_char *)s;
	c->value.len = line - s;
	c->type = REDIS_INTEGER;
	c->pos = line + 2;
	return REDIS_OK;
}

int redis_decode_header(redis_decode_t *c)
{
	if (c->pos >= (c->buf + c->size)) {
		return REDIS_ERROR;
	}

	switch (*c->pos) {

		case '*':
			return redis_decode_array(c);

		case '$':
			return redis_decode_string(c);

		case '+':
		case '-':
			return redis_decode_easy_string(c);

		case ':':
			return redis_decode_number(c);

		default:
			return REDIS_ERROR;
	}
}

redis_decode_t *redis_decode(ngx_pool_t *pool, ngx_str_t buf)
{
	return redis_decode_r(pool, buf.data, (int )buf.len);
}

redis_decode_t *redis_decode_r(ngx_pool_t *pool, void *buf, int size)
{
	if (!buf || !size) {
		return NULL;
	}

	redis_decode_t *c = ngx_pcalloc(pool, sizeof(redis_decode_t));
	c->pool = pool;
	c->pos = buf;
	c->buf = buf;
	c->size = size;
	if (redis_decode_header(c)) {
		c->type = REDIS_ERROR;
	}

	return c;
}

void redis_decode_array_print(redis_decode_t *c)
{
	redis_decode_list *node = NULL;
	for (node = c->array; node; node = node->next) {
		fprintf(stdout, "[REDIS][ARRAY][%.*s]\n", (int)node->value.len, node->value.data);
	}
}

void redis_decode_print(redis_decode_t *c)
{
	if (!c) {
		fprintf(stdout, "[REDIS][ERROR]\n");
		return;
	}

	switch (c->type) {

		case REDIS_STRING:
			fprintf(stdout, "[REDIS][STRING][%.*s]\n", (int)c->value.len, c->value.data);
			return;

		case REDIS_INTEGER:
			fprintf(stdout, "[REDIS][INTEGER][%.*s]\n", (int)c->value.len, c->value.data);
			return;

		case REDIS_NIL:
			fprintf(stdout, "[REDIS][NIL][nil]\n");
			return;

		case REDIS_ARRAY:
			return redis_decode_array_print(c);

		case REDIS_ERROR:
			fprintf(stdout, "[REDIS][ERROR]\n");
			return;
	}
}

