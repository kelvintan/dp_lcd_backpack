// Program use to control Dangerous Prototypes LCD backpack (FTDI-based).
// http://dangerousprototypes.com/docs/USB_Universal_LCD_backpack
//

#include <stdio.h>
#include <ftdi.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#define D0  1
#define D1  (1<<1)
#define D2  (1<<2)
#define D3  (1<<3)
#define D4  (1<<4)
#define D5  (1<<5)
#define D6  (1<<6)
#define D7  (1<<7)
#define D8  (1<<8)
#define D9  (1<<9)
#define D10 (1<<10)

#define SR_SER   D0
#define SR_SCLK  D3
#define SR_nSCLR D4
#define SR_RCLK  D2
#define SR_nOE   D5

#define BL_nEN D6
#define BL_nOC D7

#define LCD_RS   D8
#define LCD_RnW  D9
#define LCD_E    D10

#define LCD_INSTR 0
#define LCD_DATA  1
#define LCD_WRITE 0
#define LCD_READ  1 //read cannot be supported under current hardware

#define MAX_STR_LEN 0x40

//LCD instruction related
#define CLR_DISPLAY_INSTR    0x01
#define RTH_INSTR            0x02
#define ENTRY_MODE_INSTR     0x04
#define INC_DDRAM_ADDR  0x02
#define SHIFT_LEFT      0x01

#define DISPLAY_CTRL_INSTR   0x08
#define DISPLAY_ON      0x04
#define CURSOR_ON       0x02
#define BLINK           0x01

#define SHIFT_INSTR          0x10
#define CURSOR_SHIFT    0x08
#define DISPLAY_SHIFT   0x04

#define FUNC_SET_INSTR       0x20
#define DATA_8_BIT      0x10
#define DISPLAY_2_LINE  0x08
#define FONT_5x10       0x04

#define SET_CGRAM_ADDR_INSTR 0x40
#define SET_DDRAM_ADDR_INSTR 0x80


typedef unsigned char byte;
typedef struct {
  byte entry_mode;
  byte display_ctrl;
  byte shift;
  byte function;
} lcd_state;

//helper function for memory allocation
#define MALLOC(buf, type, n, err_code) {				\
    if((buf = (type *)malloc(sizeof(type) * n))==NULL) {		\
      fprintf(stderr, "malloc error: File:%s, Line:%d\n", __FILE__, __LINE__); \
      return(err_code);}						\
  }

int sr_clear(struct ftdi_context *ftdic, byte *ftdi_pin)
{
  int ret;

  *ftdi_pin&=~SR_nSCLR;

  if((ret=ftdi_set_bitmode(ftdic, 0xFF, BITMODE_BITBANG))<0){
    fprintf(stderr, "Error: %s\n",ftdi_get_error_string(ftdic));
    return(ret);
  }
  if((ret=ftdi_write_data(ftdic, ftdi_pin, 1))<0){
    fprintf(stderr, "Error: %s\n",ftdi_get_error_string(ftdic));
    return(ret);
  }

  return(0);
}

int sr_unclear(struct ftdi_context *ftdic, byte *ftdi_pin)
{
  int ret;

  *ftdi_pin|=SR_nSCLR;

  if((ret=ftdi_set_bitmode(ftdic, 0xFF, BITMODE_BITBANG))<0){
    fprintf(stderr, "Error: %s\n",ftdi_get_error_string(ftdic));
    return(ret);
  }
  if((ret=ftdi_write_data(ftdic, ftdi_pin, 1))<0){
    fprintf(stderr, "Error: %s\n",ftdi_get_error_string(ftdic));
    return(ret);
  }

  return(0);
}

int backlight(struct ftdi_context *ftdic, byte *ftdi_pin, byte flag)
{
  int ret;
  byte buf;

  if(flag){
    *ftdi_pin&=~BL_nEN;
    *ftdi_pin&=~BL_nOC;
  }else{
    *ftdi_pin|=BL_nEN;
    *ftdi_pin|=BL_nOC;
  }

  if((ret=ftdi_set_bitmode(ftdic, 0xFF, BITMODE_BITBANG))<0){
    fprintf(stderr, "Error: %s\n",ftdi_get_error_string(ftdic));
    return(ret);
  }
  if((ret=ftdi_write_data(ftdic, ftdi_pin, 1))<0){
    fprintf(stderr, "Error: %s\n",ftdi_get_error_string(ftdic));
    return(ret);
  }

  return(0);
}

int sr_output_enable(struct ftdi_context *ftdic, byte *ftdi_pin)
{
  int ret;

  *ftdi_pin&=~SR_nOE;

  if((ret=ftdi_set_bitmode(ftdic, 0xFF, BITMODE_BITBANG))<0){
    fprintf(stderr, "Error: %s\n",ftdi_get_error_string(ftdic));
    return(ret);
  }
  if((ret=ftdi_write_data(ftdic, ftdi_pin, 1))<0){
    fprintf(stderr, "Error: %s\n",ftdi_get_error_string(ftdic));
    return(ret);
  }

  return(0);
}

int sr_output_disable(struct ftdi_context *ftdic, byte *ftdi_pin)
{
  int ret;

  *ftdi_pin|=SR_nOE;

  if((ret=ftdi_set_bitmode(ftdic, 0xFF, BITMODE_BITBANG))<0){
    fprintf(stderr, "Error: %s\n",ftdi_get_error_string(ftdic));
    return(ret);
  }
  if((ret=ftdi_write_data(ftdic, ftdi_pin, 1))<0){
    fprintf(stderr, "Error: %s\n",ftdi_get_error_string(ftdic));
    return(ret);
  }

  return(0);
}

int sr_tx_word(struct ftdi_context *ftdic, byte *ftdi_pin, uint16_t data)
{
  byte buf[80]={0};
  unsigned int i, j;
  int ret;

  //set RCLK low
  buf[0] = *ftdi_pin & (~SR_RCLK);

  //shift in bit by bit, starting from MSB
  //set SCLK to low
  //set SER to required value
  //set SCLK to high
  for(i=0; i<16; i++) {
    j=i*4;
    buf[j+1]=buf[j] & (~SR_SCLK);
    if(data & (1<<(15-i))) buf[j+2]=buf[j+1] | SR_SER;
    else buf[j+2]=buf[j+1] & (~SR_SER);
    buf[j+3]=buf[j+2] | SR_SCLK;
    buf[j+4]=buf[j+3];  //additional cycle to make the SCLK more uniform
  }

  j+=4;
  //set RCLK high
  buf[j+1]=buf[j] | SR_RCLK;
  j+=2; //now j represent the number of bytes used in buf[]

  //for(i=0; i<j; i++) printf("%02x ", buf[i]);
  //printf("\n");

  if((ret=ftdi_set_bitmode(ftdic, 0xFF, BITMODE_BITBANG))<0){
    fprintf(stderr, "Error: %s\n",ftdi_get_error_string(ftdic));
    return(ret);
  }
  if((ret=ftdi_write_data(ftdic, &buf[0], j))<0){
    fprintf(stderr, "Error: %s\n",ftdi_get_error_string(ftdic));
    return(ret);
  }

  *ftdi_pin=buf[j-1];

  return(0);
}

//Write to LCD
//set RS depending on data(1) or instr(0) transfer
//set E high
//check busy flag (not done)
//read/write data/instr
//set E low
int lcd_write(struct ftdi_context *ftdic, byte *ftdi_pin, byte rs, byte data)
{
  uint16_t lcd_pin=0;

  //set RS accordingly
  if(rs) lcd_pin|=LCD_RS;
  lcd_pin |= ((data&0x1F) << 11) | ((data&0xe0) >>5);
  sr_tx_word(ftdic, ftdi_pin, lcd_pin);

  //toggle E, i.e. set E high then low again
  lcd_pin|=LCD_E;
  sr_tx_word(ftdic, ftdi_pin, lcd_pin);

  lcd_pin&=~LCD_E;
  sr_tx_word(ftdic, ftdi_pin, lcd_pin);

  return(0);
}

void lcd_clr_display(struct ftdi_context *ftdic, byte *ftdi_pin)
{
  lcd_write(ftdic, ftdi_pin, LCD_INSTR, CLR_DISPLAY_INSTR);
}

void lcd_inc_ddram_addr(struct ftdi_context *ftdic, byte *ftdi_pin, 
			lcd_state *lcd, byte flag)
{
  if(flag) lcd->entry_mode |= INC_DDRAM_ADDR;
  else lcd->entry_mode &=~ INC_DDRAM_ADDR;
  lcd_write(ftdic, ftdi_pin, LCD_INSTR, lcd->entry_mode);
}

void lcd_shift_left(struct ftdi_context *ftdic, byte *ftdi_pin, 
			lcd_state *lcd, byte flag)
{
  if(flag) lcd->entry_mode |=SHIFT_LEFT;
  else lcd->entry_mode &=~SHIFT_LEFT;
  lcd_write(ftdic, ftdi_pin, LCD_INSTR, lcd->entry_mode);
}

void lcd_display_on(struct ftdi_context *ftdic, byte *ftdi_pin, lcd_state *lcd, byte flag)
{
  if(flag) lcd->display_ctrl|=DISPLAY_ON;
  else lcd->display_ctrl&=~DISPLAY_ON;
  lcd_write(ftdic, ftdi_pin, LCD_INSTR, lcd->display_ctrl);
}

void lcd_cursor_on(struct ftdi_context *ftdic, byte *ftdi_pin, 
			lcd_state *lcd, byte flag)
{
  if(flag) lcd->display_ctrl|=CURSOR_ON;
  else lcd->display_ctrl&=~CURSOR_ON;
  lcd_write(ftdic, ftdi_pin, LCD_INSTR, lcd->display_ctrl);
}

void lcd_blink(struct ftdi_context *ftdic, byte *ftdi_pin, 
			lcd_state *lcd, byte flag)
{
  if(flag) lcd->display_ctrl|=BLINK;
  else lcd->display_ctrl&=~BLINK;
  lcd_write(ftdic, ftdi_pin, LCD_INSTR, lcd->display_ctrl);
}

void lcd_cursor_shift(struct ftdi_context *ftdic, byte *ftdi_pin, 
			lcd_state *lcd, byte flag)
{
  if(flag) lcd->shift|=CURSOR_SHIFT;
  else lcd->shift&=~CURSOR_SHIFT;
  lcd_write(ftdic, ftdi_pin, LCD_INSTR, lcd->shift);
}

void lcd_display_shift(struct ftdi_context *ftdic, byte *ftdi_pin, 
			lcd_state *lcd, byte flag)
{
  if(flag) lcd->shift|=DISPLAY_SHIFT;
  else lcd->shift&=~DISPLAY_SHIFT;
  lcd_write(ftdic, ftdi_pin, LCD_INSTR, lcd->shift);
}

void lcd_bus_width(struct ftdi_context *ftdic, byte *ftdi_pin, 
			lcd_state *lcd, byte flag)
{
  if(flag) lcd->function|=DATA_8_BIT;
  else lcd->function&=~DATA_8_BIT;
  lcd_write(ftdic, ftdi_pin, LCD_INSTR, lcd->function);
}

void lcd_num_of_line(struct ftdi_context *ftdic, byte *ftdi_pin, 
			lcd_state *lcd, byte flag)
{
  if(flag) lcd->function|=DISPLAY_2_LINE;
  else lcd->function&=~DISPLAY_2_LINE;
  lcd_write(ftdic, ftdi_pin, LCD_INSTR, lcd->function);
}

void lcd_font(struct ftdi_context *ftdic, byte *ftdi_pin, 
			lcd_state *lcd, byte flag)
{
  if(flag) lcd->function|=FONT_5x10;
  else lcd->function&=~FONT_5x10;
  lcd_write(ftdic, ftdi_pin, LCD_INSTR, lcd->function);
}

//init lcd
//clear display: set IR=1
void lcd_init(struct ftdi_context *ftdic, byte *ftdi_pin, lcd_state *lcd)
{

  //clear display
  lcd_clr_display(ftdic, ftdi_pin);
  
  //set function
  //8 bit, 1 line, 5x8 fonts
  lcd_bus_width(ftdic, ftdi_pin, lcd, 1);
  lcd_num_of_line(ftdic, ftdi_pin, lcd, 0);
  lcd_font(ftdic, ftdi_pin, lcd, 0);

  //Display on
  lcd_display_on(ftdic ,ftdi_pin, lcd, 1);
}


//lcd display text
int lcd_display_text(struct ftdi_context *ftdic, byte *ftdi_pin, lcd_state *lcd,
		     char *msg, byte len, byte line)
{
  byte i;
  
  //line can only be 1 or 2
  if(line==1) lcd_write(ftdic, ftdi_pin, LCD_INSTR, 0x80);
  else if (line==2) lcd_write(ftdic, ftdi_pin, LCD_INSTR, 0x80|0x40);
  else {
    fprintf(stderr, 
	    "Error displaying text. Only 1 or 2 line display supported\n");
    return(-1);
  }

  //  if(len>16) {
  //  fprintf(stderr, "Error displaying text. Maximum no. of char is 16\n");
  //  return(-2);
  //}

  for(i=0; i<len; i++)
    lcd_write(ftdic, ftdi_pin, LCD_DATA, msg[i]);

  //turn on display
  lcd_display_on(ftdic, ftdi_pin, lcd, 1);
  return(0);
}

void usage(char *cmd)
{
  fprintf(stderr, 
"%s [options]                                                        \
\n								     \
\nOptions:							     \
\n-I      : initialise LCD (set LCD to 8 bit, 1 line, 5x8 fonts)     \
\n-R      : Clear display					     \
\n-m      : Specify message to display				     \
\n-n [1|2]: specify which line to display message, default to 1	     \
\n-B <0|1>: 0: backlight off, 1: backlight on			     \
\n-t <sec>: duration (in secs) for backlight (0 (default)=>always on)\
\n-i <0|1>: 0: do not increment DDRAM addr, 1: increment DDRAM addr  \
\n-L <0|1>: 0: do not shift, 1: shift left			     \
\n-d <0|1>: 0: display off, 1: display on			     \
\n-c <0|1>: 0: cursor off, 1: cursor on				     \
\n-b <0|1>: 0: do not blink, 1: blink				     \
\n-C <0|1>: 0: do not shift cursor, 1: shift cursor		     \
\n-D <0|1>: 0: do not shift display, 1: shift display		     \
\n-w <0|1>: 0: 4-bit data bus, 1: 8-bit data bus		     \
\n-l <0|1>: 0: single line display, 1: dual line display	     \
\n-f <0|1>: 0: 5x8 fonts, 1: 5x10 font                               \
\n-h      : print this help message                                  \
\n", cmd);
}

int main(int argc, char **argv)
{
    struct ftdi_context ftdic;
    int f,c;
    byte m_set=0, B_set=0;
    byte *ftdi_pin, line=1, backlight_on;
    unsigned int time=0;
    char *msg;
    lcd_state *lcd;
    
    //Let make sure that we can open the device before doing anything
    if (ftdi_init(&ftdic) < 0) {
        fprintf(stderr, "ftdi_init failed\n");
        return EXIT_FAILURE;
    }

    f = ftdi_usb_open(&ftdic, 0x0403, 0x6001);

    if (f < 0 && f != -5) {
        fprintf(stderr, "unable to open ftdi device: %d (%s)\n", 
		f, ftdi_get_error_string(&ftdic));
        exit(-1);
    }

    //some initialisation tasks
    MALLOC(msg, char, MAX_STR_LEN+1, -1);
    MALLOC(ftdi_pin, byte, 1, -1);
    *ftdi_pin=0;

    MALLOC(lcd, lcd_state, 1, -1);
    lcd->entry_mode=ENTRY_MODE_INSTR;
    lcd->display_ctrl=DISPLAY_CTRL_INSTR;
    lcd->shift=SHIFT_INSTR;
    lcd->function=FUNC_SET_INSTR | DATA_8_BIT;

    sr_clear(&ftdic, ftdi_pin);
    sr_output_enable(&ftdic, ftdi_pin);
    sr_unclear(&ftdic, ftdi_pin);

    while((c=getopt(argc, argv, "IRm:n:B:t:i:L:d:c:b:C:D:w:l:f:h"))!=-1) {
      switch(c) {
      case 'I':
	//initialise LCD
	lcd_init(&ftdic, ftdi_pin, lcd);
	break;
      case 'R':
	//clear display
	lcd_clr_display(&ftdic, ftdi_pin);
	break;
      case 'm':
	//specify the message to display
	strncpy(msg, optarg, MAX_STR_LEN);
	m_set=1;
	break;
      case 'n':
	//indicates which line to display the message
	line=(uint8_t)strtoul(optarg, NULL, 0);
	break;
      case 'B':
	//backlight control
	B_set=1;
	backlight_on=(uint8_t)strtoul(optarg, NULL, 0);
	break;
      case 't':
	//time for turning on backlight
	time=(uint8_t)strtoul(optarg, NULL, 0);
	break;
      case 'i':
	//inc ddram addr
	lcd_inc_ddram_addr(&ftdic, ftdi_pin, lcd, (uint8_t)strtoul(optarg, NULL, 0));
	break;
      case 'L':
	//shift left
	lcd_shift_left(&ftdic, ftdi_pin, lcd, (uint8_t)strtoul(optarg, NULL, 0));
	break;
      case 'd':
	//display on
	lcd_display_on(&ftdic, ftdi_pin, lcd, (uint8_t)strtoul(optarg, NULL, 0));
	break;
      case 'c':
	//cursor on
	lcd_cursor_on(&ftdic, ftdi_pin, lcd, (uint8_t)strtoul(optarg, NULL, 0));
	break;
      case 'b':
	//blink
	lcd_blink(&ftdic, ftdi_pin, lcd, (uint8_t)strtoul(optarg, NULL, 0));
	break;
      case 'C':
	//cursor shift
	lcd_cursor_shift(&ftdic, ftdi_pin, lcd, (uint8_t)strtoul(optarg, NULL, 0));
	break;
      case 'D':
	//display shift
	lcd_display_shift(&ftdic, ftdi_pin, lcd, (uint8_t)strtoul(optarg, NULL, 0));
	break;
      case 'w':
	//8 bit or 4 bit
	lcd_bus_width(&ftdic, ftdi_pin, lcd, (uint8_t)strtoul(optarg, NULL, 0));
	break;
      case 'l':
	//2 line or 1 line
	lcd_num_of_line(&ftdic, ftdi_pin, lcd, (uint8_t)strtoul(optarg, NULL, 0));
	break;
      case 'f':
	//5x10 or 5x8
	lcd_font(&ftdic, ftdi_pin, lcd, (uint8_t)strtoul(optarg, NULL, 0));
	break;
      case 'h':
      default:
	usage(argv[0]);
      }
    }

    //tasks related to user options
    if(m_set) lcd_display_text(&ftdic, ftdi_pin, lcd, msg, strlen(msg), line);
    if(B_set) {
      if(backlight_on) {
	backlight(&ftdic, ftdi_pin, backlight_on);
	if(time) {
	  sleep(time);
	  backlight(&ftdic, ftdi_pin, 0);
	}
      }else backlight(&ftdic, ftdi_pin, 0);
    }

    //don't disable the bitbang mode, so that backlight will stay on
    //ftdi_disable_bitbang(&ftdic); 
    ftdi_usb_close(&ftdic);
    ftdi_deinit(&ftdic);

    return(0);
}
