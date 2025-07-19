/*
 * JNTD Plugin Example
 *
 * This file serves as a basic template for creating new plugins for the JNTD framework.
 * It demonstrates the essential components required for a plugin to be successfully loaded
 * and executed by the main application.
 *
 * To create your own plugin, you can copy this file and modify the `execute_example`
 * function to implement your desired functionality. Remember to also update the
 * `register_plugin` function to reflect the new name of your plugin.
 */

#include <stdio.h>
#include "plugin.h"

/*
 * This is the core function of the plugin, where you can define its primary behavior.
 * It receives a constant character pointer `args` as a parameter, which allows you to
 * pass arguments from the main application to the plugin.
 *
 * For example, if a user runs `example hello world`, the `args` parameter will contain
 * the string "hello world", which you can then parse and use within your function.
 */
void execute_example(const char *args) {
    printf("Hello from the example plugin!\n");
    if (args) {
        printf("Received arguments: %s\n", args);
    }
}

/*
 * The `register_plugin` function is responsible for creating and configuring an instance
 * of the `Plugin` struct. This struct holds the essential information about the plugin,
 * including its name and a pointer to its execution function.
 *
 * - The `name` field defines the command that users will enter to run the plugin.
 * - The `execute` field is a function pointer that points to the plugin's main logic.
 *
 * When the main application loads the plugin, it will call this function to retrieve
 * the plugin's metadata.
 */
Plugin* register_plugin() {
    static Plugin example_plugin = {
        "example",      // The command name for this plugin
        execute_example // The function to be executed
    };
    return &example_plugin;
}
