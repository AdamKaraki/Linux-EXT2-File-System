// This function reads the first 10 lines of a file and prints them out.
void head(char* filename) {
    char buf[BLKSIZE]; // a buffer to read from the file
    int fd = open_file(filename, 2); // open the file in read-only mode
    int bytes_read = myread(fd, buf, BLKSIZE); // read the file into the buffer
    int lines = 0; // a counter for the number of lines read

    // loop through the buffer, counting the number of lines read
    for (int i = 0; i < bytes_read; i++) {
        if (buf[i] == '\n') {
            lines++;
        }
        // if we've read 10 lines, terminate the buffer and break out of the loop
        if (lines == 10) {
            buf[i+1] = '\0';
            break;
        }
    }
    printf("%s\n", buf); // print the first 10 lines of the file
    close_file(fd); // close the file
}

void tail(char* filename) {
    char cbuf[BLKSIZE]; // a cbuffer to read from the file
    int fd = open_file(filename, 0); // open the file in read-only mode
    int file_size = running->fd[fd]->inodeptr->INODE.i_size; // get the size of the file
    int start_byte = (file_size < BLKSIZE) ? 0 : (file_size - BLKSIZE); // set the starting byte for reading the file
    my_lseek(fd, start_byte); // set the file offset to the starting byte
    int bytes_read = myread(fd, cbuf, BLKSIZE); // read the last BLKSIZE bytes of the file into the cbuffer
    int lines = 0; // a counter for the number of lines read
    char* cp = cbuf + bytes_read; // a pointer to the end of the cbuffer
    
    //printf("%s\n\n\n", cbuf);
    // loop through the cbuffer backwards, counting the number of lines read
    while(cp >= cbuf) {
        if (*cp == '\n') {
            lines++;
        }
        // if we've read 10 lines, break out of the loop
        if (lines == 11) {
            break;
        }
        cp--;
    }
    printf("%s\n", cp + 1); // print the last 10 lines of the file
    close_file(fd); // close the file
}
