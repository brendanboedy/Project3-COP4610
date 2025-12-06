# FAT32 File System Utility

[Description]

## Group Members

- **Brendan Boedy**: bsb23a@fsu.edu
- **Alexander Schneier**: js19@fsu.edu
- **Carson Cary**: cc22bc@fsu.edu

## Division of Labor

### Part 1: Mounting the Image

- **Responsibilities**: [Description]
- **Assigned to**: Brendan Boedy, Alexander Schneier

### Part 2: Navigation

- **Responsibilities**: [Description]
- **Assigned to**: Alexander Schneier, Carson Cary

### Part 3: Create

- **Responsibilities**: [Description]
- **Assigned to**: Carson Cary, Brendan Boedy

### Part 4: Read

- **Responsibilities**: [Description]
- **Assigned to**: Brendan Boedy, Alexander Schneier

### Part 5: Update

- **Responsibilities**: [Description]
- **Assigned to**: Alexander Schneier, Carson Cary

### Part 6: Delete

- **Responsibilities**: [Description]
- **Assigned to**: Carson Cary, Brendan Boedy

### Extra Credit

NA

## File Listing

```
.
├── include
│   ├── commands.h
│   ├── create.h
│   ├── delete.h
│   ├── fatter.h
│   ├── imager.h
│   ├── lexer.h
│   ├── navigate.h
│   ├── read.h
│   └── update.h
├── Makefile
├── README.md
└── src
    ├── commands.c
    ├── create.c
    ├── delete.c
    ├── fatter.c
    ├── imager.c
    ├── lexer.c
    ├── navigate.c
    ├── read.c
    └── update.c
```

## How to Compile & Execute

### Compilation

```bash
make clean
make
```

### Execution

```bash
make clean
make
./bin/filesys fat32.img
```

make sure you have the `fat32.img` file

[Description]
