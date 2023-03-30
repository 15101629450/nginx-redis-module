
#ifndef __REDIS_MODULE_H__
#define __REDIS_MODULE_H__

#include <nginx.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <redis_resp.h>

ngx_int_t ngx_redis_subrequest(ngx_http_request_t *r,
                               ngx_str_t location,
                               ngx_str_t redis_command,
                               ngx_http_post_subrequest_pt handler);

#endif 

