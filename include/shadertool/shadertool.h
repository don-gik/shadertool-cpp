#ifndef SHADERTOOL_H
#define SHADERTOOL_H

#include <shadertool/shadertool_types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Context Context;

Context* shadertool_create_context(void);
void shadertool_destroy_context(Context* ctx);

CompileResult shadertool_compile_shader(
    Context* ctx,
    const char* moduleName,
    const char* modulePath,
    const char* entryPoint
);

void shadertool_free_compile_result(CompileResult* result);

#ifdef __cplusplus
}
#endif

#endif // SHADERTOOL_H
