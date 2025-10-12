# Get Started with a2 and JNTD
Welcome to JNTD! This guide will help you install, and prepare to use both of them!

## Quickstart: installation and configuration
The standard way of installing it is cloning the repo, with
```bash
git clone https://github.com/Lucasplaygaemes/JNTD
```
And then running the installer
```bash
./install.sh
```
But you can clone the repository and Try to just go and run Make, The dependencies are in requeriments.txt


# Use
Both a2 and JNTD use commands or shortcuts similar to Vim.

## Commands (`:` mode)

| Command | Description |
|---|---|
| `:w` | Save the current file. |
| `:w <name>` | Save with a new name. |
| `:q` | Exit the editor. |
| `:q!` | Force exit without saving. |
| `:wq` | Save and exit. |
| `:open <name>` | Open a file. |
| `:new` | Creates a blank file. |
| `:help` | Show the help screen with commands. |
| `:ksc` | Show the help screen with shortcuts. |
| `:gcc [libs]` | Compile the current file (e.g., `:gcc -lm`). |
| `![cmd]` | Execute a shell command (e.g., `!ls -l`). |
| `:rc` | Reload the current file. |
| `:rc!` | Force reload, discarding changes. |
| `:diff <f1> <f2>` | Show the difference between two files. |
| `:set paste` | Enable paste mode (disables auto-indent). |
| `:set nopaste` | Disable paste mode. |
| `:set wrap` | Enable word wrap. |
| `:set nowrap` | Disable word wrap. |
| `:term [cmd]` | Open a command in a new terminal window. |
| `:timer` | Show the work time report. |
| `:toggle_auto_indent` | Toggle auto-indent on newline. |
| `:mtw <ws_num>` | Move current window to a specific workspace. |

### LSP Commands

| Command | Description |
|---|---|
| `:lsp-status` | Check the status of the LSP server. |
| `:lsp-restart` | Restart the LSP server. |
| `:lsp-diag` | Show diagnostics (errors/warnings). |
| `:lsp-definition` | Go to the definition of a symbol. |
| `:lsp-hover` | Show information about the symbol under the cursor. |
| `:lsp-references` | List all references to a symbol. |
| `:lsp-rename <new>` | Rename the symbol under the cursor to `<new_name>`. |
| `:lsp-symbols` | List symbols in the current document. |
| `:lsp-refresh` | Force a refresh of LSP diagnostics. |
| `:lsp-check` | Force a check of LSP diagnostics. |
| `:lsp-debug` | Send a `didChange` event for debugging. |
| `:lsp-list` | Display all current diagnostics in a list. |

## Keyboard Shortcuts

### General

| Shortcut | Description |
|---|---|
| `Ctrl+F` | Search |
| `Ctrl+D` | Search for the next occurrence |
| `Ctrl+A` | Search for the previous occurrence |
| `Ctrl+G` | Open directory navigator |
| `Alt+Z` | Undo |
| `Alt+Y` | Redo |
| `Alt+B` | Open recent files navigator |

### Window & Workspace Management

| Shortcut | Description |
|---|---|
| `Ctrl+W` | Create a new workspace |
| `Alt+N` / `Alt+M` | Cycle to the previous/next workspace |
| `Alt+Enter` | Split the screen (create a new window) |
| `Alt+X` | Close the active window/split |
| `Ctrl+]` | Move to the next window |
| `Ctrl+[` | Move to the previous window |
| `Alt+.` | Cycle through layouts |
| `Alt+R` | Rotate windows within the current layout |
| `Alt`+`[1-9]` | Move the active window to the specified workspace |
| `Shift`+`Alt`+`[Symbol]`| Move the active window to a specific position |
| `Alt+D` | Open GDB in a new workspace for debugging |

### Normal Mode (Navigation)

| Shortcut | Description |
|---|---|
| `o` / `Up Arrow` | Move cursor up |
| `l` / `Down Arrow` | Move cursor down |
| `k` / `Left Arrow` | Move cursor left |
| `ç` / `Right Arrow` | Move cursor right |
| `Alt+F` / `Alt+W` | Move to the next word |
| `Alt+B` / `Alt+Q` | Move to the previous word |
| `O` / `Page Up` | Move one page up |
| `L` / `Page Down` | Move one page down |
| `K` / `Home` | Go to the beginning of the line |
| `Ç` / `End` | Go to the end of the line |
| `g` | Go to the first line of the code |
| `G` | Go to the last line of the code |

### Normal Mode (Actions)

| Shortcut | Description |
|---|---|
| `i` | Enter Insert Mode |
| `v` | Enter Visual Mode |
| `:` | Enter Command Mode |
| `Ctrl+P` | Paste from the global yank register |
| `m` | Paste from the move register (after a visual cut) |
| `Ctrl+Del` / `Ctrl+K` | Delete the current line |

### Insert Mode

| Shortcut | Description |
|---|---|
| `Esc` | Return to Normal Mode |
| `Ctrl+P` / `Tab` | Trigger autocompletion |

### Visual Mode

| Shortcut | Description |
|---|---|
| `Esc` | Return to Normal Mode |
| `s` | Start/end a selection |
| `y` | Yank (copy) the selection to the window's register |
| `Ctrl+Y` | Yank (copy) the selection to the global register |
| `m` | Cut the selection to the move register |
| `p` | Paste the yanked/cut text |
| `Alt+O` | Copy selection to the system clipboard |
| `Alt+P` | Paste from the system clipboard |
