
#include <redis_module.h>

typedef struct {
    ngx_url_t redis_host;
    ngx_str_t redis_pass;
    ngx_http_upstream_conf_t upstream;
} ngx_redis_conf_t;

typedef struct {
	ngx_str_t redis_command;
} ngx_redis_ctx_t;

static ngx_int_t ngx_redis_create_request(ngx_http_request_t *r);
static ngx_int_t ngx_redis_process_header(ngx_http_request_t *r);
static void ngx_redis_abort_request(ngx_http_request_t *r);
static void ngx_redis_finalize_request(ngx_http_request_t *r, ngx_int_t rc);
static ngx_int_t ngx_redis_input_filter_init(void *data);
static ngx_int_t ngx_redis_input_filter(void *data, ssize_t bytes);
static void *ngx_redis_conf_create(ngx_conf_t *cf);
static char *ngx_redis_conf_init(ngx_conf_t *cf, void *parent, void *child);
static char *ngx_redis_command(ngx_conf_t *cf, ngx_command_t *cmd, void *p);
static ngx_command_t  ngx_redis_commands[] = {

        { ngx_string("redis"),
          NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
          ngx_redis_command,
          NGX_HTTP_LOC_CONF_OFFSET,
          0,
          NULL },
	ngx_null_command
};

static ngx_http_module_t  ngx_redis_module_ctx = {
	NULL,          /* preconfiguration */
	NULL,                                  /* postconfiguration */
	NULL,       /* create main configuration */
	NULL,                                  /* init main configuration */
	NULL,                                  /* create server configuration */
	NULL,                                  /* merge server configuration */
	ngx_redis_conf_create,        /* create location configuration */
	ngx_redis_conf_init          /* merge location configuration */
};

ngx_module_t redis_module = {
	NGX_MODULE_V1,
	&ngx_redis_module_ctx,            /* module context */
	ngx_redis_commands,               /* module directives */
	NGX_HTTP_MODULE,                       /* module type */
	NULL,                                  /* init master */
	NULL,                                  /* init module */
	NULL,                                  /* init process */
	NULL,                                  /* init thread */
	NULL,                                  /* exit thread */
	NULL,                                  /* exit process */
	NULL,                                  /* exit master */
	NGX_MODULE_V1_PADDING
};

static ngx_int_t ngx_redis_upstream(ngx_http_request_t *r)
{
    if (ngx_http_upstream_create(r) != NGX_OK) {
        return NGX_HTTP_NO_CONTENT;
    }

    ngx_http_upstream_t *u = r->upstream;
    u->output.tag = (ngx_buf_tag_t) &redis_module;

    ngx_redis_conf_t *conf = ngx_http_get_module_loc_conf(r, redis_module);
    u->conf = &conf->upstream;

    u->create_request = ngx_redis_create_request;
    u->process_header = ngx_redis_process_header;
    u->abort_request = ngx_redis_abort_request;
    u->finalize_request = ngx_redis_finalize_request;

    u->input_filter_init = ngx_redis_input_filter_init;
    u->input_filter = ngx_redis_input_filter;
    u->input_filter_ctx = r;
    u->input_filter = NULL;
    ngx_http_upstream_init(r);

    r->main->count++;
    return NGX_DONE;
}

static ngx_buf_t *redis_command_buffer(ngx_pool_t *pool, ngx_str_t redis_command)
{
    ngx_buf_t *buf = ngx_create_temp_buf(pool, redis_command.len);
    if (buf == NULL) {
        return NULL;
    }

    memcpy(buf->pos, redis_command.data, redis_command.len);
    buf->last = buf->pos + redis_command.len;
    buf->start = buf->pos;
    buf->end = buf->last;
    buf->memory = 1;
    return buf;
}

static ngx_int_t ngx_redis_create_request(ngx_http_request_t *r)
{
    ngx_redis_conf_t *conf = ngx_http_get_module_loc_conf(r, redis_module);
    ngx_redis_ctx_t *ctx = ngx_http_get_module_ctx(r, redis_module);
    ngx_chain_t *c1 = ngx_alloc_chain_link(r->pool);
    if (c1 == NULL) {
        return NGX_ERROR;
    }

    if (conf->redis_pass.len > 0) {
        ngx_chain_t *c2 = ngx_alloc_chain_link(r->pool);
        if (c2 == NULL) {
            return NGX_ERROR;
        }

        ngx_str_t pass = redis_encode_pass(r->pool, conf->redis_pass);
        c1->buf = redis_command_buffer(r->pool, pass);
        c1->next = c2;

        c2->buf = redis_command_buffer(r->pool, ctx->redis_command);
        c2->next = NULL;

    } else {
        c1->buf = redis_command_buffer(r->pool, ctx->redis_command);
        c1->next = NULL;
    }

    r->upstream->request_bufs = c1;
    return NGX_OK;
}

static ngx_int_t ngx_redis_process_header(ngx_http_request_t *r)
{
	ngx_redis_conf_t *conf = ngx_http_get_module_loc_conf(r, redis_module);
	if (conf->redis_pass.len > 0 && r->upstream->buffer.last > r->upstream->buffer.pos) {
		char *pos = (char *)r->upstream->buffer.pos;
		if (strncmp("+OK"CRLF, pos, 5) == 0) {
			r->upstream->buffer.pos = r->upstream->buffer.pos + 5;
		}
	}

	r->upstream->state->status = NGX_HTTP_OK;
	r->upstream->headers_in.status_n = NGX_HTTP_OK;
	r->upstream->headers_in.content_length_n =
		r->upstream->buffer.last - r->upstream->buffer.pos;
	return NGX_OK;
}

static void ngx_redis_abort_request(ngx_http_request_t *r)
{
}

static void ngx_redis_finalize_request(ngx_http_request_t *r, ngx_int_t rc)
{
}

static ngx_int_t ngx_redis_input_filter_init(void *data)
{
	return NGX_OK;
}

static ngx_int_t ngx_redis_input_filter(void *data, ssize_t size)
{
	ngx_http_request_t *r = data;
	ngx_http_upstream_t *u = r->upstream;
	ngx_buf_t *b = &u->buffer;

	u->out_bufs = ngx_chain_get_free_buf(r->pool, &u->free_bufs);
	u->out_bufs->buf->pos = b->pos;
	u->out_bufs->buf->last = b->pos + size;
	u->out_bufs->buf->tag = u->output.tag;
    u->out_bufs->buf->memory = 1;
//    u->out_bufs->buf->flush = 1;

    return NGX_OK;
}

static void *ngx_redis_conf_create(ngx_conf_t *cf)
{
	ngx_redis_conf_t *conf = ngx_pcalloc(cf->pool, sizeof(ngx_redis_conf_t));
	if (conf == NULL) {
		return NULL;
	}

	conf->upstream.connect_timeout = NGX_CONF_UNSET_MSEC;
	conf->upstream.send_timeout = NGX_CONF_UNSET_MSEC;
	conf->upstream.read_timeout = NGX_CONF_UNSET_MSEC;
	conf->upstream.buffer_size = NGX_CONF_UNSET_SIZE;
	conf->upstream.cyclic_temp_file = 0;
	conf->upstream.buffering = 0;
	conf->upstream.ignore_client_abort = 1;
	conf->upstream.send_lowat = 0;
	conf->upstream.bufs.num = 0;
	conf->upstream.busy_buffers_size = 0;
	conf->upstream.max_temp_file_size = 0;
	conf->upstream.temp_file_write_size = 0;
	conf->upstream.intercept_errors = 1;
	conf->upstream.intercept_404 = 1;
	conf->upstream.pass_request_headers = 0;
	conf->upstream.pass_request_body = 0;
	return conf;
}

static char *ngx_redis_conf_init(ngx_conf_t *cf, void *parent, void *child)
{
	ngx_redis_conf_t *prev = parent;
	ngx_redis_conf_t *conf = child;

	ngx_conf_merge_msec_value(conf->upstream.connect_timeout,
			prev->upstream.connect_timeout, 6000);

	ngx_conf_merge_msec_value(conf->upstream.send_timeout,
			prev->upstream.send_timeout, 6000);

	ngx_conf_merge_msec_value(conf->upstream.read_timeout,
			prev->upstream.read_timeout, 6000);

	ngx_conf_merge_size_value(conf->upstream.buffer_size,
			prev->upstream.buffer_size,
			(size_t) ngx_pagesize);

	ngx_conf_merge_bitmask_value(conf->upstream.next_upstream,
			prev->upstream.next_upstream,
			(NGX_CONF_BITMASK_SET
			 | NGX_HTTP_UPSTREAM_FT_ERROR
			 | NGX_HTTP_UPSTREAM_FT_TIMEOUT));

	if (conf->upstream.next_upstream & NGX_HTTP_UPSTREAM_FT_OFF) {
		conf->upstream.next_upstream = NGX_CONF_BITMASK_SET
			| NGX_HTTP_UPSTREAM_FT_OFF;
	}

	if (conf->upstream.upstream == NULL) {
		conf->upstream.upstream = prev->upstream.upstream;
	}

	return NGX_CONF_OK;
}

int ngx_redis_uri_parse(ngx_pool_t *pool, ngx_str_t redis_uri, ngx_redis_conf_t *conf)
{
    if (!redis_uri.len || !redis_uri.data) {
        return NGX_ERROR;
    }

    char *pos = (char *)redis_uri.data;
    if (strncmp(pos, REDIS_HEAD, REDIS_HEAD_SIZE) != 0) {
        return NGX_ERROR;
    }

    pos += REDIS_HEAD_SIZE;
    char *line = strchr(pos, '@');
    if (line) {
        conf->redis_pass.data = (u_char *)pos;
        conf->redis_pass.len = line - pos;
        pos = line + 1;
    }

    line = strchr(pos, ':');
    if (line) {
        conf->redis_host.url.data = (u_char *)pos;
        conf->redis_host.url.len = line - pos;
        conf->redis_host.default_port = atoi(line + 1);

    } else {
        conf->redis_host.url.data = (u_char *)pos;
        conf->redis_host.url.len = redis_uri.len - REDIS_HEAD_SIZE - (conf->redis_pass.len ? conf->redis_pass.len + 1 : 0);
        conf->redis_host.default_port = 6379;
    }

    conf->redis_host.no_resolve = 1;
    return NGX_OK;
}

int ngx_redis_uri_init(ngx_str_t redis_uri, ngx_redis_conf_t *conf, ngx_conf_t *cf)
{
    if (ngx_redis_uri_parse(cf->pool, redis_uri, conf)) {
        return NGX_ERROR;
    }

    conf->upstream.upstream = ngx_http_upstream_add(cf, &conf->redis_host, 0);
    if (conf->upstream.upstream == NULL) {
        return NGX_ERROR;
    }

    return NGX_OK;
}

static ngx_int_t ngx_redis_handler(ngx_http_request_t *r)
{
    ngx_redis_ctx_t *ctx = ngx_pcalloc(r->pool, sizeof(ngx_redis_ctx_t));
    ctx->redis_command = r->args;
    ngx_http_set_ctx(r, ctx, redis_module);
    return ngx_redis_upstream(r);
}

static char *ngx_redis_command(ngx_conf_t *cf, ngx_command_t *cmd, void *p)
{
    ngx_redis_conf_t *conf = p;
    if (conf->upstream.upstream) {
        return "redis is duplicate";
    }

    ngx_str_t redis_uri = ((ngx_str_t *)cf->args->elts)[1];
    if (ngx_redis_uri_init(redis_uri, conf, cf)) {
        return NGX_CONF_ERROR;
    }

    ngx_http_core_loc_conf_t *clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_redis_handler;
    return NGX_CONF_OK;
}


ngx_int_t ngx_redis_subrequest(ngx_http_request_t *r,
                               ngx_str_t location,
                               ngx_str_t redis_command,
                               ngx_http_post_subrequest_pt handler)
{
    ngx_http_request_t *sr = NULL;
    ngx_http_post_subrequest_t *ps = ngx_pcalloc(r->pool, sizeof(ngx_http_post_subrequest_t));
    ps->handler = handler;
    if (ngx_http_subrequest(r,
                            &location,
                            &redis_command,
                            &sr,
                            ps,
                            NGX_HTTP_SUBREQUEST_WAITED) != NGX_OK) {
        return NGX_ERROR;
    }

    sr->request_body = ngx_pcalloc(r->pool, sizeof(ngx_http_request_body_t));
    if (sr->request_body == NULL) {
        return NGX_ERROR;
    }

    sr->header_only = 1;
    return NGX_AGAIN;
}

