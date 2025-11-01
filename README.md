# Simple POP3 Server

This is a **basic implementation of a POP3 server** in C. The server allows clients to connect, authenticate, and retrieve emails following the POP3 protocol.

## Features

- User authentication with `USER` and `PASS` commands
- Retrieve mailbox statistics with `STAT`
- List emails with `LIST`
- Retrieve email content with `RETR`
- Mark emails for deletion with `DELE` and restore with `RSET`
- Keep connection alive with `NOOP`
- Graceful disconnect with `QUIT`
- Multi-client support using POSIX threads
- Supports TCP/IP networking

> Note: Some POP3 commands like `TOP`, `UIDL`, and `APOP` are **not implemented**.

## Project Structure

- `server.c` – Creates a server socket, listens for connections, and handles client threads.
- `mailuser.h/c` – Defines user authentication and mailbox handling.
- `netbuffer.h/c` – Buffered network I/O for reading/writing lines.
- `util.h/c` – Utility functions used throughout the project.
- `main.c` – Entry point of the program (if separate from `server.c`).

## Requirements

- C compiler (GCC recommended)
- POSIX-compliant system (Linux, macOS)
- Basic understanding of TCP/IP networking

## How to run
- ./pop3_server <port>

- example
- ./pop3_server 1100

## Usage
- Once connected, you can use standard POP3 commands:

USER your_username
PASS your_password
STAT
LIST
RETR 1
DELE 1
RSET
NOOP
QUIT

- The server responds with standard POP3 +OK and -ERR messages.
