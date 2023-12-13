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

## Header Cache packing

Test packing and unpacking flag members from `struct Email` and `struct Body`.

**Build**:
```sh
gcc -ggdb3 -O0 -I. -o hcache-packing{,.c}
```

**Run**:
```sh
./test-maildir MAILDIR
```

## Maildir

Open and parse a Maildir mailbox

**Build**:
```sh
gcc -ggdb3 -O0 -I. -o test-maildir{,.c} libmaildir.a libhcache.a libemail.a libaddress.a libcore.a libconfig.a libmutt.a -lidn2 -lpcre2-8
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

## Msgset

Test the IMAP message set

**Build**:
```sh
gcc -Wall -ggdb3 -O0 -I. -o test-msgset{,.c} libcore.a libimap.a libemail.a libaddress.a libconfig.a libmutt.a -lidn2 -lpcre2-8
```

**Run**:
```sh
./test-msgset [RANDOM-SEED]
```

## MX Alloc Memory

Test `mx_alloc_memory()`, which allocates space in the `Mailbox` for `emails` and `v2r`.

**Build**:
```sh
gcc -Wall -ggdb3 -O0 -I. -o test-mx-alloc{,.c} libcore.a libemail.a libaddress.a libconfig.a libmutt.a -lpcre2-8 -lidn2
```

**Run**:
```sh
./test-mx-alloc SIZE1 SIZE2
```
