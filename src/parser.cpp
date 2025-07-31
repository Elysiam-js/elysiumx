// elysiumx/src/parser.cpp

#include "elysiumx/parser.hpp"
#include <fstream>
#include <iostream>
#include <regex>
#include <filesystem>
#include <sstream>
#include <algorithm> // For std::min
#include <vector>

namespace elysiumx {

Parser::Parser(std::string base_path) : base_path_(std::move(base_path)) {}

std::string Parser::readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

std::string Parser::stripComments(std::string content) {
    std::string result;
    std::istringstream iss(content);
    std::string line;
    bool in_multiline_comment = false;
    bool in_html_comment = false;

    while (std::getline(iss, line)) {
        size_t pos = 0;
        std::string line_content;
        
        while (pos < line.length()) {
            if (in_multiline_comment) {
                size_t end_comment_pos = line.find("*/", pos);
                if (end_comment_pos != std::string::npos) {
                    in_multiline_comment = false;
                    pos = end_comment_pos + 2;
                } else {
                    break;
                }
            } else if (in_html_comment) {
                size_t end_comment_pos = line.find("-->", pos);
                if (end_comment_pos != std::string::npos) {
                    in_html_comment = false;
                    pos = end_comment_pos + 3;
                } else {
                    break;
                }
            } else {
                size_t multi_comment_pos = line.find("/*", pos);
                size_t html_comment_pos = line.find("<!--", pos);
                size_t single_line_comment_pos = line.find("//", pos);

                size_t first_comment_pos = std::min({single_line_comment_pos, multi_comment_pos, html_comment_pos});

                if (first_comment_pos == std::string::npos) {
                    line_content += line.substr(pos);
                    break;
                }
                
                line_content += line.substr(pos, first_comment_pos - pos);
                
                if (first_comment_pos == single_line_comment_pos) {
                    goto next_line;
                } else if (first_comment_pos == multi_comment_pos) {
                    in_multiline_comment = true;
                    pos = first_comment_pos + 2;
                } else if (first_comment_pos == html_comment_pos) {
                    in_html_comment = true;
                    pos = first_comment_pos + 4;
                }
            }
        }
    next_line:;
        if (!line_content.empty()) {
            result += line_content + "\n";
        }
    }
    return result;
}

std::string Parser::extractTagContent(const std::string& content, const std::string& tag_name) {
    std::regex tag_regex("<" + tag_name + "[^>]*>([\\s\\S]*?)<\\/" + tag_name + ">");
    std::smatch match;
    if (std::regex_search(content, match, tag_regex)) {
        return match[1].str();
    }
    return "";
}

// BUG FIX: The function must populate the `components_` map.
// Changing its return type and introducing recursion in parseFile was incorrect.
// This is the corrected version.
void Parser::parseImports(const std::string& import_content, const std::string& current_file_path) {
    std::regex import_regex("<(\\w+)\\s+@src=\"([^\"]+)\"\\s*/>");
    auto imports_begin = std::sregex_iterator(import_content.begin(), import_content.end(), import_regex);
    auto imports_end = std::sregex_iterator();

    for (std::sregex_iterator i = imports_begin; i != imports_end; ++i) {
        std::smatch match = *i;
        std::string component_name = match[1].str();
        std::string relative_path = match[2].str();
        
        std::filesystem::path current_path(current_file_path);
        std::filesystem::path component_path = current_path.parent_path() / (relative_path + ".elx");
        
        // This is the critical logic that was removed and is now restored:
        std::string component_file_content = readFile(component_path.string());
        std::string app_content = extractTagContent(component_file_content, "app");
        std::string style_content = extractTagContent(component_file_content, "style");

        components_[component_name] = {component_path.string(), stripComments(app_content), stripComments(style_content)};
        aggregated_styles_ += components_[component_name].style_content;
    }
}

std::map<std::string, std::string> Parser::parseProps(const std::string& props_string) {
    std::map<std::string, std::string> props;
    std::regex prop_regex("(\\w+)\\s*=\\s*\"([^\"]*)\"");
    auto props_begin = std::sregex_iterator(props_string.begin(), props_string.end(), prop_regex);
    auto props_end = std::sregex_iterator();

    for (std::sregex_iterator i = props_begin; i != props_end; ++i) {
        std::smatch match = *i;
        props[match[1].str()] = match[2].str();
    }
    
    return props;
}

std::string Parser::renderComponent(std::string template_content, const std::map<std::string, std::string>& props, const std::string& child_content) {
    std::string rendered = template_content;

    if (!child_content.empty()) {
        rendered = std::regex_replace(rendered, std::regex("\\{children\\}"), child_content);
    }

    std::regex dynamic_attr_regex("(\\w+)=\\{([^}]+)\\}");
    std::smatch match;
    auto search_start = rendered.cbegin();
    std::string processed_render;
    while(std::regex_search(search_start, rendered.cend(), match, dynamic_attr_regex)) {
        processed_render += match.prefix().str();
        std::string attr_name = match[1].str();
        std::string expression = match[2].str();
        
        std::regex concat_regex("\"([^\"]*)\"\\s*\\+\\s*props\\.(\\w+)");
        std::smatch concat_match;
        if(std::regex_search(expression, concat_match, concat_regex)) {
            std::string literal = concat_match[1].str();
            std::string prop_name = concat_match[2].str();
            if (props.count(prop_name)) {
                processed_render += attr_name + "=\"" + literal + props.at(prop_name) + "\"";
            }
        } else {
            std::regex prop_regex("props\\.(\\w+)");
            std::smatch prop_match;
            if(std::regex_search(expression, prop_match, prop_regex)) {
                std::string prop_name = prop_match[1].str();
                if (props.count(prop_name)) {
                    processed_render += attr_name + "=\"" + props.at(prop_name) + "\"";
                }
            }
        }
        search_start = match.suffix().first;
    }
    processed_render += std::string(search_start, rendered.cend());
    rendered = processed_render;

    for (const auto& pair : props) {
        rendered = std::regex_replace(rendered, std::regex("\\{props\\." + pair.first + "\\}"), pair.second);
    }
    
    return rendered;
}

std::string Parser::processAppContent(std::string content) {
    while (true) {
        bool changed_in_iteration = false;
        for (const auto& [name, component] : components_) {
            std::regex child_tag_regex("<" + name + "([^>]*)>([\\s\\S]*?)<\\/" + name + ">");
            auto child_matches_begin = std::sregex_iterator(content.begin(), content.end(), child_tag_regex);
            auto child_matches_end = std::sregex_iterator();

            std::string temp_content;
            auto last_match_end = content.cbegin();

            for (std::sregex_iterator i = child_matches_begin; i != child_matches_end; ++i) {
                std::smatch match = *i;
                temp_content.append(last_match_end, match.prefix().second);
                
                std::string attrs = match[1].str();
                std::string child = match[2].str();
                auto props = parseProps(attrs);
                std::string rendered_component = renderComponent(component.app_content, props, child);
                temp_content.append(rendered_component);

                last_match_end = match.suffix().first;
                changed_in_iteration = true;
            }
            temp_content.append(last_match_end, content.cend());
            content = temp_content;

            std::regex self_closing_tag_regex("<" + name + "\\s+@props=\\{([\\s\\S]*?)\\}\\s*\\/>");
            auto self_closing_matches_begin = std::sregex_iterator(content.begin(), content.end(), self_closing_tag_regex);
            auto self_closing_matches_end = std::sregex_iterator();

            temp_content.clear();
            last_match_end = content.cbegin();

            for (std::sregex_iterator i = self_closing_matches_begin; i != self_closing_matches_end; ++i) {
                std::smatch match = *i;
                temp_content.append(last_match_end, match.prefix().second);
                
                std::string props_str = match[1].str();
                auto props = parseProps(props_str);
                std::string rendered_component = renderComponent(component.app_content, props);
                temp_content.append(rendered_component);
                
                last_match_end = match.suffix().first;
                changed_in_iteration = true;
            }
            temp_content.append(last_match_end, content.cend());
            content = temp_content;
        }
        if (!changed_in_iteration) {
            break;
        }
    }
    return content;
}

std::string Parser::resolvePath(const std::string& relative_path) {
    return (std::filesystem::path(base_path_) / relative_path).string();
}

// BUG FIX: Removed the incorrect recursive call to parseFile.
// The correct logic is to call parseImports, which populates the parser's state,
// and then proceed with processing the current file's content.
std::string Parser::parseFile(const std::string& file_path) {
    std::string content = readFile(file_path);
    
    std::string style_content = extractTagContent(content, "style");

    aggregated_styles_ += stripComments(style_content);

    std::string reset_style = R"(    body {
        margin: 0;
        padding: 0;
        box-sizing: border-box;
        font-family: sans-serif;
        font-size: 16px;
        line-height: 1.5;
        color: #000;
        background-color: #fff;

        /* Remove default list styles */
        list-style: none;

        /* Remove default link styles */
        text-decoration: none;

        /* Ensure consistent scroll behavior */
        scroll-behavior: smooth;

        /* Smooth font rendering */
        -webkit-font-smoothing: antialiased;
        -moz-osx-font-smoothing: grayscale;

        /* Disable tap highlight on mobile */
        -webkit-tap-highlight-color: transparent;
    })";

    std::string import_content = extractTagContent(content, "import");
    
    if (!import_content.empty()) {
        // This is the correct procedure: parse the imports and load them
        // into the current parser's state.
        parseImports(import_content, file_path);
    }

    std::string app_content = extractTagContent(content, "app");

    // processAppContent will now work because `components_` has been populated by parseImports.
    std::string processed_content = processAppContent(stripComments(app_content));
    
    processed_content = std::regex_replace(processed_content, std::regex("^\\s+|\\s+$"), "");
    processed_content = std::regex_replace(processed_content, std::regex("\\n"), "");
    processed_content = std::regex_replace(processed_content, std::regex(">\\s+<"), "><");
    processed_content = std::regex_replace(processed_content, std::regex("\\s{2,}"), " ");

    return "<!DOCTYPE html>\n<html>\n<head>\n<style>\n" + aggregated_styles_ + reset_style + "\n</style>\n</head>\n<body>\n" + processed_content + "\n</body>\n</html>";
}

} // namespace elysiumx