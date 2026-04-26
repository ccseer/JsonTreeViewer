#include <simdjson.h>

#include <iostream>

int main()
{
    const char* json = R"({"name": "amet", "value": 123})";
    simdjson::ondemand::parser parser;
    simdjson::padded_string padded(json, strlen(json));
    auto doc = parser.iterate(padded);

    auto obj = doc.get_object();
    for (auto field : obj) {
        auto key = field.unescaped_key();
        auto val = field.value();

        auto tok = val.raw_json_token();
        auto raw = val.raw_json();

        std::cout << "Key: " << key << std::endl;
        std::cout << "  raw_json_token: [" << tok << "] size=" << tok.size()
                  << std::endl;
        if (!raw.error()) {
            std::cout << "  raw_json: [" << raw.value()
                      << "] size=" << raw.value().size() << std::endl;
        }
        std::cout << std::endl;
    }
    return 0;
}
