#include "jvm.h"

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "heap.h"
#include "read_class.h"

/** The name of the method to invoke to run the class file */
const char MAIN_METHOD[] = "main";
/**
 * The "descriptor" string for main(). The descriptor encodes main()'s signature,
 * i.e. main() takes a String[] and returns void.
 * If you're interested, the descriptor string is explained at
 * https://docs.oracle.com/javase/specs/jvms/se12/html/jvms-4.html#jvms-4.3.2.
 */
const char MAIN_DESCRIPTOR[] = "([Ljava/lang/String;)V";

/**
 * Represents the return value of a Java method: either void or an int or a reference.
 * For simplification, we represent a reference as an index into a heap-allocated array.
 * (In a real JVM, methods could also return object references or other primitives.)
 */
typedef struct {
    /** Whether this returned value is an int */
    bool has_value;
    /** The returned value (only valid if `has_value` is true) */
    int32_t value;
} optional_value_t;

// Struct representing the operand stack
typedef struct {
    u4 size;           // current number of ints on stack
    u4 length;         // max. number of ints that will be on stack
    int32_t *contents; // contents of operand stack
} stack_t;

// helper function simulating popping from stack
int32_t pop(stack_t *stack) {
    assert(stack->size > 0);
    int32_t popped = stack->contents[stack->size - 1];
    stack->size--;
    return popped;
}

// helper function simulating pushing onto stack
void push(stack_t *stack, int32_t b) {
    assert(stack->size < stack->length);
    stack->contents[stack->size] = b;
    stack->size++;
}

/**
 * Runs a method's instructions until the method returns.
 *
 * @param method the method to run
 * @param locals the array of local variables, including the method parameters.
 *   Except for parameters, the locals are uninitialized.
 * @param class the class file the method belongs to
 * @param heap an array of heap-allocated pointers, useful for references
 * @return an optional int containing the method's return value
 */
optional_value_t execute(method_t *method, int32_t *locals, class_file_t *class,
                         heap_t *heap) {
    u4 pc = 0;                  // program counter
    code_t code = method->code; // retrieves bytecode
    bool returned = false;      // used for loop

    // initializes empty operand stack
    stack_t *s = malloc(sizeof(stack_t));
    s->size = 0;
    s->length = code.max_stack;
    s->contents = malloc(s->length * sizeof(int32_t));

    optional_value_t result = {.has_value = false};

    while (!returned) {
        // retrieves instructions from program counter (bytes)
        jvm_instruction_t instructions = code.code[pc];
        // fprintf(stderr, "%x\n", instructions);

        // handles instructions
        if (instructions == i_bipush) {
            push(s, (int8_t) code.code[pc + 1]);
            pc += 2;
        }
        else if (instructions == i_iadd) {
            int32_t i1 = pop(s);
            int32_t i2 = pop(s);
            push(s, i1 + i2);
            pc++;
        }
        else if (instructions == i_return) {
            returned = true; // terminates loop
        }
        else if (instructions == i_getstatic) {
            pc += 3;
        }
        else if (instructions == i_invokevirtual) {
            printf("%i\n", pop(s));
            pc += 3;
        }
        else if (instructions >= i_iconst_m1 && instructions <= i_iconst_5) {
            push(s, instructions - i_iconst_0);
            pc++;
        }
        else if (instructions == i_sipush) {
            int16_t b = (int16_t)(code.code[pc + 1] << 8) | code.code[pc + 2];
            push(s, (int32_t) b);
            pc += 3;
        }
        else if (instructions == i_iadd) {
            int32_t b = pop(s);
            int32_t a = pop(s);
            push(s, a + b);
            pc++;
        }
        else if (instructions == i_isub) {
            int32_t b = pop(s);
            int32_t a = pop(s);
            push(s, a - b);
            pc++;
        }
        else if (instructions == i_imul) {
            int32_t b = pop(s);
            int32_t a = pop(s);
            push(s, a * b);
            pc++;
        }
        else if (instructions == i_idiv) {
            int32_t b = pop(s);
            int32_t a = pop(s);
            push(s, a / b);
            pc++;
        }
        else if (instructions == i_irem) {
            int32_t b = pop(s);
            int32_t a = pop(s);
            push(s, a % b);
            pc++;
        }
        else if (instructions == i_ineg) {
            int32_t a = pop(s);
            push(s, -1 * a);
            pc++;
        }
        else if (instructions == i_ishl) {
            int32_t b = pop(s);
            int32_t a = pop(s);
            push(s, a << b);
            pc++;
        }
        else if (instructions == i_ishr) {
            int32_t b = pop(s);
            int32_t a = pop(s);
            push(s, a >> b);
            pc++;
        }
        else if (instructions == i_iushr) {
            int32_t b = pop(s);
            u4 a = pop(s);
            push(s, (uint32_t) a >> b);
            pc++;
        }
        else if (instructions == i_iand) {
            int32_t b = pop(s);
            int32_t a = pop(s);
            push(s, a & b);
            pc++;
        }
        else if (instructions == i_ior) {
            int32_t b = pop(s);
            int32_t a = pop(s);
            push(s, a | b);
            pc++;
        }
        else if (instructions == i_ixor) {
            int32_t b = pop(s);
            int32_t a = pop(s);
            push(s, a ^ b);
            pc++;
        }
        else if (instructions == i_iload) {
            u1 i = code.code[pc + 1];
            int32_t b = locals[i];
            push(s, b);
            pc += 2;
        }
        else if (instructions == i_istore) {
            u1 i = code.code[pc + 1];
            locals[i] = pop(s);
            pc += 2;
        }
        else if (instructions == i_iinc) {
            u1 i = code.code[pc + 1];
            int8_t b = code.code[pc + 2];
            locals[i] += b;
            pc += 3;
        }
        else if (instructions >= i_iload_0 && instructions <= i_iload_3) {
            int32_t b = locals[instructions - i_iload_0];
            push(s, b);
            pc++;
        }
        else if (instructions >= i_istore_0 && instructions <= i_istore_3) {
            locals[instructions - i_istore_0] = pop(s);
            pc++;
        }
        else if (instructions == i_ldc) {
            u1 b = code.code[pc + 1];
            CONSTANT_Integer_info *pool =
                (CONSTANT_Integer_info *) class->constant_pool[b - 1].info;
            push(s, pool->bytes);
            pc += 2;
        }
        else if (instructions >= i_ifeq && instructions <= i_ifle) {
            int32_t a = pop(s);

            if (instructions == i_ifeq && a == 0) {
                u1 b1 = code.code[pc + 1];
                u1 b2 = code.code[pc + 2];
                int16_t jump = (b1 << 8) | b2;
                pc += jump;
            }
            else if (instructions == i_ifne && a != 0) {
                u1 b1 = code.code[pc + 1];
                u1 b2 = code.code[pc + 2];
                int16_t jump = (b1 << 8) | b2;
                pc += jump;
            }
            else if (instructions == i_iflt && a < 0) {
                u1 b1 = code.code[pc + 1];
                u1 b2 = code.code[pc + 2];
                int16_t jump = (b1 << 8) | b2;
                pc += jump;
            }
            else if (instructions == i_ifge && a >= 0) {
                u1 b1 = code.code[pc + 1];
                u1 b2 = code.code[pc + 2];
                int16_t jump = (b1 << 8) | b2;
                pc += jump;
            }
            else if (instructions == i_ifgt && a > 0) {
                u1 b1 = code.code[pc + 1];
                u1 b2 = code.code[pc + 2];
                int16_t jump = (b1 << 8) | b2;
                pc += jump;
            }
            else if (instructions == i_ifle && a <= 0) {
                u1 b1 = code.code[pc + 1];
                u1 b2 = code.code[pc + 2];
                int16_t jump = (b1 << 8) | b2;
                pc += jump;
            }
            else {
                pc += 3;
            }
        }
        else if (instructions >= i_if_icmpeq && instructions <= i_if_icmple) {
            int32_t b = pop(s);
            int32_t a = pop(s);

            if (instructions == i_if_icmpeq && a == b) {
                u1 b1 = code.code[pc + 1];
                u1 b2 = code.code[pc + 2];
                int16_t jump = (b1 << 8) | b2;
                pc += jump;
            }
            else if (instructions == i_if_icmpne && a != b) {
                u1 b1 = code.code[pc + 1];
                u1 b2 = code.code[pc + 2];
                int16_t jump = (b1 << 8) | b2;
                pc += jump;
            }
            else if (instructions == i_if_icmplt && a < b) {
                u1 b1 = code.code[pc + 1];
                u1 b2 = code.code[pc + 2];
                int16_t jump = (b1 << 8) | b2;
                pc += jump;
            }
            else if (instructions == i_if_icmpgt && a > b) {
                u1 b1 = code.code[pc + 1];
                u1 b2 = code.code[pc + 2];
                int16_t jump = (b1 << 8) | b2;
                pc += jump;
            }
            else if (instructions == i_if_icmpge && a >= b) {
                u1 b1 = code.code[pc + 1];
                u1 b2 = code.code[pc + 2];
                int16_t jump = (b1 << 8) | b2;
                pc += jump;
            }
            else if (instructions == i_if_icmple && a <= b) {
                u1 b1 = code.code[pc + 1];
                u1 b2 = code.code[pc + 2];
                int16_t jump = (b1 << 8) | b2;
                pc += jump;
            }
            else {
                pc += 3;
            }
        }
        else if (instructions == i_goto) {
            u1 b1 = code.code[pc + 1];
            u1 b2 = code.code[pc + 2];
            int16_t jump = (b1 << 8) | b2;
            pc += jump;
        }
        else if (instructions == i_ireturn) {
            result.has_value = true;
            result.value = pop(s);
            free(s->contents);
            free(s);
            return result;
            // returned = true;
        }
        else if (instructions == i_invokestatic) {
            u1 b1 = code.code[pc + 1];
            u1 b2 = code.code[pc + 2];
            int16_t index = (b1 << 8) | b2;

            method_t *meth = find_method_from_index(index, class);
            u2 num_params = get_number_of_parameters(meth);
            int32_t *loc = calloc(meth->code.max_locals, sizeof(int32_t));

            for (int i = 0; i < num_params; i++) {
                int32_t b = pop(s);
                loc[num_params - i - 1] = b;
            }

            optional_value_t new_result = execute(meth, loc, class, heap);

            if (new_result.has_value) {
                push(s, new_result.value);
            }
            pc += 3;
            free(loc);
        }
        else if (instructions == i_nop) {
            pc++;
        }
        else if (instructions == i_dup) {
            int32_t a = pop(s);
            push(s, a);
            push(s, a);
            pc++;
        }
        else if (instructions == i_newarray) {
            int32_t count = pop(s);
            int32_t *new = calloc(count + 1, sizeof(int32_t));
            new[0] = count;
            push(s, heap_add(heap, new));
            pc += 2;
        }
        else if (instructions == i_arraylength) {
            int32_t *h = heap_get(heap, pop(s));
            push(s, h[0]);
            pc++;
        }
        else if (instructions == i_areturn) {
            result.has_value = true;
            result.value = pop(s);
            returned = true;
        }
        else if (instructions == i_iastore) {
            int32_t value = pop(s);
            int32_t index = pop(s);
            int32_t ref = pop(s);
            int32_t *h = heap_get(heap, ref);
            h[index + 1] = value;
            pc++;
        }
        else if (instructions == i_iaload) {
            int32_t index = pop(s);
            int32_t ref = pop(s);
            int32_t *h = heap_get(heap, ref);
            push(s, h[index + 1]);
            pc++;
        }
        else if (instructions == i_aload) {
            int32_t b = locals[code.code[pc + 1]];
            push(s, b);
            pc += 2;
        }
        else if (instructions == i_astore) {
            locals[code.code[pc + 1]] = pop(s);
            pc += 2;
        }
        else if (instructions >= i_aload_0 && instructions <= i_aload_3) {
            push(s, locals[instructions - i_aload_0]);
            pc++;
        }
        else if (instructions >= i_astore_0 && instructions <= i_astore_3) {
            int32_t ref = pop(s);
            locals[instructions - i_astore_0] = ref;
            pc++;
        }
    }
    free(s->contents);
    free(s);
    return result;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "USAGE: %s <class file>\n", argv[0]);
        return 1;
    }

    // Open the class file for reading
    FILE *class_file = fopen(argv[1], "r");
    assert(class_file != NULL && "Failed to open file");

    // Parse the class file
    class_file_t *class = get_class(class_file);
    int error = fclose(class_file);
    assert(error == 0 && "Failed to close file");

    // The heap array is initially allocated to hold zero elements.
    heap_t *heap = heap_init();

    // Execute the main method
    method_t *main_method = find_method(MAIN_METHOD, MAIN_DESCRIPTOR, class);
    assert(main_method != NULL && "Missing main() method");
    /* In a real JVM, locals[0] would contain a reference to String[] args.
     * But since TeenyJVM doesn't support Objects, we leave it uninitialized. */
    int32_t locals[main_method->code.max_locals];
    // Initialize all local variables to 0
    memset(locals, 0, sizeof(locals));
    optional_value_t result = execute(main_method, locals, class, heap);
    assert(!result.has_value && "main() should return void");

    // Free the internal data structures
    free_class(class);

    // Free the heap
    heap_free(heap);
}
