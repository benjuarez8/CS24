#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "directory_tree.h"
#include "fat16.h"

const size_t MASTER_BOOT_RECORD_SIZE = 0x20B;

void file_follow(FILE *disk, directory_node_t *node, char *file_name, 
    directory_entry_t entry);

void follow(FILE *disk, directory_node_t *node, bios_parameter_block_t bpb) {
    // processes directory_entry_t's until encounteting name starting w/ \0
    while (true) {
        // reads into entry and initializes file name from entry
        directory_entry_t entry;
        assert(fread(&entry, sizeof(directory_entry_t), 1, disk) == 1);
        char *file_name = get_file_name(entry);
        // keeps track of current position in order to reset later
        size_t reset_position = ftell(disk);
        if (file_name[0] == '\0') {
            free(file_name);
            break;
        }
        if (is_hidden(entry)) {
            free(file_name);
        }
        else {
            // moves position to offset location
            size_t offset = get_offset_from_cluster(entry.first_cluster, bpb);
            fseek(disk, offset, SEEK_SET);
            if (is_directory(entry)) {
                directory_node_t *dnode = init_directory_node(file_name);
                node_t *d_child = (node_t *) dnode;
                add_child_directory_tree(node, d_child);
                follow(disk, dnode, bpb);
            }
            else {
                file_follow(disk, node, file_name, entry);
            }
            // resets location of file to next entry
            fseek(disk, reset_position, SEEK_SET);
        }
    }
}

void file_follow(FILE *disk, directory_node_t *node, char *file_name, 
    directory_entry_t entry) {
    uint8_t *contents = malloc(sizeof(uint8_t) * entry.file_size);
    assert(fread(contents, sizeof(uint8_t), entry.file_size, disk) 
        == entry.file_size);
    file_node_t *fnode = init_file_node(file_name, entry.file_size, contents);
    node_t *f_child = (node_t *) fnode;
    add_child_directory_tree(node, f_child);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "USAGE: %s <image filename>\n", argv[0]);
        return 1;
    }

    FILE *disk = fopen(argv[1], "r");
    if (disk == NULL) {
        fprintf(stderr, "No such image file: %s\n", argv[1]);
        return 1;
    }

    bios_parameter_block_t bpb;

    // Skips past master boot record
    fseek(disk, MASTER_BOOT_RECORD_SIZE, SEEK_SET);
    // reads bios parameter block into bpb
    size_t read = fread(&bpb, sizeof(bios_parameter_block_t), 1, disk);
    assert(read == 1);
    // Skips directly to beginning of root directory entries block
    fseek(disk, get_root_directory_location(bpb), SEEK_SET);

    directory_node_t *root = init_directory_node(NULL);
    follow(disk, root, bpb);
    print_directory_tree((node_t *) root);
    create_directory_tree((node_t *) root);
    free_directory_tree((node_t *) root);

    int result = fclose(disk);
    assert(result == 0);
}
