#include "../include/path_parser.h"
#include "../include/seefs.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

void test_root() {
    struct seefs_path_info info;
    assert(seefs_parse_path("/", &info));
    assert(info.type == SEEFS_NODE_ROOT);
    printf("test_root passed\n");
}

void test_hello() {
    struct seefs_path_info info;
    assert(seefs_parse_path("/hello", &info));
    assert(info.type == SEEFS_NODE_HELLO);
    printf("test_hello passed\n");
}

void test_users() {
    struct seefs_path_info info;
    assert(seefs_parse_path("/users", &info));
    assert(info.type == SEEFS_NODE_USERS);
    printf("test_users passed\n");
}

void test_user() {
    struct seefs_path_info info;
    assert(seefs_parse_path("/users/root", &info));
    assert(info.type == SEEFS_NODE_USER);
    assert(strcmp(info.username, "root") == 0);
    printf("test_user passed\n");
}

void test_branch_applications() {
    struct seefs_path_info info;
    assert(seefs_parse_path("/users/root/applications", &info));
    assert(info.type == SEEFS_NODE_BRANCH);
    assert(info.branch == SEEFS_BRANCH_APPLICATIONS);
    printf("test_branch_applications passed\n");
}

void test_group() {
    struct seefs_path_info info;
    assert(seefs_parse_path("/users/root/applications/bash", &info));
    assert(info.type == SEEFS_NODE_GROUP);
    assert(strcmp(info.group, "bash") == 0);
    printf("test_group passed\n");
}

void test_pid() {
    struct seefs_path_info info;
    assert(seefs_parse_path("/users/root/applications/bash/1234", &info));
    assert(info.type == SEEFS_NODE_PID);
    assert(info.pid == 1234);
    printf("test_pid passed\n");
}

void test_data_file() {
    struct seefs_path_info info;
    assert(seefs_parse_path("/users/root/applications/bash/1234/cmdline", &info));
    assert(info.type == SEEFS_NODE_DATA_FILE);
    assert(strcmp(info.data_file, "cmdline") == 0);
    printf("test_data_file passed\n");
}

void test_history() {
    struct seefs_path_info info;
    assert(seefs_parse_path("/users/root/applications/bash/1234/history", &info));
    assert(info.type == SEEFS_NODE_HISTORY);
    printf("test_history passed\n");
}

void test_timestamp() {
    struct seefs_path_info info;
    assert(seefs_parse_path("/users/root/applications/bash/1234/history/2023-10-27_10-00-00", &info));
    assert(info.type == SEEFS_NODE_TIMESTAMP);
    assert(strcmp(info.timestamp, "2023-10-27_10-00-00") == 0);
    printf("test_timestamp passed\n");
}

void test_history_file() {
    struct seefs_path_info info;
    assert(seefs_parse_path("/users/root/applications/bash/1234/history/2023-10-27_10-00-00/cmdline", &info));
    assert(info.type == SEEFS_NODE_HISTORY_FILE);
    assert(strcmp(info.data_file, "cmdline") == 0);
    printf("test_history_file passed\n");
}

void test_invalid() {
    struct seefs_path_info info;
    assert(!seefs_parse_path(NULL, &info));
    assert(!seefs_parse_path("", &info));
    assert(!seefs_parse_path("users", &info)); // missing leading slash
    assert(!seefs_parse_path("/users/root/invalid_branch", &info));
    printf("test_invalid passed\n");
}

int main() {
    test_root();
    test_hello();
    test_users();
    test_user();
    test_branch_applications();
    test_group();
    test_pid();
    test_data_file();
    test_history();
    test_timestamp();
    test_history_file();
    test_invalid();
    printf("All tests passed!\n");
    return 0;
}
