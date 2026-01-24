/*** include ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>

/*** defines ***/

#define CTRL_KEY(key) ((key) & 0x1f)

#define KILO_VERSION "0.0.1"

enum editorKey{
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN
};

/*** data ***/

struct editorConfig {
  int cx, cy; // cursor position
  int screenrows;
  int screencols;
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

int editorReadKey(){
  int num_read;
  char in_c;
  while((num_read = read(STDIN_FILENO, &in_c, 1)) != 1){
    if(num_read == -1 && errno != EAGAIN) die("read");
  }

  if (in_c == '\x1b'){
    char sequence[3];

    if (read(STDIN_FILENO, &sequence[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &sequence[1], 1) != 1) return '\x1b';

    if (sequence[0] == '['){
      switch (sequence[1]){
        case 'A': return ARROW_UP;
        case 'B': return ARROW_DOWN;
        case 'C': return ARROW_RIGHT;
        case 'D': return ARROW_LEFT;
      }
    }

    return '\x1b';
  } else {
    return in_c;
  }
}

int getCursorPosition(int *rows, int *cols){
  char buf[32];
  unsigned int i = 0;

  if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

  while(i < sizeof(buf) - 1){
    if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if(buf[i] == 'R') break;
    i++;
  }
  buf[i] = '\0';

  if(buf[0] != '\x1b' || buf[1] != '[') return -1;
  if(sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

  return 0;
 }

int getWindowSize(int *rows, int *cols){
  struct winsize ws;

  if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
    if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** string buffer ***/ 

struct stringBuf {
  char *b;
  int len;
};

#define STRBUF_INIT {NULL, 0}

void sbufAppend(struct stringBuf *sb, const char *s, int len){
  char *new = realloc(sb->b, sb->len + len);

  if (new==NULL) return;
  memcpy(&new[sb->len], s, len);
  sb->b = new;
  sb->len += len;
}

void sbufFree(struct stringBuf *sb) {
  free(sb->b);
}

/*** output ***/

void editorDrawRows(struct stringBuf *sb){
  int y;
  for(y = 0; y < Edt.screenrows; y++){
    // Welcome Message
    if (y == Edt.screenrows / 3){
      char welcome[80];
      int welcomelen = snprintf(welcome, sizeof(welcome), "Kilo Editor -- Version %s", KILO_VERSION);
      if (welcomelen > Edt.screencols) welcomelen = Edt.screencols;
      // padding to center
      int padding = (Edt.screencols - welcomelen) / 2;
      if (padding) {
        sbufAppend(sb, "|", 1);
        padding--;
      }
      while (padding--) sbufAppend(sb, " ", 1);
      sbufAppend(sb, welcome, welcomelen);
    } else {
      sbufAppend(sb, "|", 1); 
    }

    sbufAppend(sb, "\x1b[K", 3);
    if(y<Edt.screenrows - 1){
      sbufAppend(sb, "\r\n", 2);
    }
  }
}

void editorRefreshScreen(){
  struct stringBuf sb = STRBUF_INIT;

  sbufAppend(&sb, "\x1b[?25l", 6);
  sbufAppend(&sb, "\x1b[H", 3);

  editorDrawRows(&sb);

  char buffer[32];
  snprintf(buffer, sizeof(buffer), "\x1b[%d;%dH", Edt.cy + 1, Edt.cx + 1);
  sbufAppend(&sb, buffer, strlen(buffer));

  sbufAppend(&sb, "\x1b[?25h", 6);

  write(STDOUT_FILENO, sb.b, sb.len);
  sbufFree(&sb);
}

/*** input ***/

void editorMoveCursor(int key){
  switch (key) {
    case ARROW_LEFT:
      if (Edt.cx != 0){
        Edt.cx--;
      }
      break;
    case ARROW_RIGHT:
      if (Edt.cx != Edt.screencols - 1) {
        Edt.cx++;
      }
      break;
    case ARROW_UP:
      if (Edt.cy != 0){
        Edt.cy--;
      }
      break;
    case ARROW_DOWN:
      if (Edt.cy != Edt.screenrows - 1){
        Edt.cy++;
      }
      break;
  }
}

void editorProcessKeyPress(){
  int in_c = editorReadKey();

  switch (in_c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(in_c);
      break;
  }
}

/*** init ***/

void initEditor(){
  Edt.cx = 0;
  Edt.cy = 0;
  if(getWindowSize(&Edt.screenrows, &Edt.screencols) == -1) die("getWindowSize");
}

int main(){
  enableRawMode();
  initEditor();

  while(1){
    editorRefreshScreen();
    editorProcessKeyPress();
  }
  return 0;
}
