#include <cctype>
#include <string>
#include <string_view>
#include <algorithm>
#include <regex>
#include "lru_cache.hpp"

struct CachedRegex {
    std::regex regex;
    bool       valid;
    CachedRegex(bool v) : valid(v) {}
    CachedRegex(const std::regex& r) : regex(r), valid(true) {}
};

static bool regex_match_cached(const std::string& input, const std::string& pattern) {
    static LRUCache<std::string, CachedRegex> regexCache(100);
    auto it = regexCache.find(pattern);
    if (it != regexCache.end()) {
        const auto& cached = it->second->second;
        if (!cached.valid) {
            return false;
        }
        return std::regex_match(input, cached.regex);
    }
    try {
        std::regex regex(pattern);
        const auto& cached = regexCache.emplace(pattern, regex).first->second->second;
        return std::regex_match(input, cached.regex);
    } catch (...) {
        regexCache.emplace(pattern, false);
        return false;
    }
}

static std::string_view trim_string_view(std::string_view str) {
    auto isBlank = [](int c) {
        return std::isspace(c);
    };

    auto head = std::find_if_not(str.begin(), str.end(), isBlank);
    auto tail = std::find_if_not(str.rbegin(), str.rend(), isBlank).base();
    if (head < tail) {
        return std::string_view(&*head, tail - head);
    }

    return {};
}

static std::string_view trim_outer_quotes(std::string_view str) {
    if (str.size() >= 2) {
        char head = str.front();
        char tail = str.back();
        if ((head == '"' && tail == '"') || (head == '\'' && tail == '\'')) {
            return str.substr(1, str.size() - 2);
        }
    }
    return str;
}

static size_t find_operator(std::string_view expr, std::string_view opt) {
    if (expr.size() < opt.size()) {
        return std::string::npos;
    }
    for (size_t index = 0, count = expr.size() - opt.size() + 1, depth = 0; index < count; ++index) {
        if (expr[index] == '(') {
            ++depth;
        } else if (expr[index] == ')') {
            --depth;
        } else if (depth == 0 && expr.compare(index, opt.size(), opt) == 0) {
            return index;
        }
    }
    return std::string::npos;
}

static bool split_expression(std::string_view expr, std::string_view& lhs, std::string_view& rhs, std::string_view& cmp) {
    const std::vector<std::string_view> cmps = {"==", "!=", "=~", "!~"};
    for (const auto& opt : cmps) {
        size_t pos = find_operator(expr, opt);
        if (pos != std::string::npos) {
            lhs = trim_outer_quotes(trim_string_view(expr.substr(0, pos)));
            rhs = trim_outer_quotes(trim_string_view(expr.substr(pos + opt.size())));
            cmp = opt;
            return true;
        }
    }
    return false;
}

static bool compare_strings(std::string_view lhs, std::string_view rhs, std::string_view cmp) {
    if (cmp == "==") {
        return lhs == rhs;
    }

    if (cmp == "!=") {
        return lhs != rhs;
    }

    if (cmp == "=~") {
        return regex_match_cached(std::string(lhs), std::string(rhs));
    }

    if (cmp == "!~") {
        return !regex_match_cached(std::string(lhs), std::string(rhs));
    }

    return false;
}

static bool is_bracketed(std::string_view expr) {
    size_t size = expr.size();
    if (size < 2 || expr.front() != '(' || expr.back() != ')') {
        return false;
    }

    for (size_t index = 1, count = size - 1, depth = 1; index < count; ++index) {
        char c = expr[index];
        if (c == ')') {
            if (--depth == 0) {
                return false;
            }
        } else if (c == '(') {
            ++depth;
        }
    }

    return true;
}

bool evaluate_logic_expression(std::string_view input) {
    auto expr = trim_string_view(input);
    if (expr.empty()) {
        return true;
    }

    while (is_bracketed(expr)) {
        expr = trim_string_view(expr.substr(1, expr.size() - 2));
    }

    if (expr.empty()) {
        return true;
    }

    auto pos = find_operator(expr, "||");
    if (pos != std::string::npos) {
        return evaluate_logic_expression(expr.substr(0, pos)) || evaluate_logic_expression(expr.substr(pos + 2));
    }

    pos = find_operator(expr, "&&");
    if (pos != std::string::npos) {
        return evaluate_logic_expression(expr.substr(0, pos)) && evaluate_logic_expression(expr.substr(pos + 2));
    }

    std::string_view lhs, rhs, cmp;
    if (split_expression(expr, lhs, rhs, cmp)) {
        return compare_strings(lhs, rhs, cmp);
    }

    return false;
}
