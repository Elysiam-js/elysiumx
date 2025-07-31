
#ifndef ELYSIUMX_PARSER_HPP
#define ELYSIUMX_PARSER_HPP

#include <string>
#include <map>
#include <filesystem>

namespace elysiumx {

class Parser {
public:
    explicit Parser(std::string base_path);
    std::string parseFile(const std::string& file_path);

private:
    struct Component {
        std::string path;
        std::string app_content;
        std::string style_content;
    };

    std::string base_path_;
    std::map<std::string, Component> components_;
    std::string aggregated_styles_;

    std::string readFile(const std::string& path);
    std::string stripComments(std::string content);
    std::string extractTagContent(const std::string& content, const std::string& tag_name);
    void parseImports(const std::string& import_content, const std::string& current_file_path);
    std::string processAppContent(std::string content);
    std::string resolvePath(const std::string& relative_path);
    std::map<std::string, std::string> parseProps(const std::string& props_string);
    std::string renderComponent(std::string template_content, const std::map<std::string, std::string>& props, const std::string& child_content = "");
};

} // namespace elysiumx

#endif // ELYSIUMX_PARSER_HPP
