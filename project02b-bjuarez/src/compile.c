#include "compile.h"

#include <stdio.h>
#include <stdlib.h>

size_t if_counter = 0;
size_t while_counter = 0;

bool constant_identifier(node_t *node) {
    if (node->type == NUM) {
        return true;
    }
    else if (node->type == BINARY_OP) {
        binary_node_t *bin_node = (binary_node_t *) node;
        if (bin_node->op == '+' || bin_node->op == '-' || bin_node->op == '*' ||
            bin_node->op == '/') {
            return constant_identifier(bin_node->left) &&
                   constant_identifier(bin_node->right);
        }
        else {
            return false;
        }
    }
    else {
        return false;
    }
}

int64_t constant_evaluator(node_t *node) {
    if (node->type == NUM) {
        num_node_t *num_node = (num_node_t *) node;
        return num_node->value;
    }
    else {
        binary_node_t *bin_node = (binary_node_t *) node;
        int64_t right = constant_evaluator(bin_node->right);
        int64_t left = constant_evaluator(bin_node->left);

        if (bin_node->op == '+') {
            return left + right;
        }
        else if (bin_node->op == '-') {
            return left - right;
        }
        else if (bin_node->op == '*') {
            return left * right;
        }
        else if (bin_node->op == '/') {
            return left / right;
        }
        else {
            return 0;
        }
    }
}

int64_t shift_evaluator(node_t *node) {
    int64_t k = constant_evaluator(node);
    int64_t power = 0;
    if (k == 0) {
        return k;
    }
    else {
        while (k != 1) {
            if (k % 2 == 0) {
                k = k / 2;
                power++;
            }
            else {
                return 0;
            }
        }
        return power;
    }
}

bool compile_ast(node_t *node) {
    /* You should remove this cast to void in your solution.
     * It is just here so the code compiles without warnings. */
    if (node->type == NUM) {
        num_node_t *num_node = (num_node_t *) node;
        printf("movq $%lu, %%rdi\n", num_node->value);
        return true;
    }
    else if (node->type == PRINT) {
        print_node_t *print_node = (print_node_t *) node;
        compile_ast(print_node->expr);
        printf("call print_int\n");
        return true;
    }
    else if (node->type == SEQUENCE) {
        sequence_node_t *seq_node = (sequence_node_t *) node;
        for (size_t i = 0; i < seq_node->statement_count; i++) {
            compile_ast(seq_node->statements[i]);
        }
        return true;
    }
    else if (node->type == BINARY_OP) {
        binary_node_t *bin_node = (binary_node_t *) node;
        if (constant_identifier(node)) {
            printf("movq $%lu, %%rdi\n", constant_evaluator(node));
        }
        else {
            if ((bin_node->op == '*') && constant_identifier(bin_node->right)) {
                int64_t shift_value = shift_evaluator(bin_node->right);
                if (shift_value != 0) {
                    compile_ast(bin_node->left);
                    printf("shl $%zu, %%rdi\n", shift_value);
                    return true;
                }
            }
            compile_ast(bin_node->right);
            printf("pushq %%rdi\n");
            compile_ast(bin_node->left);
            printf("popq %%rsi\n");

            if (bin_node->op == '+') {
                printf("add %%rsi, %%rdi\n");
            }
            else if (bin_node->op == '-') {
                printf("subq %%rsi, %%rdi\n");
            }
            else if (bin_node->op == '*') {
                printf("imulq %%rsi, %%rdi\n");
            }
            else if (bin_node->op == '/') {
                printf("movq %%rdi, %%rax\n");
                printf("cqto\n");
                printf("idiv %%rsi\n");
                printf("movq %%rax, %%rdi\n");
            }
            else {
                printf("cmp %%rsi, %%rdi\n");
            }
        }
        return true;
    }
    else if (node->type == VAR) {
        var_node_t *var_node = (var_node_t *) node;
        printf("movq -0x%x(%%rbp), %%rdi\n", (var_node->name - 'A' + 1) * 8);
        return true;
    }
    else if (node->type == LET) {
        let_node_t *let_node = (let_node_t *) node;
        compile_ast(let_node->value);
        printf("movq %%rdi, -0x%x(%%rbp)\n", (let_node->var - 'A' + 1) * 8);
        return true;
    }
    else if (node->type == IF) {
        if_counter++;
        size_t counter = if_counter;

        if_node_t *if_node = (if_node_t *) node;
        binary_node_t *bin_node = (binary_node_t *) if_node->condition;
        compile_ast((node_t *)bin_node);

        if (bin_node->op == '=') {
            printf("je IF_%zu\n", counter);
        }
        else if (bin_node->op == '>') {
            printf("jg IF_%zu\n", counter);
        }
        else if (bin_node->op == '<') {
            printf("jl IF_%zu\n", counter);
        }
        printf("ELSE_%zu:\n", counter);
        if (if_node->else_branch != NULL) {
            compile_ast(if_node->else_branch);
        }
        printf("jmp ENDIF_%zu\n", counter);
        printf("IF_%zu:\n", counter);
        compile_ast(if_node->if_branch);
        printf("jmp ENDIF_%zu\n", counter);
        printf("ENDIF_%zu:\n", counter);

        return true;
    }
    else if (node->type == WHILE) {
        while_counter++;
        size_t counter = while_counter;
        printf("WHILE_%zu:\n", counter);

        while_node_t *while_node = (while_node_t *) node;
        binary_node_t *bin_node = (binary_node_t *) while_node->condition;
        compile_ast((node_t *)bin_node);

        if (bin_node->op == '=') {
            printf("jne END_WHILE_%zu\n", counter);
        }
        else if (bin_node->op == '>') {
            printf("jng END_WHILE_%zu\n", counter);
        }
        else if (bin_node->op == '<') {
            printf("jnl END_WHILE_%zu\n", counter);
        }
        compile_ast(while_node->body);
        printf("jmp WHILE_%zu\n", counter);
        printf("END_WHILE_%zu:\n", counter);

        return true;
    }

    return false;
}
