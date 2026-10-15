#ifndef SHADERTOOL_TYPES_H
#define SHADERTOOL_TYPES_H

#include <cstdint>
#include <slang.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Context Context;

typedef enum ErrorCode {
    SHADERTOOL_OK = 0,
    SHADERTOOL_ERROR_FILE_NOT_FOUND,
    SHADERTOOL_ERROR_SYNTAX_ERROR,
    SHADERTOOL_ERROR_LINK_ERROR,
    SHADERTOOL_ERROR_INTERNAL,
    SHADERTOOL_ERROR_UNKNOWN = 9999
} ErrorCode;

typedef struct ErrorInfo {
    ErrorCode code;
    const char* message;
    const char* filePath;
    int32_t line;
    int32_t column;
} ErrorInfo;

typedef enum ResourceKind : uint8_t {
    RESOURCE_KIND_PUSH_CONSTANT = 0,
    RESOURCE_KIND_UNIFORM = 1,
    RESOURCE_KIND_TEXTURE = 2,         // Texture2D, TextureCube
    RESOURCE_KIND_SAMPLER = 3,         // SamplerState
    RESOURCE_KIND_STORAGE_TEXTURE = 4  // RWTexture2D
} ResourceKind;

typedef enum ScalarKind : uint8_t {
    SCALAR_KIND_NONE     = 0,
    SCALAR_KIND_VOID     = 1,
    SCALAR_KIND_BOOL     = 2,
    SCALAR_KIND_INT32    = 3,
    SCALAR_KIND_UINT32   = 4,
    SCALAR_KIND_INT64    = 5,
    SCALAR_KIND_UINT64   = 6,
    SCALAR_KIND_FLOAT16  = 7,  // half
    SCALAR_KIND_FLOAT32  = 8,  // float
    SCALAR_KIND_FLOAT64  = 9,  // double
    SCALAR_KIND_INT8     = 10,
    SCALAR_KIND_UINT8    = 11,
    SCALAR_KIND_INT16    = 12,
    SCALAR_KIND_UINT16   = 13,
    SCALAR_KIND_INTPTR   = 14,
    SCALAR_KIND_UINTPTR  = 15,
    SCALAR_KIND_BFLOAT16 = 16
} ScalarKind;

typedef enum MemberKind : uint8_t {
    MEMBER_KIND_PRIMITIVE = 0,
    MEMBER_KIND_STRUCT = 1,
    MEMBER_KIND_POINTER = 2
} MemberKind;

typedef struct CompactMember {
    const char* name;
    uint32_t offset;
    uint32_t size;

    MemberKind kind;
    ScalarKind scalarType;
    uint8_t rowCount;
    uint8_t colCount;
    uint32_t arrayCount;

    int32_t parentIndex;
} CompactMember;

struct CompactResource {
    const char* name;
    ResourceKind kind;
    uint32_t set;
    uint32_t binding;
    uint32_t totalSize;
    
    CompactMember* members;
    uint32_t memberCount;
};

typedef struct CompileResult {
    bool success;
    ErrorInfo errorInfo;

    const uint8_t* codeBytes;
    size_t codeSize;

    const char* const* depFilePath;
    size_t depFileCount;

    const CompactResource* resources;
} CompileResult;


#ifdef __cplusplus
}
#endif

#endif  // SHADERTOOL_TYPES_H
