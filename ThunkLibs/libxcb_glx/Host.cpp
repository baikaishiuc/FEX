/*
$info$
tags: thunklibs|xcb-glx
$end_info$
*/

#include <cstring>
#include <stdio.h>

#include <xcb/glx.h>
#include <xcb/xcbext.h>

#include "common/Host.h"
#include <dlfcn.h>
#include <malloc.h>

#include "ldr_ptrs.inl"
#include "function_unpacks.inl"


void fexfn_unpack_libxcb_glx_FEX_xcb_glx_init_extension(void *argsv);

void fexfn_unpack_libxcb_glx_FEX_usable_size(void *argsv){
  struct arg_t {void* a_0;size_t rv;};
  auto args = (arg_t*)argsv;
  args->rv = malloc_usable_size(args->a_0);
}

void fexfn_unpack_libxcb_glx_FEX_free_on_host(void *argsv){
  struct arg_t {void* a_0;};
  auto args = (arg_t*)argsv;
  free(args->a_0);
}

static ExportEntry exports[] = {
    #include "tab_function_unpacks.inl"
    { nullptr, nullptr }
};

#include "ldr.inl"

void fexfn_unpack_libxcb_glx_FEX_xcb_glx_init_extension(void *argsv){
  struct arg_t {xcb_connection_t * a_0;xcb_extension_t * a_1;};
  auto args = (arg_t*)argsv;
  xcb_extension_t *ext{};
  if (strcmp(args->a_1->name, "GLX") == 0) {
    ext = (xcb_extension_t *)dlsym(fexldr_ptr_libxcb_glx_so, "xcb_glx_id");
  }
  else {
    fprintf(stderr, "Unknown xcb extension '%s'\n", args->a_1->name);
    __builtin_trap();
    return;
  }

  typedef const struct xcb_query_extension_reply_t * fexldr_type_libxcb_xcb_get_extension_data(xcb_connection_t * a_0,xcb_extension_t * a_1);
  fexldr_type_libxcb_xcb_get_extension_data *fexldr_ptr_libxcb_xcb_get_extension_data;

  fexldr_ptr_libxcb_xcb_get_extension_data = (fexldr_type_libxcb_xcb_get_extension_data*)dlsym(RTLD_DEFAULT, "xcb_get_extension_data");

  auto res = fexldr_ptr_libxcb_xcb_get_extension_data(args->a_0, ext);

  // Copy over the global id
  args->a_1->global_id = ext->global_id;
}

EXPORTS(libxcb_glx)
