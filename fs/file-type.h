#ifndef FILE_TYPE_H
#define FILE_TYPE_H

/**
 * File type corresponds with the output of ls. Directory is not handled.
 *  D The entry is a door.
 *  l The entry is a symbolic link.
 *  b The entry is a block special file.
 *  c The entry is a character special file.
 *  p The entry is a FIFO (or "named pipe") special file.
 *  P The entry is an event port.
 *  s The entry is an AF_UNIX address family socket.
 *  - The entry is an ordinary file.
 *  d The entry is a directory.
 */
enum FileType {
  DOOR              = 0, 
  SYMBOLIC_LINK     = 1, 
  BLOCK_SPECIAL     = 2, 
  CHARACTER_SPECIAL = 3,
  FIFO_SPECIAL      = 4,
  EVENT_PORT        = 5,
  UNIX_SOCKET       = 6,
  REGULAR           = 7,
  DIRECTORY         = 8 // don't expect to this in database
};

#endif


