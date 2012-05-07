// Program use to control Dangerous Prototypes LCD backpack (FTDI-based).
// http://dangerousprototypes.com/docs/USB_Universal_LCD_backpack
//
// Compile: gcc -o dp_lcd_backpack_utils -lftdi dp_lcd_backpack_utils.c

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

#define LCD_DATA_SHIFT 6

#define LCD_INSTR 0
#define LCD_DATA  1
#define LCD_WRITE 0
#define LCD_READ  1

#define MAX_STR_LEN 0x40

typedef unsigned char byte;

//helper function for memory allocation
#define MALLOC(buf, type, n, err_code) {				\
    if((buf = (type *)malloc(sizeof(type) * n))==NULL) {		\
      fprintf(stderr, "malloc error: File:%s, Line:%d\n", __FILE__, __LINE__); \
      return(err_code);}						\
  }

int ftdi_get_pin_state(struct ftdi_context *ftdic, byte *ftdi_pin)
{
  int ret;

  if((ret=ftdi_set_bitmode(ftdic, 0, BITMODE_BITBANG))<0){
    fprintf(stderr, "Error: %s\n",ftdi_get_error_string(ftdic));
    return(ret);
  }
  if((ret=ftdi_read_pins(ftdic, ftdi_pin))<0){
    fprintf(stderr, "Error: %s\n",ftdi_get_error_string(ftdic));
    return(ret);
  }

  return(0);
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

int backlight_on(struct ftdi_context *ftdic, byte *ftdi_pin)
{
  int ret;
  byte buf;

  *ftdi_pin&=~BL_nEN;
  *ftdi_pin&=~BL_nOC;

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

int backlight_off(struct ftdi_context *ftdic, byte *ftdi_pin)
{
  int ret;

  *ftdi_pin|=BL_nEN;
  *ftdi_pin|=BL_nOC;

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

int sr_tx_byte(struct ftdi_context *ftdic, byte *ftdi_pin, byte data)
{
  byte buf[40]={0};
  unsigned int i, j;
  int ret;

  //set RCLK low
  buf[0] = *ftdi_pin & (~SR_RCLK);

  //shift in bit by bit, starting from MSB
  //set SCLK to low
  //set SER to required value
  //set SCLK to high
  for(i=0; i<8; i++) {
    j=i*4;
    buf[j+1]=buf[j] & (~SR_SCLK);
    if(data & (1<<(7-i))) buf[j+2]=buf[j+1] | SR_SER;
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
  //sr_output_enable(ftdic, ftdi_pin);
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

//init lcd
//clear display: set IR=1
int lcd_init(struct ftdi_context *ftdic, byte *ftdi_pin)
{

  //clear display (0x1)
  lcd_write(ftdic, ftdi_pin, LCD_INSTR, 0x1);
  
  //set function              (0x20)
  //  DL: 8 bit / 4 bit       (0x10)
  //  N: 2 lines / 1 line     (0x08)
  //  F: 5x10 dots / 5x8 dots (0x04)
  //set 8 bit interface, dual line, 5x8 dots
  lcd_write(ftdic, ftdi_pin, LCD_INSTR, 0x20 | 0x10 | 0x08);

  //Display on/off control (0x08)
  //  D: Display on/off    (0x04)
  //  C: Cursor on/off     (0x02)
  //  B: Blinking on/off   (0x01)
  lcd_write(ftdic, ftdi_pin, LCD_INSTR, 0x08|0x04|0x02);

  //Entry mode                 (0x04)
  //  I/D: increment/decrement (0x02)
  //  shift/no shift           (0x01)
  lcd_write(ftdic, ftdi_pin, LCD_INSTR, 0x04 | 0x02);

  return(0);
}


//lcd display text
//display must already be turned on
int lcd_display_text(struct ftdi_context *ftdic, byte *ftdi_pin, char *msg, byte len, byte line)
{
  byte i;
  
  //line can only be 1 or 2
  if(line==1) lcd_write(ftdic, ftdi_pin, LCD_INSTR, 0x80);
  else if (line==2) lcd_write(ftdic, ftdi_pin, LCD_INSTR, 0x80|0x40);
  else {
    fprintf(stderr, "Error displaying text. Only 1 or 2 line display supported\n");
    return(-1);
  }

  if(len>16) {
    fprintf(stderr, "Error displaying text. Maximum no. of char is 16\n");
    return(-2);
  }

  for(i=0; i<len; i++)
    lcd_write(ftdic, ftdi_pin, LCD_DATA, msg[i]);

  return(0);
}

//lcd toggle blinking

//lcd scroll display

void usage(char *cmd)
{

}

int main(int argc, char **argv)
{
    struct ftdi_context ftdic;
    int f,c;
    byte i_set=0, m_set=0;
    byte *ftdi_pin, line=1;
    char *msg;
    char *str1="   Dangerous   ";
    char *str2="  Prototypes!  ";


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

    MALLOC(msg, char, MAX_STR_LEN+1, -1);


    while((c=getopt(argc, argv, "m:l:i"))!=-1) {
      switch(c) {
      case 'i':
	//initialise LCD
	i_set=1;
	break;
      case 'm':
	//specify the message to display
	strncpy(msg, optarg, MAX_STR_LEN);
	m_set=1;
	break;
      case 'l':
	//indicates which line to display the message
	line=(uint8_t)strtoul(optarg, NULL, 0);
	break;
      default:
	usage(argv[0]);
      }
    }


    //some initialisation tasks
    MALLOC(ftdi_pin, byte, 1, -1);
    *ftdi_pin=0;

    sr_clear(&ftdic, ftdi_pin);
    sr_output_enable(&ftdic, ftdi_pin);
    sr_unclear(&ftdic, ftdi_pin);

    //tasks related to user options
    if(i_set) lcd_init(&ftdic, ftdi_pin);


    backlight_on(&ftdic, ftdi_pin);

    lcd_display_text(&ftdic, ftdi_pin, str1, strlen(str1), 1);
    lcd_display_text(&ftdic, ftdi_pin, str2, strlen(str2), 2);
    sleep(3);

    backlight_off(&ftdic, ftdi_pin);

    ftdi_disable_bitbang(&ftdic);
    ftdi_usb_close(&ftdic);
    ftdi_deinit(&ftdic);

    return(0);
}
