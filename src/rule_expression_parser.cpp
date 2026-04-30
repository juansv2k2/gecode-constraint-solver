/**
 * @file rule_expression_parser.cpp
 * @brief Implementation of JSON to AST rule parser
 */

#include "rule_expression_parser.hh"
#include <regex>
#include <iostream>

namespace DynamicRules {

// Static member initializations
const std::unordered_map<std::string, ASTNodeType> RuleExpressionParser::binary_operators_ = {
    {"==", ASTNodeType::EQUALS},
    {"!=", ASTNodeType::NOT_EQUALS},
    {"<", ASTNodeType::LESS_THAN},
    {"<=", ASTNodeType::LESS_EQUAL},
    {">", ASTNodeType::GREATER_THAN},
    {">=", ASTNodeType::GREATER_EQUAL},
    {"+", ASTNodeType::PLUS},
    {"-", ASTNodeType::MINUS},
    {"*", ASTNodeType::MULTIPLY},
    {"/", ASTNodeType::DIVIDE},
    {"in", ASTNodeType::IN},
    {"not_in", ASTNodeType::NOT_IN},
    {"and", ASTNodeType::AND},
    {"&&", ASTNodeType::AND},
    {"or", ASTNodeType::OR},
    {"||", ASTNodeType::OR}
};

const std::unordered_map<std::string, ASTNodeType> RuleExpressionParser::unary_operators_ = {
    {"not", ASTNodeType::NOT},
    {"!", ASTNodeType::NOT},
    {"abs", ASTNodeType::ABS},
    {"mod", ASTNodeType::MOD}
};

const std::unordered_map<std::string, ASTNodeType> RuleExpressionParser::function_types_ = {
    {"abs", ASTNodeType::ABS},
    {"mod", ASTNodeType::MOD},
    {"all_different", ASTNodeType::ALL_DIFFERENT},
    {"interval", ASTNodeType::INTERVAL},
    {"scale_member", ASTNodeType::SCALE_MEMBER}
};

std::unique_ptr<RuleAST> RuleExpressionParser::parse_rule(const nlohmann::json& rule_json) {
    validate_rule_json(rule_json);
    
    std::string id = rule_json.at("id").get<std::string>();
    std::string type = rule_json.at("type").get<std::string>();
    
    auto rule_ast = std::make_unique<RuleAST>(id, type);
    
    // Set basic properties
    rule_ast->mode = rule_json.value("mode", "constraint");
    rule_ast->weight = rule_json.value("weight", 0);
    rule_ast->scope = rule_json.value("scope", "");
    rule_ast->description = rule_json.value("description", "");
    
    // Parse the main expression
    if (rule_json.contains("expression")) {
        rule_ast->set_root(parse_expression(rule_json.at("expression")));
    } else {
        throw RuleParseException("Rule missing 'expression' field: " + id);
    }
    
    return rule_ast;
}

std::unique_ptr<ASTNode> RuleExpressionParser::parse_expression(const nlohmann::json& expr_json) {
    if (expr_json.is_string()) {
        std::string str_val = expr_json.get<std::string>();
        
        // Check if it's a variable
        if (is_variable(str_val)) {
            return parse_variable(str_val);
        }
        
        // NEW: Check for complex expressions with operators FIRST
        if (contains_operators(str_val)) {
            return parse_string_expression(str_val);
        }
        
        // Then check if it's a simple voice access
        if (is_voice_access(str_val)) {
            return parse_voice_access(str_val);
        }
        
        // Otherwise treat as constant string
        return std::make_unique<ConstantNode>(str_val);
    }
    
    if (expr_json.is_number_integer()) {
        return std::make_unique<ConstantNode>(expr_json.get<int>());
    }
    
    if (expr_json.is_number_float()) {
        return std::make_unique<ConstantNode>(expr_json.get<double>());
    }
    
    if (expr_json.is_boolean()) {
        return std::make_unique<ConstantNode>(expr_json.get<bool>());
    }
    
    if (expr_json.is_array()) {
        // Parse array as integer array constant
        std::vector<int> int_array;
        for (const auto& item : expr_json) {
            if (item.is_number_integer()) {
                int_array.push_back(item.get<int>());
            }
        }
        return std::make_unique<ConstantNode>(int_array);
    }
    
    if (!expr_json.is_object()) {
        throw RuleParseException("Invalid expression type");
    }
    
    // Handle object expressions
    if (expr_json.contains("operator")) {
        std::string op = expr_json.at("operator").get<std::string>();
        
        // Check for binary operators
        auto binary_it = binary_operators_.find(op);
        if (binary_it != binary_operators_.end()) {
            return parse_binary_operator(expr_json, binary_it->second);
        }
        
        // Check for unary operators (in operator field)
        auto unary_it = unary_operators_.find(op);
        if (unary_it != unary_operators_.end()) {
            return parse_unary_operator(expr_json, unary_it->second);
        }
        
        // Handle special operators
        if (op == "if_then_else") {
            return parse_conditional(expr_json);
        }
        
        if (op == "and" || op == "or") {
            ASTNodeType logic_type = (op == "and") ? ASTNodeType::AND : ASTNodeType::OR;
            return parse_logical(expr_json, logic_type);
        }
        
        if (op == "all_different") {
            return parse_function(expr_json, "all_different");
        }
    }
    
    if (expr_json.contains("function")) {
        std::string func_name = expr_json.at("function").get<std::string>();
        return parse_function(expr_json, func_name);
    }
    
    if (expr_json.contains("value")) {
        return parse_constant(expr_json.at("value"));
    }
    
    throw RuleParseException("Unrecognized expression structure");
}

std::unique_ptr<ASTNode> RuleExpressionParser::parse_binary_operator(
    const nlohmann::json& expr_json, ASTNodeType op_type) {
    
    auto binary_node = std::make_unique<BinaryOpNode>(op_type);
    
    if (!expr_json.contains("left") || !expr_json.contains("right")) {
        throw RuleParseException("Binary operator missing 'left' or 'right' operand");
    }
    
    binary_node->add_child(parse_expression(expr_json.at("left")));
    binary_node->add_child(parse_expression(expr_json.at("right")));
    
    return std::move(binary_node);
}

std::unique_ptr<ASTNode> RuleExpressionParser::parse_unary_operator(
    const nlohmann::json& expr_json, ASTNodeType op_type) {
    
    auto unary_node = std::make_unique<UnaryOpNode>(op_type);
    
    if (expr_json.contains("operand")) {
        unary_node->add_child(parse_expression(expr_json.at("operand")));
    } else if (expr_json.contains("args") && expr_json.at("args").is_array() && 
               expr_json.at("args").size() == 1) {
        unary_node->add_child(parse_expression(expr_json.at("args").at(0)));
    } else {
        throw RuleParseException("Unary operator missing operand");
    }
    
    return std::move(unary_node);
}

std::unique_ptr<ASTNode> RuleExpressionParser::parse_function(
    const nlohmann::json& expr_json, const std::string& func_name) {
    
    ASTNodeType func_type = ASTNodeType::ABS; // Default
    auto func_it = function_types_.find(func_name);
    if (func_it != function_types_.end()) {
        func_type = func_it->second;
    }
    
    auto func_node = std::make_unique<FunctionNode>(func_type, func_name);
    
    if (expr_json.contains("args") && expr_json.at("args").is_array()) {
        for (const auto& arg : expr_json.at("args")) {
            func_node->add_child(parse_expression(arg));
        }
    }
    
    return std::move(func_node);
}

std::unique_ptr<ASTNode> RuleExpressionParser::parse_conditional(const nlohmann::json& expr_json) {
    auto cond_node = std::make_unique<ConditionalNode>();
    
    if (!expr_json.contains("condition") || !expr_json.contains("then") || !expr_json.contains("else")) {
        throw RuleParseException("Conditional missing 'condition', 'then', or 'else'");
    }
    
    cond_node->add_child(parse_expression(expr_json.at("condition")));
    cond_node->add_child(parse_expression(expr_json.at("then")));
    cond_node->add_child(parse_expression(expr_json.at("else")));
    
    return std::move(cond_node);
}

std::unique_ptr<ASTNode> RuleExpressionParser::parse_logical(
    const nlohmann::json& expr_json, ASTNodeType logic_type) {
    
    auto logical_node = std::make_unique<BinaryOpNode>(logic_type);
    
    if (expr_json.contains("conditions") && expr_json.at("conditions").is_array()) {
        const auto& conditions = expr_json.at("conditions");
        if (conditions.size() < 2) {
            throw RuleParseException("Logical operator requires at least 2 conditions");
        }
        
        // For multiple conditions, create a binary tree
        auto current_node = parse_expression(conditions[0]);
        for (size_t i = 1; i < conditions.size(); ++i) {
            auto new_logical = std::make_unique<BinaryOpNode>(logic_type);
            new_logical->add_child(std::move(current_node));
            new_logical->add_child(parse_expression(conditions[i]));
            current_node = std::move(new_logical);
        }
        return current_node;
    } else if (expr_json.contains("left") && expr_json.contains("right")) {
        logical_node->add_child(parse_expression(expr_json.at("left")));
        logical_node->add_child(parse_expression(expr_json.at("right")));
        return std::move(logical_node);
    }
    
    throw RuleParseException("Logical operator missing operands");
}

std::unique_ptr<ASTNode> RuleExpressionParser::parse_variable(const std::string& var_name) {
    return std::make_unique<VariableNode>(var_name);
}

std::unique_ptr<ASTNode> RuleExpressionParser::parse_voice_access(const std::string& access_expr) {
    // Parse expressions like "voice[0].pitch[i]", "voice[1].rhythm[?current]", or "voice[v].pitch[i]" 
    std::regex voice_regex(R"(voice\[([^\]]+)\]\.(\w+)(?:\[([^\]]+)\])?)");
    std::smatch matches;
    
    if (std::regex_match(access_expr, matches, voice_regex)) {
        std::string voice_index_str = matches[1].str();
        std::string property = matches[2].str();
        
        // Handle both numeric indices (voice[0]) and pattern variables (voice[v])
        int voice_index = -1;
        if (std::all_of(voice_index_str.begin(), voice_index_str.end(), ::isdigit)) {
            voice_index = std::stoi(voice_index_str);
        } else {
            // Pattern variable like 'v' - will be substituted later
            voice_index = -1; // Placeholder for pattern variables
        }
        
        auto voice_node = std::make_unique<VoiceAccessNode>(voice_index, property);
        
        if (matches.size() > 3 && !matches[3].str().empty()) {
            std::string position_str = matches[3].str();
            
            // Parse position expression (could be variable or expression)
            if (is_variable(position_str)) {
                voice_node->set_position(std::make_unique<VariableNode>(position_str));
            } else if (std::all_of(position_str.begin(), position_str.end(), ::isdigit)) {
                voice_node->set_position(std::make_unique<ConstantNode>(std::stoi(position_str)));
            } else {
                // Parse arithmetic expressions like "i+1", "i-1", etc.
                voice_node->set_position(parse_position_expression(position_str));
            }
        }
        
        return std::move(voice_node);
    }
    
    throw RuleParseException("Invalid voice access expression: " + access_expr);
}

std::unique_ptr<ASTNode> RuleExpressionParser::parse_position_expression(const std::string& pos_expr) {
    // Handle common patterns: i, i+1, i-1, i+N, i-N
    if (pos_expr == "i") {
        return std::make_unique<VariableNode>("i");
    }
    
    // Check for arithmetic expressions with i
    std::regex arith_regex(R"(i\s*([+\-])\s*(\d+))");
    std::smatch matches;
    
    if (std::regex_match(pos_expr, matches, arith_regex)) {
        std::string op = matches[1].str();
        int operand = std::stoi(matches[2].str());
        
        // Create binary operation node: i + N or i - N
        ASTNodeType op_type = (op == "+") ? ASTNodeType::PLUS : ASTNodeType::MINUS;
        auto binary_node = std::make_unique<BinaryOpNode>(op_type);
        binary_node->add_child(std::make_unique<VariableNode>("i"));
        binary_node->add_child(std::make_unique<ConstantNode>(operand));
        
        return std::move(binary_node);
    }
    
    // If it doesn't match known patterns, treat as variable
    return std::make_unique<VariableNode>(pos_expr);
}

std::unique_ptr<ASTNode> RuleExpressionParser::parse_constant(const nlohmann::json& value_json) {
    return parse_expression(value_json);
}

bool RuleExpressionParser::is_variable(const std::string& str) {
    // Variables start with ? or are common variable names (including pattern variables)
    return str.front() == '?' || str == "current" || str == "previous" || 
           str == "i" || str == "length" || str == "pos" || str == "v";
}

bool RuleExpressionParser::is_voice_access(const std::string& str) {
    return str.find("voice[") == 0;
}

void RuleExpressionParser::validate_rule_json(const nlohmann::json& rule_json) {
    if (!rule_json.is_object()) {
        throw RuleParseException("Rule must be a JSON object");
    }
    
    if (!rule_json.contains("id")) {
        throw RuleParseException("Rule missing required 'id' field");
    }
    
    if (!rule_json.contains("type")) {
        throw RuleParseException("Rule missing required 'type' field");
    }
    
    if (!rule_json.contains("expression")) {
        throw RuleParseException("Rule missing required 'expression' field");
    }
}

std::map<std::string, ASTValue> RuleExpressionParser::extract_parameters(const nlohmann::json& params_json) {
    std::map<std::string, ASTValue> parameters;
    
    if (!params_json.is_object()) {
        return parameters;
    }
    
    for (const auto& [key, value] : params_json.items()) {
        if (value.is_number_integer()) {
            parameters[key] = value.get<int>();
        } else if (value.is_number_float()) {
            parameters[key] = value.get<double>();
        } else if (value.is_boolean()) {
            parameters[key] = value.get<bool>();
        } else if (value.is_string()) {
            parameters[key] = value.get<std::string>();
        } else if (value.is_array()) {
            std::vector<int> int_array;
            for (const auto& item : value) {
                if (item.is_number_integer()) {
                    int_array.push_back(item.get<int>());
                }
            }
            parameters[key] = int_array;
        }
    }
    
    return parameters;
}

// NEW methods for string expression parsing
bool RuleExpressionParser::contains_operators(const std::string& expr) {
    // Check for comparison operators
    std::vector<std::string> operators = {">=", "<=", "!=", "==", ">", "<", "+", "-", "*", "/", "and", "or", "not"};
    
    for (const auto& op : operators) {
        if (expr.find(op) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::unique_ptr<ASTNode> RuleExpressionParser::parse_string_expression(const std::string& expr) {
    // Tokenize the expression
    std::vector<std::string> tokens = tokenize_expression(expr);
    if (tokens.empty()) {
        throw RuleParseException("Empty expression: " + expr);
    }
    
    // Parse using operator precedence
    return parse_tokens(tokens);
}

std::vector<std::string> RuleExpressionParser::tokenize_expression(const std::string& expr) {
    std::vector<std::string> tokens;
    std::string current_token;
    
    for (size_t i = 0; i < expr.length(); ++i) {
        char c = expr[i];
        
        if (std::isspace(c)) {
            if (!current_token.empty()) {
                tokens.push_back(current_token);
                current_token.clear();
            }
        }
        // Parentheses are always separate tokens (check before two-char ops)
        else if (c == '(' || c == ')') {
            if (!current_token.empty()) {
                tokens.push_back(current_token);
                current_token.clear();
            }
            tokens.push_back(std::string(1, c));
        }
        // Handle multi-character operators
        else if (i < expr.length() - 1) {
            std::string two_char = expr.substr(i, 2);
            if (two_char == ">=" || two_char == "<=" || two_char == "!=" || two_char == "==") {
                if (!current_token.empty()) {
                    tokens.push_back(current_token);
                    current_token.clear();
                }
                tokens.push_back(two_char);
                ++i; // Skip next character
            }
            else {
                current_token += c;
            }
        }
        // Handle single-character operators
        else if (c == '>' || c == '<' || c == '+' || c == '-' || c == '*' || c == '/') {
            if (!current_token.empty()) {
                tokens.push_back(current_token);
                current_token.clear();
            }
            tokens.push_back(std::string(1, c));
        }
        else {
            current_token += c;
        }
    }
    
    if (!current_token.empty()) {
        tokens.push_back(current_token);
    }
    
    return tokens;
}

std::unique_ptr<ASTNode> RuleExpressionParser::parse_tokens(const std::vector<std::string>& tokens) {
    // Simple recursive descent parser for binary operations
    if (tokens.size() == 1) {
        return parse_single_token(tokens[0]);
    }

    // Handle function calls: funcname ( args... )
    if (tokens.size() >= 3 && tokens[1] == "(") {
        // Find matching closing paren
        int depth = 0;
        int close_idx = -1;
        for (size_t i = 1; i < tokens.size(); ++i) {
            if (tokens[i] == "(") depth++;
            else if (tokens[i] == ")") {
                depth--;
                if (depth == 0) { close_idx = (int)i; break; }
            }
        }
        if (close_idx != -1) {
            std::string func_name = tokens[0];
            auto func_it = function_types_.find(func_name);
            ASTNodeType func_type = (func_it != function_types_.end()) ? func_it->second : ASTNodeType::ABS;
            auto func_node = std::make_unique<FunctionNode>(func_type, func_name);
            // Parse the argument(s) inside parens
            std::vector<std::string> args(tokens.begin() + 2, tokens.begin() + close_idx);
            if (!args.empty()) {
                func_node->add_child(parse_tokens(args));
            }
            // If there are tokens after the closing paren, wrap in a binary op
            if (close_idx + 1 < (int)tokens.size()) {
                std::string op = tokens[close_idx + 1];
                auto bin_it = binary_operators_.find(op);
                if (bin_it != binary_operators_.end()) {
                    auto binary_node = std::make_unique<BinaryOpNode>(bin_it->second);
                    binary_node->add_child(std::move(func_node));
                    std::vector<std::string> right(tokens.begin() + close_idx + 2, tokens.end());
                    binary_node->add_child(parse_tokens(right));
                    return std::move(binary_node);
                }
            }
            return std::move(func_node);
        }
    }

    // Strip outer parens: ( expr )
    if (tokens.front() == "(" && tokens.back() == ")") {
        std::vector<std::string> inner(tokens.begin() + 1, tokens.end() - 1);
        if (!inner.empty()) return parse_tokens(inner);
    }

    // Parse with operator precedence (lower precedence first)
    // 1. Comparison operators (lowest precedence)
    std::vector<std::string> comparison_ops = {">=", "<=", "!=", "==", ">", "<"};
    
    for (size_t i = 1; i < tokens.size() - 1; ++i) {
        const std::string& token = tokens[i];
        
        // Check if this is a comparison operator
        if (std::find(comparison_ops.begin(), comparison_ops.end(), token) != comparison_ops.end()) {
            // Split tokens at this operator
            std::vector<std::string> left_tokens(tokens.begin(), tokens.begin() + i);
            std::vector<std::string> right_tokens(tokens.begin() + i + 1, tokens.end());
            
            // Create binary operation node
            auto binary_it = binary_operators_.find(token);
            if (binary_it != binary_operators_.end()) {
                auto binary_node = std::make_unique<BinaryOpNode>(binary_it->second);
                binary_node->add_child(parse_tokens(left_tokens));
                binary_node->add_child(parse_tokens(right_tokens));
                return std::move(binary_node);
            }
        }
    }
    
    // 2. Addition and subtraction operators (higher precedence than comparison)
    std::vector<std::string> additive_ops = {"+", "-"};
    
    for (size_t i = 1; i < tokens.size() - 1; ++i) {
        const std::string& token = tokens[i];
        
        // Check if this is an additive operator
        if (std::find(additive_ops.begin(), additive_ops.end(), token) != additive_ops.end()) {
            // Split tokens at this operator
            std::vector<std::string> left_tokens(tokens.begin(), tokens.begin() + i);
            std::vector<std::string> right_tokens(tokens.begin() + i + 1, tokens.end());
            
            // Create binary operation node
            auto binary_it = binary_operators_.find(token);
            if (binary_it != binary_operators_.end()) {
                auto binary_node = std::make_unique<BinaryOpNode>(binary_it->second);
                binary_node->add_child(parse_tokens(left_tokens));
                binary_node->add_child(parse_tokens(right_tokens));
                return std::move(binary_node);
            }
        }
    }
    
    // 3. Multiplication and division operators (highest precedence)
    std::vector<std::string> multiplicative_ops = {"*", "/"};
    
    for (size_t i = 1; i < tokens.size() - 1; ++i) {
        const std::string& token = tokens[i];
        
        // Check if this is a multiplicative operator
        if (std::find(multiplicative_ops.begin(), multiplicative_ops.end(), token) != multiplicative_ops.end()) {
            // Split tokens at this operator
            std::vector<std::string> left_tokens(tokens.begin(), tokens.begin() + i);
            std::vector<std::string> right_tokens(tokens.begin() + i + 1, tokens.end());
            
            // Create binary operation node
            auto binary_it = binary_operators_.find(token);
            if (binary_it != binary_operators_.end()) {
                auto binary_node = std::make_unique<BinaryOpNode>(binary_it->second);
                binary_node->add_child(parse_tokens(left_tokens));
                binary_node->add_child(parse_tokens(right_tokens));
                return std::move(binary_node);
            }
        }
    }

    // If no operators found, treat as single complex token
    if (!tokens.empty()) {
        return parse_single_token(tokens[0]);
    }
    
    throw RuleParseException("Unable to parse token sequence");
}

std::unique_ptr<ASTNode> RuleExpressionParser::parse_single_token(const std::string& token) {
    // Try to parse as number
    try {
        int int_val = std::stoi(token);
        return std::make_unique<ConstantNode>(int_val);
    } catch (...) {
        // Not an integer, continue
    }
    
    // Try to parse as voice access
    if (is_voice_access(token)) {
        return parse_voice_access(token);
    }
    
    // Try to parse as variable
    if (is_variable(token)) {
        return parse_variable(token);
    }
    
    // Default to constant
    return std::make_unique<ConstantNode>(token);
}

} // namespace DynamicRules