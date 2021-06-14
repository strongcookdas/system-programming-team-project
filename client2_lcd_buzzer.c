/* ----------------------------------------------------------------------- *
 * I2C-byte: D7 D6 D5 D4 BL EN RW RS                                       *
 * ----------------------------------------------------------------------- */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#define IN 0
#define OUT 1
#define PWM 2

#define LOW 0
#define HIGH 1
#define VALUE_MAX 256

#define I2C_BUS        "/dev/i2c-1" // I2C bus device on a Raspberry Pi 3
#define I2C_ADDR       0x27         // I2C slave address for the LCD module
#define BINARY_FORMAT  " %c  %c  %c  %c  %c  %c  %c  %c\n"
#define BYTE_TO_BINARY(byte) \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0') 
  
int lcd_backlight;
int debug;
char address; 
int i2cFile;

char str2[] = "Check the window";
char str3[50];

void i2c_start() {
   if((i2cFile = open(I2C_BUS, O_RDWR)) < 0) {
      printf("Error failed to open I2C bus [%s].\n", I2C_BUS);
      exit(-1);
   }
   // set the I2C slave address for all subsequent I2C device transfers
   if (ioctl(i2cFile, I2C_SLAVE, I2C_ADDR) < 0) {
      printf("Error failed to set I2C address [%d].\n", I2C_ADDR);
      exit(-1);
   }
}

void i2c_stop() { close(i2cFile); }

void i2c_send_byte(unsigned char data) {
   unsigned char byte[1];
   byte[0] = data;
   write(i2cFile, byte, sizeof(byte)); 
   /* -------------------------------------------------------------------- *
    * Below wait creates 1msec delay, needed by display to catch commands  *
    * -------------------------------------------------------------------- */
   usleep(1000);
}

void hextobit(int t){
   //0b + bin number + 1101
   switch(t){
      case 0 :
         i2c_send_byte(0b00001101);
         i2c_send_byte(0b00001001);
         break;
      case 1 :
         i2c_send_byte(0b00011101);
         i2c_send_byte(0b00011001);
         break;
      case 2 :
         i2c_send_byte(0b00101101);
         i2c_send_byte(0b00101001);
         break;
      case 3 :
         i2c_send_byte(0b00111101);
         i2c_send_byte(0b00111001);
         break;
      case 4 :
         i2c_send_byte(0b01001101);
         i2c_send_byte(0b01001001);
         break;
      case 5 : 
         i2c_send_byte(0b01011101);
         i2c_send_byte(0b01011001);
         break;  
      case 6 :
         i2c_send_byte(0b01101101);
         i2c_send_byte(0b01101001);
         break;
      case 7 :
         i2c_send_byte(0b01111101);
         i2c_send_byte(0b01111001);
         break;
      case 8 :
         i2c_send_byte(0b10001101);
         i2c_send_byte(0b10001001);
         break;
      case 9 :
         i2c_send_byte(0b10011101);
         i2c_send_byte(0b10011001);
         break;
      case 10 :
         i2c_send_byte(0b10101101);
         i2c_send_byte(0b10101001);
         break;
      case 11 :
         i2c_send_byte(0b10111101);
         i2c_send_byte(0b10111001);
         break;
      case 12 :
         i2c_send_byte(0b11001101);
         i2c_send_byte(0b11001001);
         break;
      case 13 : 
         i2c_send_byte(0b11011101);
         i2c_send_byte(0b11011001);
         break;  
      case 14 :
         i2c_send_byte(0b11101101);
         i2c_send_byte(0b11101001);
         break;
      case 15 :
         i2c_send_byte(0b11111101);
         i2c_send_byte(0b11111001);
         break;      
      
      default :
         break;
      }
   }

void chartohex(char h[]){
   int len = strlen(h);
   
   for(int i =0; i<len; i++){   
      int let = (int) h[i];
      int t1 = let/16;
      int t2 = let%16;
      printf("%c %d %d", h[i],t1,t2);
      hextobit(t1);
      hextobit(t2);
      }
   }


void dsev_dfour(){
   i2c_send_byte(0b00000100); //
   i2c_send_byte(0b00000000); // D7-D4=0
   }
void disp_off(){
   dsev_dfour();
   i2c_send_byte(0b10000100); //
   i2c_send_byte(0b10000000); // D3=1 D2=display_off, D1=cursor_off, D0=cursor_blink
   }
void disp_on(){
   dsev_dfour();
   i2c_send_byte(0b11100100); //
   i2c_send_byte(0b11100000); // D3=1 D2=display_on, D1=cursor_on, D0=cursor_blink
   }
void eight_bit_init(){
   i2c_send_byte(0b00110100); // D7=0, D6=0, D5=1, D4=1, RS,RW=0 EN=1
   i2c_send_byte(0b00110000); // D7=0, D6=0, D5=1, D4=1, RS,RW=0 EN=0
   }
void disp_clear(){
   dsev_dfour();
   i2c_send_byte(0b00010100); //
   i2c_send_byte(0b00010000); // D0=display_clear
}

static int
PWMExport(int pwmnum)
{
#define BUFFER_MAX 3
   char buffer[BUFFER_MAX];
   int bytes_written;
   int fd;
   
   fd = open("/sys/class/pwm/pwmchip0/unexport", O_WRONLY);
   if(-1 == fd){
      fprintf(stderr, "Failed to open in unexport!\n");
      return(-1);
   }
   
   bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pwmnum);
   write(fd, buffer, bytes_written);
   close(fd);
   
   sleep(1);
   fd = open("/sys/class/pwm/pwmchip0/export", O_WRONLY);
   if(-1 == fd){
      fprintf(stderr, "Failed to open in export!\n");
      return(-1);
   }
   bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pwmnum);
   write(fd, buffer, bytes_written);
   close(fd);
   sleep(1);
   return(0);
}

static int
PWMEnable(int pwmnum){
   static const char s_unenable_str[] = "0";
   static const char s_enable_str[] = "1";
   
#define DIRECTION_MAX 45
   char path[DIRECTION_MAX];
   int fd;
   
   snprintf(path, DIRECTION_MAX,"/sys/class/pwm/pwmchip0/pwm%d/enable", pwmnum);
   
   fd = open(path, O_WRONLY);
   if(-1 == fd){
      fprintf(stderr, "Failed to open in enable!\n");
      return -1;
   }
   
   write(fd, s_unenable_str, strlen(s_unenable_str));
   close(fd);
   
   fd = open(path, O_WRONLY);
   if(-1 == fd){
      fprintf(stderr, "Failed to open in enable!\n");
      return -1;
   }
   write(fd, s_enable_str, strlen(s_enable_str));
   close(fd);
   return(0);
}

static int PWMWritePeriod(int pwmnum, int value){
   char s_values_str[VALUE_MAX];
   char path[VALUE_MAX];
   int fd,byte;
   
   snprintf(path, VALUE_MAX, "/sys/class/pwm/pwmchip0/pwm%d/period",pwmnum);
   fd = open(path, O_WRONLY);
   if(-1==fd){
      fprintf(stderr, "Failed to open period!\n");
      return(-1);
      }
      
   byte = snprintf(s_values_str, 10, "%d", value);   
   
   if(-1==write(fd, s_values_str, byte)){
      fprintf(stderr, "Failed to write value in period!\n");
      close(fd);
      return(-1);
      }
      
   close(fd);
   return(0);
}
static int PWMWriteDutyCycle(int pwmnum, int value){
   char path[VALUE_MAX];
   char s_values_str[VALUE_MAX];
   int fd,byte;
   
   snprintf(path, VALUE_MAX, "/sys/class/pwm/pwmchip0/pwm%d/duty_cycle",pwmnum);
   fd = open(path, O_WRONLY);
   if(-1==fd){
      fprintf(stderr, "Failed to open in duty_cycle!\n");
      return(-1);
      }
   byte = snprintf(s_values_str, 10, "%d", value);   
   
   if(-1==write(fd, s_values_str, byte)){
      fprintf(stderr, "Failed to write value in duty_cycle!\n");
      close(fd);
      return(-1);
      }
      
   close(fd);
   return(0);
   }
   
void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
   
int distance = 0;
int light = 0;
int humid = 0;
int tmp = 0;

int sock;
int str_len;
struct sockaddr_in serv_addr;
char msg[50];

void split(char *msg){
    distance = atoi(strtok(msg," "));
    light = atoi(strtok(NULL," "));
    humid = atoi(strtok(NULL," "));
    tmp = atoi(strtok(NULL," "));
    printf("distance = %d, light = %d, humid = %d, tmp = %d\n", distance, light, humid, tmp);	
}
char state[50];
void *read_data(){
    while (1)
    {
        str_len = read(sock, msg, sizeof(msg));
        if (str_len == -1)
            error_handling("read() error");
        printf("%s\n", msg);
        split(msg);
        printf("-------------------------------read data------------------------------------\n");
        sprintf(str3,"t:%d'C h:%d%%",tmp,humid);
        sprintf(state,"distance:%dcm",distance);
    }
}

void *buzzer(){
      // bell
   while(1){
      if(distance<15){
         PWMWriteDutyCycle(0,50000);
         sleep(2);
      }
      else
      {
         PWMWriteDutyCycle(0,0);   
      }  
   }
}

int main(int argc, char *argv[]) { 
   
   pthread_t p_thread;
   pthread_t p_buzzer;
   
   i2c_start(); 
   debug=1;

   if (argc != 3)
   {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
   }
   
   sock = socket(PF_INET, SOCK_STREAM, 0);
   if (sock == -1)
      error_handling("socket() error");
   memset(&serv_addr, 0, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
   serv_addr.sin_port = htons(atoi(argv[2]));
   if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
      error_handling("connect() error");
      
   
    
   /* -------------------------------------------------------------------- *
    * Initialize the display, using the 4-bit mode initialization sequence *
    * -------------------------------------------------------------------- */
   if(debug) printf("Init Start:\n");
   if(debug) printf("D7 D6 D5 D4 BL EN RW RS\n");

   usleep(15000);             // wait 15msec
   eight_bit_init();
   usleep(4100);              // wait 4.1msec
   eight_bit_init();
   usleep(100);               // wait 100usec
   eight_bit_init();
   usleep(4100);              // wait 4.1msec
   i2c_send_byte(0b00100100); //
   i2c_send_byte(0b00100000); // switched now to 4-bit mode

   /* -------------------------------------------------------------------- *
    * 4-bit mode initialization complete. Now configuring the function set *
    * -------------------------------------------------------------------- */
   usleep(40);                // wait 40usec
   i2c_send_byte(0b00100100); //
   i2c_send_byte(0b00100000); // keep 4-bit mode
   i2c_send_byte(0b10000100); //
   i2c_send_byte(0b10000000); // D3=2lines, D2=char5x8

   /* -------------------------------------------------------------------- *
    * Next turn display off                                                *
    * -------------------------------------------------------------------- */
   usleep(40);                // wait 40usec
   disp_off();
   
   /* -------------------------------------------------------------------- *
    * Display clear, cursor home                                           *
    * -------------------------------------------------------------------- */
   usleep(40);                // wait 40usec
   disp_clear();

   /* -------------------------------------------------------------------- *
    * Set cursor direction                                                 *
    * -------------------------------------------------------------------- */
   usleep(40);                // wait 40usec
   dsev_dfour();
   i2c_send_byte(0b01100100); //
   i2c_send_byte(0b01100000); // print left to right

   /* -------------------------------------------------------------------- *
    * Turn on the display                                                  *
    * -------------------------------------------------------------------- */
   usleep(40);                // wait 40usec
   disp_on();
   if(debug) printf("Init End.\n");
   sleep(1);

   if(debug) printf("Writing Warnings to display\n");
   if(debug) printf("D7 D6 D5 D4 BL EN RW RS\n");

   //
   PWMExport(0);
   PWMWritePeriod(0, 200000);
   PWMWriteDutyCycle(0,0);
   PWMEnable(0);
   
   pthread_create(&p_thread,NULL,read_data,NULL);
   
   /* -------------------------------------------------------------------- *
    * Start writing chars to the display, with BL=on.  *
    * -------------------------------------------------------------------- */
   //change here //16 letter e
   
   pthread_create(&p_buzzer,NULL,buzzer,NULL);
  // chartohex(str1);

   while(1){
      printf("< < < distance = %d > > >\n", distance);
      if(distance<15){
         disp_clear();
         chartohex(str2);
         usleep(1000000);
         disp_clear();
         chartohex(state);
         usleep(1000000);
         
      }
      else{
         disp_clear();
         chartohex(str3);
         usleep(1000000);
      }

   }
   
   // temp & humid
   int status;
   pthread_join(p_thread,(void **)&status);
   pthread_join(p_buzzer,(void **)&status);
   close(sock);
   i2c_stop(); 
 }
