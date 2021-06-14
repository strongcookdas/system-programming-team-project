#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <pthread.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <stdint.h>
#include <wiringPi.h>  

#define IN 0
#define OUT 1
#define LOW 0
#define HIGH 1
#define PIN 24
#define POUT 23

#define BUFFER_MAX 45
#define DIRECTION_MAX 45
#define VALUE_MAX 256
#define MAX_TIME 85  

#define DHT11PIN 7  

int dht11_val[5]={0,0,0,0,0}; 

static int distance = 0;

int serv_sock, clnt_sock = -1, clnt_sock2=-1;
struct sockaddr_in serv_addr, clnt_addr;
socklen_t clnt_addr_size;
char msg[50];

static int light =0;
static const char *DEVICE = "/dev/spidev0.0";
static uint8_t MODE = SPI_MODE_0;
static uint8_t BITS = 8;
static uint32_t CLOCK =1000000;
static uint16_t DELAY =5;

#define ARRAY_SIZE(array) sizeof(array)/sizeof(array[0])

void dht11_read_val()  

{  

  uint8_t lststate=HIGH;  

  uint8_t counter=0;  

  uint8_t j=0,i;  

  float farenheit;  

  for(i=0;i<5;i++)  

     dht11_val[i]=0;  

  pinMode(DHT11PIN,OUTPUT);  

  digitalWrite(DHT11PIN,LOW);  

  delay(18);  

  digitalWrite(DHT11PIN,HIGH);  

  delayMicroseconds(40);  

  pinMode(DHT11PIN,INPUT);  

  for(i=0;i<MAX_TIME;i++)  

  {  

    counter=0;  

    while(digitalRead(DHT11PIN)==lststate){  

      counter++;  

      delayMicroseconds(1);  

      if(counter==255)  

        break;  

    }  

    lststate=digitalRead(DHT11PIN);  

    if(counter==255)  

       break;  

    // top 3 transistions are ignored  

    if((i>=4)&&(i%2==0)){  

      dht11_val[j/8]<<=1;  

      if(counter>30)  

        dht11_val[j/8]|=1;  

      j++;  

    }  

  }  

  
  if((j>=40)&&(dht11_val[4]==((dht11_val[0]+dht11_val[1]+dht11_val[2]+dht11_val[3])& 0xFF)))  

  {  

    farenheit=dht11_val[2]*9./5.+32;  

    //printf("Humidity = %d.%d %% Temperature = %d.%d *C (%.1f *F)\n",dht11_val[0],dht11_val[1],dht11_val[2],dht11_val[3],farenheit);  

  }  

  else  

    printf("Invalid Data!!\n");  

}


static int prepare(int fd){
   if(ioctl(fd, SPI_IOC_WR_MODE, &MODE)==-1){
      perror("Can't set MODE");
      return -1;
      }
   if(ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &BITS)==-1){
      perror("Can't set number of BITS");
      return -1;
      }   
   if(ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &CLOCK)==-1){
      perror("Can't set write CLOCK");
      return -1;
      } 
   if(ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &CLOCK)==-1){
      perror("Can't set read CLOCK");
      return -1;
      }
      
   return 0;
   }

uint8_t control_bits_differential(uint8_t channel){
   return (channel & 7)<<4;
   }
   
uint8_t control_bits(uint8_t channel){
   return 0x8 | control_bits_differential(channel);
}  
 
int readadc(int fd, uint8_t channel){
   uint8_t tx[] ={1, control_bits(channel),0};
   uint8_t rx[3];
   struct spi_ioc_transfer tr ={
      .tx_buf =(unsigned long)tx,
      .rx_buf =(unsigned long)rx,
      .len = ARRAY_SIZE(tx),
      .delay_usecs = DELAY,
      .speed_hz = CLOCK,
      .bits_per_word = BITS,
      };
   if(ioctl(fd, SPI_IOC_MESSAGE(1), &tr)==1){
      perror("IO Error");
      abort();
      }
   
   return((rx[1]<<8)&0x300)|(rx[2]&0xFF);
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
   
   fd=open("/sys/class/gpio/unexport",O_WRONLY);
   if(-1==fd){
      fprintf(stderr,"falied to pen unexport\n");
      return -1;
   }
   
   bytes_written=snprintf(buffer,BUFFER_MAX,"%d",pin);
   write(fd,buffer,bytes_written);
   close(fd);
   return 0;
}

static int GPIODirection(int pin,int dir){
      static const char s_directions_str[]="in\0out";
      
   
   char path[DIRECTION_MAX]="/sys/class/gpio/gpio%d/direction";
   int fd;
   
   snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction",pin);
   
   fd=open(path,O_WRONLY);
   if(-1==fd){
      fprintf(stderr,"Failed to open gpio direction for writing!\n");
      return -1;
   }
   
   if(-1==write(fd,&s_directions_str[IN == dir ? 0 :3], IN==dir ? 2:3)){
      fprintf(stderr,"failed to set direction!\n");
      return -1;
   }
   
   close(fd);
   return 0;
}

static int GPIOWrite(int pin, int value){
      static const char s_values_str[] ="01";
      
      char path[VALUE_MAX];
      int fd;
      
      printf("write value!\n");
      
      snprintf(path,VALUE_MAX, "/sys/class/gpio/gpio%d/value",pin);
      fd=open(path,O_WRONLY);
      if(-1==fd){
         fprintf(stderr,"failed open gpio write\n");
         return -1;
      }
      
      //0 1 selection
      if(1!=write(fd,&s_values_str[LOW==value ? 0:1],1)){
         fprintf(stderr,"failed to write value\n");
         return -1;
      }
      close(fd);
      return 0;
}
static int GPIORead(int pin){
    char path[VALUE_MAX];
    char value_str[3];
    int fd;

    snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value",pin);
    fd=open(path, O_RDONLY);
    if(-1==fd){
        fprintf(stderr,"failed to open gpio value for reading\n");
        return -1;
    }

    if(-1==read(fd,value_str,3)){
        fprintf(stderr,"failed to read val\n");
        return -1;
    }
    close(fd);

    return(atoi(value_str));
}

void *ultrawave_thd(){
   clock_t start_t, end_t;
   double time;
   
   if(-1==GPIOExport(POUT)||-1==GPIOExport(PIN)){
      printf("gpio export err\n");
      exit(0);
   }
   
   usleep(100000);
   if(-1==GPIODirection(POUT,OUT)||-1==GPIODirection(PIN,IN)){
      printf("gpio direction err\n");
      exit(0);
   }
   
   GPIOWrite(POUT,0);
   usleep(10000);
   
   while(1){
      printf("ok");
      if(-1==GPIOWrite(POUT, 1)){
         printf("gpio write/trigger err\n");
         exit(0);
      }
      usleep(10);
      GPIOWrite(POUT,0);
      
      while(GPIORead(PIN)==0){
         start_t=clock();
      }
      while(GPIORead(PIN)==1){
         end_t=clock();
      }
      time =(double)(end_t-start_t)/CLOCKS_PER_SEC;
      distance = time/2*34000;
      
      printf("time 4lf\n",time);
      printf("distance : %d cm\n", distance);
      
      usleep(500000);
   }
}

static void *spi_light(){
   int fd =open(DEVICE, O_RDWR);
   if(fd<=0){
      printf("Device %s not found\n", DEVICE);
      }
   if(prepare(fd)==-1){
      }
   while(1){
      light =readadc(fd,0);
      //printf("light %d\n", light);
      usleep(500000);
      }
   close(fd);
   return 0;
}

static void *th(){
printf("Interfacing Temperature and Humidity Sensor (DHT11) With Raspberry Pi\n");  

  if(wiringPiSetup()==-1)  

    exit(1);  

  while(1)  

  {  

     dht11_read_val();  

     delay(2000);  

  }  
  return 0;   
}

void error_handling(char *message){
    fputs(message,stderr);
    fputc('\n',stderr);
    exit(1);
}

int main(int argc, char *argv[])
{

   pthread_t p_ultra;
   pthread_t p_light;
   pthread_t p_ht;
   
    if (argc != 2)
    {
        printf("Usage : %s <port>\n", argv[0]);
    }
    printf("bb");
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));
    
    if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error");
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");
        
    if (clnt_sock < 0)
    {
        clnt_addr_size = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr,
                           &clnt_addr_size);
        if (clnt_sock == -1)
            error_handling("accept() error");
    }
    
    if (clnt_sock2 < 0)
    {
        clnt_addr_size = sizeof(clnt_addr);
        clnt_sock2 = accept(serv_sock, (struct sockaddr *)&clnt_addr,
                           &clnt_addr_size);
        if (clnt_sock2 == -1)
            error_handling("accept() error");
    }
    
    pthread_create(&p_ultra,NULL,ultrawave_thd,NULL);
    pthread_create(&p_light,NULL,spi_light,NULL);
    pthread_create(&p_ht,NULL,th,NULL);
    
    while (1)
    {
      if(dht11_val[0] == 0 || dht11_val[2] == 0 || dht11_val[0]>100 || dht11_val[2] > 40 || dht11_val[2] < 17) continue;
      snprintf(msg, 50, "%d %d %d %d", distance,light,dht11_val[0],dht11_val[2]);
      write(clnt_sock, msg, sizeof(msg));
      write(clnt_sock2, msg, sizeof(msg));
      printf("msg = %s\n", msg);
      usleep(500000);
      
    }
    int status;
    pthread_join(p_ultra,(void **)&status);
    pthread_join(p_light,(void **)&status);
    pthread_join(p_ht,(void**)&status);
    close(clnt_sock);
    close(serv_sock);

    return (0);
}
