#include "directory_tree.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

const mode_t MODE = 0777;

void init_node(node_t *node, char *name, node_type_t type) {
    if (name == NULL) {
        name = strdup("ROOT");
        assert(name != NULL);
    }
    node->name = name;
    node->type = type;
}

file_node_t *init_file_node(char *name, size_t size, uint8_t *contents) {
    file_node_t *node = malloc(sizeof(file_node_t));
    assert(node != NULL);
    init_node((node_t *) node, name, FILE_TYPE);
    node->size = size;
    node->contents = contents;
    return node;
}

directory_node_t *init_directory_node(char *name) {
    directory_node_t *node = malloc(sizeof(directory_node_t));
    assert(node != NULL);
    init_node((node_t *) node, name, DIRECTORY_TYPE);
    node->num_children = 0;
    node->children = NULL;
    return node;
}

void add_child_directory_tree(directory_node_t *dnode, node_t *child) {
    // increases number of children by 1
    dnode->num_children += 1;
    // creates extra space for new child
    dnode->children = realloc(dnode->children, dnode->num_children * sizeof(node_t *));
    // loops through each child in alphabetical order to find index for new child
    size_t i;
    for (i = 0; i < dnode->num_children - 1; i++) {
        if (strcmp(child->name, dnode->children[i]->name) <= 0) {
            break;
        }
    }
    // pushes subsequent nodes "back" to allow for insertion of new child
    for (size_t j = dnode->num_children - 1; j > i; j--) {
        dnode->children[j] = dnode->children[j - 1];
    }
    // inserts new node
    dnode->children[i] = child;
}

void print_directory_tree_helper(node_t *node, int level);

void print_directory_tree(node_t *node) {
    // calls helper function to print recursively
    // level starts at 0
    print_directory_tree_helper(node, 0);
}

void print_directory_tree_helper(node_t *node, int level) {
    // handles printing of "current" node first
    // uses level to handle spaces
    for (int i = 0; i < level * 4; i++) {
        printf(" ");
    }
    printf("%s\n", node->name);

    // checks if node is a directory
    // otherwise, moves on
    if (node->type == DIRECTORY_TYPE) {
        assert(node->type == DIRECTORY_TYPE);
        // need to cast node as directory node
        directory_node_t *dnode = (directory_node_t *) node;
        // loops through child nodes and recursively calls helper function
        for (size_t i = 0; i < dnode->num_children; i++) {
            print_directory_tree_helper(dnode->children[i], level + 1);
        }
    }
}

void create_directory_tree_helper(node_t *node, char *pathname);

void create_directory_tree(node_t *node) {
    // calls helper function to initiate recursive process
    create_directory_tree_helper(node, ".");
}

void create_directory_tree_helper(node_t *node, char *pathname) {
    // allocates memory for process of creating new pathname
    char new_pathname[sizeof(char) * (strlen(pathname) + 2 + strlen(node->name))];
    // creates new pathname for child node
    strcpy(new_pathname, pathname);
    strcat(new_pathname, "/");
    strcat(new_pathname, node->name);
    // checks if node is a file
    if (node->type == FILE_TYPE) {
        // casts as file node
        file_node_t *fnode = (file_node_t *) node;
        // opens file with inputted pathname
        FILE *file = fopen(new_pathname, "w");
        assert(file != NULL);
        // writes to file and closes file
        size_t written = fwrite(fnode->contents, 1, fnode->size, file);
        assert(written == fnode->size);
        int close_result = fclose(file);
        assert(close_result == 0);
    }
    // otherwise, node is a directory
    // recursively checks children
    else {
        // confirm node is a directory and casts as directory
        assert(node->type == DIRECTORY_TYPE);
        directory_node_t *dnode = (directory_node_t *) node;
        // creates directory with given pathname
        mkdir(new_pathname, MODE);
        // loops through children of directory node
        for (size_t i = 0; i < dnode->num_children; i++) {
            // calls helper function
            create_directory_tree_helper(dnode->children[i], new_pathname);
        }
    }
    // free(new_pathname);
}

void free_directory_tree(node_t *node) {
    if (node->type == FILE_TYPE) {
        file_node_t *fnode = (file_node_t *) node;
        free(fnode->contents);
    }
    else {
        assert(node->type == DIRECTORY_TYPE);
        directory_node_t *dnode = (directory_node_t *) node;
        for (size_t i = 0; i < dnode->num_children; i++) {
            free_directory_tree(dnode->children[i]);
        }
        free(dnode->children);
    }
    free(node->name);
    free(node);
}
