/**
 * @file wildcard_rule_extension.cpp
 * @brief Implementation of wildcard rule compiler extension
 */

#include "wildcard_rule_extension.hh"
#include "rule_expression_parser.hh"
#include <iostream>
#include <stdexcept>
#include <algorithm>

using namespace Gecode;

namespace DynamicRules {

std::unique_ptr<CompiledConstraint> WildcardRuleCompiler::compile_wildcard_from_json(
    const nlohmann::json& rule_json) {
    
    std::string rule_id = rule_json.value("rule_id", "unnamed_wildcard");
    std::string description = rule_json.value("description", "Wildcard rule");
    
    // Parse wildcard specification
    WildcardSpec spec = parse_wildcard_spec(rule_json);
    
    // Parse the inner constraint expression
    RuleExpressionParser parser;
    nlohmann::json constraint_json = {
        {"id", rule_id + "_inner"},
        {"type", "expression_constraint"},
        {"expression", spec.constraint_expr}
    };
    
    std::cout << "🔍 Parsing wildcard constraint expression: \"" << spec.constraint_expr << "\"" << std::endl;
    
    std::unique_ptr<RuleAST> constraint_ast = parser.parse_rule(constraint_json);
    
    std::cout << "✅ Parsed constraint AST, root type: " << static_cast<int>(constraint_ast->root->type) << std::endl;
    
    // Create compiled constraint
    auto compiled = std::make_unique<CompiledConstraint>(rule_id, description);
    
    // Select appropriate constraint creation function based on wildcard type
    switch (spec.type) {
        case WildcardType::FOR_ALL_POSITIONS: {
            compiled->post_constraint = create_for_all_positions_constraint(spec, constraint_ast);
            break;
        }
        case WildcardType::FOR_ALL_VOICES: {
            compiled->post_constraint = create_for_all_voices_constraint(spec, constraint_ast);
            break;
        }
        case WildcardType::SLIDING_WINDOW: {
            compiled->post_constraint = create_sliding_window_constraint(spec, constraint_ast);
            break;
        }
        case WildcardType::SEQUENCE_PATTERN: {
            // Use sliding window with pattern offsets
            compiled->post_constraint = create_sliding_window_constraint(spec, constraint_ast);
            break;
        }
        default: {
            throw RuleCompileException("Unsupported wildcard type: " + std::to_string(static_cast<int>(spec.type)));
        }
    }
    
    return compiled;
}

std::function<void(ConstraintContext&)> WildcardRuleCompiler::create_for_all_positions_constraint(
    const WildcardSpec& spec, 
    const std::unique_ptr<RuleAST>& constraint_ast) {
    
    // Store constraint expression to re-parse when needed
    std::string constraint_expr = spec.constraint_expr;
    
    return [spec, constraint_expr](ConstraintContext& ctx) {
        std::cout << "  🌟 Applying FOR_ALL_POSITIONS wildcard rule" << std::endl;
        
        // Re-parse the constraint expression when applying
        RuleExpressionParser parser;
        nlohmann::json temp_json = {
            {"id", "temp_constraint"},
            {"type", "expression_constraint"},
            {"expression", constraint_expr}
        };
        
        std::unique_ptr<RuleAST> constraint_ast = parser.parse_rule(temp_json);
        if (!constraint_ast || !constraint_ast->root) {
            throw std::runtime_error("Invalid constraint AST for wildcard rule");
        }
        
        int total_constraints = 0;
        
        // Apply constraint to all positions in all voices
        for (int voice = 0; voice < ctx.num_voices; ++voice) {
            for (int pos = 0; pos < ctx.sequence_length; ++pos) {
                // Check if we have enough positions for the pattern
                bool can_apply = true;
                for (int offset : spec.pattern_offsets) {
                    if (pos + offset >= ctx.sequence_length) {
                        can_apply = false;
                        break;
                    }
                }
                
                if (can_apply) {
                    // Skip if any position in the pattern could be a rest
                    for (int offset : spec.pattern_offsets) {
                        if (ctx.position_can_be_rest(voice, pos + offset)) {
                            can_apply = false;
                            break;
                        }
                    }
                }

                if (can_apply) {
                    apply_constraint_at_position(*constraint_ast->root, ctx, voice, pos, spec.pattern_offsets);
                    total_constraints++;
                }
            }
        }
        
        std::cout << "    ✅ Posted " << total_constraints << " wildcard constraints" << std::endl;
    };
}

std::function<void(ConstraintContext&)> WildcardRuleCompiler::create_for_all_voices_constraint(
    const WildcardSpec& spec,
    const std::unique_ptr<RuleAST>& constraint_ast) {
    
    // Store constraint expression to re-parse when needed
    std::string constraint_expr = spec.constraint_expr;
    
    return [spec, constraint_expr](ConstraintContext& ctx) {
        std::cout << "  🌟 Applying FOR_ALL_VOICES wildcard rule" << std::endl;
        
        // Re-parse the constraint expression when applying
        RuleExpressionParser parser;
        nlohmann::json temp_json = {
            {"id", "temp_constraint"},
            {"type", "expression_constraint"},
            {"expression", constraint_expr}
        };
        
        std::unique_ptr<RuleAST> constraint_ast = parser.parse_rule(temp_json);
        if (!constraint_ast || !constraint_ast->root) {
            throw std::runtime_error("Invalid constraint AST for wildcard rule");
        }
        
        int total_constraints = 0;
        
        // Apply constraint separately to each voice across all positions
        for (int voice = 0; voice < ctx.num_voices; ++voice) {
            for (int pos = 0; pos < ctx.sequence_length; ++pos) {
                // Check if we have enough positions for the pattern
                bool can_apply = true;
                for (int offset : spec.pattern_offsets) {
                    if (pos + offset >= ctx.sequence_length) {
                        can_apply = false;
                        break;
                    }
                }
                
                if (can_apply) {
                    // Skip if any position in the pattern could be a rest
                    for (int offset : spec.pattern_offsets) {
                        if (ctx.position_can_be_rest(voice, pos + offset)) {
                            can_apply = false;
                            break;
                        }
                    }
                }

                if (can_apply) {
                    apply_constraint_at_position(*constraint_ast->root, ctx, voice, pos, spec.pattern_offsets);
                    total_constraints++;
                }
            }
        }
        
        std::cout << "    ✅ Posted " << total_constraints << " voice-specific constraints" << std::endl;
    };
}

std::function<void(ConstraintContext&)> WildcardRuleCompiler::create_sliding_window_constraint(
    const WildcardSpec& spec,
    const std::unique_ptr<RuleAST>& constraint_ast) {
    
    // Store constraint expression to re-parse when needed
    std::string constraint_expr = spec.constraint_expr;
    
    return [spec, constraint_expr](ConstraintContext& ctx) {
        std::cout << "  🌟 Applying SLIDING_WINDOW wildcard rule" << std::endl;
        
        // Re-parse the constraint expression when applying
        RuleExpressionParser parser;
        nlohmann::json temp_json = {
            {"id", "temp_constraint"},
            {"type", "expression_constraint"},
            {"expression", constraint_expr}
        };
        
        std::unique_ptr<RuleAST> constraint_ast = parser.parse_rule(temp_json);
        if (!constraint_ast || !constraint_ast->root) {
            throw std::runtime_error("Invalid constraint AST for wildcard rule");
        }
        
        int total_constraints = 0;
        
        // Find maximum offset to determine window size
        int max_offset = 0;
        if (!spec.pattern_offsets.empty()) {
            max_offset = *std::max_element(spec.pattern_offsets.begin(), spec.pattern_offsets.end());
        }
        
        // Apply sliding window across each voice
        for (int voice = 0; voice < ctx.num_voices; ++voice) {
            for (int pos = 0; pos <= ctx.sequence_length - max_offset - 1; pos += spec.step_size) {
                // Skip if any position in the window could be a rest
                bool can_apply = true;
                for (int offset : spec.pattern_offsets) {
                    if (ctx.position_can_be_rest(voice, pos + offset)) {
                        can_apply = false;
                        break;
                    }
                }
                if (!can_apply) continue;

                std::cout << "    🆔 About to call apply_constraint_at_position for voice=" << voice 
                          << " pos=" << pos << std::endl;
                apply_constraint_at_position(*constraint_ast->root, ctx, voice, pos, spec.pattern_offsets);
                std::cout << "    🆔 Returned from apply_constraint_at_position for voice=" << voice 
                          << " pos=" << pos << std::endl;
                total_constraints++;
            }
        }
        
        std::cout << "    ✅ Posted " << total_constraints << " sliding window constraints" << std::endl;
    };
}

void WildcardRuleCompiler::apply_constraint_at_position(
    const ASTNode& constraint_node,
    ConstraintContext& ctx,
    int voice_idx,
    int position_idx,
    const std::vector<int>& pattern_offsets) {
    
    std::cout << "    🔧 apply_constraint_at_position called for voice=" << voice_idx 
              << " pos=" << position_idx << std::endl;
    
    try {
        // Create a substituted version of the constraint for this position
        std::unique_ptr<ASTNode> substituted_node = substitute_pattern_variables(
            constraint_node, voice_idx, position_idx, pattern_offsets);
        
        std::cout << "    🔧 substitute_pattern_variables completed" << std::endl;
        
        // Apply the constraint using existing compiler infrastructure
        if (substituted_node) {
            std::cout << "    🔧 Applying constraint at voice=" << voice_idx 
                      << " pos=" << position_idx << " type=" << static_cast<int>(substituted_node->type) << std::endl;
            
            std::cout << "    🔧 About to call DynamicRuleCompiler::compile_and_post_constraint" << std::endl;
            DynamicRuleCompiler::compile_and_post_constraint(*substituted_node, ctx);
            std::cout << "    ✅ Successfully posted constraint" << std::endl;
        } else {
            std::cout << "    ⚠️  substituted_node is null!" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "    ⚠️  Failed to apply constraint at voice " << voice_idx 
                  << ", position " << position_idx << ": " << e.what() << std::endl;
    }
}

WildcardSpec WildcardRuleCompiler::parse_wildcard_spec(const nlohmann::json& rule_json) {
    std::string wildcard_type_str = rule_json.value("wildcard_type", "for_all_positions");
    std::string constraint_expr = rule_json.value("constraint", "true");
    
    // Parse wildcard type
    WildcardType type = WildcardType::FOR_ALL_POSITIONS;
    if (wildcard_type_str == "for_all_positions") {
        type = WildcardType::FOR_ALL_POSITIONS;
    } else if (wildcard_type_str == "for_all_voices") {
        type = WildcardType::FOR_ALL_VOICES;
    } else if (wildcard_type_str == "for_all_pairs") {
        type = WildcardType::FOR_ALL_PAIRS;
    } else if (wildcard_type_str == "sliding_window") {
        type = WildcardType::SLIDING_WINDOW;
    } else if (wildcard_type_str == "sequence_pattern") {
        type = WildcardType::SEQUENCE_PATTERN;
    }
    
    WildcardSpec spec(type, constraint_expr);
    
    // Parse pattern offsets
    if (rule_json.contains("pattern_offsets")) {
        spec.pattern_offsets = rule_json["pattern_offsets"].get<std::vector<int>>();
    } else {
        spec.pattern_offsets = {0}; // Default single position
    }
    
    // Parse other parameters
    spec.window_size = rule_json.value("window_size", 1);
    spec.step_size = rule_json.value("step_size", 1);
    spec.cross_voices = rule_json.value("cross_voices", false);
    
    return spec;
}

std::unique_ptr<ASTNode> WildcardRuleCompiler::substitute_pattern_variables(
    const ASTNode& node,
    int voice_idx,
    int position_idx, 
    const std::vector<int>& pattern_offsets) {
    
    // This is a simplified implementation - a full implementation would need
    // to handle all AST node types and perform proper variable substitution
    
    switch (node.type) {
        case ASTNodeType::VOICE_ACCESS: {
            // Handle voice access nodes like voice[v].pitch[i]
            const VoiceAccessNode& voice_node = static_cast<const VoiceAccessNode&>(node);
            
            // Create new voice access node with substituted values
            int new_voice_index = voice_node.voice_index;
            
            // If voice_index is -1, it's a pattern variable that needs substitution
            if (voice_node.voice_index == -1) {
                new_voice_index = voice_idx;
            }
            
            auto new_voice_node = std::make_unique<VoiceAccessNode>(new_voice_index, voice_node.property);
            
            // Substitute position expression (i -> actual position_idx)
            if (voice_node.position_expr) {
                std::unique_ptr<ASTNode> substituted_position = substitute_pattern_variables(*voice_node.position_expr, voice_idx, position_idx, pattern_offsets);
                new_voice_node->set_position(std::move(substituted_position));
            }
            
            return std::move(new_voice_node);
        }
        
        case ASTNodeType::VARIABLE: {
            const VariableNode& var_node = static_cast<const VariableNode&>(node);
            std::string var_name = var_node.variable_name;
            
            // Handle pattern variables
            if (var_name == "i") {
                return std::make_unique<ConstantNode>(position_idx);
            } else if (var_name == "v") {
                return std::make_unique<ConstantNode>(voice_idx);
            }
            
            // Replace pattern variables like voice[v].pitch[i] with actual indices (fallback for string-based)
            if (var_name.find("voice[v]") != std::string::npos) {
                std::string new_name = "voice[" + std::to_string(voice_idx) + "]";
                size_t pos = var_name.find("voice[v]");
                var_name.replace(pos, 8, new_name); // "voice[v]" is 8 characters
            }
            
            if (var_name.find("[i]") != std::string::npos) {
                std::string new_pos = "[" + std::to_string(position_idx) + "]";
                size_t pos = var_name.find("[i]");
                var_name.replace(pos, 3, new_pos); // "[i]" is 3 characters
            }
            
            if (var_name.find("[i+1]") != std::string::npos) {
                std::string new_pos = "[" + std::to_string(position_idx + 1) + "]";
                size_t pos = var_name.find("[i+1]");
                var_name.replace(pos, 5, new_pos); // "[i+1]" is 5 characters
            }
            
            return std::make_unique<VariableNode>(var_name);
        }
        
        case ASTNodeType::PLUS: {
            // Handle arithmetic: i + N
            if (node.children.size() == 2) {
                // Check if left child is variable "i" and right is constant
                if (node.children[0]->type == ASTNodeType::VARIABLE) {
                    const VariableNode& var_node = static_cast<const VariableNode&>(*node.children[0]);
                    if (var_node.variable_name == "i" && node.children[1]->type == ASTNodeType::CONSTANT) {
                        const ConstantNode& const_node = static_cast<const ConstantNode&>(*node.children[1]);
                        int const_value = const_node.get_value<int>();
                        return std::make_unique<ConstantNode>(position_idx + const_value);
                    }
                }
            }
            
            // Fallback: recursively substitute children
            auto new_node = std::make_unique<BinaryOpNode>(ASTNodeType::PLUS);
            new_node->value = node.value;
            for (const auto& child : node.children) {
                auto substituted_child = substitute_pattern_variables(*child, voice_idx, position_idx, pattern_offsets);
                new_node->add_child(std::move(substituted_child));
            }
            return std::move(new_node);
        }
        
        case ASTNodeType::MINUS: {
            // Handle arithmetic: i - N
            if (node.children.size() == 2) {
                // Check if left child is variable "i" and right is constant
                if (node.children[0]->type == ASTNodeType::VARIABLE) {
                    const VariableNode& var_node = static_cast<const VariableNode&>(*node.children[0]);
                    if (var_node.variable_name == "i" && node.children[1]->type == ASTNodeType::CONSTANT) {
                        const ConstantNode& const_node = static_cast<const ConstantNode&>(*node.children[1]);
                        int const_value = const_node.get_value<int>();
                        return std::make_unique<ConstantNode>(position_idx - const_value);
                    }
                }
            }
            
            // Fallback: recursively substitute children
            auto new_node = std::make_unique<BinaryOpNode>(ASTNodeType::MINUS);
            new_node->value = node.value;
            for (const auto& child : node.children) {
                auto substituted_child = substitute_pattern_variables(*child, voice_idx, position_idx, pattern_offsets);
                new_node->add_child(std::move(substituted_child));
            }
            return std::move(new_node);
        }
        
        case ASTNodeType::CONSTANT: {
            // Constants remain unchanged
            const ConstantNode& const_node = static_cast<const ConstantNode&>(node);
            auto new_node = std::make_unique<ConstantNode>(const_node.value);
            return std::move(new_node);
        }
        
        default: {
            // For compound nodes, we need to create a concrete node type
            // Since we can't instantiate the abstract ASTNode directly,
            // we'll create a BinaryOpNode or similar concrete type
            
            if (node.children.size() == 2) {
                // Binary operation node
                auto new_node = std::make_unique<BinaryOpNode>(node.type);
                new_node->value = node.value;
                
                for (const auto& child : node.children) {
                    auto substituted_child = substitute_pattern_variables(*child, voice_idx, position_idx, pattern_offsets);
                    new_node->add_child(std::move(substituted_child));
                }
                
                return std::move(new_node);
            } else if (node.children.size() == 1) {
                // Unary operation node  
                auto new_node = std::make_unique<UnaryOpNode>(node.type);
                new_node->value = node.value;
                
                for (const auto& child : node.children) {
                    auto substituted_child = substitute_pattern_variables(*child, voice_idx, position_idx, pattern_offsets);
                    new_node->add_child(std::move(substituted_child));
                }
                
                return std::move(new_node);
            } else {
                // Function node or other multi-child node
                std::string function_name = "compound_expr"; // Default function name
                if (std::holds_alternative<std::string>(node.value)) {
                    function_name = std::get<std::string>(node.value);
                }
                auto new_node = std::make_unique<FunctionNode>(node.type, function_name);
                new_node->value = node.value;
                
                for (const auto& child : node.children) {
                    auto substituted_child = substitute_pattern_variables(*child, voice_idx, position_idx, pattern_offsets);
                    new_node->add_child(std::move(substituted_child));
                }
                
                return std::move(new_node);
            }
        }
    }
}

} // namespace DynamicRules