/*** include ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

/*** data ***/

struct termios original_terminal;

/*** terminal ***/

void die(const char *function_used){
  perror(function_used);
  exit(1);
}

void disableRawMode(){
  if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_terminal) == -1) die("tcsetattr");
}

void enableRawMode(){
  if(tcgetattr(STDIN_FILENO, &original_terminal) == -1) die("tcgetattr");
  atexit(disableRawMode);

  struct termios raw_mode = original_terminal;
  raw_mode.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw_mode.c_oflag &= ~(OPOST);
  raw_mode.c_cflag |= (CS8);
  raw_mode.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw_mode.c_cc[VMIN] = 0;
  raw_mode.c_cc[VTIME] = 1;

  if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_mode) == -1) die("tcsetattr");
}

/*** init ***/

int main(){
  enableRawMode();

  while(1){
    char input_c = '\0';
    if(read(STDIN_FILENO, &input_c, 1) == -1 && errno != EAGAIN) die("read");
    if(iscntrl(input_c)){
      printf("%d\r\n", input_c);
    } else {
      printf("%d ('%c')\r\n", input_c, input_c);
    }
    if(input_c == 'q') break;
  }
  return 0;
}
