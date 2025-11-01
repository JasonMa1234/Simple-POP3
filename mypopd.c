#include "netbuffer.h"
#include "mailuser.h"
#include "server.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 1024

typedef enum state {
    Undefined,
    // TODO: Add additional states as necessary
    AUTHORIZATION,
    TRANSACTION,
    UPDATE
} State;

typedef struct serverstate {
    int fd;
    net_buffer_t nb;
    char recvbuf[MAX_LINE_LENGTH + 1];
    char *words[MAX_LINE_LENGTH];
    int nwords;
    State state;
    struct utsname my_uname;
    // TODO: Add additional fields as necessary
    char current_user[MAX_USERNAME_SIZE + 1];
    int userSuccess;
    int passSuccess;
    mail_list_t mails;
} serverstate;

static void handle_client(void *new_fd);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Invalid arguments. Expected: %s <port>\n", argv[0]);
        return 1;
    }
    run_server(argv[1], handle_client);
    return 0;
}

// syntax_error returns
//   -1 if the server should exit
//    1 otherwise
int syntax_error(serverstate *ss) {
    if (send_formatted(ss->fd, "-ERR %s\r\n", "Syntax error in parameters or arguments") <= 0) return -1;
    return 1;
}

// checkstate returns
//   -1 if the server should exit
//    0 if the server is in the appropriate state
//    1 if the server is not in the appropriate state
int checkstate(serverstate *ss, State s) {
    if (ss->state != s) {
        if (send_formatted(ss->fd, "-ERR %s\r\n", "Bad sequence of commands") <= 0) return -1;
        return 1;
    }
    return 0;
}

// All the functions that implement a single command return
//   -1 if the server should exit
//    0 if the command was successful
//    1 if the command was unsuccessful

int do_quit(serverstate *ss) {
    // Note: This method has been filled in intentionally!
    dlog("Executing quit\n");
    send_formatted(ss->fd, "+OK Service closing transmission channel\r\n");
    ss->state = Undefined;
    return -1;
}

int do_user(serverstate *ss) {
    dlog("Executing user\n");
    // TODO: Implement this function
    State expectedState = AUTHORIZATION;
    int state_result = checkstate(ss, expectedState);
    if (state_result != 0) {
        return 1;
    }
    if (ss->userSuccess == 1 && ss->passSuccess == 1) {
        send_formatted(ss->fd, "-ERR %s\r\n", "Bad sequence of commands");
        return 1;
    }
    if (ss->nwords != 2) {
        return syntax_error(ss);
    }

    char *username = ss->words[1];
    if (is_valid_user(username, NULL)) {
        strncpy(ss->current_user, username, MAX_USERNAME_SIZE);
        ss->current_user[MAX_USERNAME_SIZE] = '\0';
        ss->userSuccess = 1;
        send_formatted(ss->fd, "+OK User is valid, proceed with password\r\n");
        return 0;
    } else {
        send_formatted(ss->fd, "-ERR sorry, no mailbox for %s here\r\n", username);
        return 1;
    }
    return 0;
}

int do_pass(serverstate *ss) {
    dlog("Executing pass\n");
    // TODO: Implement this function
    
    State expectedState = AUTHORIZATION;
    int state_result = checkstate(ss, expectedState);
    if (state_result != 0) {
        return 1;
    }
    if (ss->userSuccess != 1) {
        send_formatted(ss->fd, "-ERR %s\r\n", "Bad sequence of commands");
    }
    if (ss->nwords <= 1) {
        return syntax_error(ss);
    }

    char *username = ss->current_user;

    int index;
    size_t total_len = 0;

    for (index = 1; index < ss->nwords; index++) {
        total_len += strlen(ss->words[index]) + 1;
    }

    char* password = malloc(total_len + 1);

    password[0] = '\0';
    
    for (index = 1; index < ss->nwords; index++) {
        strcat(password, ss->words[index]);
        if (index < ss->nwords - 1)
            strcat(password, " ");
    }

    if (is_valid_user(ss->current_user, password)) {
        ss->passSuccess = 1;

        // mail_list_t mails = load_user_mail(username);
        // int numEmail = mail_list_length(mails, 0);
        // size_t numOctets = mail_list_size(mails);
        strncpy(ss->current_user, username, MAX_USERNAME_SIZE);
        ss->current_user[MAX_USERNAME_SIZE] = '\0';
        ss->state = TRANSACTION;
        ss->mails = load_user_mail(username);
        send_formatted(ss->fd, "+OK Password is valid, mail loaded\r\n");
    } else {
        send_formatted(ss->fd, "-ERR Invalid password\r\n");
        return 1;
    }

    free(password);
    return 0;
}

int do_stat(serverstate *ss) {
    dlog("Executing stat\n");
    // TODO: Implement this function
    
    State expectedState = TRANSACTION;
    int state_result = checkstate(ss, expectedState);
    if (state_result != 0) {
        return 1;
    }
    if (ss->nwords != 1) {
        return syntax_error(ss);
    }

    // char *username = ss->current_user;

    mail_list_t mails = ss->mails;
    int numEmail = mail_list_length(mails, 0);
    size_t numOctets = mail_list_size(mails);
    send_formatted(ss->fd, "+OK %d %zu\r\n",numEmail ,numOctets);

    return 0;
}

int do_list(serverstate *ss) {
    dlog("Executing list\n");
    // TODO: Implement this function
    State expectedState = TRANSACTION;
    int state_result = checkstate(ss, expectedState);
    if (state_result != 0) {
        return 1;
    }
    if ((ss->nwords != 1 && ss->nwords != 2)) {
        return syntax_error(ss);
    }
    // char *username = ss->current_user;

    mail_list_t mails = ss->mails;
    int numEmail = mail_list_length(mails, 0);
    // size_t numOctets = mail_list_size(mails);
    if (ss->nwords == 1) {
        send_formatted(ss->fd, "+OK %d messages\r\n", numEmail);
        int numEmailWithDelete = mail_list_length(mails, 1);
        // int count = 1;
        for (int i = 0; i < numEmailWithDelete; i++) {
            mail_item_t emailGet = mail_list_retrieve(mails, i);
            if (emailGet != NULL) {
                size_t size = mail_item_size(emailGet);
                // send_formatted(ss->fd, "%d %zu\r\n", count, size);
                // count ++;
                send_formatted(ss->fd, "%d %zu\r\n", i + 1, size);
            }
        }
        send_formatted(ss->fd, ".\r\n");
    } else {
        // int emailNum = atoi(ss->words[1]);
        // if (emailNum < 1 || emailNum > numEmail) {
        //     send_formatted(ss->fd, "-ERR no such message\r\n");
        //     return 1;
        // }
        // int count = 0;
        // mail_item_t emailGet = NULL;

        // int found = 0;
        // int totalEmails = mail_list_length(mails, );
        // for (int i = 0; i < totalEmails; i++) {
        //     emailGet = mail_list_retrieve(mails, i);
        //     if (emailGet == NULL) continue;
        //     count++;
        //     if (count == emailNum) {
        //         size_t size = mail_item_size(emailGet);
        //         send_formatted(ss->fd, "+OK %d %zu\r\n", emailNum, size);
        //         found = 1;
        //         break;
        //     }
        // }
        // if (!found) {
        //     send_formatted(ss->fd, "-ERR no such message, only %d messages in maildrop\r\n", numEmail);
        //     return 1;
        // }
        // return 0;
        int emailNum = atoi(ss->words[1]);
        mail_item_t emailGet = NULL;

        emailGet = mail_list_retrieve(mails, emailNum - 1);
        if (emailGet == NULL) {
            send_formatted(ss->fd, "-ERR no such message\r\n");
            return 1;
        }

        size_t size = mail_item_size(emailGet);
        send_formatted(ss->fd, "+OK %d %zu\r\n", emailNum, size);
        return 0;
    }
    return 0;
}

int do_retr(serverstate *ss) {
    dlog("Executing retr\n");
    // TODO: Implement this function
    
    State expectedState = TRANSACTION;
    int state_result = checkstate(ss, expectedState);
    if (state_result != 0) {
        return 1;
    }

    if (ss->nwords != 2) {
        return syntax_error(ss);
    }

    // char *username = ss->current_user;
    mail_list_t mails = ss->mails;
    int numEmail = mail_list_length(mails, 0);

    int emailNum = atoi(ss->words[1]);
    if (emailNum < 1 || emailNum > numEmail) {
        send_formatted(ss->fd, "-ERR no such message\r\n");
        return 1;
    }
    int count = 0;
    int numEmailWithDelete = mail_list_length(mails, 1);
    for (int i = 1; i <= numEmailWithDelete; i++) {
        mail_item_t emailGet = mail_list_retrieve(mails, count);
        if (emailGet == NULL) continue;
        count++;
        if (count == emailNum) {
            FILE *emailContent = mail_item_contents(emailGet);
            char line[MAX_LINE_LENGTH];
            // size_t size = mail_item_size(emailGet);
            send_formatted(ss->fd, "+OK Message follows\r\n");  
            while (fgets(line, sizeof(line), emailContent)) {
                // if (line[0] == '.') {
                //     send_formatted(ss->fd, ".\r\n");
                // }
                // send_formatted(ss->fd, "%s", line);
                if (line[0] == '.') {
                    send_formatted(ss->fd, ".%s", line);
                } else {
                    send_formatted(ss->fd, "%s", line);
                }
            }
            send_formatted(ss->fd, ".\r\n");
            fclose(emailContent);
            return 0;
        }
    }

    send_formatted(ss->fd, "-ERR no such message\r\n");
    return 1;
}

int do_rset(serverstate *ss) {
    dlog("Executing rset\n");
    // TODO: Implement this function
    State expectedState = TRANSACTION;
    int state_result = checkstate(ss, expectedState);
    if (state_result != 0) {
        return 1;
    }
    if (ss->nwords != 1) {
        return syntax_error(ss);
    }
    
    // char *username = ss->current_user;
    mail_list_t mails = ss->mails;
    // int numEmail = mail_list_length(mails, 0);
    // int numEmailD = mail_list_length(mails, 1);
    // size_t numOctets = mail_list_size(mails);
    int recovered = mail_list_undelete(mails);
    ss->mails = mails;
    // send_formatted(ss->fd, "num email without delete: %d\r\n", numEmail);
    // send_formatted(ss->fd, "num email with delete: %d\r\n", numEmailD);
    send_formatted(ss->fd, "+OK %d message(s) restored\r\n", recovered);

    return 0;
}

int do_noop(serverstate *ss) {
    dlog("Executing noop\n");
    // TODO: Implement this function
    State expectedState = TRANSACTION;
    int state_result = checkstate(ss, expectedState);
    if (state_result != 0) {
        return 1;
    }
    if (ss->nwords != 1) {
        return syntax_error(ss);
    }
    send_formatted(ss->fd, "+OK (noop)\r\n");
    return 0;
}

int do_dele(serverstate *ss) {
    dlog("Executing dele\n");
    // TODO: Implement this function
    State expectedState = TRANSACTION;
    int state_result = checkstate(ss, expectedState);
    if (state_result != 0) {
        return 1;
    }
    if (ss->nwords != 2) {
        return syntax_error(ss);
    }

    // char *username = ss->current_user;
    mail_list_t mails = ss->mails;
    int emailNum = atoi(ss->words[1]);
    int numEmail = mail_list_length(mails, 0);
    if (emailNum > numEmail || emailNum < 1) {
        send_formatted(ss->fd, "-ERR no such message\r\n");
        return 1;
    }
    int numEmailWithDelete = mail_list_length(mails, 1);

    // int count = 0;
    for (int i = 1; i <= numEmailWithDelete; i++) {
        mail_item_t emailGet = mail_list_retrieve(mails, i - 1);
        if (emailGet == NULL) continue;
        // count ++;
        // if (count == emailNum) {
        //     mail_item_delete(emailGet);
        //     numEmail = mail_list_length(mails, 0);
        //     ss->mails = mails;
        //     send_formatted(ss->fd, "+OK Message deleted\r\n");
        //     return 0;
        // } 
        if (i == emailNum) {
            mail_item_delete(emailGet);
            numEmail = mail_list_length(mails, 0);
            ss->mails = mails;
            send_formatted(ss->fd, "+OK Message deleted\r\n");
            return 0;
        } 
    }

    send_formatted(ss->fd, "-ERR no such message\r\n");
    return 1;
}

void handle_client(void *new_fd) {
    int fd = *(int *)(new_fd);

    size_t len;
    serverstate mstate, *ss = &mstate;

    ss->fd = fd;
    ss->nb = nb_create(fd, MAX_LINE_LENGTH);
    ss->state = Undefined;
    uname(&ss->my_uname);
    // TODO: Initialize additional fields in `serverstate`, if any
    State initialState = AUTHORIZATION;
    ss->state = initialState;
    ss->current_user[0] = '\0';
    ss->userSuccess = 0;
    ss->passSuccess = 0;
    if (send_formatted(fd, "+OK POP3 Server on %s ready\r\n", ss->my_uname.nodename) <= 0) return;
    // if (send_formatted(fd, "+OK POP3 Server on norm2022 ready\r\n") <= 0) return;

    while ((len = nb_read_line(ss->nb, ss->recvbuf)) >= 0) {
        if (ss->recvbuf[len - 1] != '\n') {
            // command line is too long, stop immediately
            send_formatted(fd, "-ERR Syntax error, command unrecognized\r\n");
            break;
        }
        if (strlen(ss->recvbuf) < len) {
            // received null byte somewhere in the string, stop immediately.
            send_formatted(fd, "-ERR Syntax error, command unrecognized\r\n");
            break;
        }
        // Remove CR, LF and other space characters from end of buffer
        while (isspace(ss->recvbuf[len - 1])) ss->recvbuf[--len] = 0;

        dlog("%x: Command is %s\n", fd, ss->recvbuf);
        if (strlen(ss->recvbuf) == 0) {
            send_formatted(fd, "-ERR Syntax error, blank command unrecognized\r\n");
            break;
        }
        // Split the command into its component "words"
        ss->nwords = split(ss->recvbuf, ss->words);
        char *command = ss->words[0];
        // send_formatted(ss->fd, "current command: %s\r\n", command);
        /* TODO: Handle the different values of `command` and dispatch it to the correct implementation
         *  TOP, UIDL, APOP commands do not need to be implemented and therefore may return an error response */
        int result = 1;
        if (strcasecmp(command, "USER") == 0) {
            result = do_user(ss);
        } else if (strcasecmp(command, "PASS") == 0) {
            result = do_pass(ss);
        } else if (strcasecmp(command, "STAT") == 0) {
            result = do_stat(ss);
        } else if (strcasecmp(command, "LIST") == 0) {
            result = do_list(ss);
        } else if (strcasecmp(command, "RETR") == 0) {
            result = do_retr(ss);
        } else if (strcasecmp(command, "DELE") == 0) {
            result = do_dele(ss);
        } else if (strcasecmp(command, "NOOP") == 0) {
            result = do_noop(ss);
        } else if (strcasecmp(command, "RSET") == 0) {
            result = do_rset(ss);
        } else if (strcasecmp(command, "TOP") == 0 || 
            strcasecmp(command, "UIDL") == 0 || 
            strcasecmp(command, "APOP") == 0) {
            send_formatted(ss->fd, "-ERR command not implemented\r\n");
        } else if (strcasecmp(command, "QUIT") == 0) {
            result = do_quit(ss);
            ss->state = UPDATE;
            mail_list_destroy(ss->mails);
            break;
        } else {
            send_formatted(ss->fd, "-ERR unrecognized command %s\r\n", command);
        }

        if (result == -1) {
            break;
        }
    }
    // TODO: Clean up fields in `serverstate`, if required
    nb_destroy(ss->nb);
    close(fd);
    free(new_fd);
}
