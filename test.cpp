#include "shadertool/shadertool_types.h"
#include <shadertool/shadertool.h>
#include <iostream>
#include <fstream>
#include <iomanip>

inline const char* resourceKindToString(ResourceKind kind) {
    switch (kind) {
        case RESOURCE_KIND_PUSH_CONSTANT:   return "Push Constant";
        case RESOURCE_KIND_UNIFORM:         return "Uniform Buffer (UBO)";
        case RESOURCE_KIND_TEXTURE:         return "Sampled Texture";
        case RESOURCE_KIND_SAMPLER:         return "Sampler";
        case RESOURCE_KIND_STORAGE_TEXTURE: return "Storage Texture (RW)";
        default:                          return "Unknown";
    }
}

inline const char* memberKindToString(MemberKind kind) {
    switch (kind) {
        case MEMBER_KIND_POINTER:   return "BDA";
        case MEMBER_KIND_STRUCT:    return "Struct";
        case MEMBER_KIND_PRIMITIVE: return "Primitive";
        default:                    return "Unknown";
    }
}

std::string printType(ScalarKind scalarType, uint8_t rowCount, uint8_t colCount) {
    std::string str;
    
    switch (scalarType) {
        case SCALAR_KIND_NONE:
            str = "";
            break;
        case SCALAR_KIND_VOID:
            str = "void";
            break;
        case SCALAR_KIND_BOOL:
            str = "bool";
            break;
        case SCALAR_KIND_INT32:
            str = "int";
            break;
        case SCALAR_KIND_UINT32:
            str = "uint";
            break;
        case SCALAR_KIND_INT64:
            str = "int64_t";
            break;
        case SCALAR_KIND_UINT64:
            str = "uint64_t";
            break;
        case SCALAR_KIND_FLOAT16:
            str = "half";
            break;
        case SCALAR_KIND_FLOAT32:
            str = "float";
            break;
        case SCALAR_KIND_FLOAT64:
            str = "float64_t";
            break;
        case SCALAR_KIND_INT8:
            str = "int8_t";
            break;
        case SCALAR_KIND_UINT8:
            str = "uint8_t";
            break;
        case SCALAR_KIND_INT16:
            str = "int16_t";
            break;
        case SCALAR_KIND_UINT16:
            str = "uint16_t";
            break;
        case SCALAR_KIND_INTPTR:
            str = "intptr_t";
            break;
        case SCALAR_KIND_UINTPTR:
            str = "uintptr_t";
            break;
        case SCALAR_KIND_BFLOAT16:
            str = "BFloat16";
            break;
        default:
            str = "";
    }

    if (rowCount <= 1 && colCount <= 1) {
        return str;
    }

    if (rowCount <= 1) {
        return str + std::to_string(colCount);
    }
    
    return str + std::to_string(rowCount) + "x" + std::to_string(colCount);
}

void printReflectionInfo(const CompileResult& result) {
    if (!result.success) {
        std::cout << "[Reflection] Shader compilation failed, skipping reflection dump.\n";
        return;
    }

    if (!result.resources) {
        std::cout << "[Reflection] No reflection resources found.\n";
        return;
    }

    std::cout << "\n=================================================================\n";
    std::cout << "                    [ SHADER REFLECTION DATA ]                   \n";
    std::cout << "=================================================================\n";

    size_t resIdx = 0;
    // Search until Null Sentinel (name == nullptr)
    while (result.resources[resIdx].name != nullptr) {
        const CompactResource& res = result.resources[resIdx];

        std::cout << "Resource [" << resIdx << "] : " << res.name << "\n";
        std::cout << "  - Kind         : " << resourceKindToString(res.kind) << "\n";

        if (res.kind != RESOURCE_KIND_PUSH_CONSTANT) {
            std::cout << "  - Descriptor   : Set " << res.set << ", Binding " << res.binding << "\n";
        }

        // PushConstant, Uniform -> Print member features
        if (res.kind == RESOURCE_KIND_PUSH_CONSTANT || res.kind == RESOURCE_KIND_UNIFORM) {
            std::cout << "  - Total Size   : " << res.totalSize << " bytes\n";
            std::cout << "  - Member Count : " << res.memberCount << "\n";

            if (res.memberCount > 0 && res.members) {
                std::cout << "  - Members Layout:\n";
                std::cout << "    -------------------------------------------------------------------------------------------------------------\n";
                std::cout << "    " << std::left 
                          << std::setw(6)  << "[Idx]" 
                          << std::setw(24) << "Name" 
                          << std::setw(10) << "Offset" 
                          << std::setw(10) << "Size" 
                          << std::setw(15) << "Type"
                          << std::setw(10) << "Parent" 
                          << "Member Kind\n";
                std::cout << "    -------------------------------------------------------------------------------------------------------------\n";

                for (size_t m = 0; m < res.memberCount; ++m) {
                    const CompactMember& mem = res.members[m];
                    std::cout << "    " << std::left 
                              << std::setw(6)  << (std::string("[") + std::to_string(m) + "]")
                              << std::setw(24) << (mem.name ? mem.name : "<unnamed>")
                              << std::setw(10) << (std::to_string(mem.offset) + " B")
                              << std::setw(10) << (std::to_string(mem.size) + " B")
                              << std::setw(15) << printType(mem.scalarType, mem.rowCount, mem.colCount)
                              << std::setw(10) << mem.parentIndex
                              << memberKindToString(mem.kind) << "\n";
                }
                std::cout << "    -------------------------------------------------------------------------------------------------------------\n";
            }
        }
        std::cout << "-----------------------------------------------------------------------------------------------------------------\n";
        resIdx++;
    }

    if (resIdx == 0) {
        std::cout << "  (No resources or descriptors bound in this shader)\n";
    }
    std::cout << "=================================================================\n\n";
}

void writeContent(const char* filePath, const char* content) {
    std::ofstream file(filePath);

    if (file.is_open()) {
        file << content;
        file.close();
    } else {
        std::cerr << "Writing to file " << filePath << " failed.\n";
    }
}

int main() {
    Context* ctx = shadertool_create_context();
    if (!ctx) {
        std::cerr << "Failed to create context!\n";
        return 1;
    }
    std::cout << "[1] Context created successfully.\n";

    CompileResult result = shadertool_compile_shader(
        ctx,
        "testModule",
        "test.slang",
        "computeMain"
    );
    if (result.success) {
        std::cout << "[2] Shader Compilation SUCCESS!\n";
        std::cout << "    - SPIR-V Code Size: " << result.codeSize << " bytes\n";
        std::cout << "    - Dependencies Count: " << result.depFileCount << "\n";

        for (size_t i = 0; i < result.depFileCount; ++i) {
            std::cout << "      dep[" << i << "]: " << result.depFilePath[i] << "\n";
        }

        printReflectionInfo(result);
    } else {
        std::cerr << "[2] Shader Compilation FAILED!\n";
        std::cerr << "    - Error Code: " << static_cast<int>(result.errorInfo.code) << "\n";
        std::cerr << "    - File: " << (result.errorInfo.filePath ? result.errorInfo.filePath : "N/A") << "\n";
        std::cerr << "    - Message: " << (result.errorInfo.message ? result.errorInfo.message : "N/A") << "\n";
    }

    shadertool_free_compile_result(&result);
    std::cout << "[3] CompileResult memory freed.\n";

    shadertool_destroy_context(ctx);
    std::cout << "[4] Context destroyed.\n";

    return 0;
}
