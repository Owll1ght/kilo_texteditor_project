/*** include ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

/*** defines ***/

#define CTRL_KEY(key) ((key) & 0x1f)

/*** data ***/

struct editorConfig {
  struct termios original_terminal;
};

struct editorConfig Edt;

/*** terminal ***/

void die(const char *function_used){
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(function_used);
  exit(1);
}

void disableRawMode(){
  if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &Edt.original_terminal) == -1) die("tcsetattr");
}

void enableRawMode(){
  if(tcgetattr(STDIN_FILENO, &Edt.original_terminal) == -1) die("tcgetattr");
  atexit(disableRawMode);

  struct termios raw_mode = Edt.original_terminal;
  raw_mode.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw_mode.c_oflag &= ~(OPOST);
  raw_mode.c_cflag |= (CS8);
  raw_mode.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw_mode.c_cc[VMIN] = 0;
  raw_mode.c_cc[VTIME] = 1;

  if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_mode) == -1) die("tcsetattr");
}

char editorReadKey(){
  int num_read;
  char in_c;
  while((num_read = read(STDIN_FILENO, &in_c, 1)) != 1){
    if(num_read == -1 && errno != EAGAIN) die("read");
  }
  return in_c;
}

/*** output ***/

void editorDrawRows(){
  int y;
  for(y = 0; y < 24; y++){
    write(STDOUT_FILENO, "|\r\n", 3);
    printf("|\r\n");
  }
}

void editorRefreshScreen(){
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  editorDrawRows();

  write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** input ***/

void editorProcessKeyPress(){
  char in_c = editorReadKey();

  switch (in_c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;
  }
}

/*** init ***/

int main(){
  enableRawMode();

  while(1){
    editorRefreshScreen();
    editorProcessKeyPress();
  }
  return 0;
}
