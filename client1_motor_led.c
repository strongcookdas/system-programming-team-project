#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <arpa/inet.h>
#include <sys/socket.h>


#define BUFFER_MAX 3
#define DIRECTION_MAX 45
#define VALUE_MAX 256

#define IN 0
#define OUT 1
#define LOW 0
#define HIGH 1
#define L_POUT 17
#define POUT 23
#define PIN 24
#define PWM 2

int distance = 0;
int light=0;
int humid=0;
int tmp=0;

int twindow = 0;
  int tcurtain = 0;
int status=0;

int check=0;   // no error, error=1
int sock;
struct sockaddr_in serv_addr;
char msg[50];
char on[2] = "1";
int str_len;
int LED = 0;
int window=0;
int curtain=0;
int opennum = 0;
int closenum = 1;
int lock = 0;
   
   
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

static int GPIOExport(int pin){
   char buffer[BUFFER_MAX];
   ssize_t bytes_written;
   int fd;
   
   fd = open("/sys/class/gpio/export", O_WRONLY);
   if(-1==fd){
      fprintf(stderr,"failed open export for writing'\n'");
      return(-1);
   }
   
   bytes_written =snprintf(buffer,BUFFER_MAX,"%d",pin);
   write(fd,buffer,bytes_written);
   close(fd);
   return(0);
}

static int GPIOUnexport(int pin){
   char buffer[BUFFER_MAX];
   ssize_t bytes_written;
   int fd;
   fd =open("/sys/class/gpio/unexport",O_WRONLY);
   if(-1==fd){
      fprintf(stderr, "Failed to open gpio direction for writing'\n'");
      return(-1);
   }
   bytes_written = snprintf(buffer, BUFFER_MAX,"%d", pin);
   write(fd, buffer, bytes_written);
   close(fd);
   return(0);
}

static int GPIODirection(int pin, int dir){
   static const char s_directions_str[] = "in\0out";
   
   char path[DIRECTION_MAX]="/sys/class/gpio/gpio%d/direction";
   int fd;
   
   snprintf(path, DIRECTION_MAX,"/sys/class/gpio/gpio%d/direction",pin);
   
   fd = open(path, O_WRONLY);
   if(-1==fd){
      fprintf(stderr, "Failed to open gpio direction for writing!\n");
      return(-1);
   }
   
   if(-1==write(fd, &s_directions_str[IN==dir ? 0:3],IN==dir ? 2:3)){
      fprintf(stderr,"Failed to set direction!\n");
      close(fd);
      return(-1);
   }
   
   close(fd);
   return(0);
}

static int GPIORead(int pin){
   char path[VALUE_MAX];
   char value_str[3];
   int fd;
   
   snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value",pin);
   fd =open(path,O_RDONLY);
   if(-1==fd){
      fprintf(stderr, "Failed to open gpio value for reading!\n");
      return(-1);
   }
   if(-1==read(fd, value_str,3)){
      fprintf(stderr,"Failed to read value!\n");
      close(fd);
      return(-1);
   }
   close(fd);

   return(atoi(value_str));
}

static int GPIOWrite(int pin, int value){
   static const char s_values_str[]="01";
   
   char path[VALUE_MAX];
   int fd;
   
   snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value",pin);
   fd =open(path,O_WRONLY);
   if(-1==fd){
      fprintf(stderr, "Failed to open gpio value for writing!\n");
      return(-1);
   }
   
   if(1!=write(fd, &s_values_str[LOW==value ? 0:1],1)){
      fprintf(stderr,"Failed to write value!\n");
      close(fd);
      return(-1);
   }
   close(fd);
   return(0);
}


void OpenCurtain(){
    for(int i=0; i<250; i++){
	PWMWriteDutyCycle(1,i*10000);
	usleep(5000);
    }
}


void OpenWindow(){	
   for(int i=0; i<200; i++){
      PWMWriteDutyCycle(0,i*10000);
      usleep(5000);
   }
}

void CloseCurtain(){
    for(int i=250; i>0; i--){
         PWMWriteDutyCycle(1,i*10000);
         usleep(5000);
    } 
}
    
    
void CloseWindow(){
     for(int i=200; i>0; i--){
         PWMWriteDutyCycle(0,i*10000);
         usleep(5000);
	}     
}


void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

void split(char *msg){
    distance = atoi(strtok(msg," "));
    light = atoi(strtok(NULL," "));
    humid = atoi(strtok(NULL," "));
    tmp = atoi(strtok(NULL," "));
    printf("distance = %d, light = %d, humid = %d, tmp = %d\n", distance, light, humid, tmp);	
}


void lock_off(){
	if(-1 == GPIOWrite(L_POUT,0))
		printf("failed led_off");
}

void lock_on(){
	if(-1 == GPIOWrite(L_POUT,1))
		printf("failed led_on");
}

void *gain_data(){
   while (1)
    {
        str_len = read(sock, msg, sizeof(msg));
        if (str_len == -1)
            error_handling("read() error");
        printf("Receive message from Server : %s\n", msg);
        split(msg);
   }
   return 0;
}

void *act_curtain(){
   while(1){
      // curtain
      if(distance < 15){
         tcurtain = closenum;
         //printf("curtain--------------------------------------");
      }
		else if(light < 100 || light > 900){
			tcurtain = closenum;
		}
      else {
         tcurtain = opennum;
      }
		
		//run system
		if(curtain==closenum&&tcurtain==opennum){
         OpenCurtain();
         curtain = opennum;
      }
		else if (curtain==opennum &&tcurtain==closenum){
			CloseCurtain();
			curtain = closenum;
        sleep(5);
		}
   }
}



int main(int argc, char *argv[]){
   
    pthread_t p_data;
    pthread_t p_curtain;
    
    // window
    PWMExport(0);
    PWMWritePeriod(0, 5000000);
    PWMWriteDutyCycle(0,0);
    PWMEnable(0);
    
   // curtain
    PWMExport(1);
    PWMWritePeriod(1, 5000000);
    PWMWriteDutyCycle(1,0);
    PWMEnable(1);
    
   if(-1==GPIOExport(L_POUT)) return(1);
	
	if(-1==GPIODirection(L_POUT, OUT)) return(2);
    
    if (argc != 3)
    {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }
    //Enable GPIO pins
    if (-1 == GPIOExport(POUT))
        return (1);
    //Set GPIO directions
    if (-1 == GPIODirection(POUT, OUT))
        return (2);
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        error_handling("socket() error");
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");
   
   
   pthread_create(&p_data,NULL,gain_data,NULL);
   pthread_create(&p_curtain,NULL,act_curtain,NULL);
    while(1){
   
      if(distance < 15){
         // closenum window and lock
         twindow = closenum;
         if(distance < 8) {
            lock = closenum;
            lock_on();
         }
         //CloseWindow();
      }
      else if(humid >= 92){
         twindow = closenum;
         //CloseWindow();
      }
      else if(tmp <= 23){
         twindow = closenum;
         //CloseWindow();
      }
      else if(tmp>24){
         twindow = opennum;
         //OpenWindow();
      }
      else if(light < 100){
         twindow = closenum;
         //CloseWindow();
      }
      
      //run system
      if(window==closenum&&twindow==opennum){
         if(lock==closenum){
            lock =opennum;
            lock_off();
         }
         OpenWindow();
         window = opennum;
      }
      else if (window==opennum&&twindow==closenum){
         CloseWindow();
         window = closenum;
        sleep(5);
      }
      
   }
   
	pthread_join(p_data,(void **)&status);
   pthread_join(p_curtain,(void**)&status);
    
    close(sock);
    //Disable GPIO pins
    if (-1 == GPIOUnexport(POUT))
        return (4);
    return (0);
   
}
