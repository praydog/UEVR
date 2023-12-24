#pragma once

#include <memory>

#include <sdkgenny/Sdk.hpp>

namespace sdk {
class UStruct;
class UClass;
class FProperty;
}

class SDKDumper {
public:
    virtual ~SDKDumper() = default;

public:
    static void dump();

private:
    void dump_internal();

private:
    void initialize_sdk();
    void initialize_boilerplate_classes();
    void initialize_tarray();
    void initialize_uobject();
    void initialize_ustruct();
    void initialize_uobject_array();
    void initialize_fname();
    void populate_sdk();
    void write_sdk();
    
private:
    sdkgenny::Struct* get_or_generate_struct(sdk::UStruct* ustruct);
    sdkgenny::Namespace* get_or_generate_namespace_chain(sdk::UStruct* uclass);
    void generate_inheritance(sdkgenny::Struct* s, sdk::UStruct* ustruct);
    void generate_properties(sdkgenny::Struct* s, sdk::UStruct* ustruct);
    sdkgenny::Variable* generate_property(sdkgenny::Struct* s, sdk::FProperty* fprop);
    void generate_functions(sdkgenny::Struct* s, sdk::UStruct* ustruct);

private:
    std::unique_ptr<sdkgenny::Sdk> m_sdk{};

    struct ExtraSdkClass {
        std::string name{};
        std::string header{};
        std::optional<std::string> source{};
    };

    std::vector<ExtraSdkClass> m_extra_classes{};
};