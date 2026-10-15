#include <cstdint>
#include <shadertool/shadertool.h>
#include <slang-com-ptr.h>
#include <slang.h>
#include "shadertool/shadertool_types.h"

#include <array>
#include <fstream>
#include <iostream>
#include <string>
#include <iterator>
#include <cstring>
#include <vector>

struct Context {
    Slang::ComPtr<slang::IGlobalSession> globalSession;
};

namespace {
    namespace fileio {
        std::string readSourceString(const char* sourcePath) {
            std::ifstream file(sourcePath);
            if (!file.is_open()) {
                std::cerr << "Failed to open file: " << sourcePath << '\n';
                return "";
            }
        
            std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        
            return contents;
        }
        
        // void writeSpirvCode(slang::IBlob* spirvCode, const char* moduleName, const char* modulePath) {
        //     std::string spirvPath = static_cast<std::string>(moduleName) + ".spv";
        
        //     std::ofstream spirvFile(spirvPath, std::ios::out | std::ios::binary);
        //     if (!spirvFile.is_open()) {
        //         std::cerr << "Failed to open file: " << spirvPath << '\n';
        //         return;
        //     }
        
        //     const char* bufferPointer = reinterpret_cast<const char*>(spirvCode->getBufferPointer());
        //     size_t bufferSize = spirvCode->getBufferSize();
        
        //     spirvFile.write(bufferPointer, bufferSize);
        
        //     if (!spirvFile) {
        //         std::cerr << "Failed to write SPIR-V data to file: " << spirvPath << '\n';
        //     } else {
        //         std::cout << "Successfully wrote SPIR-V to: " << spirvPath << " (" << bufferSize << " bytes)\n";
        //     }
        // }
    };
    
    namespace error_util {
        static const char* duplicateString(const std::string& str) {
            if (str.empty()) return nullptr;
            char* buf = new char[str.size() + 1];
            std::memcpy(buf, str.c_str(), str.size() + 1);
            return buf;
        }
    
        static CompileResult makeError(
            ErrorCode code,
            const std::string& msg,
            const char* filePath = nullptr,
            int32_t line = -1,
            int32_t col = -1
        ) {
            CompileResult res = {};
            res.success = false;
            res.errorInfo.code = code;
            res.errorInfo.message = duplicateString(msg);
            res.errorInfo.filePath = duplicateString(filePath ? filePath : "");
            res.errorInfo.line = line;
            res.errorInfo.column = col;
            return res;
        }
    }
    
    namespace compile {
        inline constexpr ErrorInfo kSuccessInfo = {
            .code = SHADERTOOL_OK,
            .message = nullptr,
            .filePath = nullptr,
            .line = -1,
            .column = -1
        };
        
        static Slang::ComPtr<slang::IGlobalSession> getGlobalSession() {
            static Slang::ComPtr<slang::IGlobalSession> s_globalSession;
            
            if (!s_globalSession) {
                slang::createGlobalSession(s_globalSession.writeRef());
            }
            
            return s_globalSession;
        }
        
        Slang::ComPtr<slang::ISession> createSession(Context* ctx) {
            slang::TargetDesc targetDesc = {};
            targetDesc.format = SLANG_SPIRV;
            targetDesc.profile = ctx->globalSession->findProfile("spirv_1_6");
        
            std::array<slang::CompilerOptionEntry, 1> options = {
                {
                    slang::CompilerOptionName::EmitSpirvDirectly,
                    {slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}
                }
            };
        
            slang::SessionDesc sessionDesc = {};
            sessionDesc.targets = &targetDesc;
            sessionDesc.targetCount = 1;
            sessionDesc.compilerOptionEntries = options.data();
            sessionDesc.compilerOptionEntryCount = options.size();
        
            Slang::ComPtr<slang::ISession> session;
            ctx->globalSession->createSession(sessionDesc, session.writeRef());
        
            return session;
        }
    
        Slang::ComPtr<slang::IModule> createModule(
            Context* ctx,
            slang::ISession* session,
            const char* moduleName,
            const char* modulePath,
            Slang::ComPtr<slang::IBlob>& outDiagnostics
        ) {
            std::string sourceString = fileio::readSourceString(modulePath);
            if (sourceString.empty()) {
                return nullptr;
            }
    
            Slang::ComPtr<slang::IModule> module;
            
            module = session->loadModuleFromSourceString(
                moduleName,
                modulePath,
                sourceString.c_str(),
                outDiagnostics.writeRef()
            );
    
            return module;
        }
    };
    
    namespace reflect {
        struct CategoryInfo {
            bool isPushConstant = false;
            bool isUniform = false;
            bool isDescriptorSlot = false;
        };

        void extractMemberRecursive(
            slang::TypeLayoutReflection* typeLayout,
            uint32_t currentOffset,
            int32_t parentIdx,
            std::vector<CompactMember>& outMembers
        ) {
            if (!typeLayout) return;
    
            uint32_t fieldCount = typeLayout->getFieldCount();
            for (uint32_t i = 0; i < fieldCount; ++i) {
                slang::VariableLayoutReflection* fieldVar = typeLayout->getFieldByIndex(i);
                slang::TypeLayoutReflection* fieldType = fieldVar->getTypeLayout();
    
                const char* rawName = fieldVar->getName();
    
                CompactMember member = {};
                member.name = error_util::duplicateString(rawName ? rawName : "");
                member.offset = currentOffset + static_cast<uint32_t>(fieldVar->getOffset(SLANG_PARAMETER_CATEGORY_UNIFORM));
                member.size = static_cast<uint32_t>(fieldType->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM));
                member.parentIndex = parentIdx;
                member.scalarType = SCALAR_KIND_NONE;

                slang::TypeReflection::Kind kind = fieldType->getKind();
                slang::TypeLayoutReflection* targetLayout = fieldType;
                member.arrayCount = 0;

                // Check if array, and swaps layout into element
                if (kind == slang::TypeReflection::Kind::Array) {
                    member.arrayCount = static_cast<uint32_t>(fieldType->getElementCount());
                    
                    targetLayout = fieldType->getElementTypeLayout();
                    if (targetLayout) {
                        kind = targetLayout->getKind();
                    }
                }

                if (kind == slang::TypeReflection::Kind::Pointer) {
                    member.kind = MEMBER_KIND_POINTER; // BDA
                } else if (kind == slang::TypeReflection::Kind::Struct) {
                    member.kind = MEMBER_KIND_STRUCT;  // Struct
                } else {
                    member.kind = MEMBER_KIND_PRIMITIVE; // Primitive
                }

                // If primitive, capture the scalar type, row, and column
                if (slang::TypeReflection* typeRef = targetLayout->getType()) {
                    slang::TypeReflection::ScalarType slangScalar = typeRef->getScalarType();
                    member.scalarType = static_cast<ScalarKind>(slangScalar);

                    uint32_t rows = typeRef->getRowCount();
                    uint32_t cols = typeRef->getColumnCount();

                    member.rowCount = static_cast<uint8_t>(rows > 0 ? rows : 1);
                    member.colCount = static_cast<uint8_t>(cols > 0 ? cols : 1);
                } else {
                    member.scalarType = SCALAR_KIND_NONE;
                    member.rowCount = 1;
                    member.colCount = 1;
                }

                int32_t myIndex = static_cast<int32_t>(outMembers.size());
                outMembers.push_back(member);

                if (member.kind == MEMBER_KIND_STRUCT) {
                    // Struct -> recursively parse members
                    extractMemberRecursive(targetLayout, member.offset, myIndex, outMembers);
                } else if (member.kind == MEMBER_KIND_POINTER) {
                    // BDA -> then start with offset 0, relative offset.
                    if (auto elemLayout = targetLayout->getElementTypeLayout()) {
                        extractMemberRecursive(elemLayout, 0, myIndex, outMembers);
                    }
                }
            }
        }

        CategoryInfo inspectVariableCategories(slang::VariableLayoutReflection* varLayout) {
            CategoryInfo info = {};
            if (!varLayout) return info;

            uint32_t categoryCount = varLayout->getCategoryCount();
            for (uint32_t i = 0; i < categoryCount; ++i) {
                auto category = varLayout->getCategoryByIndex(i);

                switch (category) {
                    case slang::ParameterCategory::PushConstantBuffer:
                        info.isPushConstant = true;
                        break;
                    case slang::ParameterCategory::Uniform:
                        info.isUniform = true;
                        break;
                    case slang::ParameterCategory::DescriptorTableSlot:
                        info.isDescriptorSlot = true;
                        break;
                    default:
                        break;
                }
            }

            return info;
        }

        bool parseResourceVariable(
            slang::VariableLayoutReflection* varLayout,
            CompactResource& outResource
        ) {
            if (!varLayout) return false;

            slang::TypeLayoutReflection* typeLayout = varLayout->getTypeLayout();
            if (!typeLayout) return false;

            // If the var is container
            slang::VariableLayoutReflection* bindingVar = varLayout;
            if (auto containerVar = typeLayout->getContainerVarLayout()) {
                bindingVar = containerVar;
            }

            // Check category of binding var
            CategoryInfo catInfo = inspectVariableCategories(bindingVar);
            
            slang::TypeReflection::Kind kind = typeLayout->getKind();
            bool isContainer = (kind == slang::TypeReflection::Kind::ConstantBuffer ||
                                kind == slang::TypeReflection::Kind::ParameterBlock ||
                                kind == slang::TypeReflection::Kind::TextureBuffer ||
                                kind == slang::TypeReflection::Kind::ShaderStorageBuffer);
            bool isResource = (kind == slang::TypeReflection::Kind::Resource);
            bool isSampler = (kind == slang::TypeReflection::Kind::SamplerState);

            // Check if push constant
            bool isPushConstant = catInfo.isPushConstant;
            bool isUniform = !isPushConstant && isContainer;
        
            if (!isPushConstant && !isUniform && !isResource && !isSampler) {
                return false;
            }

            // Set the CompactResource
            const char* varName = varLayout->getName();
            outResource.name = error_util::duplicateString(varName ? varName : "$Globals");

            if (isPushConstant) {
                outResource.kind = RESOURCE_KIND_PUSH_CONSTANT;
                outResource.set = 0;
                outResource.binding = 0;
            } else if (isUniform) {
                    outResource.kind = RESOURCE_KIND_UNIFORM;
                    outResource.set = static_cast<uint32_t>(bindingVar->getBindingSpace(slang::ParameterCategory::DescriptorTableSlot));
                    outResource.binding = static_cast<uint32_t>(bindingVar->getBindingIndex());
            } else if (isSampler) {
                outResource.kind = RESOURCE_KIND_SAMPLER;
                outResource.set = static_cast<uint32_t>(bindingVar->getBindingSpace(slang::ParameterCategory::DescriptorTableSlot));
                outResource.binding = static_cast<uint32_t>(bindingVar->getBindingIndex());
            } else if (isResource) {
                SlangResourceAccess access = typeLayout->getResourceAccess();
                outResource.kind = (access == SLANG_RESOURCE_ACCESS_READ_WRITE || access == SLANG_RESOURCE_ACCESS_WRITE) 
                                    ? RESOURCE_KIND_STORAGE_TEXTURE 
                                    : RESOURCE_KIND_TEXTURE;
                outResource.set = static_cast<uint32_t>(bindingVar->getBindingSpace(slang::ParameterCategory::DescriptorTableSlot));
                outResource.binding = static_cast<uint32_t>(bindingVar->getBindingIndex());
            }

            // Now fill up CompactMembers
            if (isPushConstant || isUniform) {
                slang::TypeLayoutReflection* elementLayout = typeLayout->getElementTypeLayout();
                slang::TypeLayoutReflection* targetLayout = elementLayout ? elementLayout : typeLayout;
        
                outResource.totalSize = static_cast<uint32_t>(targetLayout->getSize(slang::ParameterCategory::Uniform));
        
                std::vector<CompactMember> memberList;
                extractMemberRecursive(targetLayout, 0, -1, memberList);
        
                outResource.memberCount = static_cast<uint32_t>(memberList.size());
                if (outResource.memberCount > 0) {
                    outResource.members = new CompactMember[outResource.memberCount];
                    std::memcpy(outResource.members, memberList.data(), sizeof(CompactMember) * outResource.memberCount);
                } else {
                    outResource.members = nullptr;
                }
            } else {
                outResource.totalSize = 0;
                outResource.memberCount = 0;
                outResource.members = nullptr;
            }
        
            return true;
        }

        std::vector<CompactResource> extractProgramResources(slang::IComponentType* linkedProgram) {
            slang::ProgramLayout* programLayout = linkedProgram->getLayout();
            std::vector<CompactResource> resourceList;
            if (!programLayout) return resourceList;
    
            // Global Scope
            slang::VariableLayoutReflection* globalParams = programLayout->getGlobalParamsVarLayout();
            if (globalParams) {
                CompactResource res = {};
                if (parseResourceVariable(globalParams, res)) {
                    resourceList.push_back(res);
                } else {
                    slang::TypeLayoutReflection* globalType = globalParams->getTypeLayout();
                    if (globalType) {
                        uint32_t fieldCount = globalType->getFieldCount();
                        for (uint32_t i = 0; i < fieldCount; ++i) {
                            slang::VariableLayoutReflection* field = globalType->getFieldByIndex(i);
                            CompactResource fieldRes = {};
                            if (parseResourceVariable(field, fieldRes)) {
                                resourceList.push_back(fieldRes);
                            }
                        }
                    }
                }
            }
    
            // Entry Point Scope
            SlangInt entryCount = programLayout->getEntryPointCount();
            for (SlangInt e = 0; e < entryCount; ++e) {
                slang::EntryPointLayout* entryPoint = programLayout->getEntryPointByIndex(e);
                if (!entryPoint) continue;
    
                SlangInt paramCount = entryPoint->getParameterCount();
                for (SlangInt p = 0; p < paramCount; ++p) {
                    slang::VariableLayoutReflection* param = entryPoint->getParameterByIndex(p);
                    CompactResource res = {};
                    if (parseResourceVariable(param, res)) {
                        resourceList.push_back(res);
                    }
                }
            }
    
            return resourceList;
        }
    };
};

extern "C" {
    Context* shadertool_create_context(void) {
        auto* ctx = new Context();

        ctx->globalSession = compile::getGlobalSession();

        return ctx;
    }

    void shadertool_destroy_context(Context* ctx) {
        if (ctx) {
            delete ctx;
        }
    }

    CompileResult shadertool_compile_shader(
        Context* ctx,
        const char* moduleName,
        const char* modulePath,
        const char* entryPoint
    ) {
        // Check if ctx is valid
        if (!ctx || !ctx->globalSession) {
            return error_util::makeError(SHADERTOOL_ERROR_INTERNAL, "Invalid context, perhaps you forgot to initialize global context?");
        }

        // ----------------------------------------------------------------
        // Create session
        auto session = compile::createSession(ctx);
        if (!session) {
            return error_util::makeError(SHADERTOOL_ERROR_INTERNAL, "Failed to create Slang Session");
        }

        // ----------------------------------------------------------------
        // Create diagnostics
        Slang::ComPtr<slang::IModule> module;
        {
            Slang::ComPtr<slang::IBlob> diagnostics;
            module = compile::createModule(ctx, session.get(), moduleName, modulePath, diagnostics);
    
            // If module is not created, and diagnostics size is 0 -> File not found
            if (!module && (!diagnostics || diagnostics->getBufferSize() == 0)) {
                return error_util::makeError(SHADERTOOL_ERROR_FILE_NOT_FOUND, "Failed to open or read shader file", modulePath);
            }
            // If module is not created, and diagnostics size > 0 -> Syntax error
            if (!module || (diagnostics && diagnostics->getBufferSize() > 0)) {
                std::string diagMsg = diagnostics ? static_cast<const char*>(diagnostics->getBufferPointer()) : "Unknown module error";
                return error_util::makeError(SHADERTOOL_ERROR_SYNTAX_ERROR, diagMsg, modulePath);
            }
        }
        
        // ----------------------------------------------------------------
        // Create Entry point
        Slang::ComPtr<slang::IEntryPoint> entryPointComp;
        {
            SlangResult result = module->findEntryPointByName(entryPoint, entryPointComp.writeRef());
            if (SLANG_FAILED(result) || !entryPointComp) {
                std::string msg = std::string("Entry point not found: ") + entryPoint;
                return error_util::makeError(SHADERTOOL_ERROR_LINK_ERROR, msg, modulePath);
            }
        }
        
        // ----------------------------------------------------------------
        // Link program
        std::array<slang::IComponentType*, 2> componentTypes = {
            module,
            entryPointComp,
        };

        Slang::ComPtr<slang::IComponentType> composedProgram;
        {
            Slang::ComPtr<slang::IBlob> diagnostics;
            SlangResult result = session->createCompositeComponentType(
                componentTypes.data(),
                componentTypes.size(),
                composedProgram.writeRef(),
                diagnostics.writeRef()
            );

            if (SLANG_FAILED(result)) {
                std::string diagMsg = diagnostics ? static_cast<const char*>(diagnostics->getBufferPointer()) : "Linking failed";
                return error_util::makeError(SHADERTOOL_ERROR_LINK_ERROR, diagMsg, modulePath);
            }
        }

        Slang::ComPtr<slang::IComponentType> linkedProgram;
        {
            Slang::ComPtr<slang::IBlob> diagnostics;
            SlangResult result = composedProgram->link(
                linkedProgram.writeRef(),
                diagnostics.writeRef()
            );

            if (SLANG_FAILED(result)) {
                std::string diagMsg = diagnostics ? static_cast<const char*>(diagnostics->getBufferPointer()) : "Linking failed";
                return error_util::makeError(SHADERTOOL_ERROR_LINK_ERROR, diagMsg, modulePath);
            }
        }
        
        // ----------------------------------------------------------------
        // Create SPIR-V
        Slang::ComPtr<slang::IBlob> spirvCode;
        {
            Slang::ComPtr<slang::IBlob> diagnostics;
            SlangResult result = linkedProgram->getEntryPointCode(
                0,  // entryPointIndex
                0,  // targetIndex
                spirvCode.writeRef(),
                diagnostics.writeRef()
            );

            if (SLANG_FAILED(result)) {
                std::string diagMsg = diagnostics ? static_cast<const char*>(diagnostics->getBufferPointer()) : "Linking failed";
                return error_util::makeError(SHADERTOOL_ERROR_LINK_ERROR, diagMsg, modulePath);
            }
        }

        size_t spirvCodeSize = spirvCode->getBufferSize();
        uint8_t* spirvCodeBuf = new uint8_t[spirvCodeSize];
        std::memcpy(spirvCodeBuf, spirvCode->getBufferPointer(), spirvCodeSize);

        // ----------------------------------------------------------------
        // Get dependencies
        size_t depCount = static_cast<size_t>(module->getDependencyFileCount());

        const char** depPaths = nullptr;
        if (depCount > 0) {
            depPaths = new const char*[depCount];
            for (size_t i = 0; i < depCount; ++i) {
                const char* rawPath = module->getDependencyFilePath(i);
                depPaths[i] = error_util::duplicateString(rawPath ? rawPath : "");
            }
        }

        // Get reflection data
        std::vector<CompactResource> resourceList = reflect::extractProgramResources(linkedProgram.get());
        CompactResource* resourcesArray = nullptr;
        if (!resourceList.empty()) {
            resourcesArray = new CompactResource[resourceList.size() + 1]();
            std::memcpy(resourcesArray, resourceList.data(), sizeof(CompactResource) * resourceList.size());
        }
        
        // ----------------------------------------------------------------
        // Create CompileResult and return the object.
        CompileResult compileResult = {
            .success = true,
            .errorInfo = compile::kSuccessInfo,
            
            .codeBytes = spirvCodeBuf,
            .codeSize = spirvCodeSize,

            .depFilePath = depPaths,
            .depFileCount = depCount,

            .resources = resourcesArray,
        };

        return compileResult;
    }

    void shadertool_free_compile_result(CompileResult* result) {
        if (!result) return;

        // Delete errorInfo
        if (result->errorInfo.message) {
            delete[] result->errorInfo.message;
            result->errorInfo.message = nullptr;
        }
        if (result->errorInfo.filePath) {
            delete[] result->errorInfo.filePath;
            result->errorInfo.filePath = nullptr;
        }

        // Delete codeBytes
        if (result->codeBytes) {
            delete[] result->codeBytes;
            result->codeBytes = nullptr;
        }

        // Delete depFilePath
        if (result->depFilePath) {
            for (size_t i = 0; i < result->depFileCount; ++i) {
                if (result->depFilePath[i]) {
                    delete[] result->depFilePath[i];
                }
            }
            
            delete[] result->depFilePath;
            result->depFilePath = nullptr;
        }

        // Delete reflection info
        if (result->resources) {
            for (size_t i = 0; result->resources[i].name != nullptr; ++i) {
                const CompactResource& res = result->resources[i];
                delete[] res.name;
    
                if (res.members) {
                    for (size_t j = 0; j < res.memberCount; ++j) {
                        if (res.members[j].name)           delete[] res.members[j].name;
                    }
                    delete[] res.members;
                }
            }
            delete[] result->resources;
            result->resources = nullptr;
        }
    }
}
