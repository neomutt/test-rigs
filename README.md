# Test Rigs

NeoMutt comes with a set of [unit tests](https://github.com/neomutt/neomutt-test-files)
that test individual functions.

Sometimes, though, it's helpful to test the behaviour of large blocks of code.
This repo contains three programs to do just that.

They were useful once; they may not be useful again :-)

For each of them, first build NeoMutt, then compile the program in the NeoMutt
source dir.

## ANSI

Test the ANSI escape sequence parsing: `ansi_color_parse()`

**Build:**
```sh
gcc -Wall -ggdb3 -O0 -I. -o test-ansi{,.c} libcolor.a libconfig.a libmutt.a -lpcre2-8 -lncursesw
```
    
**Run**:
```sh
./test-ansi SEQUENCE
```

## Maildir

Open an parse a Maildir mailbox

**Build**:
```sh
gcc -ggdb3 -O0 -I. -o test-maildir{,.c} libmaildir.a libemail.a libaddress.a libcore.a libconfig.a libmutt.a -lidn2
```

**Run**:
```sh
./test-maildir MAILDIR
```

## Menu

Test the Menu code (work in progress)

**Build**:
```sh
gcc -Wall -ggdb3 -O0 -I. -o test-menu{,.c} libmenu.a libcore.a libemail.a libconfig.a libaddress.a libmutt.a -lidn2
```

**Run**:
```sh
./test-menu
```
