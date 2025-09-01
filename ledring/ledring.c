/*
 * LED Ring control
 *
 * Copyright (C) 2025 Terry Sanders.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 */
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <linux/i2c-dev.h>
#include <sys/time.h>

#define LED_ANIMATIONS    "led-animation"
#define MAX_LOOPS       10
#define MAX_ANIMATIONS  512
#define MAX_NAME_LENGTH 32
#define FILE_EXTENSION  ".anim"

#define I2C_DEVICE        "/dev/i2c-1"
#define I2C_ADDRESS       0x3f
#define LML_SHUTDOWN_REG       0x00
#define LML_PWM_BASE_REG       0x01
#define LML_PWM_UPDATE_REG     0x25
#define LML_LED_BASE_REG       0x26
#define LML_GLOBAL_CONTROL_REG 0x4a
#define LML_FREQ_SET_REG       0x4b
#define LML_RESET_REG          0x4f

static struct option Options[] = {
  {"help",        no_argument,       NULL,  'h' },    // Help
  {"i2c",         required_argument, NULL,  'i' },    // i2c device
  {"anim_dir",    required_argument, NULL,  'a' },    // Directory containing animations
  {"loops",       required_argument, NULL,  'l' },    // Maximum number of animation loops
  {0,             0,                 0,  0 }
};
typedef struct _Frame *Frame;
struct _Frame {
    uint32_t Delay;
    uint8_t  LED[48];
    Frame  Next;
};

typedef struct _Animation *Animation;
struct _Animation {
    char Name[MAX_NAME_LENGTH];
    Frame First;
    Frame Loop;
};

struct _Animation Animations[MAX_ANIMATIONS];
uint32_t AnimationCount;

// Recursive search for animation
// Returns matching Animation or NULL
static Animation FindAnimation(char *Name,int32_t Begin,int32_t End) {
    // Begin will never be OOB but End might
    // printf("FS B:%i E:%i\n",Begin,End);
    if(Begin >= End) {
      if(strcmp(Animations[Begin].Name,Name) == 0)
        return &Animations[Begin];
      else
        return NULL;
    }
    uint32_t middle = (Begin + End) / 2;
    int n = strcmp(Animations[middle].Name,Name);
    if(n > 0) {
      return FindAnimation(Name,Begin,middle - 1);
    } else if( n < 0 ) {
      return FindAnimation(Name,middle + 1,End);
    }
    return &Animations[middle];
}
// Insert a new animation into the list
// and return its index or -1 on failure
static int InsertAnimation(char *Name) {

    if(AnimationCount >= MAX_ANIMATIONS) {
      fprintf(stderr,"No space for animation %s\n",Name);
      return -1;
    }
    int animidx;
    for(animidx = 0; animidx < AnimationCount; animidx++)
      if(strcmp(Name,Animations[animidx].Name) < 0)
        break;

    for(int i = AnimationCount; i > animidx; i--)
      memcpy(&Animations[i],&Animations[i-1],sizeof(Animations[0]));

    AnimationCount++;
    return animidx;
}
static int LoadAnimation(char *Path,char *AnimFile) {
    char file[PATH_MAX];
    FILE *fptr;

    // Generate the file name
    snprintf(file,PATH_MAX,"%s/%s",Path,AnimFile);
    // Try and open it
    if((fptr = fopen(file,"r")) == NULL) {
      fprintf(stderr,"Error opening file %s:%s\n",AnimFile,strerror(errno));
      return -1;
    }
    // Extract animation name from file name
    // Remove directory part
    char *name = rindex(file,'/');
    name = name ? name+1 : file;
    // Remove any file extension
    char *s = rindex(name,'.');
    if(s)
      *s = 0;

    // Get its insert position in the array
    uint32_t animidx = InsertAnimation(name);
    if(animidx < 0) {
      fclose(fptr);
      return -1;
    }
    // Zero out where it is going
    memset(&Animations[animidx],0,sizeof(Animations[0]));
    // Copy the name
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
    strncpy(Animations[animidx].Name,name,MAX_NAME_LENGTH);
#pragma GCC diagnostic pop
    //
    // Parse the animation frames.
    //   Not being precise here so,
    //   for example, "loops","loopx",
    //   "loopqwerty" all match the
    //   keyword "loop"
    //
    Animation anim = &Animations[animidx];
    char inbuf[BUFSIZ];
    int setloop = 0;
    int lineno = 0;
    Frame prev = NULL;
    Frame f;
    while(fgets(inbuf,BUFSIZ,fptr)) {
      int32_t delay;
      uint8_t led[48];
      char *s;

      lineno++;

      // Truncate at any comment
      if((s = index(inbuf,'#')) != NULL)
        *s = 0;
      // Skip any leading whitespace
      for(s = inbuf; *s && isspace(*s);s++)
        ;
      // Nothing left? Then next line
      if( *s == 0 )
        continue;
      // Is it the "loop" keyword?
      if(strncasecmp(s,"loop",4) == 0) {
        if(anim->Loop || setloop) {
          fprintf(stderr,"Multiple 'loop' directives in %s. Ignoring.\n",anim->Name);
          continue;
        }
        setloop = 1;
        continue;
      }
      // Only thing left is the led settings.
      // Extract the delay
      errno = 0;
      delay = strtoul(s, &s, 10);
      if(errno) {
        fprintf(stderr,"Error parsing delay. %s line %i\n",anim->Name,lineno);
        continue;
      }
      // Skip past the ':' (or whatever it was)
      if(*s)
        s++;
      // Extract individual settings
      int n;
      for(n = 0; n < 12; n++) {
        // Skip leading whitespace so as
        // to get accurate string length
        while(*s && isspace(*s))
          s++;
        char *sb = s;
        errno = 0;
        int32_t rgb = strtoul(s, &s, 16);
        if(errno) {
          fprintf(stderr,"Bad led definition. %s line %i\n",anim->Name,lineno);
          break;
        }
        int len = s - sb;
        // Length MUST be 3 or 6 and indicates
        // values of RGB or RRGGBB.
        if(len == 3) {
          led[0 + n*3] = (rgb >> 4) & 0xf0;
          led[1 + n*3] = (rgb     ) & 0xf0;
          led[2 + n*3] = (rgb << 4) & 0xf0;
          // Mirror upper nibble to lower so
          // that 0xd0 becomes 0xdd for example
          led[0 + n*3] |= led[0 + n*3] >> 4;
          led[1 + n*3] |= led[1 + n*3] >> 4;
          led[2 + n*3] |= led[2 + n*3] >> 4;
        } else if(len == 6) {
          led[0 + n*3] = (rgb >> 16) & 0xf0;
          led[1 + n*3] = (rgb >>  8) & 0xf0;
          led[2 + n*3] = (rgb      ) & 0xf0;
        } else {
          fprintf(stderr,"Bad led definition. %s line %i\n",anim->Name,lineno);
          break;
        }
        // Skip past the ',' (or whatever it was)
        if(*s)
          s++;
      }
      // If n != 12 then errors or line to short
      if(n != 12) {
        fprintf(stderr,"Error %s line %i. Ignoring line\n",anim->Name,lineno);
        continue;
      }
      // Create and populate the frame
      f = calloc(1,sizeof(struct _Frame));
      f->Delay = delay * 1000;
      for(int i=0; i < 48; i++)
        f->LED[i] = led[i];
      // Insert into animation
      if(anim->First) {
        prev->Next = f;
        prev = f;
      } else {
        prev = anim->First = f;
      }
      // Setup loop
      if(setloop) {
        anim->Loop = f;
        setloop = 0;
      }
    }
    fclose(fptr);
    return 0;
}
static int LoadResources(char *Dir) {
    DIR *d;
    struct dirent *de;
    if(!(d = opendir(Dir))) {
      fprintf(stderr,"Error opening directory %s:%s\n",Dir,strerror(errno));
      return -1;
    }
    while((de = readdir(d))) {
      char *s = rindex(de->d_name,'.');
      if(s == NULL || strcmp(s,FILE_EXTENSION))
        continue;
      //printf("Entry: %s\n",de->d_name);
      LoadAnimation(Dir,de->d_name);
    }
    closedir(d);
    return 0;
}
static int I2CwriteReg(int FileD,uint8_t Register,uint8_t Value) {
    uint8_t buf[2];
    buf[0] = Register;
    buf[1] = Value;
    if(write(FileD,buf,2) < 0) {
      fprintf(stderr,"Error writing to i2c device:%s\n",strerror(errno));
      return -1;
    }
    return 0;
}
static int I2CwriteArray(int FileD,uint8_t Register,uint8_t *Values,int Length) {
    uint8_t buf[257];
    // Maximum Length is 256 so adjust if greater
    Length = Length > 0x100 ? 0x100 : Length;
    // Copy into a new buffer
    buf[0] = Register;
    memcpy(&buf[1],Values,Length);
    if(write(FileD,buf,Length+1) < 0) {
      fprintf(stderr,"Error writing to i2c device:%s\n",strerror(errno));
      return -1;
    }
    return 0;
}
static int usage(const char *Name) {
    const char *s;

    if( (s = strrchr(Name,'/')) == NULL)
      s = Name;
    else
      s++;

    fprintf(stderr,"usage: %s [options]\n",s);
    fprintf(stderr,"\nOptions:\n");
    fprintf(stderr," %-30s%s\n","-h, --help","This message");
    fprintf(stderr," %-30s%s\n","-i, --i2c <device>","i2c device. Default: " I2C_DEVICE);
    fprintf(stderr," %-30s%s\n","-a, --anim_dir <directory>","Directory with animations. Default: " LED_ANIMATIONS);
    fprintf(stderr," %-30s%s\n","-l, --loops <count>","Number of time to execute animation loop. Default: 10");
    exit(-1);
}
int main(int ac, char *av[]) {
    char *i2cdev  = I2C_DEVICE;
    char *ledanim = LED_ANIMATIONS;
    int  maxloops = MAX_LOOPS;
    //int interactive = 0;
    //int demo = 0;

    int c;
    int idx;
    while ( (c = getopt_long(ac, av, "i:a:l:h",Options,&idx)) >= 0) {
      switch (c) {
        case 'h': usage(av[0]); break;
        case 'i': i2cdev = optarg; break;
        case 'a': ledanim = optarg; break;
        case 'l': maxloops = strtol(optarg,NULL,0); break;
        case '?':
        default:  usage(av[0]); break;
      }
    }
    LoadResources(ledanim);
//    SaveResources("test");
    // Open i2c device
    int i2cfd = open(i2cdev,O_RDWR);
    if(i2cfd < 0) {
      fprintf(stderr,"Could not open device file %s: %s\n",i2cdev,strerror(errno));
      return -1;
    }
    // Set the address
    if(ioctl(i2cfd, I2C_SLAVE, I2C_ADDRESS) < 0) {
      fprintf(stderr,"Error setting i2c address:%s\n",strerror(errno));
      return -1;
    }
    // Reset the device
    I2CwriteReg(i2cfd,LML_RESET_REG,0x00);
    // Set Normal Operation
    I2CwriteReg(i2cfd,LML_SHUTDOWN_REG,0x01);
    // Set output frequency to 22kHz
    I2CwriteReg(i2cfd,LML_FREQ_SET_REG,0x01);
    // Enable and set max output current
    for(uint8_t i = 0; i < 36;i++)
      I2CwriteReg(i2cfd,LML_LED_BASE_REG+i,0x01);
    // Set Luminosity of the registers
    for(uint8_t i = 0; i < 36;i++)
      I2CwriteReg(i2cfd,LML_PWM_BASE_REG,0x00);
    // Update
    I2CwriteReg(i2cfd,LML_GLOBAL_CONTROL_REG,0x00);

    // Play any animations from command line
    while(optind < ac) {
      Animation a = FindAnimation(av[optind++],0,AnimationCount-1);
      if(a) {
        printf("Playing %s\n",a->Name);
        struct timeval lastframe;
        int delay = 0;
        int max_loop = maxloops;
        for(Frame f = a->First; f && max_loop;) {
          // Set up the LEDs
          I2CwriteArray(i2cfd,LML_PWM_BASE_REG,f->LED,36);
          // Wait for the previous frame to finish
          if(delay) {
            // Calculate the elapsed time since
            // last frame was displayed
            struct timeval now,elapsed;
            gettimeofday(&now,NULL);
            timersub(&now,&lastframe,&elapsed);
            suseconds_t remain = delay - (elapsed.tv_sec * 1000000 + elapsed.tv_usec);
            // sleep unless running late
            if(remain > 0) {
              usleep(remain);
            }
          }
          // Display the frame
          I2CwriteReg(i2cfd,LML_PWM_UPDATE_REG,0x00);
          // Note the time and delay for this frame
          gettimeofday(&lastframe,NULL);
          delay = f->Delay;
          f = f->Next;
          if(f == NULL) {
            f = a->Loop;
            max_loop--;
          }
        }
        // Wait for last frame to finish
        if(delay) {
          usleep(delay);
        }
      }
    }
    // Reset the LEDs
    for(uint8_t i = 0; i < 36;i++) {
      I2CwriteReg(i2cfd,LML_PWM_BASE_REG+i,0);
    }
    I2CwriteReg(i2cfd,LML_PWM_UPDATE_REG,0x00);
    return 0;
}
