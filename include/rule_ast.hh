/**
 * @file rule_ast.hh
 * @brief Abstract Syntax Tree for dynamic musical rule expressions
 * 
 * This implements the AST structure for parsing and compiling dynamic
 * musical constraints from JSON expressions to Gecode propagators.
 */

#ifndef RULE_AST_HH
#define RULE_AST_HH

#include <memory>
#include <vector>
#include <string>
#include <variant>
#include <map>

namespace DynamicRules {

// Forward declarations
class RuleAST;
class GecodePropagator;

/**
 * @brief Types of AST nodes for rule expressions
 */
enum class ASTNodeType {
    // Leaf nodes
    VARIABLE,           // ?1, ?2, ?current, ?previous, voice[0].pitch[i]
    CONSTANT,           // 3, [0,2,4,5,7,9,11], true, false
    
    // Unary operators
    NOT,                // not, !
    ABS,                // abs()
    MOD,                // mod()
    
    // Binary operators  
    EQUALS,             // ==
    NOT_EQUALS,         // !=
    LESS_THAN,          // <
    LESS_EQUAL,         // <=
    GREATER_THAN,       // >
    GREATER_EQUAL,      // >=
    PLUS,               // +
    MINUS,              // -
    MULTIPLY,           // *
    DIVIDE,             // /
    
    // Set operations
    IN,                 // in
    NOT_IN,             // not_in
    
    // Logical operations
    AND,                // and, &&
    OR,                 // or, ||
    
    // Complex operations
    IF_THEN_ELSE,       // if_then_else
    ALL_DIFFERENT,      // all_different
    
    // Musical functions
    INTERVAL,           // interval between notes
    SCALE_MEMBER,       // scale membership
    VOICE_ACCESS,       // voice[n].pitch[i]
    
    // Quantifiers
    FORALL,             // forall
    EXISTS              // exists
};

/**
 * @brief Value types that can be stored in AST nodes
 */
using ASTValue = std::variant<
    int,                        // Integer constants
    double,                     // Float constants  
    bool,                       // Boolean constants
    std::string,                // Variable names, functions
    std::vector<int>,           // Integer arrays (scales, etc.)
    std::vector<double>         // Float arrays
>;

/**
 * @brief Context information for rule evaluation
 */
struct RuleContext {
    int current_voice = 0;
    int current_position = 0;
    int sequence_length = 0;
    int num_voices = 0;
    std::map<std::string, ASTValue> parameters;
};

/**
 * @brief Abstract base class for AST nodes
 */
class ASTNode {
public:
    ASTNodeType type;
    std::vector<std::unique_ptr<ASTNode>> children;
    ASTValue value;
    
    ASTNode(ASTNodeType t) : type(t) {}
    virtual ~ASTNode() = default;
    
    // Virtual methods for rule compilation
    virtual std::string to_string() const = 0;
    virtual bool is_leaf() const { return children.empty(); }
    
    // Helper methods
    void add_child(std::unique_ptr<ASTNode> child) {
        children.push_back(std::move(child));
    }
    
    template<typename T>
    void set_value(const T& val) {
        value = val;
    }
    
    template<typename T>
    T get_value() const {
        if (std::holds_alternative<T>(value)) {
            return std::get<T>(value);
        }
        throw std::runtime_error("Invalid value type access in AST node");
    }
};

/**
 * @brief Variable node (e.g., ?current, ?previous, voice[0].pitch[i])
 */
class VariableNode : public ASTNode {
public:
    std::string variable_name;
    
    VariableNode(const std::string& name) 
        : ASTNode(ASTNodeType::VARIABLE), variable_name(name) {
        set_value(name);
    }
    
    std::string to_string() const override {
        return variable_name;
    }
};

/**
 * @brief Constant value node
 */
class ConstantNode : public ASTNode {
public:
    template<typename T>
    ConstantNode(const T& val) : ASTNode(ASTNodeType::CONSTANT) {
        set_value(val);
    }
    
    std::string to_string() const override {
        return std::visit([](const auto& v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, int>) {
                return std::to_string(v);
            } else if constexpr (std::is_same_v<T, double>) {
                return std::to_string(v);
            } else if constexpr (std::is_same_v<T, bool>) {
                return v ? "true" : "false";
            } else if constexpr (std::is_same_v<T, std::string>) {
                return v;
            } else if constexpr (std::is_same_v<T, std::vector<int>>) {
                std::string result = "[";
                for (size_t i = 0; i < v.size(); ++i) {
                    if (i > 0) result += ",";
                    result += std::to_string(v[i]);
                }
                result += "]";
                return result;
            } else if constexpr (std::is_same_v<T, std::vector<double>>) {
                std::string result = "[";
                for (size_t i = 0; i < v.size(); ++i) {
                    if (i > 0) result += ",";
                    result += std::to_string(v[i]);
                }
                result += "]";
                return result;
            } else {
                return "unknown";
            }
        }, value);
    }
};

/**
 * @brief Binary operator node
 */
class BinaryOpNode : public ASTNode {
public:
    BinaryOpNode(ASTNodeType op_type) : ASTNode(op_type) {}
    
    std::string to_string() const override {
        if (children.size() != 2) return "invalid_binary_op";
        
        std::string op_str;
        switch (type) {
            case ASTNodeType::EQUALS: op_str = "=="; break;
            case ASTNodeType::NOT_EQUALS: op_str = "!="; break;
            case ASTNodeType::LESS_THAN: op_str = "<"; break;
            case ASTNodeType::LESS_EQUAL: op_str = "<="; break;
            case ASTNodeType::GREATER_THAN: op_str = ">"; break;
            case ASTNodeType::GREATER_EQUAL: op_str = ">="; break;
            case ASTNodeType::PLUS: op_str = "+"; break;
            case ASTNodeType::MINUS: op_str = "-"; break;
            case ASTNodeType::MULTIPLY: op_str = "*"; break;
            case ASTNodeType::DIVIDE: op_str = "/"; break;
            case ASTNodeType::IN: op_str = "in"; break;
            case ASTNodeType::NOT_IN: op_str = "not_in"; break;
            case ASTNodeType::AND: op_str = "&&"; break;
            case ASTNodeType::OR: op_str = "||"; break;
            default: op_str = "unknown_op"; break;
        }
        
        return "(" + children[0]->to_string() + " " + op_str + " " + children[1]->to_string() + ")";
    }
};

/**
 * @brief Unary operator node
 */
class UnaryOpNode : public ASTNode {
public:
    UnaryOpNode(ASTNodeType op_type) : ASTNode(op_type) {}
    
    std::string to_string() const override {
        if (children.size() != 1) return "invalid_unary_op";
        
        std::string op_str;
        switch (type) {
            case ASTNodeType::NOT: op_str = "not"; break;
            case ASTNodeType::ABS: op_str = "abs"; break;
            case ASTNodeType::MOD: op_str = "mod"; break;
            default: op_str = "unknown_unary_op"; break;
        }
        
        return op_str + "(" + children[0]->to_string() + ")";
    }
};

/**
 * @brief Function node with multiple arguments
 */
class FunctionNode : public ASTNode {
public:
    std::string function_name;
    
    FunctionNode(ASTNodeType func_type, const std::string& name) 
        : ASTNode(func_type), function_name(name) {}
    
    std::string to_string() const override {
        std::string result = function_name + "(";
        for (size_t i = 0; i < children.size(); ++i) {
            if (i > 0) result += ", ";
            result += children[i]->to_string();
        }
        result += ")";
        return result;
    }
};

/**
 * @brief Conditional (if-then-else) node
 */
class ConditionalNode : public ASTNode {
public:
    ConditionalNode() : ASTNode(ASTNodeType::IF_THEN_ELSE) {}
    
    std::string to_string() const override {
        if (children.size() != 3) return "invalid_conditional";
        
        return "if " + children[0]->to_string() + " then " + 
               children[1]->to_string() + " else " + children[2]->to_string();
    }
};

/**
 * @brief Voice access node (voice[n].pitch[i])
 */
class VoiceAccessNode : public ASTNode {
public:
    int voice_index;
    std::string property;  // "pitch", "rhythm", etc.
    std::unique_ptr<ASTNode> position_expr;
    
    VoiceAccessNode(int voice, const std::string& prop) 
        : ASTNode(ASTNodeType::VOICE_ACCESS), voice_index(voice), property(prop) {}
    
    void set_position(std::unique_ptr<ASTNode> pos) {
        position_expr = std::move(pos);
    }
    
    std::string to_string() const override {
        std::string result = "voice[" + std::to_string(voice_index) + "]." + property;
        if (position_expr) {
            result += "[" + position_expr->to_string() + "]";
        }
        return result;
    }
};

/**
 * @brief Complete rule AST with metadata
 */
class RuleAST {
public:
    std::string rule_id;
    std::string rule_type;
    std::string mode;           // "constraint" or "heuristic"
    int weight = 0;             // For heuristic rules
    std::string scope;          // "voice[0].pitches", etc.
    std::string description;
    
    std::unique_ptr<ASTNode> root;
    
    RuleAST(const std::string& id, const std::string& type) 
        : rule_id(id), rule_type(type) {}
    
    void set_root(std::unique_ptr<ASTNode> node) {
        root = std::move(node);
    }
    
    std::string to_string() const {
        std::string result = "Rule[" + rule_id + "]: ";
        if (root) {
            result += root->to_string();
        }
        return result;
    }
    
    bool is_constraint() const { return mode == "constraint"; }
    bool is_heuristic() const { return mode == "heuristic"; }
};

} // namespace DynamicRules

#endif // RULE_AST_HH