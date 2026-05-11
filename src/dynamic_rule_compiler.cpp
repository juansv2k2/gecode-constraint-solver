/**
 * @file dynamic_rule_compiler.cpp
 * @brief Implementation of dynamic rule compiler - Direct Gecode constraint posting
 */

#include "dynamic_rule_compiler.hh"
#include "gecode_cluster_integration.hh"
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <sstream>

using namespace Gecode;
using namespace GecodeClusterIntegration;

namespace DynamicRules {

namespace {

double mid_domain_value(const IntVar& var) {
    if (var.assigned()) {
        return static_cast<double>(var.val());
    }
    return 0.5 * (static_cast<double>(var.min()) + static_cast<double>(var.max()));
}

double read_array_value(const IntVarArray* vars, int idx) {
    if (!vars) {
        return 0.0;
    }
    if (idx < 0 || idx >= static_cast<int>(vars->size())) {
        return 0.0;
    }
    return mid_domain_value((*vars)[idx]);
}

int voice_pos_to_index(const ConstraintContext& ctx, int voice, int pos) {
    if (voice < 0 || pos < 0) {
        return -1;
    }
    return voice * ctx.sequence_length + pos;
}

double eval_numeric_expr(const ASTNode& node, const ConstraintContext& ctx, const HeuristicCandidateContext& candidate_ctx);

int eval_position_expr(const ASTNode& node, const HeuristicCandidateContext& candidate_ctx) {
    switch (node.type) {
        case ASTNodeType::CONSTANT: {
            const ConstantNode& cn = static_cast<const ConstantNode&>(node);
            return cn.get_value<int>();
        }
        case ASTNodeType::VARIABLE: {
            const VariableNode& vn = static_cast<const VariableNode&>(node);
            if (vn.variable_name == "i" || vn.variable_name == "pos" || vn.variable_name == "?current") {
                return candidate_ctx.position;
            }
            return candidate_ctx.position;
        }
        case ASTNodeType::PLUS:
        case ASTNodeType::MINUS:
        case ASTNodeType::MULTIPLY:
        case ASTNodeType::DIVIDE: {
            const BinaryOpNode& bn = static_cast<const BinaryOpNode&>(node);
            if (bn.children.size() != 2) {
                return candidate_ctx.position;
            }
            int lhs = eval_position_expr(*bn.children[0], candidate_ctx);
            int rhs = eval_position_expr(*bn.children[1], candidate_ctx);
            switch (node.type) {
                case ASTNodeType::PLUS: return lhs + rhs;
                case ASTNodeType::MINUS: return lhs - rhs;
                case ASTNodeType::MULTIPLY: return lhs * rhs;
                case ASTNodeType::DIVIDE: return rhs == 0 ? lhs : lhs / rhs;
                default: return candidate_ctx.position;
            }
        }
        default:
            return candidate_ctx.position;
    }
}

double eval_voice_access_numeric(const VoiceAccessNode& vn, const ConstraintContext& ctx, const HeuristicCandidateContext& candidate_ctx) {
    int voice = vn.voice_index >= 0 ? vn.voice_index : candidate_ctx.voice;
    int pos = candidate_ctx.position;
    if (vn.position_expr) {
        pos = eval_position_expr(*vn.position_expr, candidate_ctx);
    }

    if (voice == candidate_ctx.voice && pos == candidate_ctx.position &&
        (vn.property == "pitch" || vn.property == "pitches")) {
        return static_cast<double>(candidate_ctx.candidate_value);
    }

    int idx = voice_pos_to_index(ctx, voice, pos);
    if (vn.property == "pitch" || vn.property == "pitches") {
        return read_array_value(ctx.pitch_vars, idx);
    }
    if (vn.property == "rhythm" || vn.property == "rhythms") {
        return read_array_value(ctx.rhythm_vars, idx);
    }
    return 0.0;
}

bool eval_boolean_expr(const ASTNode& node, const ConstraintContext& ctx, const HeuristicCandidateContext& candidate_ctx) {
    switch (node.type) {
        case ASTNodeType::AND:
        case ASTNodeType::OR: {
            const BinaryOpNode& bn = static_cast<const BinaryOpNode&>(node);
            if (bn.children.size() != 2) {
                return false;
            }
            bool lhs = eval_boolean_expr(*bn.children[0], ctx, candidate_ctx);
            bool rhs = eval_boolean_expr(*bn.children[1], ctx, candidate_ctx);
            return node.type == ASTNodeType::AND ? (lhs && rhs) : (lhs || rhs);
        }
        case ASTNodeType::NOT: {
            if (node.children.empty()) {
                return false;
            }
            return !eval_boolean_expr(*node.children[0], ctx, candidate_ctx);
        }
        case ASTNodeType::EQUALS:
        case ASTNodeType::NOT_EQUALS:
        case ASTNodeType::LESS_THAN:
        case ASTNodeType::LESS_EQUAL:
        case ASTNodeType::GREATER_THAN:
        case ASTNodeType::GREATER_EQUAL: {
            const BinaryOpNode& bn = static_cast<const BinaryOpNode&>(node);
            if (bn.children.size() != 2) {
                return false;
            }
            double lhs = eval_numeric_expr(*bn.children[0], ctx, candidate_ctx);
            double rhs = eval_numeric_expr(*bn.children[1], ctx, candidate_ctx);
            switch (node.type) {
                case ASTNodeType::EQUALS: return lhs == rhs;
                case ASTNodeType::NOT_EQUALS: return lhs != rhs;
                case ASTNodeType::LESS_THAN: return lhs < rhs;
                case ASTNodeType::LESS_EQUAL: return lhs <= rhs;
                case ASTNodeType::GREATER_THAN: return lhs > rhs;
                case ASTNodeType::GREATER_EQUAL: return lhs >= rhs;
                default: return false;
            }
        }
        case ASTNodeType::IN:
        case ASTNodeType::NOT_IN: {
            const BinaryOpNode& bn = static_cast<const BinaryOpNode&>(node);
            if (bn.children.size() != 2) {
                return false;
            }
            int target = static_cast<int>(std::llround(eval_numeric_expr(*bn.children[0], ctx, candidate_ctx)));
            const ASTNode& right = *bn.children[1];
            if (right.type != ASTNodeType::CONSTANT) {
                return false;
            }
            const ConstantNode& cn = static_cast<const ConstantNode&>(right);
            std::vector<int> allowed;
            try {
                allowed = cn.get_value<std::vector<int>>();
            } catch (...) {
                return false;
            }
            bool found = std::find(allowed.begin(), allowed.end(), target) != allowed.end();
            return node.type == ASTNodeType::IN ? found : !found;
        }
        default:
            return eval_numeric_expr(node, ctx, candidate_ctx) != 0.0;
    }
}

double eval_numeric_expr(const ASTNode& node, const ConstraintContext& ctx, const HeuristicCandidateContext& candidate_ctx) {
    switch (node.type) {
        case ASTNodeType::CONSTANT: {
            const ConstantNode& cn = static_cast<const ConstantNode&>(node);
            try { return static_cast<double>(cn.get_value<int>()); } catch (...) {}
            try { return cn.get_value<double>(); } catch (...) {}
            try { return cn.get_value<bool>() ? 1.0 : 0.0; } catch (...) {}
            return 0.0;
        }
        case ASTNodeType::VARIABLE: {
            const VariableNode& vn = static_cast<const VariableNode&>(node);
            const std::string& name = vn.variable_name;
            if (name == "?current" || name == "current") {
                return static_cast<double>(candidate_ctx.candidate_value);
            }
            if (name == "?previous" || name == "previous") {
                int idx = voice_pos_to_index(ctx, candidate_ctx.voice, candidate_ctx.position - 1);
                return read_array_value(ctx.pitch_vars, idx);
            }
            if (!name.empty() && name.front() == '?' && name.size() > 1 && std::isdigit(name[1])) {
                int idx = std::stoi(name.substr(1)) - 1;
                return read_array_value(ctx.pitch_vars, idx);
            }
            return 0.0;
        }
        case ASTNodeType::VOICE_ACCESS: {
            const VoiceAccessNode& vn = static_cast<const VoiceAccessNode&>(node);
            return eval_voice_access_numeric(vn, ctx, candidate_ctx);
        }
        case ASTNodeType::PLUS:
        case ASTNodeType::MINUS:
        case ASTNodeType::MULTIPLY:
        case ASTNodeType::DIVIDE: {
            const BinaryOpNode& bn = static_cast<const BinaryOpNode&>(node);
            if (bn.children.size() != 2) {
                return 0.0;
            }
            double lhs = eval_numeric_expr(*bn.children[0], ctx, candidate_ctx);
            double rhs = eval_numeric_expr(*bn.children[1], ctx, candidate_ctx);
            switch (node.type) {
                case ASTNodeType::PLUS: return lhs + rhs;
                case ASTNodeType::MINUS: return lhs - rhs;
                case ASTNodeType::MULTIPLY: return lhs * rhs;
                case ASTNodeType::DIVIDE: return rhs == 0.0 ? 0.0 : lhs / rhs;
                default: return 0.0;
            }
        }
        case ASTNodeType::ABS: {
            if (node.children.empty()) {
                return 0.0;
            }
            return std::abs(eval_numeric_expr(*node.children[0], ctx, candidate_ctx));
        }
        case ASTNodeType::MOD: {
            if (node.children.size() != 2) {
                return 0.0;
            }
            double lhs = eval_numeric_expr(*node.children[0], ctx, candidate_ctx);
            double rhs = eval_numeric_expr(*node.children[1], ctx, candidate_ctx);
            if (rhs == 0.0) {
                return 0.0;
            }
            return std::fmod(lhs, rhs);
        }
        case ASTNodeType::IF_THEN_ELSE: {
            if (node.children.size() != 3) {
                return 0.0;
            }
            bool cond = eval_boolean_expr(*node.children[0], ctx, candidate_ctx);
            return cond ? eval_numeric_expr(*node.children[1], ctx, candidate_ctx)
                        : eval_numeric_expr(*node.children[2], ctx, candidate_ctx);
        }
        default:
            return eval_boolean_expr(node, ctx, candidate_ctx) ? 1.0 : 0.0;
    }
}

} // namespace

std::unique_ptr<CompiledConstraint> DynamicRuleCompiler::compile_from_json(const nlohmann::json& rule_json) {
    // Parse JSON to RuleAST
    RuleExpressionParser parser;
    std::unique_ptr<RuleAST> rule_ast = parser.parse_rule(rule_json);
    
    // Compile the AST directly - don't use the other compile_rule method
    auto compiled = std::make_unique<CompiledConstraint>(rule_ast->rule_id, rule_ast->description);
    compiled->weight = rule_ast->weight;
    compiled->priority = rule_ast->priority;

    RuleAST::Mode normalized_mode = rule_ast->normalized_mode();
    if (normalized_mode == RuleAST::Mode::HEUR_SWITCH) {
        compiled->is_heuristic = true;
        compiled->heuristic_mode = HeuristicMode::HEUR_SWITCH;
    } else if (normalized_mode == RuleAST::Mode::REAL_HEURISTIC) {
        compiled->is_heuristic = true;
        compiled->heuristic_mode = HeuristicMode::REAL_HEURISTIC;
    }
    
    // Create the constraint posting function with copied values
    std::string rule_id = rule_ast->rule_id;
    std::string rule_type = rule_ast->rule_type;
    std::string scope = rule_ast->scope;
    
    std::shared_ptr<RuleAST> shared_ast(rule_ast.release());
    const bool has_candidate_position = rule_json.contains("candidate_position") &&
        rule_json["candidate_position"].is_number_integer();
    const int candidate_position = has_candidate_position
        ? rule_json["candidate_position"].get<int>()
        : -1;
    const bool has_candidate_voice = rule_json.contains("candidate_voice") &&
        rule_json["candidate_voice"].is_number_integer();
    const int candidate_voice = has_candidate_voice
        ? rule_json["candidate_voice"].get<int>()
        : -1;
    std::vector<int> candidate_voices;
    if (rule_json.contains("candidate_voices") && rule_json["candidate_voices"].is_array()) {
        for (const auto& voice_json : rule_json["candidate_voices"]) {
            if (voice_json.is_number_integer()) {
                candidate_voices.push_back(voice_json.get<int>());
            }
        }
    }
    compiled->applies_to_position = candidate_position;
    compiled->applies_to_voice = candidate_voice;
    compiled->applies_to_voices = candidate_voices;

    compiled->post_constraint = [rule_id, rule_type, scope, ast = shared_ast](ConstraintContext& ctx) {
        try {
            if (ast->is_heuristic()) {
                // Heuristic modes are score-only and must not post constraints.
                return;
            }

            std::cout << "  🔧 Compiling rule: " << rule_id << " (type: " << rule_type << ")" << std::endl;
            if (ast->root) {
                std::cout << "  🔧 Expression type: " << static_cast<int>(ast->root->type) << std::endl;
            }
            
            // Handle different types of rules
            if (rule_type == "distinctness_constraint") {
                // Special handling for all_different constraints
                if (scope.find("voice[0].pitches") != std::string::npos) {
                    // Get pitch variables for voice 0
                    int voice_start = 0 * ctx.sequence_length;
                    IntVarArray voice_vars(*ctx.space, ctx.sequence_length);
                    for (int i = 0; i < ctx.sequence_length; ++i) {
                        voice_vars[i] = (*ctx.pitch_vars)[voice_start + i];
                    }
                    distinct(*ctx.space, voice_vars);
                    std::cout << "    ✅ Posted all_different constraint" << std::endl;
                }
            } else {
                // General expression-based constraints
                if (ast->root && ast->root->type == ASTNodeType::ALL_DIFFERENT) {
                    // Handle all_different function
                    const FunctionNode* func_node = dynamic_cast<const FunctionNode*>(ast->root.get());
                    if (func_node) {
                        post_function_constraint(*func_node, ctx);
                    }
                } else if (ast->root) {
                    // Compile general constraint - direct posting approach
                    compile_and_post_constraint(*ast->root, ctx);
                    std::cout << "    ✅ Posted general constraint" << std::endl;
                }
            }
            
        } catch (const std::exception& e) {
            throw RuleCompileException("Failed to compile rule '" + rule_id + "': " + e.what());
        }
    };

    if (compiled->is_heuristic) {
        int score_weight = compiled->weight;
        double direction_scale =
            (shared_ast && shared_ast->direction == "minimize") ? -1.0 : 1.0;
        if (compiled->heuristic_mode == HeuristicMode::HEUR_SWITCH) {
            compiled->score_candidate = [score_weight, direction_scale, ast = shared_ast,
                                         has_candidate_position, candidate_position,
                                         has_candidate_voice, candidate_voice, candidate_voices](
                const ConstraintContext& score_ctx, const HeuristicCandidateContext& candidate_ctx) {
                if (has_candidate_position && candidate_ctx.position != candidate_position) {
                    return 0.0;
                }
                if (has_candidate_voice && candidate_ctx.voice != candidate_voice) {
                    return 0.0;
                }
                if (!candidate_voices.empty() &&
                    std::find(candidate_voices.begin(), candidate_voices.end(), candidate_ctx.voice) == candidate_voices.end()) {
                    return 0.0;
                }
                if (!ast || !ast->root) {
                    return 0.0;
                }
                bool pass = eval_boolean_expr(*ast->root, score_ctx, candidate_ctx);
                return pass ? direction_scale * static_cast<double>(score_weight) : 0.0;
            };
        } else if (compiled->heuristic_mode == HeuristicMode::REAL_HEURISTIC) {
            compiled->score_candidate = [score_weight, direction_scale, ast = shared_ast,
                                         has_candidate_position, candidate_position,
                                         has_candidate_voice, candidate_voice, candidate_voices](
                const ConstraintContext& score_ctx, const HeuristicCandidateContext& candidate_ctx) {
                if (has_candidate_position && candidate_ctx.position != candidate_position) {
                    return 0.0;
                }
                if (has_candidate_voice && candidate_ctx.voice != candidate_voice) {
                    return 0.0;
                }
                if (!candidate_voices.empty() &&
                    std::find(candidate_voices.begin(), candidate_voices.end(), candidate_ctx.voice) == candidate_voices.end()) {
                    return 0.0;
                }
                if (!ast || !ast->root) {
                    return 0.0;
                }
                double raw_score = eval_numeric_expr(*ast->root, score_ctx, candidate_ctx);
                double scale = score_weight == 0 ? 1.0 : static_cast<double>(score_weight);
                return direction_scale * raw_score * scale;
            };
        }
    }
    
    return compiled;
}

void DynamicRuleCompiler::compile_and_post_constraint(const ASTNode& node, ConstraintContext& ctx) {
    std::cout << "🔧 Compiling constraint type: " << static_cast<int>(node.type) << std::endl;
    switch (node.type) {
        case ASTNodeType::EQUALS:
        case ASTNodeType::NOT_EQUALS:
        case ASTNodeType::LESS_THAN:
        case ASTNodeType::LESS_EQUAL:
        case ASTNodeType::GREATER_THAN:
        case ASTNodeType::GREATER_EQUAL: {
            const BinaryOpNode& binary_node = static_cast<const BinaryOpNode&>(node);
            std::cout << "📊 Posting binary comparison constraint" << std::endl;
            post_binary_comparison(binary_node, ctx);
            break;
        }
        
        case ASTNodeType::AND: {
            // For AND operations, post all sub-constraints
            const BinaryOpNode& binary_node = static_cast<const BinaryOpNode&>(node);
            if (binary_node.children.size() == 2) {
                compile_and_post_constraint(*binary_node.children[0], ctx);
                compile_and_post_constraint(*binary_node.children[1], ctx);
            }
            break;
        }
        
        case ASTNodeType::ALL_DIFFERENT: {
            const FunctionNode& func_node = static_cast<const FunctionNode&>(node);
            post_function_constraint(func_node, ctx);
            break;
        }
        
        case ASTNodeType::IN:
        case ASTNodeType::NOT_IN: {
            const BinaryOpNode& binary_node = static_cast<const BinaryOpNode&>(node);
            post_membership_constraint_from_binary(binary_node, ctx);
            break;
        }
        
        default:
            throw RuleCompileException("Unsupported constraint type for direct posting: " + std::to_string(static_cast<int>(node.type)));
    }
}

void DynamicRuleCompiler::post_binary_comparison(const BinaryOpNode& binary_node, ConstraintContext& ctx) {
    if (binary_node.children.size() != 2) {
        throw RuleCompileException("Binary comparison requires 2 operands");
    }
    
    std::cout << "🔍 Processing binary comparison with " << binary_node.children.size() << " operands" << std::endl;
    
    // Try to get left side as IntVar
    IntVar left_var;
    bool left_is_var = false;
    int left_value = 0;
    
    try {
        std::cout << "🔍 Compiling left side as IntVar (type: " << static_cast<int>(binary_node.children[0]->type) << ")" << std::endl;
        left_var = compile_arithmetic_to_var(*binary_node.children[0], ctx);
        left_is_var = true;
        std::cout << "✅ Left side compiled as IntVar" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "⚠️ Left side failed as IntVar: " << e.what() << ", trying as constant" << std::endl;
        // If not a variable, try as constant
        left_value = compile_arithmetic_to_int(*binary_node.children[0], ctx);
    }
    
    // Try to get right side
    IntVar right_var;
    bool right_is_var = false;
    int right_value = 0;
    
    try {
        std::cout << "🔍 Compiling right side as IntVar (type: " << static_cast<int>(binary_node.children[1]->type) << ")" << std::endl;
        right_var = compile_arithmetic_to_var(*binary_node.children[1], ctx);
        right_is_var = true;
        std::cout << "✅ Right side compiled as IntVar (arithmetic result)" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "⚠️ Right side failed as IntVar: " << e.what() << ", trying as constant" << std::endl;
        right_value = compile_arithmetic_to_int(*binary_node.children[1], ctx);
        std::cout << "✅ Right side compiled as constant: " << right_value << std::endl;
    }
    
    // Determine relation type
    IntRelType rel_type;
    switch (binary_node.type) {
        case ASTNodeType::EQUALS: rel_type = IRT_EQ; break;
        case ASTNodeType::NOT_EQUALS: rel_type = IRT_NQ; break;
        case ASTNodeType::LESS_THAN: rel_type = IRT_LE; break;
        case ASTNodeType::LESS_EQUAL: rel_type = IRT_LQ; break;
        case ASTNodeType::GREATER_THAN: rel_type = IRT_GR; break;
        case ASTNodeType::GREATER_EQUAL: rel_type = IRT_GQ; break;
        default:
            throw RuleCompileException("Unsupported comparison operator");
    }
    
    // Post the appropriate constraint
    if (left_is_var && right_is_var) {
        // var rel var
        std::cout << "🎯 Posting IntVar == IntVar constraint" << std::endl;
        std::cout << "🎯 Left var domain: [" << left_var.min() << ".." << left_var.max() << "]" << std::endl;
        std::cout << "🎯 Right var domain: [" << right_var.min() << ".." << right_var.max() << "]" << std::endl;
        rel(*ctx.space, left_var, rel_type, right_var);
    } else if (left_is_var && !right_is_var) {
        // var rel constant
        std::cout << "🎯 Posting IntVar == " << right_value << " constraint" << std::endl;
        std::cout << "🎯 Var domain: [" << left_var.min() << ".." << left_var.max() << "]" << std::endl;
        rel(*ctx.space, left_var, rel_type, right_value);
    } else if (!left_is_var && right_is_var) {
        // constant rel var - reverse the relation
        std::cout << "🎯 Posting " << left_value << " == IntVar constraint" << std::endl;
        std::cout << "🎯 Var domain: [" << right_var.min() << ".." << right_var.max() << "]" << std::endl;
        IntRelType reversed_rel = rel_type;
        switch (rel_type) {
            case IRT_LQ: reversed_rel = IRT_GR; break;
            case IRT_LE: reversed_rel = IRT_GQ; break;
            case IRT_GR: reversed_rel = IRT_LQ; break;
            case IRT_GQ: reversed_rel = IRT_LE; break;
            default: break; // EQ and NQ are symmetric
        }
        rel(*ctx.space, right_var, reversed_rel, left_value);
    } else {
        // constant rel constant - evaluate at compile time
        bool result = false;
        switch (rel_type) {
            case IRT_EQ: result = (left_value == right_value); break;
            case IRT_NQ: result = (left_value != right_value); break;
            case IRT_LQ: result = (left_value < right_value); break;
            case IRT_LE: result = (left_value <= right_value); break;
            case IRT_GR: result = (left_value > right_value); break;
            case IRT_GQ: result = (left_value >= right_value); break;
        }
        
        if (!result) {
            // Force failure by posting unsatisfiable constraint
            IntVar failure_var(*ctx.space, 0, 0);
            rel(*ctx.space, failure_var, IRT_EQ, 1);
        }
        // If result is true, no constraint needed (trivially satisfied)
    }
}

IntVar DynamicRuleCompiler::compile_arithmetic_to_var(const ASTNode& node, ConstraintContext& ctx) {
    switch (node.type) {
        case ASTNodeType::CONSTANT: {
            const ConstantNode& const_node = static_cast<const ConstantNode&>(node);
            int val = const_node.get_value<int>();
            return IntVar(*ctx.space, val, val);
        }

        case ASTNodeType::VARIABLE: {
            const VariableNode& var_node = static_cast<const VariableNode&>(node);
            return compile_variable_to_var(var_node, ctx);
        }
        
        case ASTNodeType::VOICE_ACCESS: {
            const VoiceAccessNode& voice_node = static_cast<const VoiceAccessNode&>(node);
            return compile_voice_access_to_var(voice_node, ctx);
        }
        
        case ASTNodeType::PLUS:
        case ASTNodeType::MINUS:
        case ASTNodeType::MULTIPLY:
        case ASTNodeType::DIVIDE: {
            // For arithmetic operations, create new variables and constraints
            const BinaryOpNode& binary_node = static_cast<const BinaryOpNode&>(node);
            if (binary_node.children.size() != 2) {
                throw RuleCompileException("Arithmetic operation requires 2 operands");
            }
            
            std::cout << "🧮 Compiling arithmetic operation type: " << static_cast<int>(node.type) << std::endl;
            std::cout << "🧮 Left child type: " << static_cast<int>(binary_node.children[0]->type) << std::endl;
            std::cout << "🧮 Right child type: " << static_cast<int>(binary_node.children[1]->type) << std::endl;
            
            // Try mixed variable/constant operations first
            try {
                IntVar left_var = compile_arithmetic_to_var(*binary_node.children[0], ctx);
                int right_val = compile_arithmetic_to_int(*binary_node.children[1], ctx);
                
                std::cout << "🧮 Successfully compiled as var + constant: " << right_val << std::endl;
                
                // Create result variable with a wide domain to accommodate weighted sums
                IntVar result(*ctx.space, -10000, 100000);
                if (node.type == ASTNodeType::PLUS) {
                    rel(*ctx.space, result, IRT_EQ, expr(*ctx.space, left_var + right_val));
                    std::cout << "🧮 Posted arithmetic constraint: result = left_var + " << right_val << std::endl;
                    std::cout << "🧮 Result var domain: [" << result.min() << ".." << result.max() << "]" << std::endl;
                } else if (node.type == ASTNodeType::MINUS) {
                    rel(*ctx.space, result, IRT_EQ, expr(*ctx.space, left_var - right_val));
                } else if (node.type == ASTNodeType::MULTIPLY) {
                    rel(*ctx.space, result, IRT_EQ, expr(*ctx.space, left_var * right_val));
                } else { // DIVIDE
                    rel(*ctx.space, result, IRT_EQ, expr(*ctx.space, left_var / right_val));
                }
                return result;
            } catch (const std::exception&) {
                // Right side is not a constant — fall back to var+var
                IntVar left = compile_arithmetic_to_var(*binary_node.children[0], ctx);
                IntVar right = compile_arithmetic_to_var(*binary_node.children[1], ctx);
                
                // Create result variable with a wide domain to accommodate weighted sums
                IntVar result(*ctx.space, -10000, 100000);
                if (node.type == ASTNodeType::PLUS) {
                    rel(*ctx.space, result, IRT_EQ, expr(*ctx.space, left + right));
                } else if (node.type == ASTNodeType::MINUS) {
                    rel(*ctx.space, result, IRT_EQ, expr(*ctx.space, left - right));
                } else if (node.type == ASTNodeType::MULTIPLY) {
                    rel(*ctx.space, result, IRT_EQ, expr(*ctx.space, left * right));
                } else { // DIVIDE - Note: requires special handling in Gecode
                    rel(*ctx.space, result, IRT_EQ, expr(*ctx.space, left / right));
                }
                return result;
            }
        }
        
        case ASTNodeType::ABS: {
            const FunctionNode& func_node = static_cast<const FunctionNode&>(node);
            if (func_node.children.size() != 1) {
                throw RuleCompileException("ABS function requires 1 argument");
            }
            
            IntVar arg = compile_arithmetic_to_var(*func_node.children[0], ctx);
            IntVar result(*ctx.space, 0, 1000); // ABS result is non-negative
            abs(*ctx.space, arg, result);
            return result;
        }
        
        case ASTNodeType::MOD: {
            const FunctionNode& func_node = static_cast<const FunctionNode&>(node);
            if (func_node.children.size() != 2) {
                throw RuleCompileException("MOD function requires 2 arguments");
            }
            
            IntVar arg1 = compile_arithmetic_to_var(*func_node.children[0], ctx);
            
            // Handle second argument - try as constant first, then as variable
            IntVar result(*ctx.space, 0, 100000); // MOD result domain
            
            try {
                int modulus = compile_arithmetic_to_int(*func_node.children[1], ctx);
                IntVar modulus_var(*ctx.space, modulus, modulus); // Convert to constant IntVar
                mod(*ctx.space, arg1, modulus_var, result);
            } catch (...) {
                IntVar arg2 = compile_arithmetic_to_var(*func_node.children[1], ctx);
                mod(*ctx.space, arg1, arg2, result);
            }
            
            return result;
        }
        
        default:
            throw RuleCompileException("Cannot convert node to IntVar");
    }
}

int DynamicRuleCompiler::compile_arithmetic_to_int(const ASTNode& node, ConstraintContext& ctx) {
    if (node.type == ASTNodeType::CONSTANT) {
        const ConstantNode& const_node = static_cast<const ConstantNode&>(node);
        try {
            return const_node.get_value<int>();
        } catch (const std::exception&) {
            throw RuleCompileException("Invalid integer constant");
        }
    }
    
    throw RuleCompileException("Cannot convert node to integer constant");
}

IntVar DynamicRuleCompiler::compile_variable_to_var(const VariableNode& var_node, ConstraintContext& ctx) {
    std::string var_name = var_node.variable_name;
    
    if (var_name == "?current" || var_name == "current") {
        int current_idx = ctx.current_voice * ctx.sequence_length + 0; // First position for now
        if (current_idx >= 0 && current_idx < ctx.pitch_vars->size()) {
            return (*ctx.pitch_vars)[current_idx];
        }
    }
    
    // Handle numbered variables like ?1, ?2
    if (var_name.front() == '?' && std::isdigit(var_name[1])) {
        int index = std::stoi(var_name.substr(1)) - 1; // Convert to 0-based
        if (index >= 0 && index < ctx.pitch_vars->size()) {
            return (*ctx.pitch_vars)[index];
        }
    }
    
    throw RuleCompileException("Unknown variable: " + var_name);
}

IntVar DynamicRuleCompiler::compile_voice_access_to_var(const VoiceAccessNode& voice_node, ConstraintContext& ctx) {
    int voice_index = voice_node.voice_index;
    std::string property = voice_node.property;
    
    if (voice_index >= ctx.num_voices) {
        throw RuleCompileException("Voice index " + std::to_string(voice_index) + " out of range");
    }
    
    // Determine position - simplified for now
    int position = 0;
    if (voice_node.position_expr && voice_node.position_expr->type == ASTNodeType::CONSTANT) {
        const ConstantNode* const_node = dynamic_cast<const ConstantNode*>(voice_node.position_expr.get());
        if (const_node) {
            position = const_node->get_value<int>();
        }
    }
    
    // Calculate actual index in the variable arrays
    int var_index = voice_index * ctx.sequence_length + position;
    
    std::cout << "    🔍 Voice access: voice[" << voice_index << "]." << property 
              << "[" << position << "] → var_index=" << var_index << std::endl;
    
    if (property == "pitch") {
        if (var_index >= 0 && var_index < ctx.pitch_vars->size()) {
            return (*ctx.pitch_vars)[var_index];
        }
    } else if (property == "rhythm") {
        if (ctx.rhythm_vars && var_index >= 0 && var_index < ctx.rhythm_vars->size()) {
            return (*ctx.rhythm_vars)[var_index];
        }
    }
    
    throw RuleCompileException("Invalid voice access: voice[" + std::to_string(voice_index) + "]." + property + "[" + std::to_string(position) + "]");
}

void DynamicRuleCompiler::post_function_constraint(const FunctionNode& func_node, ConstraintContext& ctx) {
    if (func_node.function_name == "all_different") {
        // Apply all_different to current voice pitch variables
        int voice_start = ctx.current_voice * ctx.sequence_length;
        IntVarArray voice_vars(*ctx.space, ctx.sequence_length);
        
        for (int i = 0; i < ctx.sequence_length; ++i) {
            voice_vars[i] = (*ctx.pitch_vars)[voice_start + i];
        }
        
        distinct(*ctx.space, voice_vars);
        std::cout << "    ✅ Posted all_different constraint" << std::endl;
    } else {
        throw RuleCompileException("Unsupported function: " + func_node.function_name);
    }
}

void DynamicRuleCompiler::post_membership_constraint(
    const IntVar& var, const std::vector<int>& values, ConstraintContext& ctx) {
    
    if (values.empty()) {
        // Force failure for empty set
        rel(*ctx.space, var, IRT_EQ, -9999); // Impossible value
        return;
    }
    
    if (values.size() == 1) {
        rel(*ctx.space, var, IRT_EQ, values[0]);
        return;
    }
    
    // Create domain constraint for membership
    IntSet value_set(values.data(), static_cast<int>(values.size()));
    dom(*ctx.space, var, value_set);
}

void DynamicRuleCompiler::create_distinctness_constraint(const IntVarArray& vars, ConstraintContext& ctx) {
    distinct(*ctx.space, vars);
}

void DynamicRuleCompiler::post_membership_constraint_from_binary(const BinaryOpNode& binary_node, ConstraintContext& ctx) {
    if (binary_node.children.size() != 2) {
        throw RuleCompileException("Membership constraint requires 2 operands");
    }
    
    // Get the variable to constrain
    IntVar var = compile_arithmetic_to_var(*binary_node.children[0], ctx);
    
    // Get the values to test membership against
    const ASTNode& right_node = *binary_node.children[1];
    if (right_node.type != ASTNodeType::CONSTANT) {
        throw RuleCompileException("Right side of membership constraint must be constant array");
    }
    
    const ConstantNode& const_node = static_cast<const ConstantNode&>(right_node);
    std::vector<int> values;

    try {
        values = const_node.get_value<std::vector<int>>();
    } catch (...) {
        // Be tolerant of parser output variants for array literals.
        try {
            const auto dvals = const_node.get_value<std::vector<double>>();
            values.reserve(dvals.size());
            for (double d : dvals) {
                const int iv = static_cast<int>(d);
                if (std::abs(d - static_cast<double>(iv)) > 1e-9) {
                    throw RuleCompileException("Membership array contains non-integer value: " + std::to_string(d));
                }
                values.push_back(iv);
            }
        } catch (const RuleCompileException&) {
            throw;
        } catch (...) {
            try {
                const auto sval = const_node.get_value<std::string>();
                if (!sval.empty() && sval.front() == '[' && sval.back() == ']') {
                    std::string inner = sval.substr(1, sval.size() - 2);
                    std::stringstream ss(inner);
                    std::string tok;
                    while (std::getline(ss, tok, ',')) {
                        tok.erase(std::remove_if(tok.begin(), tok.end(), [](unsigned char c) { return std::isspace(c); }), tok.end());
                        if (tok.empty()) continue;
                        values.push_back(std::stoi(tok));
                    }
                }
            } catch (...) {
                // fallthrough to final validation below
            }
        }
    }

    if (values.empty()) {
        throw RuleCompileException("Right side of membership constraint must be integer array");
    }
    
    if (binary_node.type == ASTNodeType::IN) {
        // Variable must be IN the set
        post_membership_constraint(var, values, ctx);
        std::cout << "    ✅ Posted IN constraint with " << values.size() << " values" << std::endl;
    } else { // NOT_IN
        // Variable must NOT be in the set - implement as domain exclusion
        for (int val : values) {
            rel(*ctx.space, var, IRT_NQ, val);
        }
        std::cout << "    ✅ Posted NOT_IN constraint excluding " << values.size() << " values" << std::endl;
    }
}

IntVarArray* DynamicRuleCompiler::get_voice_vars(
    int voice_index, const std::string& property, ConstraintContext& ctx) {
    
    if (property == "pitch" || property == "pitches") {
        return ctx.pitch_vars;
    } else if (property == "rhythm" || property == "rhythms") {
        if (!ctx.rhythm_vars) {
            throw RuleCompileException("Rhythm variables not available");
        }
        return ctx.rhythm_vars;
    }
    
    throw RuleCompileException("Unknown property: " + property);
}

int DynamicRuleCompiler::resolve_variable_index(const std::string& var_name, ConstraintContext& ctx) {
    if (var_name == "?current" || var_name == "current") {
        return 0; // Current position - context dependent
    }
    
    if (var_name == "?previous" || var_name == "previous") {
        return -1; // Previous position
    }
    
    if (var_name == "i") {
        return 0; // Loop variable - context dependent
    }
    
    return 0; // Default
}

} // namespace DynamicRules