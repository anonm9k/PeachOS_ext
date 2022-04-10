#/bin/bash
export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"
make all

#ToDoList (filesystem) --- prio: 1
#   Implement write() function in the filesystem

#ToDoList (useful software)
#   Create a text editor: can write, search text

#ToDoList (GUI)
#   OS welcome screen
#   Print can have size
#   Scrolling capability in terminal

#ToDoList(ELF loader)
#   Implement .so dynamic loading functionality