/**
 * @file rule_expression_parser.hh
 * @brief Parser for converting JSON rule expressions to AST
 * 
 * This parser converts JSON-based rule definitions into executable
 * Abstract Syntax Trees for musical constraint compilation.
 */

#ifndef RULE_EXPRESSION_PARSER_HH
#define RULE_EXPRESSION_PARSER_HH

#include "rule_ast.hh"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <unordered_map>

namespace DynamicRules {

/**
 * @brief Exception for rule parsing errors
 */
class RuleParseException : public std::exception {
private:
    std::string message_;
    
public:
    RuleParseException(const std::string& msg) : message_("Rule Parse Error: " + msg) {}
    const char* what() const noexcept override { return message_.c_str(); }
};

/**
 * @brief Parser for converting JSON expressions to AST
 */
class RuleExpressionParser {
private:
    // Mapping of string operators to AST node types
    static const std::unordered_map<std::string, ASTNodeType> binary_operators_;
    static const std::unordered_map<std::string, ASTNodeType> unary_operators_;
    static const std::unordered_map<std::string, ASTNodeType> function_types_;
    
public:
    /**
     * @brief Parse a complete rule from JSON
     */
    static std::unique_ptr<RuleAST> parse_rule(const nlohmann::json& rule_json);
    
    /**
     * @brief Parse an expression tree from JSON
     */
    static std::unique_ptr<ASTNode> parse_expression(const nlohmann::json& expr_json);
    
private:
    /**
     * @brief Parse binary operator expressions
     */
    static std::unique_ptr<ASTNode> parse_binary_operator(
        const nlohmann::json& expr_json, ASTNodeType op_type);
    
    /**
     * @brief Parse unary operator expressions  
     */
    static std::unique_ptr<ASTNode> parse_unary_operator(
        const nlohmann::json& expr_json, ASTNodeType op_type);
    
    /**
     * @brief Parse function expressions
     */
    static std::unique_ptr<ASTNode> parse_function(
        const nlohmann::json& expr_json, const std::string& func_name);
    
    /**
     * @brief Parse conditional (if-then-else) expressions
     */
    static std::unique_ptr<ASTNode> parse_conditional(const nlohmann::json& expr_json);
    
    /**
     * @brief Parse logical expressions (and, or)
     */
    static std::unique_ptr<ASTNode> parse_logical(
        const nlohmann::json& expr_json, ASTNodeType logic_type);
    
    /**
     * @brief Parse variable references
     */
    static std::unique_ptr<ASTNode> parse_variable(const std::string& var_name);
    
    /**
     * @brief Parse voice access expressions (voice[n].pitch[i])
     */
    static std::unique_ptr<ASTNode> parse_voice_access(const std::string& access_expr);
    
    /**
     * @brief Parse position expressions in array indices (i, i+1, i-1, etc.)
     */
    static std::unique_ptr<ASTNode> parse_position_expression(const std::string& pos_expr);
    
    /**
     * @brief Parse constant values
     */
    static std::unique_ptr<ASTNode> parse_constant(const nlohmann::json& value_json);
    
    /**
     * @brief Determine if string is a variable reference
     */
    static bool is_variable(const std::string& str);
    
    /**
     * @brief Determine if string is a voice access expression
     */
    static bool is_voice_access(const std::string& str);
    
    /**
     * @brief Get operator precedence for expression parsing
     */
    static int get_precedence(ASTNodeType op_type);
    
    /**
     * @brief Validate rule JSON structure
     */
    static void validate_rule_json(const nlohmann::json& rule_json);
    
    /**
     * @brief Extract parameters from JSON object
     */
    static std::map<std::string, ASTValue> extract_parameters(const nlohmann::json& params_json);
    
    // NEW: String expression parsing methods
    
    /**
     * @brief Check if string contains operators that need parsing
     */
    static bool contains_operators(const std::string& expr);
    
    /**
     * @brief Parse complex string expressions with operators
     */
    static std::unique_ptr<ASTNode> parse_string_expression(const std::string& expr);
    
    /**
     * @brief Tokenize expression string into components
     */
    static std::vector<std::string> tokenize_expression(const std::string& expr);
    
    /**
     * @brief Parse tokenized expression into AST
     */
    static std::unique_ptr<ASTNode> parse_tokens(const std::vector<std::string>& tokens);
    
    /**
     * @brief Parse single token (number, variable, or voice access)
     */
    static std::unique_ptr<ASTNode> parse_single_token(const std::string& token);
};

} // namespace DynamicRules

#endif // RULE_EXPRESSION_PARSER_HH