#include <keyboard.h>
#include <ports.h>
#include <vga.h>
#include <multiboot.h>
#include <string.h>
#include <stdint.h>

bool capsLock = false;
bool enter = false;
bool capsOn = false;
int barrier = 0;
char *input;

void handler(struct InterruptRegisters *regs)
{
    char scanCode = inb(0x60) & 0x7F;
	  char press = inb(0x60) & 0x80;
    //kprint(lowercase[0x22]);

    switch (scanCode)
    {
        case 1:
        case 29:
        case 56:
        case 59:
        case 60:
        case 61:
        case 62:
        case 63:
        case 64:
        case 65:
        case 66:
        case 67:
        case 68:
        case 87:
        case 88:
            break;
        case 42:
            if (press == 0) {
                capsOn = true;
            }else{
                capsOn = false;
            }
            break;
        case 58:
            if (!capsLock && press == 0)
            {
                capsLock = true;
            }
            else if (capsLock && press == 0)
            {
                capsLock = false;
            }
            break;
        case 40:
            if (press)
                kprint("\'");
                join(input, '\'');
            break;
        case 0x29:
            if (press)
                kprint("~");
                join(input, '~');
            break;
        case 0x0E:
            if (press == 0)
                if (get_cursor_x() > barrier)
                {
                  backspace_func(input);
                  kprint("\b");
                }
            break;
        case 0x1C:
            if (press == 0)
            {
                kprint("\n");
                //input[0] = '\0';
                enter = true;
                //irq_uninstall_handler(1);
                return;
            }
            else;    
                
            break;
        case 0x0F:
            if (press);
            break;
        default:
            if (press == 0)
            {
                if (capsOn || capsLock)
                {
                    kputchar(get_acsii_high(scanCode), 0x0f);
                    // _globl_cursor.x++;
                    join(input, get_acsii_high(scanCode));
                }
                else
                {
                    kputchar(get_acsii_low(scanCode), 0x0f);
                    // _globl_cursor.x++;
                    join(input, get_acsii_low(scanCode));
                }
            }
            // else{
            //   int old = get_cursor();
            //   set_cursor(150);
            //   kprintc("     ", 0x70);
            //   set_cursor(old);
            // }
            break;
    }
}

int backspace_func(char buffer[])
{
    int len = strlen(buffer);
    if (len > 0)
    {
        buffer[len - 1] = '\0';
        return 1;
    }
    else
    {
        return 0;
    }
}

void get_string(char *buffer)
{
    barrier = get_cursor_x();
    strnone(input);
    while (enter != true)
    {
      irq_install_handler(1, &handler);
    }
    enter = false;
    strncpy(buffer, input, strlen(buffer));
    
}
void agent_handler(struct InterruptRegisters *regs);

void agent_get_string(char *buffer)
{
    strnone(input);
    while (enter != true)
    {
      irq_install_handler(1, &agent_handler);
    }
    enter = false;
    strncpy(buffer, input, strlen(buffer));
}

void agent_handler(struct InterruptRegisters *regs)
{
      char scanCode = inb(0x60) & 0x7F;
	  char press = inb(0x60) & 0x80;
    //kprint(lowercase[0x22]);

    switch (scanCode)
    {
        case 1:
        case 29:
        case 56:
        case 59:
        case 60:
        case 61:
        case 62:
        case 63:
        case 64:
        case 65:
        case 66:
        case 67:
        case 68:
        case 87:
        case 88:
            break;
        case 42:
            if (press == 0) {
                capsOn = true;
            }else{
                capsOn = false;
            }
            break;
        case 58:
            if (!capsLock && press == 0)
            {
                capsLock = true;
            }
            else if (capsLock && press == 0)
            {
                capsLock = false;
            }
            break;
        case 40:
            if (press)
                kputchar('*', 0x0f);
                join(input, '\'');
            break;
        case 0x29:
            if (press)
                kputchar('*', 0x0f);
                join(input, '~');
            break;
        case 0x0E:
            if (press == 0)
                kputchar('*', 0x0f);
                backspace_func(input);
            break;
        case 0x1C:
            if (press == 0)
            {
                kprint("\n");
                //input[0] = '\0';
                enter = true;
                break;
            }
            else;    
                
            break;
        case 0x0F:
            if (press);
            break;
        default:
            if (press == 0)
            {
                if (capsOn || capsLock)
                {
                    kputchar('*', 0x0f);
                    join(input, get_acsii_high(scanCode));
                }
                else
                {
                    kputchar('*', 0x0f);
                    join(input, get_acsii_low(scanCode));
                }
            }
            break;
    }
    
}

char get_acsii_low(char code)
{
     switch (code)
  {
  case KEY_A:
    return 'a';
  case KEY_B:
    return 'b';
  case KEY_C:
    return 'c';
  case KEY_D:
    return 'd';
  case KEY_E:
    return 'e';
  case KEY_F:
    return 'f';
  case KEY_G:
    return 'g';
  case KEY_H:
    return 'h';
  case KEY_I:
    return 'i';
  case KEY_J:
    return 'j';
  case KEY_K:
    return 'k';
  case KEY_L:
    return 'l';
  case KEY_M:
    return 'm';
  case KEY_N:
    return 'n';
  case KEY_O:
    return 'o';
  case KEY_P:
    return 'p';
  case KEY_Q:
    return 'q';
  case KEY_R:
    return 'r';
  case KEY_S:
    return 's';
  case KEY_T:
    return 't';
  case KEY_U:
    return 'u';
  case KEY_V:
    return 'v';
  case KEY_W:
    return 'w';
  case KEY_X:
    return 'x';
  case KEY_Y:
    return 'y';
  case KEY_Z:
    return 'z';
  case KEY_1:
    return '1';
  case KEY_2:
    return '2';
  case KEY_3:
    return '3';
  case KEY_4:
    return '4';
  case KEY_5:
    return '5';
  case KEY_6:
    return '6';
  case KEY_7:
    return '7';
  case KEY_8:
    return '8';
  case KEY_9:
    return '9';
  case KEY_0:
    return '0';
  case KEY_MINUS:
    return '-';
  case KEY_EQUAL:
    return '=';
  case KEY_SQUARE_OPEN_BRACKET:
    return '[';
  case KEY_SQUARE_CLOSE_BRACKET:
    return ']';
  case KEY_SEMICOLON:
    return ';';
  case KEY_BACKSLASH:
    return '\\';
  case KEY_COMMA:
    return ',';
  case KEY_DOT:
    return '.';
  case KEY_FORESLHASH:
    return '/';
  case KEY_SPACE:
    return ' ';
  default:
    return 0;
  }
}
char get_acsii_high(char code)
{
     switch (code)
  {
  case KEY_A:
    return 'A';
  case KEY_B:
    return 'B';
  case KEY_C:
    return 'C';
  case KEY_D:
    return 'D';
  case KEY_E:
    return 'E';
  case KEY_F:
    return 'F';
  case KEY_G:
    return 'G';
  case KEY_H:
    return 'H';
  case KEY_I:
    return 'I';
  case KEY_J:
    return 'J';
  case KEY_K:
    return 'K';
  case KEY_L:
    return 'L';
  case KEY_M:
    return 'M';
  case KEY_N:
    return 'N';
  case KEY_O:
    return 'O';
  case KEY_P:
    return 'P';
  case KEY_Q:
    return 'Q';
  case KEY_R:
    return 'R';
  case KEY_S:
    return 'S';
  case KEY_T:
    return 'T';
  case KEY_U:
    return 'U';
  case KEY_V:
    return 'V';
  case KEY_W:
    return 'W';
  case KEY_X:
    return 'X';
  case KEY_Y:
    return 'Y';
  case KEY_Z:
    return 'Z';
  case KEY_1:
    return '!';
  case KEY_2:
    return '@';
  case KEY_3:
    return '#';
  case KEY_4:
    return '$';
  case KEY_5:
    return '%';
  case KEY_6:
    return '^';
  case KEY_7:
    return '&';
  case KEY_8:
    return '*';
  case KEY_9:
    return '(';
  case KEY_0:
    return ')';
  case KEY_MINUS:
    return '_';
  case KEY_EQUAL:
    return '+';
  case KEY_SQUARE_OPEN_BRACKET:
    return '{';
  case KEY_SQUARE_CLOSE_BRACKET:
    return '}';
  case KEY_SEMICOLON:
    return ':';
  case KEY_BACKSLASH:
    return '|';
  case KEY_COMMA:
    return '<';
  case KEY_DOT:
    return '>';
  case KEY_FORESLHASH:
    return '?';
  case KEY_SPACE:
    return ' ';
  default:
    return 0;
  }
}